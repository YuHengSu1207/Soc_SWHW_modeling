#include "ACALSim.hh"
#include "DataStruct.hh"

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
};

#endif  // SRC_RISCV_INCLUDE_PIPELINEUTIL_HH_