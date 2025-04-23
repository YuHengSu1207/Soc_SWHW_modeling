#include "SystolicArray.hh"

SystolicArray::SystolicArray(const std::string& name) : acalsim::CPPSimBase(name) {
	LABELED_INFO(this->getName()) << "Constructing SystolicArray...";
	// register the slave port for MMIO
	this->addSlavePort("bus-s", 1);
}

SystolicArray::~SystolicArray() {}

void SystolicArray::init() {
	// Master ports for issuing memory requests
	m_req_  = this->getPipeRegister("bus-m");
	m_resp_ = this->getPipeRegister("bus-m-2");
}

void SystolicArray::step() {
	// Try to send any outstanding packets
	if (!req_Q_.empty() || !resp_Q_.empty()) { trySendPacket(); }

	// Handle incoming packets on slave ports
	auto rc = acalsim::top->getRecycleContainer();
	for (auto& sp : this->s_ports_) {
		if (!sp.second->isPopValid()) continue;
		auto pkt            = sp.second->pop();
		int  delay_lentency = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");
		/* READ‑REQUEST ------------------------------------------------ */
		if (auto rd = dynamic_cast<XBarMemReadReqPacket*>(pkt)) {
			assert(rd->getPayloads().size() == rd->getBurstSize());
			auto payload = rd->getPayloads();
			auto addr0   = payload[0]->getAddr();

			/* decide MMIO vs. on‑chip SRAM */
			bool toSram = (addr0 >= SA_MEMORY_BASE && addr0 < SA_MEMORY_BASE + SA_SRAM_SIZE);

			/* prepare burst tracker */
			pending_[rd->getAutoIncTID()].expected = payload.size();
			if (toSram) {
				for (int i = 0; i < rd->getBurstSize(); i++) {
					acalsim::LambdaEvent<void()>* event =
					    new acalsim::LambdaEvent<void()>([this, i, payload, delay_lentency]() {
						    this->SRAMReadReqHandler(acalsim::top->getGlobalTick() + i + delay_lentency, payload[i]);
					    });
					this->scheduleEvent(event, acalsim::top->getGlobalTick() + i + delay_lentency);
				}
			} else { /* legacy MMIO path (unchanged) */
				assert(payload.size() == 1);
				acalsim::LambdaEvent<void()>* event =
				    new acalsim::LambdaEvent<void()>([this, payload, delay_lentency]() {
					    this->readMMIO(acalsim::top->getGlobalTick() + delay_lentency, payload[0]);
				    });
				this->scheduleEvent(event, acalsim::top->getGlobalTick() + delay_lentency);
			}
			rc->recycle(rd);
		}
		/* WRITE‑REQUEST --------------------------------------------- */
		else if (auto wr = dynamic_cast<XBarMemWriteReqPacket*>(pkt)) {
			assert(wr->getPayloads().size() == wr->getBurstSize());
			auto payload = wr->getPayloads();
			auto addr0   = payload[0]->getAddr();
			bool toSram  = (addr0 >= SA_MEMORY_BASE && addr0 < SA_MEMORY_BASE + SA_SRAM_SIZE);

			pending_[wr->getAutoIncTID()].expected = payload.size();

			if (toSram) {
				for (int i = 0; i < wr->getBurstSize(); i++) {
					acalsim::LambdaEvent<void()>* event =
					    new acalsim::LambdaEvent<void()>([this, i, payload, delay_lentency]() {
						    this->SRAMWriteReqHandler(acalsim::top->getGlobalTick() + i + delay_lentency, payload[i]);
					    });
					this->scheduleEvent(event, acalsim::top->getGlobalTick() + i + delay_lentency);
				}
			} else {
				assert(payload.size() == 1);
				acalsim::LambdaEvent<void()>* event =
				    new acalsim::LambdaEvent<void()>([this, payload, delay_lentency]() {
					    this->writeMMIO(acalsim::top->getGlobalTick() + delay_lentency, payload[0]);
				    });
				this->scheduleEvent(event, acalsim::top->getGlobalTick() + delay_lentency);
			}
			rc->recycle(wr);
		}
		/* RESPONSE packets coming back from *downstream* ------------ */
		else if (auto rresp = dynamic_cast<XBarMemReadRespPacket*>(pkt)) {
			handleReadResponse(rresp);
		} else if (auto wresp = dynamic_cast<XBarMemWriteRespPacket*>(pkt)) {
			handleWriteCompletion(wresp);
		} else {
			CLASS_ERROR << "Unknown packet type in SystolicArray::step";
		}
	}
}

void SystolicArray::trySendPacket() {
	// request channel
	if (!req_Q_.empty() && !m_req_->isStalled()) {
		m_req_->push(req_Q_.front());
		req_Q_.pop();
	}
	// response channel
	if (!resp_Q_.empty() && !m_resp_->isStalled()) {
		m_resp_->push(resp_Q_.front());
		resp_Q_.pop();
	}
}

void SystolicArray::masterPortRetry(const std::string& portName) { trySendPacket(); }

void SystolicArray::readMMIO(acalsim::Tick when, XBarMemReadReqPayload* req) {
	uint32_t addr = req->getAddr() & 0x3F;
	uint32_t data = 0;
	switch (addr) {
		case 0x0: data = enabled_ ? 1 : 0; break;
		case 0x4: data = done_ ? 1 : 0; break;
		case 0x8: data = (M_ << 16) | K_; break;
		case 0xC: data = (K_ << 16) | N_; break;
		case 0x10: data = (M_ << 16) | N_; break;
		case 0x14: data = A_addr_; break;
		case 0x18: data = B_addr_; break;
		case 0x1C: data = C_addr_; break;
		case 0x20: data = (strideA_ & 0xFF) | ((strideB_ & 0xFF) << 8) | ((strideC_ & 0xFF) << 16); break;

		default: LABELED_ERROR(this->getName()) << "Invalid read addr " << std::hex << addr;
	}
	// build response
	auto rc   = acalsim::top->getRecycleContainer();
	auto resp = rc->acquire<XBarMemReadRespPayload>(&XBarMemReadRespPayload::renew, req->getInstr(), req->getOP(), data,
	                                                req->getA1());
	resp->setTid(req->getTid());
	std::vector<XBarMemReadRespPayload*> beats = {resp};
	auto                                 pkt   = Construct_MemReadRespPkt(beats, "sa", req->getCaller());
	resp_Q_.push(pkt);
	rc->recycle(req);
}

/* ------------------------------------------------------------- */
/* read / write handlers (one beat each)                         */
/* ------------------------------------------------------------- */
void SystolicArray::SRAMReadReqHandler(acalsim::Tick when, XBarMemReadReqPayload* p) {
	instr      i    = p->getInstr();
	instr_type op   = p->getOP();
	uint32_t   addr = p->getAddr();
	operand    a1   = p->getA1();
	uint32_t   ret  = 0;

	// Base pointer to sram_ as byte array
	uint8_t* mem     = reinterpret_cast<uint8_t*>(sram_);
	size_t   memSize = SA_SRAM_SIZE * sizeof(uint32_t);  // 32000 bytes

	// Check bounds before read
	if (addr < SA_MEMORY_BASE || addr >= SA_MEMORY_BASE + memSize) {
		CLASS_ERROR << "SRAM read out of range at 0x" << std::hex << addr;
		return;
	}

	uint32_t offset = addr - SA_MEMORY_BASE;

	switch (op) {
		case LB: ret = static_cast<uint32_t>(static_cast<int8_t>(mem[offset])); break;
		case LBU: ret = static_cast<uint32_t>(mem[offset]); break;
		case LH:
			if (offset + 1 >= memSize) {
				CLASS_ERROR << "SRAM LH read out of bounds at 0x" << std::hex << addr;
				return;
			}
			ret = static_cast<uint32_t>(*reinterpret_cast<int16_t*>(mem + offset));
			break;
		case LHU:
			if (offset + 1 >= memSize) {
				CLASS_ERROR << "SRAM LHU read out of bounds at 0x" << std::hex << addr;
				return;
			}
			ret = static_cast<uint32_t>(*reinterpret_cast<uint16_t*>(mem + offset));
			break;
		case LW:
			if (offset + 3 >= memSize) {
				CLASS_ERROR << "SRAM LW read out of bounds at 0x" << std::hex << addr;
				return;
			}
			ret = *reinterpret_cast<uint32_t*>(mem + offset);
			break;
		default: CLASS_ERROR << "Unsupported load OP: " << static_cast<int>(op); return;
	}

	// Construct read response
	auto rc  = acalsim::top->getRecycleContainer();
	auto rsp = rc->acquire<XBarMemReadRespPayload>(&XBarMemReadRespPayload::renew, i, op, ret, a1);
	rsp->setTid(p->getTid());

	auto& tk = pending_[p->getTid()];
	tk.rbeats.push_back(rsp);

	if ((int)tk.rbeats.size() == tk.expected) {
		auto beats = std::move(tk.rbeats);
		pending_.erase(p->getTid());
		auto pkt = Construct_MemReadRespPkt(beats, "sa", p->getCaller());
		pkt->setTID(p->getTid());
		resp_Q_.push(pkt);
	}

	rc->recycle(p);
}

//------------------------------------------------------------------------------
// MMIO write: CPU programs registers or kicks off compute on ENABLE
void SystolicArray::writeMMIO(acalsim::Tick when, XBarMemWriteReqPayload* req) {
	uint32_t addr = req->getAddr() & 0x3F;
	uint32_t data = req->getData();
	CLASS_INFO << "SA: MMIO write reg with data " << data;
	switch (addr) {
		case 0x0:  // ENABLE
			if (data & 1) initialized_transaction();
			break;
		case 0x4:  // STATUS clear
			done_ = false;
			break;
		case 0x8:  // MATA_SIZE
			M_ = ((data >> 16) & 0xFFF) + 1;
			K_ = (data & 0xFFF) + 1;
			break;
		case 0xC:  // MATB_SIZE
			K_ = ((data >> 16) & 0xFFF) + 1;
			N_ = (data & 0xFFF) + 1;
			break;
		case 0x10:  // MATC_SIZE
			M_ = ((data >> 16) & 0xFFF) + 1;
			N_ = (data & 0xFFF) + 1;
			break;
		case 0x14: A_addr_ = data; break;
		case 0x18: B_addr_ = data; break;
		case 0x1C: C_addr_ = data; break;
		case 0x20:
			strideA_ = data & 0xFF;
			strideB_ = (data >> 8) & 0xFF;
			strideC_ = (data >> 16) & 0xFF;
			break;
		case 0x24: A_addr_dm_ = data; break;
		case 0x28: B_addr_dm_ = data; break;
		case 0x2C: C_addr_dm_ = data; break;
		default: LABELED_ERROR(this->getName()) << "Invalid write addr " << std::hex << addr;
	}
	// ACK
	auto rc   = acalsim::top->getRecycleContainer();
	auto resp = rc->acquire<XBarMemWriteRespPayload>(&XBarMemWriteRespPayload::renew, req->getInstr());
	resp->setTid(req->getTid());
	std::vector<XBarMemWriteRespPayload*> beats = {resp};
	auto                                  pkt   = Construct_MemWriteRespPkt(beats, "sa", req->getCaller());
	resp_Q_.push(pkt);
	rc->recycle(req);
}

void SystolicArray::SRAMWriteReqHandler(acalsim::Tick when, XBarMemWriteReqPayload* p) {
	instr      i    = p->getInstr();
	instr_type op   = p->getOP();
	uint32_t   addr = p->getAddr();
	uint32_t   dat  = p->getData();

	// Get pointer to SRAM base
	uint8_t* mem = reinterpret_cast<uint8_t*>(&sram_[0]);
	assert(addr >= SA_MEMORY_BASE);

	size_t memSize = SA_SRAM_SIZE * sizeof(uint32_t);  // = 32000 bytes

	// Sanity check
	if (addr < SA_MEMORY_BASE || addr >= SA_MEMORY_BASE + memSize) {
		LABELED_ERROR(this->getName()) << "SRAM write out of range at 0x" << std::hex << addr;
		return;
	}

	// Safe memory write lambda
	auto writeData = [&](uint32_t addr, const void* data, size_t size) -> bool {
		int32_t offset = addr - SA_MEMORY_BASE;
		if (offset < 0 || offset + size > memSize) {
			LABELED_ERROR(this->getName()) << "SRAM write out of range at 0x" << std::hex << addr;
			return false;
		}
		std::memcpy(mem + offset, data, size);
		return true;
	};

	bool ok = false;
	switch (op) {
		case SB: {
			uint8_t byte = dat & 0xFF;
			ok           = writeData(addr, &byte, sizeof(byte));
			break;
		}
		case SH: {
			uint16_t half = dat & 0xFFFF;
			ok            = writeData(addr, &half, sizeof(half));
			break;
		}
		case SW: {
			ok = writeData(addr, &dat, sizeof(dat));
			break;
		}
		default: LABELED_ERROR(this->getName()) << "Unsupported store OP"; return;
	}

	if (!ok) return;

	// Send write response
	auto rc  = acalsim::top->getRecycleContainer();
	auto rsp = rc->acquire<XBarMemWriteRespPayload>(&XBarMemWriteRespPayload::renew, i);
	rsp->setTid(p->getTid());

	auto& tk = pending_[p->getTid()];
	tk.wbeats.push_back(rsp);

	if ((int)tk.wbeats.size() == tk.expected) {
		auto beats = std::move(tk.wbeats);
		pending_.erase(p->getTid());
		auto pkt = Construct_MemWriteRespPkt(beats, "sa", p->getCaller());
		pkt->setTID(p->getTid());
		resp_Q_.push(pkt);
	}

	rc->recycle(p);
}

//------------------------------------------------------------------------------
// Called when ENABLE=1
void SystolicArray::initialized_transaction() {
	done_    = false;
	enabled_ = true;
	CLASS_INFO << "Initializing transaction: M=" << M_ << " K=" << K_ << " N=" << N_;
	CLASS_INFO << "Base address for the mat A: " << A_addr_ << " mat B: " << B_addr_ << " mat C: " << C_addr_;
	CLASS_INFO << "Stride A: " << strideA_ << " Stride B: " << strideB_ << " Stride C: " << strideC_;
	CLASS_INFO << "MatA in DM :" << A_addr_dm_ << " Mat B in DM: " << B_addr_dm_ << " MAT C in DM :" << C_addr_dm_;
	LABELED_ASSERT((A_addr_ % 4 == 0 && B_addr_ % 4 == 0 && C_addr_ % 4 == 0),
	               "Current support the address be multiple of 4");
	// clear buffers
	expected_MatA_size = M_ * K_;
	expected_MatB_size = K_ * N_;
	this->phase_       = READ_MAT_A;
	read_MatA_size     = 0;
	read_MatB_size     = 0;
	LABELED_ASSERT(expected_MatA_size <= 2048 && expected_MatB_size <= 2048,
	               "We assumes that matrix size to be less than 2048");
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 64; j++) {
			A_matrix[i][j] = 0;
			B_matrix[i][j] = 0;
			C_matrix[i][j] = 0;
		}
	}
	// start preloading weights / input
	AskDMAtoWrite_matA();
}

void SystolicArray::handleReadResponse(XBarMemReadRespPacket* pkt) {
	auto rc  = acalsim::top->getRecycleContainer();
	int  src = pkt->getSrcIdx();
	LABELED_ASSERT(src == 1, "Should from DMA");
	if (pkt->getPayloads().size() == 1 && pkt->getPayloads()[0]->getA1().imm == 114154) {
		if (pkt->getPayloads()[0]->getData() == 1) {
			// DMA finish signal
			if (phase_ == READ_MAT_A) {
				CLASS_INFO << "DMA Finished MatA loading.";
				phase_ = READ_MAT_B;
				AskDMAtoWrite_matB();
			} else if (phase_ == READ_MAT_B) {
				CLASS_INFO << "DMA Finished MatB loading.";

				// -------------------- Load A_matrix --------------------
				for (int i = 0; i < strideA_; ++i) {
					for (int j = 0; j < strideA_; ++j) {
						// Each row has strideA_ elements, and 4 elements are in one sram_ word
						uint32_t word_idx = A_addr_ / 4 + i * ((strideA_ + 3) / 4) + (j / 4);
						uint32_t word     = sram_[word_idx];
						uint8_t  value    = (word >> ((j % 4) * 8)) & 0xFF;
						A_matrix[i][j]    = value;
					}
				}

				// -------------------- Load B_matrix --------------------
				for (int i = 0; i < strideB_; ++i) {
					for (int j = 0; j < strideB_; ++j) {
						uint32_t word_idx = B_addr_ / 4 + i * ((strideB_ + 3) / 4) + (j / 4);
						uint32_t word     = sram_[word_idx];
						uint8_t  value    = (word >> ((j % 4) * 8)) & 0xFF;
						B_matrix[i][j]    = value;
					}
				}

				// Print A_matrix
				CLASS_INFO << "[A_matrix]";
				for (uint32_t i = 0; i < strideA_; ++i) {
					std::ostringstream row;
					row << "A[" << i << "]: ";
					for (uint32_t j = 0; j < strideA_; ++j) {
						row << std::setw(3) << std::setfill(' ') << static_cast<int>(A_matrix[i][j]) << " ";
					}
					CLASS_INFO << row.str();
				}

				// Print B_matrix
				CLASS_INFO << "[B_matrix]";
				for (uint32_t i = 0; i < strideB_; ++i) {
					std::ostringstream row;
					row << "B[" << i << "]: ";
					for (uint32_t j = 0; j < strideB_; ++j) {
						row << std::setw(3) << std::setfill(' ') << static_cast<int>(B_matrix[i][j]) << " ";
					}
					CLASS_INFO << row.str();
				}
				phase_ = COMPUTE;
				this->computeMatrix(M_);
				this->writeOutputs();
				this->done_ = true;
			}
		} else {
			this->PokeDMAReady();
		}
		rc->recycle(pkt->getPayloads()[0]);
	} else {
		// Receiving data from DMA
		CLASS_ERROR << "Shouln't receive the read respond other than the poke ready?";
		for (auto* p : pkt->getPayloads()) { rc->recycle(p); }
	}

	rc->recycle(pkt);
	// check if all weights loaded?
}

struct Valid8 {
	bool    valid;
	uint8_t data;
};
struct Valid16 {
	bool     valid;
	uint16_t sum;
};

struct PE {
	// weight register
	Valid8 weightReg{false, 0};
	// input pipeline
	Valid8 inReg{false, 0}, fwdIn{false, 0};
	// partial‐sum pipeline
	Valid16 psumReg{false, 0}, fwdPsum{false, 0};
};

void SystolicArray::computeMatrix(int SA_Size) {
	/* -------- data structures ------------------------------------------------ */
	std::vector<std::vector<PE>> pe(SA_Size, std::vector<PE>(SA_Size));
	std::vector<int>             emitRow(SA_Size, 0);  // “where do I write next” per column

	/* -------- 1.  weight preload (same as before) ---------------------------- */
	for (int cyc = SA_Size - 1; cyc >= 0; --cyc) {
		for (int i = SA_Size - 1; i >= 1; --i)
			for (int j = 0; j < SA_Size; ++j) pe[i][j].weightReg = pe[i - 1][j].weightReg;

		for (int j = 0; j < SA_Size; ++j) {
			pe[0][j].weightReg.valid = true;
			pe[0][j].weightReg.data  = B_matrix[cyc][j];
		}
	}

	CLASS_INFO << "[PE_W_matrix]";
	for (uint32_t i = 0; i < strideB_; ++i) {
		std::ostringstream row;
		row << "W[" << i << "]: ";
		for (uint32_t j = 0; j < strideB_; ++j) {
			row << std::setw(3) << std::setfill(' ') << static_cast<int>(pe[i][j].weightReg.data) << " ";
		}
		CLASS_INFO << row.str();
	}

	auto emit_results = [&](int cycle) {
		for (int j = 0; j < SA_Size; ++j) {
			auto& bot = pe[SA_Size - 1][j].psumReg;
			if (bot.valid && emitRow[j] < SA_Size) {
				C_matrix[emitRow[j]][j] = bot.sum;  // store
				++emitRow[j];                       // advance pointer

				// create the “bubble” that must drop on the next cycle
				bot.valid                        = false;
				pe[SA_Size - 1][j].fwdPsum.valid = false;
			}
		}
	};

	/* -------- 2.  A-propagation phase ---------------------------------------- */
	int total = 2 * SA_Size - 1;
	for (int cyc = 0; cyc < total; ++cyc) {
		/* shift / inject A */
		for (int i = 0; i < SA_Size; ++i) {
			for (int j = SA_Size - 1; j >= 0; --j) {
				if (j == 0) {
					int aCol             = cyc - i;
					pe[i][j].inReg.valid = (aCol >= 0 && aCol < SA_Size);
					pe[i][j].inReg.data  = pe[i][j].inReg.valid ? A_matrix[aCol][i] : 0;
				} else {
					pe[i][j].inReg = pe[i][j - 1].fwdIn;
				}
			}
		}

		/* MAC + forward */
		for (int i = SA_Size - 1; i >= 0; --i) {
			for (int j = 0; j < SA_Size; ++j) {
				auto& P = pe[i][j];
				if (P.weightReg.valid && P.inReg.valid) {
					uint16_t prod   = uint16_t(P.inReg.data) * uint16_t(P.weightReg.data);
					uint16_t prev   = (i > 0 && pe[i - 1][j].fwdPsum.valid) ? pe[i - 1][j].fwdPsum.sum : 0;
					P.psumReg.valid = true;
					P.psumReg.sum   = prev + prod;
				} else {
					P.psumReg.valid = false;
				}
			}
		}

		for (int i = SA_Size - 1; i >= 0; --i) {
			for (int j = 0; j < SA_Size; ++j) {
				auto& P   = pe[i][j];
				P.fwdIn   = P.inReg;
				P.fwdPsum = P.psumReg;
			}
		}

		/*CLASS_INFO << "[Output cycle:" << cyc << "]";
		std::ostringstream out;
		out << "Output: ";
		for (int j = 0; j < SA_Size; ++j) {
		    auto& bot = pe[SA_Size - 1][j].psumReg;
		    out << std::setw(3) << (bot.valid ? std::to_string(bot.sum) : std::string(" -1")) << " ";
		}
		CLASS_INFO << out.str();*/

		emit_results(cyc);  // store anything that just finished
	}

	/* -------- 3.  flush phase ------------------------------------------------- */
	for (int cyc = total; cyc < total + SA_Size - 1; ++cyc) {
		/* just move tokens that are already in the pipes */
		for (int i = 0; i < SA_Size; ++i)
			for (int j = 0; j < SA_Size; ++j) pe[i][j].inReg = pe[i][j - 1].fwdIn;

		for (int i = SA_Size - 1; i >= 0; i--) {
			for (int j = 0; j < SA_Size; ++j) {
				auto& P = pe[i][j];
				if (P.weightReg.valid && P.inReg.valid) {
					uint16_t prod   = uint16_t(P.inReg.data) * uint16_t(P.weightReg.data);
					uint16_t prev   = (i > 0 && pe[i - 1][j].fwdPsum.valid) ? pe[i - 1][j].fwdPsum.sum : 0;
					P.psumReg.valid = true;
					P.psumReg.sum   = prev + prod;
				} else {
					P.psumReg.valid = false;
				}
			}
		}

		for (int i = SA_Size - 1; i >= 0; --i) {
			for (int j = 0; j < SA_Size; ++j) {
				auto& P   = pe[i][j];
				P.fwdIn   = P.inReg;
				P.fwdPsum = P.psumReg;
			}
		}

		CLASS_INFO << "[Output cycle:" << cyc << "]";
		std::ostringstream out;
		out << "Output: ";
		for (int j = 0; j < SA_Size; ++j) {
			auto& bot = pe[SA_Size - 1][j].psumReg;
			out << std::setw(3) << (bot.valid ? std::to_string(bot.sum) : std::string(" -1")) << " ";
		}
		CLASS_INFO << out.str();

		emit_results(cyc);  // bottom-row tokens that just arrived
	}
}

void SystolicArray::writeOutputs() {
	CLASS_INFO << "[C_matrix]";
	for (uint32_t i = 0; i < strideC_; ++i) {
		std::ostringstream row;
		row << "C[" << i << "]: ";
		for (uint32_t j = 0; j < strideC_; ++j) {
			row << std::setw(3) << std::setfill(' ') << static_cast<int>(C_matrix[i][j]) << " ";
		}
		CLASS_INFO << row.str();
	}
}

void SystolicArray::handleWriteCompletion(XBarMemWriteRespPacket* pkt) {
	auto rc = acalsim::top->getRecycleContainer();
	for (auto* p : pkt->getPayloads()) rc->recycle(p);
	rc->recycle(pkt);
}

void SystolicArray::AskDMAtoWrite_matA() {
	auto rc = acalsim::top->getRecycleContainer();

	instr       dummy;
	std::string caller = "sa";

	// DMA MMIO base
	constexpr uint32_t DMA_BASE     = 0x0000F000;
	constexpr uint32_t DMA_ENABLE   = DMA_BASE + 0x0;
	constexpr uint32_t DMA_SRC      = DMA_BASE + 0x4;
	constexpr uint32_t DMA_DST      = DMA_BASE + 0x8;
	constexpr uint32_t DMA_SIZE_CFG = DMA_BASE + 0xC;
	constexpr uint32_t DMA_DONE     = DMA_BASE + 0x14;

	// Prepare values
	uint32_t src    = A_addr_dm_;
	uint32_t dst    = A_addr_ + SA_MEMORY_BASE;
	uint32_t enable = 1;

	// Build a size config value based on M_, K_, and strideA_
	uint8_t  TW       = static_cast<uint8_t>((K_ - 1) & 0xff);
	uint8_t  TH       = static_cast<uint8_t>((M_ - 1) & 0xff);
	uint8_t  stride   = static_cast<uint8_t>(strideA_ & 0xFF);
	uint32_t size_cfg = (stride << 24) | (stride << 16) | (TW << 8) | TH;
	// Issue MMIO writes (non-burst)
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_SRC, src, caller));
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_DST, dst, caller));
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_SIZE_CFG, size_cfg, caller));
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_ENABLE, enable, caller));
	this->PokeDMAReady();
}

void SystolicArray::AskDMAtoWrite_matB() {
	auto rc = acalsim::top->getRecycleContainer();

	instr       dummy;
	std::string caller = "sa";

	// DMA MMIO base
	constexpr uint32_t DMA_BASE     = 0x0000F000;
	constexpr uint32_t DMA_ENABLE   = DMA_BASE + 0x0;
	constexpr uint32_t DMA_SRC      = DMA_BASE + 0x4;
	constexpr uint32_t DMA_DST      = DMA_BASE + 0x8;
	constexpr uint32_t DMA_SIZE_CFG = DMA_BASE + 0xC;
	constexpr uint32_t DMA_DONE     = DMA_BASE + 0x14;

	// Prepare values
	uint32_t src    = B_addr_dm_;
	uint32_t dst    = B_addr_ + SA_MEMORY_BASE;
	uint32_t enable = 1;

	// Build a size config value based on M_, K_, and strideA_
	uint32_t TW       = K_ - 1;
	uint32_t TH       = M_ - 1;
	uint8_t  stride   = static_cast<uint8_t>(strideB_ & 0xFF);
	uint32_t size_cfg = (stride << 24) | (stride << 16) | (TW << 8) | TH;
	// Issue MMIO writes (non-burst)
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_SRC, src, caller));
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_DST, dst, caller));
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_SIZE_CFG, size_cfg, caller));
	req_Q_.push(Construct_MemWritepkt_non_burst(dummy, SW, DMA_ENABLE, enable, caller));
	this->PokeDMAReady();
}

void SystolicArray::PokeDMAReady() {
	int delay_lentency = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");
	// Add a polling loop for DONE (read+check+clear)
	acalsim::LambdaEvent<void()>* poll_event = new acalsim::LambdaEvent<void()>([this]() {
		auto    rc = acalsim::top->getRecycleContainer();
		instr   dummy;
		operand a1;
		a1.imm   = 114154;  // Use for tracking
		auto pkt = Construct_MemReadpkt_non_burst(dummy, LW, 0x0000F014, a1, "sa");
		req_Q_.push(pkt);
	});

	// Schedule the polling event after a few cycles
	this->scheduleEvent(poll_event, acalsim::top->getGlobalTick() + delay_lentency);
}

std::string SystolicArray::instrToString(instr_type _op) const {
	switch (_op) {
		case LB: return "LB";
		case LBU: return "LBU";
		case LH: return "LH";
		case LHU: return "LHU";
		case LW: return "LW";
		// Store
		case SB: return "SB";
		case SH: return "SH";
		case SW: return "SW";
		default: return "UNKNOWN";
	}
}

void SystolicArray::DumpMemory() const {
	const uint8_t* mem     = reinterpret_cast<const uint8_t*>(sram_);
	size_t         memSize = 100 * sizeof(uint32_t);  // Total size in bytes

	std::cout << "[SystolicArray::DumpMemory] Dumping " << memSize << " bytes of SRAM...\n";

	for (uint32_t offset = 0; offset < memSize; offset += 8) {
		uint32_t addr = SA_MEMORY_BASE + offset;

		// Print address label
		std::cout << std::hex << std::setw(8) << std::setfill('0') << addr << ": ";

		// Print 8 bytes of data
		for (int i = 0; i < 8 && (offset + i) < memSize; ++i) {
			std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mem[offset + i]) << " ";
		}

		std::cout << "\n";
	}

	std::cout << "[SystolicArray::DumpMemory] Done.\n";
}