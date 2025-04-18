#include "packet/XBarPacket.hh"

#include "DMA.hh"
#include "DataMemory.hh"

void XBarMemReadReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto dm = dynamic_cast<DataMemory*>(&_simulator)) {
		dm->memReadReqHandler(_when, this);
	} else if (auto dma = dynamic_cast<DMAController*>(&_simulator)) {
		dma->readMMIO(_when, this);
	} else {
		CLASS_ERROR << "Invalid module type!";
	}
}

void XBarMemWriteReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto dm = dynamic_cast<DataMemory*>(&_simulator)) {
		dm->memWriteReqHandler(_when, this);
	} else if (auto dma = dynamic_cast<DMAController*>(&_simulator)) {
		dma->writeMMIO(_when, this);
	} else {
		CLASS_ERROR << "Invalid module type!";
	}
}

void XBarMemReadRespPacket::visit(acalsim::Tick when, acalsim::SimBase& simulator) {}

void XBarMemWriteRespPacket::visit(acalsim::Tick when, acalsim::SimBase& simulator) {}