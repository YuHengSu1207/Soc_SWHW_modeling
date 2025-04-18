#include "event/BusTransactionEvent.hh"

#include "TraceRecord.hh"
void BusReqEvent::renew(AXIBus* _bus, acalsim::SimPacket* _memReqPkt) {
	this->acalsim::SimEvent::renew();
	this->bus       = _bus;
	this->memReqPkt = _memReqPkt;
	return;
}

void BusReqEvent::process() {
	if (auto Readreq = dynamic_cast<BusMemReadReqPacket*>(this->memReqPkt)) {
		const std::vector<MemReadReqPacket*> readPackets = Readreq->getMemReadReqPkt();
		auto                                 _addr       = readPackets[0]->getAddr();
		if (_addr >= 0xf000 && _addr <= 0xf03f) {
			// MMIO
			this->bus->memReadDMA(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this->memReqPkt));
		} else {
			this->bus->memReadDM(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this->memReqPkt));
		}
	} else if (auto Writereq = dynamic_cast<BusMemWriteReqPacket*>(this->memReqPkt)) {
		const std::vector<MemWriteReqPacket*> writePackets = Writereq->getMemWriteReqPkt();
		auto                                  _addr        = writePackets[0]->getAddr();
		if (_addr >= 0xf000 && _addr <= 0xf03f) {
			this->bus->memWriteDMA(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this->memReqPkt));
		} else {
			this->bus->memWriteDM(acalsim::top->getGlobalTick() + 1, (acalsim::SimPacket*)(this->memReqPkt));
		}
	} else {
		LABELED_ERROR(this->getName()) << "Packet unrecognized in bus";
	}
	return;
}

void BusRespEvent::renew(AXIBus* _bus, acalsim::SimBase* _callee, acalsim::SimPacket* _memRespPkt) {
	this->acalsim::SimEvent::renew();
	this->bus        = _bus;
	this->callee     = _callee;
	this->memRespPkt = _memRespPkt;
}

void BusRespEvent::process() {
	if (auto cpu = dynamic_cast<CPU*>(this->callee)) {
		if (auto Readresp = dynamic_cast<BusMemReadRespPacket*>(this->memRespPkt)) {
			// LABELED_INFO(this->getName()) << "Bus doing read resp to cpu";
			cpu->memReadBusRespHandler(Readresp);
		} else if (auto Writeresp = dynamic_cast<BusMemWriteRespPacket*>(this->memRespPkt)) {
			// LABELED_INFO(this->getName()) << "Bus doing write resp to cpu";
			cpu->memWriteBusRespHandler(Writeresp);
		}
	} else if (auto dma = dynamic_cast<DMAController*>(this->callee)) {
		if (auto Readresp = dynamic_cast<BusMemReadRespPacket*>(this->memRespPkt)) {
			// LABELED_INFO(this->getName()) << "Bus doing read resp to cpu";
			dma->handleReadResponse(Readresp);

		} else if (auto Writeresp = dynamic_cast<BusMemWriteRespPacket*>(this->memRespPkt)) {
			// LABELED_INFO(this->getName()) << "Bus doing write resp to cpu";
			dma->handleWriteCompletion(Writeresp);
		}
	} else {
		LABELED_ERROR(this->getName()) << "Callee unrecognized in bus (resp)";
	}
}
