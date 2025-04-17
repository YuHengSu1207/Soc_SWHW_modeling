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

#include "packet/MemPacket.hh"

#include "DMA.hh"
#include "DataMemory.hh"

void MemReadRespPacket::renew(const instr& _i, instr_type _op, uint32_t _data, operand _a1) {
	this->acalsim::SimPacket::renew();
	this->i    = _i;
	this->op   = _op;
	this->data = _data;
	this->a1   = _a1;
}

void MemReadReqPacket::renew(std::function<void(MemReadRespPacket*)> _callback, const instr& _i, instr_type _op,
                             uint32_t _addr, operand _a1) {
	this->acalsim::SimPacket::renew();
	this->callback = _callback;
	this->i        = _i;
	this->op       = _op;
	this->addr     = _addr;
	this->a1       = _a1;
}

void MemReadReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto dm = dynamic_cast<DataMemory*>(&_simulator)) {
		dm->memReadReqHandler(_when, this);
	} else if (auto dma = dynamic_cast<DMAController*>(&_simulator)) {
		dma->readMMIO(_when, this);
	} else {
		CLASS_ERROR << "Invalid module type!";
	}
}

void MemReadReqPacket::visit(acalsim::Tick _when, acalsim::SimModule& _module) {
	CLASS_ERROR << "void MemReadReqPacket::visit (SimModule& _module) is not implemented yet!";
}

void MemWriteRespPacket::renew(const instr& _i) {
	this->acalsim::SimPacket::renew();
	this->i = _i;
}

void MemWriteReqPacket::renew(std::function<void(MemWriteRespPacket*)> _callback, const instr& _i, instr_type _op,
                              uint32_t _addr, uint32_t _data) {
	this->acalsim::SimPacket::renew();
	this->callback = _callback;
	this->i        = _i;
	this->op       = _op;
	this->addr     = _addr;
	this->data     = _data;
}

void MemWriteReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto dm = dynamic_cast<DataMemory*>(&_simulator)) {
		dm->memWriteReqHandler(_when, this);
	} else if (auto dma = dynamic_cast<DMAController*>(&_simulator)) {
		dma->writeMMIO(_when, this);
	} else {
		CLASS_ERROR << "Invalid module type!";
	}
}

void MemWriteReqPacket::visit(acalsim::Tick _when, acalsim::SimModule& _simulator) {
	CLASS_ERROR << "void MemWriteReqPacket::visit (SimBase& simulator) is not implemented yet!";
}

void MemReadRespPacket::visit(acalsim::Tick _when, acalsim::SimModule& _module) {
	CLASS_ERROR << "void MemReadRespPacket::visit (SimModule& module) is not implemented yet!";
}

void MemReadRespPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	CLASS_ERROR << "void MemReadRespPacket::visit (SimBase& simulator) is not implemented yet!";
}

void MemWriteRespPacket::visit(acalsim::Tick _when, acalsim::SimModule& _module) {
	CLASS_ERROR << "void MemWriteRespPacket::visit (SimModule& module) is not implemented yet!";
}

void MemWriteRespPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	CLASS_ERROR << "void MemWriteRespPacket::visit (SimBase& simulator) is not implemented yet!";
}
