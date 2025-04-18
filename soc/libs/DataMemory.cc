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

#include "DataMemory.hh"

void DataMemory::memReadReqHandler(acalsim::Tick _when, XBarMemReadReqPayload* _memReqPkt) {
	// LABELED_INFO(this->getName()) << "DataMemory doing mem read for tid " << _memReqPkt->getTid();
	instr      i    = _memReqPkt->getInstr();
	instr_type op   = _memReqPkt->getOP();
	uint32_t   addr = _memReqPkt->getAddr();
	operand    a1   = _memReqPkt->getA1();

	size_t   bytes = 0;
	uint32_t ret   = 0;

	switch (op) {
		case LB:
		case LBU: bytes = 1; break;
		case LH:
		case LHU: bytes = 2; break;
		case LW: bytes = 4; break;
	}

	void* data = this->readData(addr, bytes, false);

	switch (op) {
		case LB: ret = static_cast<uint32_t>(*(int8_t*)data); break;
		case LBU: ret = *(uint8_t*)data; break;
		case LH: ret = static_cast<uint32_t>(*(int16_t*)data); break;
		case LHU: ret = *(uint16_t*)data; break;
		case LW: ret = *(uint32_t*)data; break;
	}

	auto                    rc = acalsim::top->getRecycleContainer();
	XBarMemReadRespPayload* memRespPkt =
	    rc->acquire<XBarMemReadRespPayload>(&XBarMemReadRespPayload::renew, i, op, ret, a1);
	memRespPkt->setTid(_memReqPkt->getTid());
	rc->recycle(_memReqPkt);
}

void DataMemory::memWriteReqHandler(acalsim::Tick _when, XBarMemWriteReqPayload* _memReqPkt) {
	// LABELED_INFO(this->getName()) << "DataMemory doing mem write for tid " << _memReqPkt->getTid();
	instr      i    = _memReqPkt->getInstr();
	instr_type op   = _memReqPkt->getOP();
	uint32_t   addr = _memReqPkt->getAddr();
	uint32_t   data = _memReqPkt->getData();

	size_t bytes = 0;

	switch (op) {
		case SB: bytes = 1; break;
		case SH: bytes = 2; break;
		case SW: bytes = 4; break;
	}

	switch (op) {
		case SB: {
			uint8_t val8 = static_cast<uint8_t>(data);
			this->writeData(&val8, addr, 1);
			break;
		}
		case SH: {
			uint16_t val16 = static_cast<uint16_t>(data);
			this->writeData(&val16, addr, 2);
			break;
		}
		case SW: {
			uint32_t val32 = static_cast<uint32_t>(data);
			this->writeData(&val32, addr, 4);
			break;
		}
	}

	auto                     rc         = acalsim::top->getRecycleContainer();
	XBarMemWriteRespPayload* memRespPkt = rc->acquire<XBarMemWriteRespPayload>(&XBarMemWriteRespPayload::renew, i);
	memRespPkt->setTid(_memReqPkt->getTid());
	rc->recycle(_memReqPkt);
}
