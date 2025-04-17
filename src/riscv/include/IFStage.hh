/*
 * Copyright 2023-2024 Playlab/ACAL
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_RISCV_INCLUDE_IFSTAGE_HH_
#define SRC_RISCV_INCLUDE_IFSTAGE_HH_

#include <string>

#include "ACALSim.hh"
#include "Emulator.hh"
#include "InstPacket.hh"
#include "PipelineUtils.hh"

class IFStage : public acalsim::CPPSimBase, public PipelineUtils {
public:
	IFStage(std::string name) : acalsim::CPPSimBase(name) {}
	~IFStage() {}

	void init() override {}
	void step() override;
	void cleanup() override {}
	void instPacketHandler(Tick when, SimPacket* pkt);
	void flush();

private:
	int         mem_access_count   = 0;
	int         last_mem_access_pc = -1;
	bool        flush_flag         = false;
	bool        memRespValid       = true;
	InstPacket* IDInstPacket       = nullptr;
	InstPacket* EXEInstPacket      = nullptr;
	InstPacket* MEMInstPacket      = nullptr;
	InstPacket* WBInstPacket       = nullptr;
};

#endif  // SRC_RISCV_INCLUDE_IFSTAGE_HH_