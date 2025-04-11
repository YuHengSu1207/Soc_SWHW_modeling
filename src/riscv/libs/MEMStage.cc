#include "MEMStage.hh"

void MEMStage::step() {
	acalsim::Tick currTick      = top->getGlobalTick();
	bool          hasInst       = this->getPipeRegister("prEXE2MEM-out")->isValid();
	bool          stall_ma      = false;
	bool          is_mem_packet = false;
	if (hasInst) {
		InstPacket* instPacket = (InstPacket*)this->getPipeRegister("prEXE2MEM-out")->value();

		CLASS_INFO << " the MEM instruction @PC= " << instPacket->pc
		           << "\n the WB instruction @PC= " << ((WBInstPacket) ? WBInstPacket->pc : 9487);

		if (is_MemPacket(instPacket)) {
			is_mem_packet = true;
			if (last_mem_access_pc != instPacket->pc) {
				last_mem_access_pc = instPacket->pc;
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
	}
	if (hasInst) {
		if (!this->getPipeRegister("prMEM2WB-in")->isStalled()) {
			if (!stall_ma) {
				SimPacket* pkt = this->getPipeRegister("prEXE2MEM-out")->pop();
				this->accept(currTick, *pkt);
				if (is_mem_packet) { CLASS_INFO << "PC: " << ((InstPacket*)pkt)->pc << " Resolve the memory access"; }
			} else {
				this->forceStepInNextIteration();
			}
		}
	}
}

void MEMStage::instPacketHandler(acalsim::Tick when, SimPacket* pkt) {
	CLASS_INFO << "   MEMStage::instPacketHandler(): Pushing InstPacket @PC=" << ((InstPacket*)pkt)->pc
	           << " to prMEM2WB-in";

	if (!this->getPipeRegister("prMEM2WB-in")->push(pkt)) { CLASS_ERROR << "MEMStage failed to push InstPacket!"; }

	WBInstPacket = (InstPacket*)pkt;
}
