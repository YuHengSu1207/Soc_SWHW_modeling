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

	acalsim::Tick currTick = top->getGlobalTick();
	bool          hasInst  = this->getPipeRegister("prID2EXE-out")->isValid();
	bool          hazard   = false;

	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prID2EXE-out")->value();

		// Use unified hazard check
		auto [__, ___, exeHazard] = hazard_check(nullptr, nullptr, instPacket, MEMInstPacket, WBInstPacket);
		hazard                    = exeHazard;

		if (!hazard && !this->getPipeRegister("prEXE2MEM-in")->isStalled()) {
			SimPacket* pkt = this->getPipeRegister("prID2EXE-out")->pop();
			this->accept(currTick, *pkt);
		} else {
			this->forceStepInNextIteration();
			MEMInstPacket = nullptr;
			CLASS_INFO << "   EXEStage step(): Hazard or Downstream stall";
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