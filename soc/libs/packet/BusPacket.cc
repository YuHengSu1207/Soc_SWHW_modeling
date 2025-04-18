#include "packet/BusPacket.hh"

void BusMemReadReqPacket::renew(int burst_size, std::vector<MemReadReqPacket*>& memPackets,
                                std::function<void(BusMemReadRespPacket*)> cb) {
	this->acalsim::SimPacket::renew();
	this->burstSize     = burst_size;               // Update burst size
	this->TransactionID = generateTransactionID();  // Generate new transaction ID
	this->memReadReqPkt = memPackets;               // Update memory packet vector
	this->callback      = std::move(cb);            // Update callback function
}

void BusMemReadRespPacket::renew(int burst_size, std::vector<MemReadRespPacket*>& memPackets) {
	this->acalsim::SimPacket::renew();
	this->burstSize      = burst_size;
	this->burstLength    = burst_size * burst_size;
	this->memReadRespPkt = memPackets;
}

void BusMemWriteReqPacket::renew(int burst_size, std::vector<MemWriteReqPacket*>& memPackets,
                                 std::function<void(BusMemWriteRespPacket*)> cb) {
	this->acalsim::SimPacket::renew();
	this->burstSize      = burst_size;               // Update burst size
	this->TransactionID  = generateTransactionID();  // Generate new transaction ID
	this->memWriteReqPkt = memPackets;               // Update memory packet vector
	this->callback       = std::move(cb);            // Update callback function
}

void BusMemWriteRespPacket::renew(int burst_size, std::vector<MemWriteRespPacket*>& memPackets) {
	this->acalsim::SimPacket::renew();
	this->burstSize       = burst_size;
	this->burstLength     = burst_size * burst_size;
	this->memWriteRespPkt = memPackets;
}

void BusMemReadReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto bus = dynamic_cast<AXIBus*>(&_simulator)) {
		const std::vector<MemReadReqPacket*> readPackets = this->getMemReadReqPkt();
		auto                                 _addr       = readPackets[0]->getAddr();
		if (_addr >= 0xf000 && _addr <= 0xf03f) {
			// MMIO
			bus->memReadDMA(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this));
		} else {
			bus->memReadDM(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this));
		}
	} else {
		CLASS_ERROR << "Unrecongized simulator";
	}
}

void BusMemWriteReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto bus = dynamic_cast<AXIBus*>(&_simulator)) {
		const std::vector<MemWriteReqPacket*> writePackets = this->getMemWriteReqPkt();
		auto                                  _addr        = writePackets[0]->getAddr();
		if (_addr >= 0xf000 && _addr <= 0xf03f) {
			bus->memWriteDMA(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this));
		} else {
			bus->memWriteDM(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this));
		}
	} else {
		CLASS_ERROR << "Unrecongized simulator";
	}
}