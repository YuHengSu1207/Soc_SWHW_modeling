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

#ifndef SRC_RISCV_INCLUDE_MEMSTAGE_HH_
#define SRC_RISCV_INCLUDE_MEMSTAGE_HH_

#include <string>

#include "ACALSim.hh"
#include "InstPacket.hh"
#include "PipelineUtils.hh"

class MEMStage : public acalsim::CPPSimBase, public PipelineUtils {
public:
	MEMStage(std::string name) : acalsim::CPPSimBase(name) {}
	~MEMStage() {}

	void init() override {}

	void step() override;

	void cleanup() override {}

	void instPacketHandler(acalsim::Tick when, acalsim::SimPacket* pkt);

private:
	InstPacket* WBInstPacket = nullptr;
};

#endif  // SRC_RISCV_INCLUDE_MEMSTAGE_HH_
