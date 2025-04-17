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

#ifndef EMULATOR_INCLUDE_DATASTRUCT_HH_
#define EMULATOR_INCLUDE_DATASTRUCT_HH_

#include <cstdint>
#include <cstdlib>

#define MAX_LABEL_LEN 32

typedef enum {
	UNIMPL = 0,
	ADD,
	ADDI,
	AND,
	ANDI,
	AUIPC,
	BEQ,
	BGE,
	BGEU,
	BLT,
	BLTU,
	BNE,
	JAL,
	JALR,
	LB,
	LBU,
	LH,
	LHU,
	LUI,
	LW,
	OR,
	ORI,
	SB,
	SH,
	SLL,
	SLLI,
	SLT,
	SLTI,
	SLTIU,
	SLTU,
	SRA,
	SRAI,
	SRL,
	SRLI,
	SUB,
	SW,
	XOR,
	XORI,
	S_ADDI8I8S_VV,
	S_ADDI16I16S_VV,
	S_SUBI8I8S_VV,
	S_SUBI16I16S_VV,
	S_PMULI8I16S_VV_L,
	S_PMULI8I16S_VV_H,
	S_AMULI8I8S_VV_NQ,
	S_AMULI8I8S_VV_L,
	MUL,
	HCF
} instr_type;

typedef struct {
	char* src;
	int   offset;
} source;

typedef enum {
	OPTYPE_NONE,  // more like "don't care"
	OPTYPE_REG,
	OPTYPE_IMM,
	OPTYPE_LABEL,
} operand_type;

typedef struct {
	operand_type type = OPTYPE_NONE;
	char         label[MAX_LABEL_LEN];
	int          reg;
	uint32_t     imm;

} operand;

typedef struct {
	instr_type op;
	operand    a1;
	operand    a2;
	operand    a3;
	char*      psrc       = NULL;
	int        orig_line  = -1;
	bool       breakpoint = false;
} instr;

typedef struct {
	char label[MAX_LABEL_LEN];
	int  loc = -1;
} label_loc;

#endif
