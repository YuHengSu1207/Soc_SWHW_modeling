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
	acalsim::Tick currTick       = top->getGlobalTick();
	bool          hasInst        = this->getPipeRegister("prID2EXE-out")->isValid();
	bool          stall_ma       = false;
	bool          control_hazard = false;
	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prID2EXE-out")->value();

		control_hazard = instPacket->isTakenBranch;

		CLASS_INFO << " the EXE instruction @PC= " << instPacket->pc
		           << " instruction : " << instrToString(instPacket->inst.op)
		           << "\n the MEM instruction @PC= " << ((MEMInstPacket) ? MEMInstPacket->pc : 9487)
		           << " instruction : " << ((MEMInstPacket) ? instrToString(MEMInstPacket->inst.op) : "nop")
		           << "\n the WB instruction @PC= " << ((WBInstPacket) ? WBInstPacket->pc : 9487)
		           << " instruction : " << ((WBInstPacket) ? instrToString(WBInstPacket->inst.op) : "nop");

		if (is_MemPacket(MEMInstPacket)) {
			if (last_mem_access_pc != MEMInstPacket->pc) {
				last_mem_access_pc = MEMInstPacket->pc;
				mem_access_count   = 1;
				stall_ma           = true;
			} else {
				if (mem_access_count < get_memory_access_time()) {
					mem_access_count++;
					stall_ma = true;
				} else {
					stall_ma = false;
				}
			}
			CLASS_INFO << "  EXEStage step():  Memory is " << ((stall_ma) ? "stalled" : "not stalled");
		}

		if (!this->getPipeRegister("prEXE2MEM-in")->isStalled()) {
			if (!stall_ma) {
				SimPacket* pkt = this->getPipeRegister("prID2EXE-out")->pop();
				this->accept(currTick, *pkt);
			} else {
				this->forceStepInNextIteration();
				CLASS_INFO << "   EXEStage step(): Stall Ma, pc : " << MEMInstPacket->pc;
			}
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