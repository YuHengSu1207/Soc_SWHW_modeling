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

#include "IFStage.hh"

void IFStage::step() {
	acalsim::Tick currTick       = top->getGlobalTick();
	bool          hasInst        = this->getSlavePort("soc-s")->isPopValid();
	bool          hazard         = false;
	bool          stall_ma       = false;
	bool          control_hazard = false;
	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getSlavePort("soc-s")->front();

		// Use the unified hazard check
		auto [ifHazard, idHazard, exeHazard] =
		    hazard_check(instPacket, IDInstPacket, EXEInstPacket, MEMInstPacket, WBInstPacket);
		hazard = ifHazard || idHazard || exeHazard;

		CLASS_INFO << "IFstage : the IF instruction @PC=" << instPacket->pc
		           << " instruction : " << instrToString(instPacket->inst.op)
		           << "\n the ID instruction @PC= " << ((IDInstPacket) ? IDInstPacket->pc : 9487)
		           << " instruction : " << ((IDInstPacket) ? instrToString(IDInstPacket->inst.op) : "nop")
		           << "\n the EXE instruction @PC= " << ((EXEInstPacket) ? EXEInstPacket->pc : 9487)
		           << " instruction : " << ((EXEInstPacket) ? instrToString(EXEInstPacket->inst.op) : "nop")
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
		}

		if (EXEInstPacket && EXEInstPacket->isTakenBranch) { control_hazard = true; }

		if (!this->getPipeRegister("prIF2ID-in")->isStalled()) {
			if (!hazard && !stall_ma && !control_hazard) {
				SimPacket* pkt = this->getSlavePort("soc-s")->pop();
				this->accept(currTick, *pkt);
			} else {
				// either hazard or stall
				this->forceStepInNextIteration();
				if (!stall_ma) {
					if (hazard) { CLASS_INFO << "   IFStage step(): Hazard at IF"; }
					if (control_hazard) { CLASS_INFO << "   IFStage step(): Control hazard"; }
					WBInstPacket  = MEMInstPacket;
					MEMInstPacket = EXEInstPacket;
					EXEInstPacket = nullptr;
					IDInstPacket  = IDInstPacket;  // keep the IDInstPacket
				} else {
					CLASS_INFO << "   IFStage step(): Stall Ma, pc : " << MEMInstPacket->pc;
				}
			}
		}
	}
}

void IFStage::instPacketHandler(Tick when, SimPacket* pkt) {
	CLASS_INFO << "   IFStage::instPacketHandler(): Received InstPacket @PC=" << ((InstPacket*)pkt)->pc
	           << " pushing to prIF2ID-in";

	if (!this->getPipeRegister("prIF2ID-in")->push(pkt)) { CLASS_ERROR << "IFStage failed to push InstPacket!"; }

	// Pipeline tracking shift
	WBInstPacket  = MEMInstPacket;
	MEMInstPacket = EXEInstPacket;
	EXEInstPacket = IDInstPacket;
	IDInstPacket  = (InstPacket*)pkt;
}