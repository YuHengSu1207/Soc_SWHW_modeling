#include "IDStage.hh"

void IDStage::step() {
	if (flushed) {
		this->flush();
		CLASS_INFO << "   IDStage step() : Flushed, skipping execution";
		this->forceStepInNextIteration();
		flushed = false;  // Only flush for one cycle
		return;
	}

	acalsim::Tick currTick = top->getGlobalTick();
	bool          hasInst  = this->getPipeRegister("prIF2ID-out")->isValid();
	bool          hazard   = false;
	bool          stall_ma = false;
	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prIF2ID-out")->value();

		// Use the unified hazard check
		auto [_, idHazard, exeHazard] = hazard_check(nullptr, instPacket, EXEInstPacket, MEMInstPacket, WBInstPacket);
		hazard                        = idHazard;

		CLASS_INFO << "IDstage : the ID instruction @PC=" << instPacket->pc
		           << "\n the EXE instruction @PC= " << ((EXEInstPacket) ? EXEInstPacket->pc : 9487)
		           << "\n the MEM instruction @PC= " << ((MEMInstPacket) ? MEMInstPacket->pc : 9487)
		           << "\n the WB instruction @PC= " << ((WBInstPacket) ? WBInstPacket->pc : 9487);

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
					if (exeHazard) {
						CLASS_INFO << "   IDStage step(): Hazard at EXE";
						// EXE stage hazard - stall IF, ID, EXE
						WBInstPacket  = MEMInstPacket;
						MEMInstPacket = nullptr;  // EXE can't advance to MEM
						                          // EXE and ID stay as they are
					} else if (idHazard) {
						CLASS_INFO << "   IDStage step(): Hazard at ID";
						// ID stage hazard - stall IF and ID
						WBInstPacket  = MEMInstPacket;
						MEMInstPacket = EXEInstPacket;
						EXEInstPacket = nullptr;  // ID can't advance to EXE
						                          // ID stays as it is
					}
					CLASS_INFO << "   IDStage step(): Data hazard";
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

void IDStage::flush() {
	CLASS_INFO << "   IDStage flush() called: clearing pipeline state";
	EXEInstPacket = nullptr;
	MEMInstPacket = nullptr;
	WBInstPacket  = nullptr;
	flushed       = true;
}