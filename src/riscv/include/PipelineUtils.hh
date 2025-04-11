#include <tuple>

#include "ACALSim.hh"
#include "DataStruct.hh"
#include "InstPacket.hh"
#ifndef SRC_RISCV_INCLUDE_PIPELINEUTIL_HH_
#define SRC_RISCV_INCLUDE_PIPELINEUTIL_HH_

class PipelineUtils {
protected:
	int getDestReg(const instr& _inst) {
		switch (_inst.op) {
			case ADD:
			case ADDI:
			case AND:
			case ANDI:
			case OR:
			case ORI:
			case XOR:
			case XORI:
			case SUB:
			case SLL:
			case SLLI:
			case SRL:
			case SRLI:
			case SRA:
			case SRAI:
			case SLT:
			case SLTI:
			case SLTU:
			case SLTIU:
			case LB:
			case LH:
			case LW:
			case LBU:
			case LHU:
			case LUI:
			case AUIPC:
			case JAL:
			case JALR: return _inst.a1.reg;

			default: return 0;  // x0 (no write)
		}
	}

	bool isRs1Used(const instr& i) {
		switch (i.op) {
			case ADD:
			case SUB:
			case AND:
			case OR:
			case XOR:
			case SLT:
			case SLTU:
			case SLL:
			case SRL:
			case SRA:
			case BEQ:
			case BNE:
			case BLT:
			case BGE:
			case BLTU:
			case BGEU:
			case SW:
			case SB:
			case SH:
			case ADDI:
			case ANDI:
			case ORI:
			case XORI:
			case SLTI:
			case SLTIU:
			case SLLI:
			case SRLI:
			case SRAI:
			case LW:
			case LH:
			case LHU:
			case LB:
			case LBU:
			case JALR: return true;
			default: return false;
		}
	}

	bool isRs2Used(const instr& i) {
		switch (i.op) {
			case ADD:
			case SUB:
			case AND:
			case OR:
			case XOR:
			case SLT:
			case SLTU:
			case SLL:
			case SRL:
			case SRA:
			case BEQ:
			case BNE:
			case BLT:
			case BGE:
			case BLTU:
			case BGEU:
			case SW:
			case SB:
			case SH: return true;
			default: return false;
		}
	}

	bool checkRAW(int rd, const instr& inst) {
		if (rd == 0) return false;

		int rs1 = 0, rs2 = 0;
		switch (inst.op) {
			case ADD:
			case AND:
			case OR:
			case XOR:
			case SUB:
			case SLL:
			case SLT:
			case SLTU:
			case SRL:
			case SRA:
				rs1 = inst.a2.reg;
				rs2 = inst.a3.reg;
				break;

			case ADDI:
			case ANDI:
			case ORI:
			case XORI:
			case SLTI:
			case SLTIU:
			case SLLI:
			case SRLI:
			case SRAI:
			case LB:
			case LH:
			case LW:
			case LBU:
			case LHU:
			case JALR: rs1 = inst.a2.reg; break;

			case SB:
			case SH:
			case SW:
				rs1 = inst.a1.reg;
				rs2 = inst.a2.reg;
				break;

			case BEQ:
			case BNE:
			case BLT:
			case BGE:
			case BLTU:
			case BGEU:
				rs1 = inst.a1.reg;
				rs2 = inst.a2.reg;
				break;

			default: break;
		}

		return (rs1 == rd || rs2 == rd);
	}
	// it would be better if we can utilized the share container, but we don't have time
	std::tuple<bool, bool, bool> hazard_check(const InstPacket* IF_packet, const InstPacket* ID_packet,
	                                          const InstPacket* EXE_packet, const InstPacket* MEM_packet,
	                                          const InstPacket* WB_packet) {
		bool IF_hazard  = false;
		bool ID_hazard  = false;
		bool EXE_hazard = false;

		int exe_rd = EXE_packet ? getDestReg(EXE_packet->inst) : 0;
		int mem_rd = MEM_packet ? getDestReg(MEM_packet->inst) : 0;
		int wb_rd  = WB_packet ? getDestReg(WB_packet->inst) : 0;

		// IF stage hazard: ID depends on future writes
		if (ID_packet) {
			const instr& id_instr = ID_packet->inst;
			if (isRs1Used(id_instr))
				IF_hazard |= (checkRAW(exe_rd, id_instr) || checkRAW(mem_rd, id_instr) || checkRAW(wb_rd, id_instr));
			if (isRs2Used(id_instr))
				IF_hazard |= (checkRAW(exe_rd, id_instr) || checkRAW(mem_rd, id_instr) || checkRAW(wb_rd, id_instr));
		}

		// ID stage hazard: IF depends on future writes
		if (IF_packet) {
			const instr& if_instr = IF_packet->inst;
			ID_hazard |= (checkRAW(exe_rd, if_instr) || checkRAW(mem_rd, if_instr) || checkRAW(wb_rd, if_instr));
		}

		// EXE stage hazard: ID depends on MEM/WB
		if (ID_packet) {
			const instr& id_instr = ID_packet->inst;
			EXE_hazard |= (checkRAW(mem_rd, id_instr) || checkRAW(wb_rd, id_instr));
		}

		return {IF_hazard, ID_hazard, EXE_hazard};
	}

	bool is_MemPacket(const InstPacket* packet) {
		if (!packet) { return false; }
		switch (packet->inst.op) {
			case LB:
			case LH:
			case LW:
			case LBU:
			case LHU:
			case SB:
			case SH:
			case SW: return true;
			default: return false;
		}
	}

	int get_memory_access_time() const { return memory_access_time; }
	int Stage_memory_count = 0;

private:
	const int memory_access_time = 5;
};

#endif  // SRC_RISCV_INCLUDE_PIPELINEUTIL_HH_