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

#include "EXEStage.hh"

void EXEStage::step() {
	if (flushed) {
		CLASS_INFO << "   EXEStage step(): Flushed, skipping this cycle";
		this->forceStepInNextIteration();
		MEMInstPacket = WBInstPacket = nullptr;
		flushed                      = false;
		return;
	}

	bool hazard = false;
	if (this->getPipeRegister("prID2EXE-out")->isValid()) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prID2EXE-out")->value();

		int mem_rd = MEMInstPacket ? getDestReg(MEMInstPacket->inst) : 0;
		int wb_rd  = WBInstPacket ? getDestReg(WBInstPacket->inst) : 0;

		hazard = (MEMInstPacket && checkRAW(mem_rd, instPacket->inst)) ||
		         (WBInstPacket && checkRAW(wb_rd, instPacket->inst));
	}

	Tick currTick = top->getGlobalTick();

	if (this->getPipeRegister("prID2EXE-out")->isValid()) {
		if (!this->getPipeRegister("prEXE2MEM-in")->isStalled() && !hazard) {
			SimPacket* pkt = this->getPipeRegister("prID2EXE-out")->pop();
			this->accept(currTick, *pkt);
		} else {
			if (hazard) { CLASS_INFO << "Hazard"; }
			if (this->getPipeRegister("prEXE2MEM-in")->isStalled())
				CLASS_INFO << "Pipeline register prEXE2MEM-in is stalled";
			WBInstPacket  = MEMInstPacket;
			MEMInstPacket = nullptr;
			this->forceStepInNextIteration();
			CLASS_INFO
			    << "   EXEStage step(): Waiting for prID2EXE-out to become valid or prEXE2MEM-in to unstall or Hazard";
		}
	}
}

void EXEStage::instPacketHandler(Tick when, SimPacket* pkt) {
	CLASS_INFO << "   EXEStage::instPacketHandler(): Received InstPacket @PC=" << ((InstPacket*)pkt)->pc
	           << " from prID2EXE-out and pushes to prEXE2MEM-in";

	if (!this->getPipeRegister("prEXE2MEM-in")->push(pkt)) {
		CLASS_ERROR << "EXEStage failed to push InstPacket to prEXE2MEM-in!";
	}

	// Shift the tracking
	WBInstPacket  = MEMInstPacket;
	MEMInstPacket = (InstPacket*)pkt;
}

void EXEStage::flush() {
	flushed = true;
	CLASS_INFO << "   EXEStage::flush(): Control hazard flush issued";
}