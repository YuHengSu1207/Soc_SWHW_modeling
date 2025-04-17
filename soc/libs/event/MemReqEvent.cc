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

#include "event/MemReqEvent.hh"

#include "DMA.hh"
#include "DataMemory.hh"

MemReqEvent::MemReqEvent(DataMemory* _dm, acalsim::SimPacket* _memReqPkt)
    : acalsim::SimEvent("MemReqEvent"), dm(_dm), memReqPkt(_memReqPkt) {}

MemReqEvent::MemReqEvent(DMAController* _dma, acalsim::SimPacket* _memReqPkt)
    : acalsim::SimEvent("MemReqEvent"), dma(_dma), memReqPkt(_memReqPkt) {}

void MemReqEvent::renew_dm(DataMemory* _dm, acalsim::SimPacket* _memReqPkt) {
	this->acalsim::SimEvent::renew();
	this->dm        = _dm;
	this->memReqPkt = _memReqPkt;
	this->callee    = MemReqCallee::dm;
}

void MemReqEvent::renew_dma(DMAController* _dma, acalsim::SimPacket* _memReqPkt) {
	this->acalsim::SimEvent::renew();
	this->dma       = _dma;
	this->memReqPkt = _memReqPkt;
	this->callee    = MemReqCallee::dma;
}

void MemReqEvent::process() {
	if (this->callee == MemReqCallee::dm) {
		this->dm->accept(acalsim::top->getGlobalTick(), *this->memReqPkt);
	} else if (this->callee == MemReqCallee::dma) {
		this->dma->accept(acalsim::top->getGlobalTick(), *(this->memReqPkt));
	}
}
