#include "IDStage.hh"

void IDStage::step() {
	acalsim::Tick currTick       = top->getGlobalTick();
	bool          hasInst        = this->getPipeRegister("prIF2ID-out")->isValid();
	bool          hazard         = false;
	bool          stall_ma       = false;
	bool          control_hazard = false;
	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prIF2ID-out")->value();

		// Use the unified hazard check
		auto [_, idHazard, exeHazard] = hazard_check(nullptr, instPacket, EXEInstPacket, MEMInstPacket, WBInstPacket);
		hazard                        = idHazard || exeHazard;

		CLASS_INFO << "IDstage : the ID instruction @PC=" << instPacket->pc
		           << " instruction : " << instrToString(instPacket->inst.op)
		           << "\n the EXE instruction @PC= " << ((EXEInstPacket) ? EXEInstPacket->pc : 9487)
		           << " instruction : " << ((EXEInstPacket) ? instrToString(EXEInstPacket->inst.op) : "nop")
		           << "\n the MEM instruction @PC= " << ((MEMInstPacket) ? MEMInstPacket->pc : 9487)
		           << " instruction : " << ((MEMInstPacket) ? instrToString(MEMInstPacket->inst.op) : "nop")
		           << "\n the WB instruction @PC= " << ((WBInstPacket) ? WBInstPacket->pc : 9487)
		           << " instruction : " << ((WBInstPacket) ? instrToString(WBInstPacket->inst.op) : "nop");

		if (EXEInstPacket && EXEInstPacket->isTakenBranch) { control_hazard = true; }

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

		if (!this->getPipeRegister("prID2EXE-in")->isStalled()) {
			if (!hazard && !stall_ma) {
				SimPacket* pkt = this->getPipeRegister("prIF2ID-out")->pop();
				this->accept(currTick, *pkt);
			} else {
				this->forceStepInNextIteration();
				if (!stall_ma) {
					// Only advance stages if there are no hazards downstream
					if (control_hazard || hazard) {
						// CLASS_INFO << "   IDStage step(): Control hazard / hazard";
						WBInstPacket  = MEMInstPacket;
						MEMInstPacket = EXEInstPacket;
						EXEInstPacket = nullptr;
					}
				} else {
					CLASS_INFO << "   IDStage step(): STALL MA, pc :  " << MEMInstPacket->pc;
				}
			}
		}
	}
}

void IDStage::instPacketHandler(acalsim::Tick when, acalsim::SimPacket* pkt) {
	// CLASS_INFO << "   IDStage::instPacketHandler() received InstPacket @PC=" << ((InstPacket*)pkt)->pc
	//            << " from prIF2ID-out and pushes to prID2EXE-in";

	if (!this->getPipeRegister("prID2EXE-in")->push(pkt)) { CLASS_ERROR << "IDStage failed to push InstPacket!"; }

	// Pipeline tracking for hazard detection
	WBInstPacket  = MEMInstPacket;
	MEMInstPacket = EXEInstPacket;
	EXEInstPacket = (InstPacket*)pkt;
}