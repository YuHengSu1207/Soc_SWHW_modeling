#include "IDStage.hh"

void IDStage::step() {
	if (flushed) {
		this->flush();
		CLASS_INFO << "   IDStage step() : Flushed, skipping execution";
		this->forceStepInNextIteration();
		flushed = false;  // Only flush for one cycle
		return;
	}

	bool dataHazard = false;
	if (this->getPipeRegister("prIF2ID-out")->isValid()) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prIF2ID-out")->value();

		int exe_rd = EXEInstPacket ? getDestReg(EXEInstPacket->inst) : 0;
		int mem_rd = MEMInstPacket ? getDestReg(MEMInstPacket->inst) : 0;
		int wb_rd  = WBInstPacket ? getDestReg(WBInstPacket->inst) : 0;

		if (instPacket) {
			dataHazard = (EXEInstPacket && checkRAW(exe_rd, instPacket->inst)) ||
			             (MEMInstPacket && checkRAW(mem_rd, instPacket->inst)) ||
			             (WBInstPacket && checkRAW(wb_rd, instPacket->inst));
		}
	}

	Tick currTick = top->getGlobalTick();
	if (this->getPipeRegister("prIF2ID-out")->isValid()) {
		if (!dataHazard && !this->getPipeRegister("prID2EXE-in")->isStalled()) {
			SimPacket* pkt = this->getPipeRegister("prIF2ID-out")->pop();
			this->accept(currTick, *pkt);
		} else {
			WBInstPacket  = MEMInstPacket;
			MEMInstPacket = EXEInstPacket;
			EXEInstPacket = nullptr;
			this->forceStepInNextIteration();
			CLASS_INFO << "   IDStage step() : Data hazard detected, stalling";
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