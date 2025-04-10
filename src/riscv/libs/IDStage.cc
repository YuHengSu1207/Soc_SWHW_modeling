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

	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prIF2ID-out")->value();

		// Use the unified hazard check
		auto [_, idHazard, exeHazard] = hazard_check(nullptr, instPacket, EXEInstPacket, MEMInstPacket, WBInstPacket);
		hazard                        = idHazard;

		if (!hazard && !this->getPipeRegister("prID2EXE-in")->isStalled()) {
			SimPacket* pkt = this->getPipeRegister("prIF2ID-out")->pop();
			this->accept(currTick, *pkt);
		} else {
			this->forceStepInNextIteration();
			WBInstPacket = MEMInstPacket;
			if (!exeHazard) { MEMInstPacket = EXEInstPacket; }
			EXEInstPacket = nullptr;
			CLASS_INFO << "   IDStage step(): Data hazard or downstream stall";
		}
	}
}

void IDStage::instPacketHandler(acalsim::Tick when, acalsim::SimPacket* pkt) {
	CLASS_INFO << "   IDStage::instPacketHandler() received InstPacket @PC=" << ((InstPacket*)pkt)->pc
	           << " from prIF2ID-out and pushes to prID2EXE-in";

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