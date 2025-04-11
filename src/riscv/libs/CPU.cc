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
#include "InstPacket.hh"
#include "SOC.hh"
#include "event/ExecOneInstrEvent.hh"
#include "event/MemReqEvent.hh"

CPU::CPU(std::string _name, SOC* _soc)
    : acalsim::SimModule(_name), pc(0), inst_cnt(0), soc(_soc), pendingInstPacket(nullptr) {
	auto data_offset = acalsim::top->getParameter<int>("Emulator", "data_offset");
	this->imem       = new instr[data_offset / 4];
	for (int i = 0; i < data_offset / 4; i++) {
		this->imem[i].op      = UNIMPL;
		this->imem[i].a1.type = OPTYPE_NONE;
		this->imem[i].a2.type = OPTYPE_NONE;
		this->imem[i].a3.type = OPTYPE_NONE;
	}
	for (int i = 0; i < 32; i++) { this->rf[i] = 0; }
}

void CPU::execOneInstr() {
	// This lab models a single-CPU cycle as shown in Lab7
	// Fetch instrucion
	instr i = this->fetchInstr(this->pc);

	// Prepare instruction packet
	auto        rc         = top->getRecycleContainer();
	InstPacket* instPacket = rc->acquire<InstPacket>(&InstPacket::renew, i);
	instPacket->pc         = this->pc;

	// Execute the instruction in the same cycle
	processInstr(i, instPacket);
}

void CPU::processInstr(const instr& _i, InstPacket* instPacket) {
	bool  done   = false;
	auto& rf_ref = this->rf;
	this->incrementInstCount();
	int pc_next = this->pc + 4;

	switch (_i.op) {
		case ADD: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] + rf_ref[_i.a3.reg]; break;
		case SUB: rf_ref[_i.a1.reg] = rf_ref[_i.a2.reg] - rf_ref[_i.a3.reg]; break;
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
		case LW: this->memRead(_i, _i.op, this->rf[_i.a2.reg] + _i.a3.imm, _i.a1, instPacket); break;
		case SB:
		case SH:
		case SW: this->memWrite(_i, _i.op, this->rf[_i.a2.reg] + _i.a3.imm, this->rf[_i.a1.reg], instPacket); break;

		case HCF: break;
		case UNIMPL:
		default:
			CLASS_INFO << "Reached an unimplemented instruction!";
			if (_i.psrc) printf("Instruction: %s\n", _i.psrc);
			break;
	}

	this->commitInstr(_i, instPacket);
	if (pc_next != pc + 4) instPacket->isTakenBranch = true;
	this->pc = pc_next;
}

void CPU::commitInstr(const instr& _i, InstPacket* instPacket) {
	if (_i.op == HCF) {
		// end of simulation.
		// Stop scheduling new events to process instructions.
		// There might be pending events in the simulator.
		if (!this->soc->getMasterPort("sIF-m")->push(instPacket)) {
			pendingInstPacket = instPacket;
		} else {
			CLASS_INFO << "Instruction " << this->instrToString(_i.op)
			           << " is completed at Tick = " << acalsim::top->getGlobalTick() << " | PC = " << this->pc;
		}
		return;
	}

	// send the packet to the IF stage
	if (this->soc->getMasterPort("sIF-m")->push(instPacket)) {
		// send the instruction packet to the IF stage successfully
		// schedule the next trigger event
		CLASS_INFO << "Instruction " << this->instrToString(_i.op)
		           << " is completed at Tick = " << acalsim::top->getGlobalTick() << " | PC = " << this->pc;
		CLASS_INFO << "send " + this->instrToString(instPacket->inst.op) << "@ PC=" << instPacket->pc
		           << " to IFStage successfully";
		auto               rc = acalsim::top->getRecycleContainer();
		ExecOneInstrEvent* event =
		    rc->acquire<ExecOneInstrEvent>(&ExecOneInstrEvent::renew, this->getInstCount() /*id*/, this);
		this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
	} else {
		// get backpressure from the IF stage
		// Wait until the master port pops out the entry and retry
		// This case, we need to store the instruction packet locally
		pendingInstPacket = instPacket;
		/*CLASS_INFO << "send " + this->instrToString(instPacket->inst.op) << "@ PC=" << instPacket->pc
		           << ", Got backpressure";*/
	}
}

void CPU::retrySendInstPacket(MasterPort* mp) {
	if (!pendingInstPacket) return;
	if (mp->push(pendingInstPacket)) {
		CLASS_INFO << "resend " + this->instrToString(pendingInstPacket->inst.op) << "@ PC=" << pendingInstPacket->pc
		           << " to IFStage successfully";
		CLASS_INFO << "Instruction " << this->instrToString(pendingInstPacket->inst.op)
		           << " is completed at Tick = " << acalsim::top->getGlobalTick()
		           << " | PC = " << pendingInstPacket->pc;

		if (pendingInstPacket->inst.op == HCF) {
			pendingInstPacket = nullptr;
			return;
		}

		// send the instruction packet to the IF stage successfully
		// schedule the next trigger event
		auto               rc = acalsim::top->getRecycleContainer();
		ExecOneInstrEvent* event =
		    rc->acquire<ExecOneInstrEvent>(&ExecOneInstrEvent::renew, this->getInstCount() /*id*/, this);
		this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
		pendingInstPacket = nullptr;
	} else {
		CLASS_ERROR << " CPU::retrySendInstPacket() failed!";
	}
}

bool CPU::memRead(const instr& _i, instr_type _op, uint32_t _addr, operand _a1, InstPacket* instPacket) {
	// If latency is larger than 1, e.g. cache miss or multi-cycle SRAM reads

	auto              rc  = acalsim::top->getRecycleContainer();
	MemReadReqPacket* pkt = rc->acquire<MemReadReqPacket>(&MemReadReqPacket::renew, nullptr, _i, _op, _addr, _a1);
	auto data = ((DataMemory*)this->getDownStream("DSDmem"))->memReadReqHandler(acalsim::top->getGlobalTick(), pkt);
	this->rf[_i.a1.reg] = data;
	// CLASS_INFO << "handle memRead for " << this->instrToString(instPacket->inst.op) << " @ PC=" << instPacket->pc;
	return true;
}

bool CPU::memWrite(const instr& _i, instr_type _op, uint32_t _addr, uint32_t _data, InstPacket* instPacket) {
	auto rc = acalsim::top->getRecycleContainer();

	MemWriteReqPacket* pkt = rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, nullptr, _i, _op, _addr, _data);
	this->getDownStream("DSDmem")->accept(acalsim::top->getGlobalTick(), *((acalsim::SimPacket*)pkt));
	// CLASS_INFO << "handle memWrite for " << this->instrToString(instPacket->inst.op) << " @ PC=" << instPacket->pc;

	return true;
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

		// Special
		case HCF: return "HCF";

		default: return "UNKNOWN";
	}
}
