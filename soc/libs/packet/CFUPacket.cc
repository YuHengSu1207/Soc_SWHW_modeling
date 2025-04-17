#include "packet/CFUPacket.hh"

#include "CFU.hh"
#include "CPU.hh"
void CFUReqPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto cfu = dynamic_cast<CFU*>(&_simulator)) {
		cfu->packet_handler(this);
	} else {
		CLASS_ERROR << "Not a valid module";
	}
}

void CFUReqPacket::visit(acalsim::Tick _when, acalsim::SimModule& _module) {
	CLASS_ERROR << "Visit by SimModule not implemented";
}

void CFURespPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto cpu = dynamic_cast<CPU*>(&_simulator)) {
		cpu->CFURespHandler(this);
	} else {
		CLASS_ERROR << "Not a valid module";
	}
}

void CFURespPacket::visit(acalsim::Tick _when, acalsim::SimModule& _module) {
	CLASS_ERROR << "Visit by SimModule not implemented";
}
