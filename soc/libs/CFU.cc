#include "CFU.hh"

#include "DataStruct.hh"
class Controller;
class AddSubActivationUnit;
class MulUnit;
class RegisterUnit;

void CFU::step() {
	for (auto s_port : this->s_ports_) {
		if (s_port.second->isPopValid()) {
			auto packet = s_port.second->pop();
			this->accept(acalsim::top->getGlobalTick(), *packet);
		}
	}
}

uint32_t CFU::get_result(instr i, uint32_t rs1, uint32_t rs2) {
	this->controller->send_information_to_unit(i, rs1, rs2);
	this->mulUnit->computation();
	this->addSubUnit->computation();
	Unit_Decision decision = this->controller->outputSel();
	if (decision == Unit_Decision::AddSub) {
		return this->addSubUnit->getResult();
	} else if (decision == Unit_Decision::Mul) {
		return this->mulUnit->getResult();
	} else {
		CLASS_ERROR << "(CFU) No such decision";
	}
	return 0;
}

void Controller::send_information_to_unit(instr i, uint32_t rs1, uint32_t rs2) {
	static_cast<CFU*>(sim)->addSubUnit->setInstrType(i.op);
	static_cast<CFU*>(sim)->addSubUnit->setOperands(rs1, rs2);
	static_cast<CFU*>(sim)->mulUnit->setInstrType(i.op);
	static_cast<CFU*>(sim)->mulUnit->setOperands(rs1, rs2);
	this->setInstrType(i.op);
}

Unit_Decision Controller::outputSel() {
	switch (this->instr_type) {
		case S_ADDI8I8S_VV:
		case S_ADDI16I16S_VV:
		case S_SUBI8I8S_VV:
		case S_SUBI16I16S_VV: return Unit_Decision::AddSub;
		case S_PMULI8I16S_VV_L:
		case S_PMULI8I16S_VV_H:
		case S_AMULI8I8S_VV_NQ:
		case S_AMULI8I8S_VV_L: return Unit_Decision::Mul;
		default: return Unit_Decision::Unknown;
	}
}

void AddSubActivationUnit::computation() {
	switch (this->instr_type) {
		case S_ADDI8I8S_VV: {
			this->result = 0;
			for (int i = 0; i < 4; ++i) {
				int8_t  b1  = (this->rs1 >> (i * 8)) & 0xFF;
				int8_t  b2  = (this->rs2 >> (i * 8)) & 0xFF;
				uint8_t res = static_cast<int8_t>(b1 + b2);
				this->result |= (res << (i * 8));
			}
			break;
		}
		case S_ADDI16I16S_VV: {
			this->result = 0;
			for (int i = 0; i < 2; ++i) {
				int16_t  h1  = (this->rs1 >> (i * 16)) & 0xFFFF;
				int16_t  h2  = (this->rs2 >> (i * 16)) & 0xFFFF;
				uint16_t res = static_cast<int16_t>(h1 + h2);
				this->result |= (res << (i * 16));
			}
			break;
		}
		case S_SUBI8I8S_VV: {
			this->result = 0;
			for (int i = 0; i < 4; ++i) {
				int8_t  b1  = (this->rs1 >> (i * 8)) & 0xFF;
				int8_t  b2  = (this->rs2 >> (i * 8)) & 0xFF;
				uint8_t res = static_cast<int8_t>(b1 - b2);
				this->result |= (res << (i * 8));
			}
			break;
		}
		case S_SUBI16I16S_VV: {
			this->result = 0;
			for (int i = 0; i < 2; ++i) {
				int16_t  h1  = (this->rs1 >> (i * 16)) & 0xFFFF;
				int16_t  h2  = (this->rs2 >> (i * 16)) & 0xFFFF;
				uint16_t res = static_cast<int16_t>(h1 - h2);
				this->result |= (res << (i * 16));
			}
			break;
		}
		default: this->result = 0; break;
	}
}

void MulUnit::computation() {
	switch (this->instr_type) {
		case S_PMULI8I16S_VV_L: {
			this->result = 0;
			for (int i = 0; i < 2; ++i) {
				int8_t  b1  = (this->rs1 >> (i * 8)) & 0xFF;
				int8_t  b2  = (this->rs2 >> (i * 8)) & 0xFF;
				int16_t mul = static_cast<int16_t>(b1) * static_cast<int16_t>(b2);
				this->result |= ((uint16_t)mul << (i * 16));  // LSB 16-bit of each
			}
			break;
		}
		case S_PMULI8I16S_VV_H: {
			this->result = 0;
			for (int i = 2; i < 4; ++i) {
				int8_t  b1  = (this->rs1 >> (i * 8)) & 0xFF;
				int8_t  b2  = (this->rs2 >> (i * 8)) & 0xFF;
				int16_t mul = static_cast<int16_t>(b1) * static_cast<int16_t>(b2);
				this->result |= ((uint16_t)mul << ((i - 2) * 16));  // next 2 16-bit segments
			}
			break;
		}
		case S_AMULI8I8S_VV_NQ: {
			this->result = 0;
			for (int i = 0; i < 4; ++i) {
				int8_t  b1  = (this->rs1 >> (i * 8)) & 0xFF;
				int8_t  b2  = (this->rs2 >> (i * 8)) & 0xFF;
				int16_t mul = static_cast<int16_t>(b1) * static_cast<int16_t>(b2);
				uint8_t msb = (mul >> 8) & 0xFF;  // take MSB of the 16-bit product
				this->result |= (msb << (i * 8));
			}
			break;
		}
		case S_AMULI8I8S_VV_L: {
			this->result = 0;
			for (int i = 0; i < 4; ++i) {
				int8_t  b1  = (this->rs1 >> (i * 8)) & 0xFF;
				int8_t  b2  = (this->rs2 >> (i * 8)) & 0xFF;
				int16_t mul = static_cast<int16_t>(b1) * static_cast<int16_t>(b2);
				uint8_t lsb = mul & 0xFF;  // Take the lower 8 bits
				this->result |= (lsb << (i * 8));
			}
			break;
		}
		default: this->result = 0; break;
	}
}

void CFU::packet_handler(CFUReqPacket* _pkt) {
	auto           rc           = acalsim::top->getRecycleContainer();
	uint32_t       result       = this->get_result(_pkt->getInstr(), _pkt->getRs1(), _pkt->getRs2());
	CFURespPacket* cfu_resp_pkt = rc->acquire<CFURespPacket>(&CFURespPacket::renew, _pkt->getInstr(), result);
	rc->recycle(_pkt);
	this->pushToMasterChannelPort(this->getName() + "-m_cpu", cfu_resp_pkt);
	return;
}
