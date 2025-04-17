#ifndef SOC_INCLUDE_BUS_HH_
#define SOC_INCLUDE_BUS_HH_

#include "ACALSim.hh"
#include "CPU.hh"
#include "DataMemory.hh"
#include "event/BusTransactionEvent.hh"
#include "event/MemReqEvent.hh"

// AXI Bus  model
class AXIBus : public acalsim::CPPSimBase {
public:
	AXIBus(std::string _name) : acalsim::CPPSimBase(_name), transactionID(0) {
		LABELED_INFO(_name) << "Constructing...";
		// intialize the port
		// this->addMasterPort(name + "-m_dm");
		// this->addMasterPort(name + "-m_dma");
		this->addSlavePort(name + "-s", 1);
	}
	virtual ~AXIBus() {}
	void init() override { ; };
	void cleanup() override { ; };
	void step() override;
	/*
	DataMem
	*/
	void memReadDM(acalsim::Tick when, acalsim::SimPacket* pkt);
	void memWriteDM(acalsim::Tick when, acalsim::SimPacket* pkt);
	/*
	DMA
	*/
	void memReadDMA(acalsim::Tick when, acalsim::SimPacket* pkt);
	void memWriteDMA(acalsim::Tick when, acalsim::SimPacket* pkt);

	void memReadRespHandler_CPU(acalsim::SimPacket* pkt);
	void memWriteRespHandler_CPU(acalsim::SimPacket* pkt);

	void memReadRespHandler_DMA(acalsim::SimPacket* pkt);
	void memWriteRespHandler_DMA(acalsim::SimPacket* pkt);

private:
	std::map<int, int>                              burst_num_map;  // tid -> goal number of packets
	std::map<int, int>                              cur_num_map;    // tid -> current number of packets
	std::map<int, std::vector<MemReadRespPacket*>>  readResponses;
	std::map<int, std::vector<MemWriteRespPacket*>> writeResponses;
	int                                             transactionID;
};

#endif  // SOC_INCLUDE_BUS_HH_
