#include "MEMStage.hh"

void MEMStage::step() {
	acalsim::Tick currTick = top->getGlobalTick();

	if (this->getPipeRegister("prEXE2MEM-out")->isValid() && !this->getPipeRegister("prMEM2WB-in")->isStalled()) {
		SimPacket* pkt = this->getPipeRegister("prEXE2MEM-out")->pop();
		this->accept(currTick, *pkt);
	}
}

void MEMStage::instPacketHandler(acalsim::Tick when, SimPacket* pkt) {
	CLASS_INFO << "   MEMStage::instPacketHandler(): Pushing InstPacket @PC=" << ((InstPacket*)pkt)->pc
	           << " to prMEM2WB-in";

	if (!this->getPipeRegister("prMEM2WB-in")->push(pkt)) { CLASS_ERROR << "MEMStage failed to push InstPacket!"; }

	WBInstPacket = (InstPacket*)pkt;
}