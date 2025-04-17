#ifndef SOC_INCLUDE_CFU_HH_
#define SOC_INCLUDE_CFU_HH_

#include <memory>

#include "ACALSim.hh"
#include "DataStruct.hh"
#include "packet/CFUPacket.hh"

using instr_enum = ::instr_type;

class Controller;
class AddSubActivationUnit;
class MulUnit;

enum class Unit_Decision { AddSub, Mul, Unknown };

class CFU : public acalsim::CPPSimBase {
public:
	CFU(std::string name) : CPPSimBase(name) {
		LABELED_INFO(this->getName()) << "Constructing ...";
		controller = std::make_unique<Controller>(this);
		addSubUnit = std::make_unique<AddSubActivationUnit>(this);
		mulUnit    = std::make_unique<MulUnit>(this);
	}
	virtual ~CFU() {}

	void                                  init() override { ; };
	void                                  step() override;
	void                                  cleanup() override { ; };
	void                                  packet_handler(CFUReqPacket* _pkt);
	uint32_t                              get_result(instr i, uint32_t rs1, uint32_t rs2);
	std::unique_ptr<Controller>           controller;
	std::unique_ptr<AddSubActivationUnit> addSubUnit;
	std::unique_ptr<MulUnit>              mulUnit;

private:
	friend class Controller;
	friend class AddSubActivationUnit;
	friend class MulUnit;
};

class Controller {
public:
	Controller(acalsim::CPPSimBase* sim) : sim(sim) {}
	Unit_Decision outputSel();
	void          setInstrType(instr_enum type) { instr_type = type; }
	void          send_information_to_unit(instr i, uint32_t rs1, uint32_t rs2);

private:
	instr_enum           instr_type;
	acalsim::CPPSimBase* sim;
};

class AddSubActivationUnit {
public:
	AddSubActivationUnit(acalsim::CPPSimBase* sim) : sim(sim) {}
	void setOperands(uint32_t rs1, uint32_t rs2) {
		this->rs1 = rs1;
		this->rs2 = rs2;
	}
	void     computation();
	void     setInstrType(instr_enum type) { instr_type = type; }
	uint32_t getResult() { return result; };

private:
	instr_enum           instr_type;
	uint32_t             rs1, rs2;
	uint32_t             result;
	acalsim::CPPSimBase* sim;
};

class MulUnit {
public:
	MulUnit(acalsim::CPPSimBase* sim) : sim(sim) {}
	void setOperands(uint32_t rs1, uint32_t rs2) {
		this->rs1 = rs1;
		this->rs2 = rs2;
	}
	void     computation();
	void     setInstrType(instr_enum type) { instr_type = type; }
	uint32_t getResult() { return result; };

private:
	instr_enum           instr_type;
	uint32_t             rs1, rs2;
	uint32_t             result;
	acalsim::CPPSimBase* sim;
};

#endif  // SOC_INCLUDE_CFU_HH_