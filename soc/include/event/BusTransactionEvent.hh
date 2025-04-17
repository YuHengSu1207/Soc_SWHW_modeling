#ifndef SOC_INCLUDE_EVENT_BUSTRANSACTIONEVENT_HH_
#define SOC_INCLUDE_EVENT_BUSTRANSACTIONEVENT_HH_

#include "ACALSim.hh"
#include "event/MemReqEvent.hh"
#include "packet/BusPacket.hh"

class DataMemory;
class CPU;
class AXIBus;
class BusMemWriteReqPacket;
class BusMemReadReqPacket;
class BusMemWriteRespPacket;
class BusMemReadRespPacket;
class MemReqEvent;

class BusReqEvent : public acalsim::SimEvent {
public:
	BusReqEvent() = default;
	BusReqEvent(AXIBus* _bus, acalsim::SimPacket* _memReqPkt)
	    : acalsim::SimEvent("BusReqEvent"), bus(_bus), memReqPkt(_memReqPkt){};
	void renew(AXIBus* _bus, acalsim::SimPacket* _memReqPkt);
	void process() override;

private:
	acalsim::SimPacket* memReqPkt;
	AXIBus*             bus;
};

class BusRespEvent : public acalsim::SimEvent {
public:
	BusRespEvent() = default;
	BusRespEvent(AXIBus* _bus, acalsim::SimBase* _callee, acalsim::SimPacket* _memRespPkt)
	    : acalsim::SimEvent("BusRespEvent"), bus(_bus), callee(_callee), memRespPkt(_memRespPkt){};
	void renew(AXIBus* _bus, acalsim::SimBase* _callee, acalsim::SimPacket* _memRespPkt);
	void process() override;

private:
	acalsim::SimPacket* memRespPkt;
	acalsim::SimBase*   callee;
	AXIBus*             bus;
};

#endif  // SOC_INCLUDE_EVENT_BUSTRANSACTIONEVENT_HH_