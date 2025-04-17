#include "Bus.hh"

#include "TraceRecord.hh"

void AXIBus::memReadDM(acalsim::Tick when, acalsim::SimPacket* pkt) {
	int  latency     = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");
	auto memReqPkt   = dynamic_cast<BusMemReadReqPacket*>(pkt);
	int  bus_latency = 1;
	burst_num_map[memReqPkt->getTransactionID()] = memReqPkt->getBurstSize();
	// LABELED_INFO(this->getName()) << "AXIBus to memory transfer read with tid : " << memReqPkt->getTransactionID();
	if (auto dm = dynamic_cast<DataMemory*>(this->getDownStream("DSDMem"))) {
		int tid = memReqPkt->getTransactionID();
		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
		    /* ph */ "B", /* pid */ "Req-" + std::to_string(tid), /* name */ "DM read",
		    /* ts */ when, /* cat */ "", /* tid */ std::to_string(tid)));
		for (int i = 0; i < memReqPkt->getBurstSize(); i++) {
			MemReadReqPacket* dm_pkt = memReqPkt->getMemReadReqPkt()[i];
			dm_pkt->setTid(memReqPkt->getTransactionID());
			MemReqEvent* event =
			    acalsim::top->getRecycleContainer()->acquire<MemReqEvent>(&MemReqEvent::renew_dm, dm, dm_pkt);
			auto event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
			    &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + latency + i + bus_latency);
			this->pushToMasterChannelPort(this->getName() + "-m_dm", event_pkt);
		}
	} else {
		CLASS_ERROR << "Invalid downstream module";
	}

	acalsim::top->getRecycleContainer()->recycle(pkt);
	return;
}

void AXIBus::memWriteDM(acalsim::Tick when, acalsim::SimPacket* pkt) {
	int  latency     = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");
	auto memReqPkt   = dynamic_cast<BusMemWriteReqPacket*>(pkt);
	int  bus_latency = 1;
	// LABELED_INFO(this->getName()) << "AXIBus to memory transfer write with tid : " << memReqPkt->getTransactionID();
	burst_num_map[memReqPkt->getTransactionID()] = memReqPkt->getBurstSize();
	if (auto dm = dynamic_cast<DataMemory*>(this->getDownStream("DSDMem"))) {
		int tid = memReqPkt->getTransactionID();
		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
		    /* ph */ "B", /* pid */ "Req-" + std::to_string(tid), /* name */ "DM write",
		    /* ts */ when, /* cat */ "", /* tid */ std::to_string(tid)));

		for (int i = 0; i < memReqPkt->getBurstSize(); i++) {
			MemWriteReqPacket* dm_pkt = memReqPkt->getMemWriteReqPkt()[i];
			MemReqEvent*       event =
			    acalsim::top->getRecycleContainer()->acquire<MemReqEvent>(&MemReqEvent::renew_dm, dm, dm_pkt);
			dm_pkt->setTid(memReqPkt->getTransactionID());  // Set transaction ID
			auto event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
			    &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + latency + i + bus_latency);
			this->pushToMasterChannelPort(this->getName() + "-m_dm", event_pkt);
		}
	} else {
		CLASS_ERROR << "Invalid downstream module";
	}

	acalsim::top->getRecycleContainer()->recycle(pkt);
	return;
}

void AXIBus::memReadDMA(acalsim::Tick when, acalsim::SimPacket* pkt) {
	int  latency     = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");
	auto memReqPkt   = dynamic_cast<BusMemReadReqPacket*>(pkt);
	int  dma_latency = 1;
	// LABELED_INFO(this->getName()) << "AXIBus to memory transfer write mmio with tid : " <<
	// memReqPkt->getTransactionID();
	if (auto dma = dynamic_cast<DMAController*>(this->getDownStream("DSDMA"))) {
		int tid = memReqPkt->getTransactionID();
		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
		    /* ph */ "B", /* pid */ "Req-" + std::to_string(tid), /* name */ "CPU peek MMIO",
		    /* ts */ when, /* cat */ "", /* tid */ std::to_string(tid)));

		for (int i = 0; i < memReqPkt->getBurstSize(); i++) {
			MemReadReqPacket* dm_pkt = memReqPkt->getMemReadReqPkt()[i];
			dm_pkt->setTid(memReqPkt->getTransactionID());
			MemReqEvent* event =
			    acalsim::top->getRecycleContainer()->acquire<MemReqEvent>(&MemReqEvent::renew_dma, dma, dm_pkt);
			auto event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
			    &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + latency + i + dma_latency);
			this->pushToMasterChannelPort(this->getName() + "-m_dma", event_pkt);
		}
	} else {
		CLASS_ERROR << "Invalid downstream module";
	}

	acalsim::top->getRecycleContainer()->recycle(pkt);
	return;
}

void AXIBus::memWriteDMA(acalsim::Tick when, acalsim::SimPacket* pkt) {
	int  latency     = acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency");
	auto memReqPkt   = dynamic_cast<BusMemWriteReqPacket*>(pkt);
	int  dma_latency = 1;
	// LABELED_INFO(this->getName()) << "AXIBus to memory transfer for write with tid (DMA): "
	//                              << memReqPkt->getTransactionID();
	if (auto dma = dynamic_cast<DMAController*>(this->getDownStream("DSDMA"))) {
		int tid = memReqPkt->getTransactionID();
		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
		    /* ph */ "B", /* pid */ "Req-" + std::to_string(tid), /* name */ "CPU write MMIO",
		    /* ts */ when, /* cat */ "", /* tid */ std::to_string(tid)));

		for (int i = 0; i < memReqPkt->getBurstSize(); i++) {
			MemWriteReqPacket* dm_pkt = memReqPkt->getMemWriteReqPkt()[i];
			MemReqEvent*       event =
			    acalsim::top->getRecycleContainer()->acquire<MemReqEvent>(&MemReqEvent::renew_dma, dma, dm_pkt);
			dm_pkt->setTid(memReqPkt->getTransactionID());  // Set transaction ID
			auto event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
			    &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + latency + i + dma_latency);
			this->pushToMasterChannelPort(this->getName() + "-m_dma", event_pkt);
		}
	} else {
		CLASS_ERROR << "Invalid downstream module";
	}

	acalsim::top->getRecycleContainer()->recycle(pkt);
	return;
}

void AXIBus::memReadRespHandler_CPU(acalsim::SimPacket* pkt) {
	// cpu with burst read being 1 ,thus directly send to CPU
	auto memRespPkt = dynamic_cast<MemReadRespPacket*>(pkt);
	auto rc         = acalsim::top->getRecycleContainer();

	this->burst_num_map.erase(memRespPkt->getTid());

	int tid = memRespPkt->getTid();
	acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
	    /* ph */ "E", /* pid */ "Req-" + std::to_string(tid), /* name */ "DM read",
	    /* ts */ acalsim::top->getGlobalTick(), /* cat */ "", /* tid */ std::to_string(tid)));

	acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createCompleteEvent(
	    /* pid */ "Req-" + std::to_string(tid), /* name */ "Bus acq path (CPU)",
	    /* ts */ acalsim::top->getGlobalTick(), /* dur */ 1, /* cat */ "", /* tid */ std::to_string(tid)));

	// LABELED_INFO(this->getName()) << "AXIBus sending read resp back to cpu at Tid " << memRespPkt->getTid();
	std::vector<MemReadRespPacket*> memPackets = {memRespPkt};
	BusMemReadRespPacket* new_pkt = rc->acquire<BusMemReadRespPacket>(&BusMemReadRespPacket::renew, 1, memPackets);
	new_pkt->setTransactionID(memRespPkt->getTid());
	// recycle packages
	rc->recycle(pkt);
	BusRespEvent* event = rc->acquire<BusRespEvent>(&BusRespEvent::renew, this, this->getUpStream("USCPU"), new_pkt);
	auto          event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
        &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + 1);
	this->pushToMasterChannelPort(this->getName() + "-m_cpu", event_pkt);
	return;
}

void AXIBus::memWriteRespHandler_CPU(acalsim::SimPacket* pkt) {
	// cpu with burst read being 1 ,thus directly send to CPU
	auto memRespPkt = dynamic_cast<MemWriteRespPacket*>(pkt);
	auto rc         = acalsim::top->getRecycleContainer();

	this->burst_num_map.erase(memRespPkt->getTid());

	int tid = memRespPkt->getTid();
	acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
	    /* ph */ "E", /* pid */ "Req-" + std::to_string(tid), /* name */ "DM write",
	    /* ts */ acalsim::top->getGlobalTick(), /* cat */ "", /* tid */ std::to_string(tid)));

	acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createCompleteEvent(
	    /* pid */ "Req-" + std::to_string(tid), /* name */ "Bus acq path (CPU)",
	    /* ts */ acalsim::top->getGlobalTick(), /* dur */ 1, /* cat */ "", /* tid */ std::to_string(tid)));

	// LABELED_INFO(this->getName()) << "AXIBus sending write resp back to cpu at Tid " << memRespPkt->getTid();
	std::vector<MemWriteRespPacket*> memPackets = {memRespPkt};
	BusMemWriteRespPacket* new_pkt = rc->acquire<BusMemWriteRespPacket>(&BusMemWriteRespPacket::renew, 1, memPackets);
	new_pkt->setTransactionID(memRespPkt->getTid());

	// recycle packages
	rc->recycle(pkt);
	BusRespEvent* event = rc->acquire<BusRespEvent>(&BusRespEvent::renew, this, this->getUpStream("USCPU"), new_pkt);
	auto          event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
        &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + 1);
	this->pushToMasterChannelPort(this->getName() + "-m_cpu", event_pkt);
	return;
}

void AXIBus::memReadRespHandler_DMA(acalsim::SimPacket* pkt) {
	auto memRespPkt = dynamic_cast<MemReadRespPacket*>(pkt);
	auto rc         = acalsim::top->getRecycleContainer();
	// LABELED_INFO("BurstMode") << "DM sending one read resp back to DMA at Tid " << memRespPkt->getTid();
	this->cur_num_map[memRespPkt->getTid()]++;
	std::vector<MemReadRespPacket*>& memPackets = this->readResponses[memRespPkt->getTid()];
	memPackets.push_back(memRespPkt);
	if (this->cur_num_map[memRespPkt->getTid()] == this->burst_num_map[memRespPkt->getTid()]) {
		int tid = memRespPkt->getTid();
		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
		    /* ph */ "E", /* pid */ "Req-" + std::to_string(tid), /* name */ "CPU peek MMIO",
		    /* ts */ acalsim::top->getGlobalTick(), /* cat */ "", /* tid */ std::to_string(tid)));

		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createCompleteEvent(
		    /* pid */ "Req-" + std::to_string(tid), /* name */ "Bus acq path (DMA)",
		    /* ts */ acalsim::top->getGlobalTick(), /* dur */ 1, /* cat */ "", /* tid */ std::to_string(tid)));

		int                   burst_num = this->burst_num_map[memRespPkt->getTid()];
		BusMemReadRespPacket* new_pkt =
		    rc->acquire<BusMemReadRespPacket>(&BusMemReadRespPacket::renew, burst_num, memPackets);
		new_pkt->setTransactionID(memRespPkt->getTid());
		this->cur_num_map.erase(memRespPkt->getTid());
		this->burst_num_map.erase(memRespPkt->getTid());
		this->readResponses.erase(memRespPkt->getTid());
		BusRespEvent* event =
		    rc->acquire<BusRespEvent>(&BusRespEvent::renew, this, this->getUpStream("USDMA"), new_pkt);
		auto event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
		    &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + 1);
		this->pushToMasterChannelPort(this->getName() + "-m_dma", event_pkt);
	}
	// recycle packages
	rc->recycle(pkt);
	return;
}

void AXIBus::memWriteRespHandler_DMA(acalsim::SimPacket* pkt) {
	auto memRespPkt = dynamic_cast<MemWriteRespPacket*>(pkt);
	auto rc         = acalsim::top->getRecycleContainer();

	this->cur_num_map[memRespPkt->getTid()]++;
	std::vector<MemWriteRespPacket*>& memPackets = this->writeResponses[memRespPkt->getTid()];
	memPackets.push_back(memRespPkt);
	if (this->cur_num_map[memRespPkt->getTid()] == this->burst_num_map[memRespPkt->getTid()]) {
		int tid = memRespPkt->getTid();
		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createDurationEvent(
		    /* ph */ "E", /* pid */ "Req-" + std::to_string(tid), /* name */ "CPU write MMIO",
		    /* ts */ acalsim::top->getGlobalTick(), /* cat */ "", /* tid */ std::to_string(tid)));

		acalsim::top->addChromeTraceRecord(acalsim::ChromeTraceRecord::createCompleteEvent(
		    /* pid */ "Req-" + std::to_string(tid), /* name */ "Bus acq path (DMA)",
		    /* ts */ acalsim::top->getGlobalTick(), /* dur */ 1, /* cat */ "", /* tid */ std::to_string(tid)));

		int                    burst_num = burst_num_map[memRespPkt->getTid()];
		BusMemWriteRespPacket* new_pkt =
		    rc->acquire<BusMemWriteRespPacket>(&BusMemWriteRespPacket::renew, burst_num, memPackets);
		new_pkt->setTransactionID(memRespPkt->getTid());
		this->cur_num_map.erase(memRespPkt->getTid());
		this->burst_num_map.erase(memRespPkt->getTid());
		this->writeResponses.erase(memRespPkt->getTid());
		BusRespEvent* event =
		    rc->acquire<BusRespEvent>(&BusRespEvent::renew, this, this->getUpStream("USDMA"), new_pkt);
		auto event_pkt = acalsim::top->getRecycleContainer()->acquire<acalsim::ChannelEventPacket>(
		    &acalsim::ChannelEventPacket::renew, event, acalsim::top->getGlobalTick() + 1);
		this->pushToMasterChannelPort(this->getName() + "-m_dma", event_pkt);
	}

	// recycle packages
	rc->recycle(pkt);
	return;
}

void AXIBus::step() {
	for (auto s_port : this->s_ports_) {
		if (s_port.second->isPopValid()) {
			auto packet = s_port.second->pop();
			// LABELED_INFO(this->getName()) << "Acccept a packet from the slave port.";
			this->accept(acalsim::top->getGlobalTick(), *packet);
		}
	}
}