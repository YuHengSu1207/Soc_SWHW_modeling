/*
 * Copyright 2023-2024 Playlab/ACAL
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CPU.hh"

#include <iomanip>
#include <sstream>

#include "DataMemory.hh"
#include "TraceRecord.hh"
#include "event/ExecOneInstrEvent.hh"
#include "event/MemReqEvent.hh"

CPU::CPU(std::string _name, Emulator* _emulator)
    : acalsim::CPPSimBase(_name), pc(0), inst_cnt(0), isaEmulator(_emulator) {
	this->addMasterPort(_name + "-m");
}

void CPU::init() {
	// Inject trigger event
	auto data_offset = acalsim::top->getParameter<int>("Emulator", "data_offset");
	this->imem       = new instr[data_offset / 4];
	for (int i = 0; i < data_offset / 4; i++) {
		this->imem[i].op      = UNIMPL;
		this->imem[i].a1.type = OPTYPE_NONE;
		this->imem[i].a2.type = OPTYPE_NONE;
		this->imem[i].a3.type = OPTYPE_NONE;
	}
	for (int i = 0; i < 32; i++) { this->rf[i] = 0; }

	auto               rc    = acalsim::top->getRecycleContainer();
	ExecOneInstrEvent* event = rc->acquire<ExecOneInstrEvent>(&ExecOneInstrEvent::renew, 1 /*id*/, this);
	this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
}

void CPU::execOneInstr() {
	// This lab models a single-CPU cycle as shown in Lab7
	// Fetch instruction
	instr i = this->fetchInstr(this->pc);
	// Execute the instruction in the same cycle
	processInstr(i);
}

void CPU::processInstr(const instr& _i) {
	bool  done   = false;
	auto& rf_ref = this->rf;
	this->incrementInstCount();
	int pc_next = this->pc + 4;

	switch (_i.op) {
		case ADD: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] + rf_ref[_i.a3.reg]; break;
		case SUB: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] - rf_ref[_i.a3.reg]; break;
		case MUL: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] * rf_ref[_i.a3.reg]; break;
		case SLT: rf_ref[_i.a1.reg] = (*(int32_t*)&rf_ref[_i.a2.reg]) < (*(int32_t*)&rf_ref[_i.a3.reg]) ? 1 : 0; break;
		case SLTU: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] + rf_ref[_i.a3.reg]; break;
		case AND: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] & rf_ref[_i.a3.reg]; break;
		case OR: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] | rf_ref[_i.a3.reg]; break;
		case XOR: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] ^ rf_ref[_i.a3.reg]; break;
		case SLL: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] << rf_ref[_i.a3.reg]; break;
		case SRL: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] >> rf_ref[_i.a3.reg]; break;
		case SRA: rf_ref[_i.a1.reg] = (*(int32_t*)&rf_ref[_i.a2.reg]) >> rf_ref[_i.a3.reg]; break;

		case ADDI: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] + _i.a3.imm; break;
		case SLTI: rf_ref[_i.a1.reg] = (*(int32_t*)&rf_ref[_i.a2.reg]) < (*(int32_t*)&(_i.a3.imm)) ? 1 : 0; break;
		case SLTIU: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] < _i.a3.imm ? 1 : 0; break;
		case ANDI: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] & _i.a3.imm; break;
		case ORI: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] | _i.a3.imm; break;
		case XORI: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] ^ _i.a3.imm; break;
		case SLLI: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] << _i.a3.imm; break;
		case SRLI: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] >> _i.a3.imm; break;
		case SRAI: rf_ref[_i.a1.reg] = (*(int32_t*)&rf_ref[_i.a2.reg]) >> _i.a3.imm; break;

		case BEQ:
			if (rf_ref[_i.a1.reg] == rf_ref[_i.a2.reg]) pc_next = _i.a3.imm;
			break;
		case BGE:
			if (*(int32_t*)&rf_ref[_i.a1.reg] >= *(int32_t*)&rf_ref[_i.a2.reg]) pc_next = _i.a3.imm;
			break;
		case BGEU:
			if (rf_ref[_i.a1.reg] >= rf_ref[_i.a2.reg]) pc_next = _i.a3.imm;
			break;
		case BLT:
			if (*(int32_t*)&rf_ref[_i.a1.reg] < *(int32_t*)&rf_ref[_i.a2.reg]) pc_next = _i.a3.imm;
			break;
		case BLTU:
			if (rf_ref[_i.a1.reg] < rf_ref[_i.a2.reg]) pc_next = _i.a3.imm;
			break;
		case BNE:
			if (rf_ref[_i.a1.reg] != rf_ref[_i.a2.reg]) pc_next = _i.a3.imm;
			break;

		case JAL:
			rf_ref[_i.a1.reg] = this->pc + 4;
			pc_next           = _i.a2.imm;
			break;
		case JALR:
			rf_ref[_i.a1.reg] = this->pc + 4;
			pc_next           = rf_ref[_i.a2.reg] + _i.a3.imm;
			break;
		case AUIPC: rf_ref[_i.a1.reg] = this->pc + (_i.a2.imm << 12); break;
		case LUI: rf_ref[_i.a1.reg] = (_i.a2.imm << 12); break;

		case LB:
		case LBU:
		case LH:
		case LHU:
		case LW:
			done = this->BusMemRead(_i, _i.op, this->rf[_i.a2.reg] + _i.a3.imm, _i.a1);
			if (!done) return;
			break;
		case SB:
		case SH:
		case SW:
			done = this->BusmemWrite(_i, _i.op, this->rf[_i.a2.reg] + _i.a3.imm, this->rf[_i.a1.reg]);
			if (!done) return;
			break;
		case S_ADDI8I8S_VV:
		case S_ADDI16I16S_VV:
		case S_SUBI8I8S_VV:
		case S_SUBI16I16S_VV:
		case S_PMULI8I16S_VV_L:
		case S_PMULI8I16S_VV_H:
		case S_AMULI8I8S_VV_NQ:
		case S_AMULI8I8S_VV_L: this->CFUReq(_i, this->rf[_i.a2.reg], this->rf[_i.a3.reg]); return;
		case HCF: break;
		case UNIMPL:
		default:
			CLASS_INFO << "Reached an unimplemented instruction!";
			if (_i.psrc) printf("Instruction: %s\n", _i.psrc);
			break;
	}

	this->commitInstr(_i);
	this->pc = pc_next;
}

void CPU::commitInstr(const instr& _i) {
	/*CLASS_INFO << "Instruction " << this->instrToString(_i.op)
	           << " is completed at Tick = " << acalsim::top->getGlobalTick() << " | PC = " << this->pc;*/

	if (_i.op == HCF) return;

	// schedule the next trigger event
	auto               rc = acalsim::top->getRecycleContainer();
	ExecOneInstrEvent* event =
	    rc->acquire<ExecOneInstrEvent>(&ExecOneInstrEvent::renew, this->getInstCount() /*id*/, this);
	this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
}

bool CPU::BusMemRead(const instr& _i, instr_type _op, uint32_t _addr, operand _a1) {
	auto rc      = acalsim::top->getRecycleContainer();
	int  latency = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");

	MemReadReqPacket*              pkt = rc->acquire<MemReadReqPacket>(&MemReadReqPacket::renew, _i, _op, _addr, _a1);
	std::vector<MemReadReqPacket*> packetVec = {pkt};  // Explicit vector

	BusMemReadReqPacket* bus_pkt =
	    rc->acquire<BusMemReadReqPacket>(&BusMemReadReqPacket::renew, 1, packetVec, cpu_callback);
	int tid = bus_pkt->getTransactionID();
	bus_pkt->setTransactionID(tid);

	auto m_port = this->getMasterPort(this->getName() + "-m");
	if (m_port->isPushReady()) {
		// LABELED_INFO(this->getName()) << "Send a read request to bus";
		// send packet instead of event trigger
		m_port->push(bus_pkt);
	} else {
		this->request_queue.push(bus_pkt);
	}
	return false;
}

bool CPU::BusmemWrite(const instr& _i, instr_type _op, uint32_t _addr, uint32_t _data) {
	auto rc       = acalsim::top->getRecycleContainer();
	int  latency  = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_write_latency");
	auto callback = [this](MemWriteRespPacket* _pkt) { this->memWriteRespHandler(_pkt); };

	AXIBus* bus = dynamic_cast<AXIBus*>(this->getDownStream("DSBus"));

	auto bus_callback = [this](MemWriteRespPacket* _pkt) {
		dynamic_cast<AXIBus*>(this->getDownStream("DSBus"))->memWriteRespHandler_CPU(_pkt);
	};
	auto cpu_callback = [this](BusMemWriteRespPacket* _pkt) { this->memWriteBusRespHandler(_pkt); };

	MemWriteReqPacket* pkt =
	    rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, bus_callback, _i, _op, _addr, _data);
	std::vector<MemWriteReqPacket*> packetVec = {pkt};  // Explicit vector

	BusMemWriteReqPacket* bus_pkt =
	    rc->acquire<BusMemWriteReqPacket>(&BusMemWriteReqPacket::renew, 1, packetVec, cpu_callback);
	int tid = bus_pkt->getTransactionID();
	bus_pkt->setTransactionID(tid);

	auto m_port = this->getMasterPort(this->getName() + "-m");
	if (m_port->isPushReady()) {
		// LABELED_INFO(this->getName()) << "Send a write request to bus";
		m_port->push(bus_pkt);
		return true;
	} else {
		this->request_queue.push(bus_pkt);
	}
	return false;
}

/*  CFU extension*/
void CPU::CFUReq(const instr& _i, uint32_t _rs1, uint32_t _rs2) {
	auto          rc      = acalsim::top->getRecycleContainer();
	CFUReqPacket* cfu_pkt = rc->acquire<CFUReqPacket>(&CFUReqPacket::renew, _i, _rs1, _rs2);
	this->pushToMasterChannelPort(this->getName() + "-m_cfu", cfu_pkt);
	return;
}

void CPU::CFURespHandler(CFURespPacket* _pkt) {
	this->rf[_pkt->getInstr().a1.reg] = _pkt->getRd();
	commitInstr(_pkt->getInstr());
	acalsim::top->getRecycleContainer()->recycle(_pkt);
	this->pc += 4;
	return;
}

void CPU::masterPortRetry(const std::string& portName) {
	// LABELED_INFO(this->getName()) << ": Master port retry at " << portName;
	auto m_port = this->getMasterPort(this->getName() + "-m");
	// LABELED_ASSERT(m_port->isPushReady(), "The master port should be ready at this time");
	if (!this->request_queue.empty() && m_port->isPushReady()) {
		// LABELED_INFO(this->getName()) << " send to the port";
		if (auto read_req = dynamic_cast<BusMemReadReqPacket*>(this->request_queue.front())) {
			auto real_req = read_req->getMemReadReqPkt()[0];
			auto _addr    = real_req->getAddr();
			auto tid      = read_req->getTransactionID();
			m_port->push(read_req);
		} else if (auto write_req = dynamic_cast<BusMemWriteReqPacket*>(this->request_queue.front())) {
			auto  real_req = write_req->getMemWriteReqPkt()[0];
			auto  _addr    = real_req->getAddr();
			auto  tid      = write_req->getTransactionID();
			auto& _i       = real_req->getInstr();
			m_port->push(write_req);
			pc += 4;
			commitInstr(_i);
		} else {
			CLASS_ERROR << "Unexpected type";
		}
		this->request_queue.pop();
	}
}

void CPU::memReadBusRespHandler(BusMemReadRespPacket* _pkt) {
	auto memPackets = _pkt->getMemReadRespPkt();
	for (auto& memPkt : memPackets) { this->memReadRespHandler(memPkt); }
	int tid = _pkt->getTransactionID();
	acalsim::top->getRecycleContainer()->recycle(_pkt);
}

void CPU::memWriteBusRespHandler(BusMemWriteRespPacket* _pkt) {
	// LABELED_INFO(this->getName()) << "CPU finish write transaction" << _pkt->getTransactionID();
	auto memPackets = _pkt->getMemWriteRespPkt();
	int  tid        = _pkt->getTransactionID();
}

void CPU::memReadRespHandler(MemReadRespPacket* _pkt) {
	// LABELED_INFO(this->getName()) << "CPU memReadRespHandler() ";
	uint32_t data    = _pkt->getData();
	instr    i       = _pkt->getInstr();
	operand  a1      = _pkt->getA1();
	this->rf[a1.reg] = data;
	// acalsim::top->getRecycleContainer()->recycle(_pkt);
	commitInstr(i);
	this->pc += 4;
}

void CPU::memWriteRespHandler(MemWriteRespPacket* _pkt) {
	// LABELED_INFO(this->getName()) << "CPU memWriteRespHandler() ";
	instr i = _pkt->getInstr();
	// acalsim::top->getRecycleContainer()->recycle(_pkt);
	commitInstr(i);
	this->pc += 4;
}

void CPU::printRegfile() const {
	std::ostringstream oss;

	oss << "Register File Snapshot:\n\n";
	for (int i = 0; i < 32; i++) {
		oss << "x" << std::setw(2) << std::setfill('0') << std::dec << i << ":0x";

		oss << std::setw(8) << std::setfill('0') << std::hex << this->rf[i] << " ";

		if ((i + 1) % 8 == 0) { oss << "\n"; }
	}

	oss << '\n';

	CLASS_INFO << oss.str();
}

void CPU::dumpMemory() {
	AXIBus*     bus = dynamic_cast<AXIBus*>(this->getDownStream("DSBus"));
	DataMemory* dm  = dynamic_cast<DataMemory*>(bus->getDownStream("DSDMem"));
	// Open output file
	std::ofstream outFile("memory_dump.txt");
	if (!outFile) {
		std::cerr << "Failed to open file for writing!" << std::endl;
		return;
	}
	// Read and print memory contents from 0x2000 to 0xEFFF
	std::cout << "Memory Dump (0x2000 to 0xEFFF):" << std::endl;
	uint32_t START_ADDR = 0x2000;
	uint32_t END_ADDR   = 0xEFFF;

	for (uint32_t addr = START_ADDR; addr <= END_ADDR; addr += 8) {
		void*    data  = dm->readData(addr, 8, false);
		uint8_t* bytes = static_cast<uint8_t*>(data);
		// outFile << std::hex << std::setw(4) << std::setfill('0') << addr << ": ";
		for (size_t i = 0; i < 8; ++i) {
			outFile << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]) << "\n";
		}
	}

	outFile.close();
	return;
}

instr CPU::fetchInstr(uint32_t _pc) const {
	uint32_t iid = _pc / 4;
	return this->imem[iid];
}

std::string CPU::instrToString(instr_type _op) const {
	switch (_op) {
		case UNIMPL: return "UNIMPL";

		// R-type
		case ADD: return "ADD";
		case AND: return "AND";
		case OR: return "OR";
		case XOR: return "XOR";
		case SUB: return "SUB";
		case SLL: return "SLL";
		case SRL: return "SRL";
		case SRA: return "SRA";
		case SLT: return "SLT";
		case SLTU: return "SLTU";

		// I-type
		case ADDI: return "ADDI";
		case ANDI: return "ANDI";
		case ORI: return "ORI";
		case XORI: return "XORI";
		case SLLI: return "SLLI";
		case SRLI: return "SRLI";
		case SRAI: return "SRAI";
		case SLTI: return "SLTI";
		case SLTIU: return "SLTIU";

		// Load
		case LB: return "LB";
		case LBU: return "LBU";
		case LH: return "LH";
		case LHU: return "LHU";
		case LW: return "LW";

		// Store
		case SB: return "SB";
		case SH: return "SH";
		case SW: return "SW";

		// Branch
		case BEQ: return "BEQ";
		case BNE: return "BNE";
		case BGE: return "BGE";
		case BGEU: return "BGEU";
		case BLT: return "BLT";
		case BLTU: return "BLTU";

		// Jump
		case JAL: return "JAL";
		case JALR: return "JALR";

		// Upper / Immediate
		case AUIPC: return "AUIPC";
		case LUI: return "LUI";

		// SIMD Instructions
		case S_ADDI8I8S_VV: return "S_ADDI8I8S.vv";
		case S_ADDI16I16S_VV: return "S_ADDI16I16S.vv";
		case S_SUBI8I8S_VV: return "S_SUBI8I8S.vv";
		case S_SUBI16I16S_VV: return "S_SUBI16I16S.vv";
		case S_PMULI8I16S_VV_L: return "S_PMULI8I16S.vv.L";
		case S_PMULI8I16S_VV_H: return "S_PMULI8I16S.vv.H";
		case S_AMULI8I8S_VV_NQ: return "S_AMULI8I8S.vv.NQ";
		case S_AMULI8I8S_VV_L: return "S_AMULI8I8S.vv.L";

		// Special
		case HCF: return "HCF";

		default: return "UNKNOWN";
	}
}

void CPU::cleanup() {
	LABELED_ASSERT(this->request_queue.empty(), "The request queue should be empty");
	this->printRegfile();
	// this->dumpMemory();
	CLASS_INFO << "CPU::cleanup() ";
}