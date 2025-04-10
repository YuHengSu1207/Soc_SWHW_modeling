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
	acalsim::Tick currTick = top->getGlobalTick();
	bool          hasInst  = this->getSlavePort("soc-s")->isPopValid();
	bool          hazard   = false;
	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getSlavePort("soc-s")->front();
		instr       i          = instPacket->inst;

		int exe_rd = EXEInstPacket ? getDestReg(EXEInstPacket->inst) : 0;
		int mem_rd = MEMInstPacket ? getDestReg(MEMInstPacket->inst) : 0;
		int wb_rd  = WBInstPacket ? getDestReg(WBInstPacket->inst) : 0;

		if (IDInstPacket) {
			bool use_rs1 = isRs1Used(IDInstPacket->inst);
			bool use_rs2 = isRs2Used(IDInstPacket->inst);

			hazard |= (use_rs1 && checkRAW(exe_rd, IDInstPacket->inst));
			hazard |= (use_rs2 && checkRAW(exe_rd, IDInstPacket->inst));
			hazard |= (use_rs1 && checkRAW(mem_rd, IDInstPacket->inst));
			hazard |= (use_rs2 && checkRAW(mem_rd, IDInstPacket->inst));
			hazard |= (use_rs1 && checkRAW(wb_rd, IDInstPacket->inst));
			hazard |= (use_rs2 && checkRAW(wb_rd, IDInstPacket->inst));
		}
	}

	if (hasInst) {
		if (!this->getPipeRegister("prIF2ID-in")->isStalled() && !hazard) {
			SimPacket* pkt = this->getSlavePort("soc-s")->pop();
			this->accept(currTick, *pkt);
		} else {
			// Pipeline tracking shift
			WBInstPacket       = MEMInstPacket;
			MEMInstPacket      = EXEInstPacket;
			EXEInstPacket      = IDInstPacket;
			this->IDInstPacket = nullptr;
			this->forceStepInNextIteration();
			CLASS_INFO << "   IFStage step(): Hazard or Downstream stall";
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