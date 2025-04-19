#include "packet/XBarPacket.hh"

#include "DMA.hh"
#include "DataMemory.hh"

void XBarMemReadReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	CLASS_INFO << "Cross Bar Packet handled!";
}

void XBarMemWriteReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	CLASS_INFO << "Cross Bar Packet handled!";
}

void XBarMemReadRespPacket::visit(acalsim::Tick when, acalsim::SimBase& simulator) {
	
}

void XBarMemWriteRespPacket::visit(acalsim::Tick when, acalsim::SimBase& simulator) {}