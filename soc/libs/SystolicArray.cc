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
						    this->memReadReqHandler(acalsim::top->getGlobalTick() + i + delay_lentency, payload[i]);
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
				for (int i = 0; i < rd->getBurstSize(); i++) {
					acalsim::LambdaEvent<void()>* event =
					    new acalsim::LambdaEvent<void()>([this, i, payload, delay_lentency]() {
						    this->memWriteReqHandler(acalsim::top->getGlobalTick() + i + delay_lentency, payload[i]);
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

uint32_t* SystolicArray::getSramPtr(uint32_t addr) {
	if (addr < SA_MEMORY_BASE || addr >= SA_MEMORY_BASE + SA_SRAM_SIZE) return nullptr;  // out of range
	return &sram_[addr - SA_MEMORY_BASE];
}

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
void SystolicArray::memReadReqHandler(acalsim::Tick when, XBarMemReadReqPayload* p) {
	instr      i    = p->getInstr();
	instr_type op   = p->getOP();
	uint32_t   addr = p->getAddr();
	operand    a1   = p->getA1();
	uint32_t   ret  = 0;

	uint32_t* ptr = getSramPtr(addr);
	if (!ptr) {  // out of window → ERROR
		CLASS_ERROR << "SRAM read out of range 0x" << std::hex << addr;
		return;
	}

	switch (op) {
		case LB: ret = static_cast<uint32_t>(*(int8_t*)ptr); break;
		case LBU: ret = *(uint8_t*)ptr; break;
		case LH: ret = static_cast<uint32_t>(*(int16_t*)ptr); break;
		case LHU: ret = *(uint16_t*)ptr; break;
		case LW: ret = *(uint32_t*)ptr; break;
		default: CLASS_ERROR << "Unsupported load OP"; return;
	}

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
		default: LABELED_ERROR(this->getName()) << "Invalid write addr " << std::hex << addr;
	}
	// ACK
	auto rc   = acalsim::top->getRecycleContainer();
	auto resp = rc->acquire<XBarMemWriteRespPayload>(&XBarMemWriteRespPayload::renew, req->getInstr());
	resp->setTid(req->getTid());
	std::vector<XBarMemWriteRespPayload*> beats = {resp};
	auto                                  pkt   = Construct_MemWriteRespPkt(beats, "systolic", req->getCaller());
	resp_Q_.push(pkt);
	rc->recycle(req);
}

void SystolicArray::memWriteReqHandler(acalsim::Tick when, XBarMemWriteReqPayload* p) {
	instr      i    = p->getInstr();
	instr_type op   = p->getOP();
	uint32_t   addr = p->getAddr();
	uint32_t   dat  = p->getData();

	// Get pointer to SRAM base
	uint8_t* mem     = reinterpret_cast<uint8_t*>(getSramPtr(0));  // base address of SRAM
	size_t   memSize = 8000;                                       // total SRAM size in bytes

	assert(addr >= SA_MEMORY_BASE);

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
	// clear buffers
	for (int i = 0; i < 2048; i++) {
		A_matrix[i] = 0;
		B_matrix[i] = 0;
		C_matrix[i] = 0;
	}
	CLASS_ERROR << "Not implemented yet";
	// start preloading weights
	preloadWeights();
}

void SystolicArray::preloadWeights() {
	// for simplicity, read B element by element
	auto rc = acalsim::top->getRecycleContainer();
	for (uint32_t k = 0; k < K_; ++k) {
		for (uint32_t j = 0; j < N_; ++j) {
			uint32_t addr = B_addr_ + (k * strideB_ + j) * sizeof(uint32_t);
			instr    dummy;
			operand  opnd;
			opnd.imm = k * N_ + j;
			auto req = rc->acquire<XBarMemReadReqPayload>(&XBarMemReadReqPayload::renew, dummy, LW, addr, opnd);
			std::vector<XBarMemReadReqPayload*> beats = {req};
			auto                                pkt   = Construct_MemReadpkt_burst("systolic", beats);
			req_Q_.push(pkt);
		}
	}
}

void SystolicArray::handleReadResponse(XBarMemReadRespPacket* pkt) {
	auto rc = acalsim::top->getRecycleContainer();
	for (auto* p : pkt->getPayloads()) {
		uint32_t idx  = p->getA1().imm;
		B_matrix[idx] = p->getData();
		rc->recycle(p);
	}
	rc->recycle(pkt);
	// check if all weights loaded?
	static size_t count = 0;
	count++;
	if (count == K_ * N_) {
		count = 0;
		computeMatrix();
	}
}

void SystolicArray::computeMatrix() {
	// simple functional model: read each A and multiply-accumulate
	for (uint32_t i = 0; i < M_; ++i) {
		for (uint32_t j = 0; j < N_; ++j) {
			uint32_t sum = 0;
			for (uint32_t k = 0; k < K_; ++k) {
				uint32_t addrA = A_addr_ + (i * strideA_ + k) * sizeof(uint32_t);
				uint32_t a     = 0;
				uint32_t b     = B_matrix[k * N_ + j];
				sum += a * b;
			}
			C_matrix[i * N_ + j] = sum;
		}
	}
	writeOutputs();
}

void SystolicArray::writeOutputs() {
	auto rc = acalsim::top->getRecycleContainer();
	for (uint32_t i = 0; i < M_; ++i) {
		for (uint32_t j = 0; j < N_; ++j) {
			uint32_t addr = C_addr_ + (i * strideC_ + j) * sizeof(uint32_t);
			instr    dummy;
			auto     wr = rc->acquire<XBarMemWriteReqPayload>(&XBarMemWriteReqPayload::renew, dummy, SW, addr,
                                                          C_matrix[i * N_ + j]);
			std::vector<XBarMemWriteReqPayload*> beats = {wr};
			auto                                 pkt   = Construct_MemWritepkt_burst("systolic", beats);
			req_Q_.push(pkt);
		}
	}
	// mark done after writes
	done_ = true;
}

void SystolicArray::handleWriteCompletion(XBarMemWriteRespPacket* pkt) {
	auto rc = acalsim::top->getRecycleContainer();
	for (auto* p : pkt->getPayloads()) rc->recycle(p);
	rc->recycle(pkt);
}
