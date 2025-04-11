#ifndef SRC_RISCV_INCLUDE_IDSTAGE_HH_
#define SRC_RISCV_INCLUDE_IDSTAGE_HH_

#include <string>

#include "ACALSim.hh"
#include "InstPacket.hh"
#include "PipelineUtils.hh"

class IDStage : public acalsim::CPPSimBase, public PipelineUtils {
public:
	IDStage(std::string name) : acalsim::CPPSimBase(name) {}
	~IDStage() {}

	void init() override {}
	void step() override;
	void cleanup() override {}
	void instPacketHandler(acalsim::Tick when, acalsim::SimPacket* pkt);
	void flush();

private:
	int         mem_access_count   = 0;
	int         last_mem_access_pc = -1;
	InstPacket* EXEInstPacket      = nullptr;
	InstPacket* MEMInstPacket      = nullptr;
	InstPacket* WBInstPacket       = nullptr;
	bool        flushed            = false;
};

#endif  // SRC_RISCV_INCLUDE_IDSTAGE_HH_