#ifndef SOC_INCLUDE_PACKET_CFUPACKET_HH_
#define SOC_INCLUDE_PACKET_CFUPACKET_HH_

#include <cstdint>

#include "ACALSim.hh"
#include "DataStruct.hh"

/**
 * @brief CFU computation packet (input: instr, rs1, rs2; output: rd, instr)
 */
class CFUReqPacket : public acalsim::SimPacket {
public:
	CFUReqPacket() {}

	CFUReqPacket(const instr& _i, uint32_t _rs1, uint32_t _rs2) : i(_i), rs1(_rs1), rs2(_rs2) {}

	virtual ~CFUReqPacket() {}

	/// Renew with new values
	void renew(const instr& _i, uint32_t _rs1, uint32_t _rs2) {
		this->acalsim::SimPacket::renew();
		i   = _i;
		rs1 = _rs1;
		rs2 = _rs2;
	}

	/// Visit simulation module (to be implemented)
	void visit(acalsim::Tick _when, acalsim::SimModule& _module) override;
	/// Visit simulation base (to be implemented)
	void visit(acalsim::Tick _when, acalsim::SimBase& _simulator) override;

	// Getters
	const instr& getInstr() const { return i; }
	uint32_t     getRs1() const { return rs1; }
	uint32_t     getRs2() const { return rs2; }

private:
	instr    i;
	uint32_t rs1 = 0;
	uint32_t rs2 = 0;
};

/**
 * @brief CFU response packet (returns rd + instr)
 */
class CFURespPacket : public acalsim::SimPacket {
public:
	CFURespPacket() {}

	CFURespPacket(const instr& _i, uint32_t _rd) : i(_i), rd(_rd) {}

	virtual ~CFURespPacket() {}

	/// Renew the response packet
	void renew(const instr& _i, uint32_t _rd) {
		this->acalsim::SimPacket::renew();
		i  = _i;
		rd = _rd;
	}
	/// Visit simulation module (to be implemented)
	void visit(acalsim::Tick _when, acalsim::SimModule& _module) override;
	/// Visit simulation base (to be implemented)
	void visit(acalsim::Tick _when, acalsim::SimBase& _simulator) override;

	const instr& getInstr() const { return i; }
	uint32_t     getRd() const { return rd; }

private:
	instr    i;
	uint32_t rd = 0;
};

#endif  // SOC_INCLUDE_PACKET_CFUPACKET_HH_
