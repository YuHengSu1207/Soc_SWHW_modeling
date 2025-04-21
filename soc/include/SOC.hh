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

#ifndef SOC_INCLUDE_SOC_HH_
#define SOC_INCLUDE_SOC_HH_

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "ACALSim.hh"
#include "CFU.hh"
#include "CPU.hh"
#include "DMA.hh"
#include "DataMemory.hh"
#include "DataStruct.hh"
#include "Emulator.hh"
#include "SystemConfig.hh"

/**
 * @class SOC
 * @brief System-on-Chip (SOC) module integrating CPU, memory, and ISA emulator
 * @details Represents the top-level hardware system that combines:
 *          - An ISA behavior emulator
 *          - A single-cycle CPU model
 *          - A data memory subsystem
 *          Inherits from STSimBase to provide simulation functionality
 */
class SOC : public acalsim::SimTop {
public:
	SOC(const std::string& name = "SOC", const std::string& configFile = "") : acalsim::SimTop(name, configFile) {
		this->traceCntr.run(0, &acalsim::SimTraceContainer::setFilePath, "trace", "soc/trace");
	}

	/**
	 * @brief Virtual destructor
	 */
	virtual ~SOC() {}

	void registerConfigs() override {
		auto emuConfig = new EmulatorConfig("Emulator configuration");
		this->addConfig("Emulator", emuConfig);
		auto socConfig = new SOCConfig("SOC configuration");
		this->addConfig("SOC", socConfig);
	}

	/**
	 * @brief Registers command-line interface arguments
	 * @details Sets up CLI options for the simulation:
	 *          - --asm_file_path: Path to the assembly code file
	 * @override Overrides base class method
	 */
	void registerCLIArguments() override {
		this->addCLIOption<std::string>("--asm_file_path",                    // Option name
		                                "The file path of an assembly code",  // Description
		                                "Emulator",                           // Config section
		                                "asm_file_path"                       // Parameter name
		);
	}

	void registerSimulators() override {
		// Get the maximal memory footprint size in the Emulator Configuration
		size_t mem_size = acalsim::top->getParameter<int>("Emulator", "memory_size");

		// Data Memory Timing Model
		this->dmem = new DataMemory("DataMemory", mem_size);
		// XBar
		this->XBar = new acalsim::crossbar::CrossBar("CrossBar", 2, 2);

		// Instruction Set Architecture Emulator (Functional Model)
		this->isaEmulator = new Emulator("RISCV RV32I Emulator");

		// CPU Timing Model
		this->cpu = new CPU("ScCPU", this->isaEmulator);

		// DMA Controller
		this->dma = new DMAController("DMA");

		// CFU
		this->cfu = new CFU("CFU");

		// register simulators
		this->addSimulator(this->cpu);
		this->addSimulator(this->dmem);
		this->addSimulator(this->XBar);
		this->addSimulator(this->dma);
		this->addSimulator(this->cfu);

		// still add those upstream & downstream
		// Keep the dump memory functional
		this->cpu->addDownStream(this->XBar, "DSBus");
		this->XBar->addDownStream(this->dmem, "DSDMem");
		// add CFU
		this->cpu->addDownStream(this->cfu, "DSCFU");
		this->cfu->addUpStream(this->cpu, "USCPU");

		// master construction (cpu, dma)
		// Register PRMasterPort to Masters in `SimTop`
		this->cpu->addPRMasterPort("bus-m", XBar->getPipeRegister("Req", 0));
		this->dma->addPRMasterPort("bus-m", XBar->getPipeRegister("Req", 1));
		// Make SimPort Connection to Slaves in `SimTop`
		for (auto mp : XBar->getMasterPortsBySlave("Req", 0)) {
			acalsim::SimPortManager::ConnectPort(XBar, this->dmem, mp->getName(), "bus-s");
		}
		for (auto mp : XBar->getMasterPortsBySlave("Req", 1)) {
			acalsim::SimPortManager::ConnectPort(XBar, this->dma, mp->getName(), "bus-s");
		}

		// slave construction (dm)
		// Register PRMasterPort to Slaves for the response channel
		this->dmem->addPRMasterPort("bus-m", XBar->getPipeRegister("Resp", 0));
		// to avoid renaming // send request back to cpu / accelator
		this->dma->addPRMasterPort("bus-m-2", XBar->getPipeRegister("Resp", 1));
		// Simport Connection (Bus <> SlavePort at Devices)
		for (auto mp : XBar->getMasterPortsBySlave("Resp", 0)) {
			acalsim::SimPortManager::ConnectPort(XBar, this->cpu, mp->getName(), "bus-s");
		}
		for (auto mp : XBar->getMasterPortsBySlave("Resp", 1)) {
			acalsim::SimPortManager::ConnectPort(XBar, this->dma, mp->getName(), "bus-s");
		}

		// channel cpu <-> cfu
		ChannelPortManager::ConnectPort(cpu, cfu, cpu->getName() + "-m_cfu", cfu->getName() + "-s_cpu");
		ChannelPortManager::ConnectPort(cfu, cpu, cfu->getName() + "-m_cpu", cpu->getName() + "-s_cfu");
	}

	void postSimInitSetup() override {
		// Get read latency for the data memory model
		CLASS_INFO << "memory_read_latency : "
		           << acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_read_latency") << " Ticks";
		// Get write latency for the data memory model
		CLASS_INFO << "memory_write_latency : "
		           << acalsim::top->getParameter<acalsim::Tick>("SOC", "memory_write_latency") << " Ticks";
		CLASS_INFO << " SOC::preSimInitSetup()!";

		// Initialize the ISA Emulator
		// Parse assmebly file and initialize data memory and instruction memory
		std::string asm_file_path = acalsim::top->getParameter<std::string>("Emulator", "asm_file_path");

		this->isaEmulator->parse(asm_file_path, ((uint8_t*)this->dmem->getMemPtr()), this->cpu->getIMemPtr());
		this->isaEmulator->normalize_labels(this->cpu->getIMemPtr());
	}

	void registerPipeRegisters() {
		this->SimTop::registerPipeRegisters();
		auto bus = static_cast<acalsim::crossbar::CrossBar*>(this->getSimulator("CrossBar"));
		for (auto reg : bus->getAllPipeRegisters("Req")) { this->getPipeRegisterManager()->addPipeRegister(reg); }
		for (auto reg : bus->getAllPipeRegisters("Resp")) { this->getPipeRegisterManager()->addPipeRegister(reg); }
	}

private:
	Emulator*                    isaEmulator;  ///< ISA behavior model for instruction emulation
	CPU*                         cpu;          ///< Single-cycle CPU hardware model
	DataMemory*                  dmem;         ///< Data memory subsystem model
	DMAController*               dma;          ///< DMA controller for data transfer
	CFU*                         cfu;
	acalsim::crossbar::CrossBar* XBar;
};

#endif  // SOC_INCLUDE_SOC_HH_
