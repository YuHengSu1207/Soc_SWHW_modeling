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

#include "InstPacket.hh"

#include "EXEStage.hh"
#include "IDStage.hh"
#include "IFStage.hh"
#include "MEMStage.hh"
#include "WBStage.hh"

void InstPacket::visit(acalsim::Tick _when, acalsim::SimModule& _module) {
	CLASS_ERROR << "void InstPacket::visit (SimModule& module) is not implemented yet!";
}

void InstPacket::visit(acalsim::Tick _when, acalsim::SimBase& _simulator) {
	if (auto sim = dynamic_cast<IFStage*>(&_simulator)) {
		sim->instPacketHandler(_when, this);
	} else if (auto sim = dynamic_cast<EXEStage*>(&_simulator)) {
		sim->instPacketHandler(_when, this);
	} else if (auto sim = dynamic_cast<WBStage*>(&_simulator)) {
		sim->instPacketHandler(_when, this);
	} else if (auto sim = dynamic_cast<IDStage*>(&_simulator)) {
		sim->instPacketHandler(_when, this);
	} else if (auto sim = dynamic_cast<MEMStage*>(&_simulator)) {
		sim->instPacketHandler(_when, this);
	} else {
		CLASS_ERROR << "Invalid simulator type!";
	}
}
