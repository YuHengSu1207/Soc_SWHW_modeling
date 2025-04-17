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
#include "Bus.hh"
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
		this->bus  = new AXIBus("AXIBus");
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
		this->addSimulator(this->bus);
		this->addSimulator(this->dma);
		this->addSimulator(this->cfu);

		// still add those upstream & downstream
		// connect modules (connected_module, master port name, slave port name)
		this->cpu->addDownStream(this->bus, "DSBus");
		this->bus->addDownStream(this->dmem, "DSDMem");
		this->bus->addUpStream(this->cpu, "USCPU");
		this->dmem->addUpStream(this->bus, "USBus");
		// add the DMA
		this->dma->addDownStream(this->bus, "DSBus");
		this->bus->addUpStream(this->dma, "USDMA");
		this->dma->addUpStream(this->bus, "USBus");
		this->bus->addDownStream(this->dma, "DSDMA");
		// add CFU
		this->cpu->addDownStream(this->cfu, "DSCFU");
		this->cfu->addUpStream(this->cpu, "USCPU");

		// connect ports cpu -> bus
		// connect ports dma -> bus
		acalsim::SimPortManager::ConnectPort(cpu, bus, cpu->getName() + "-m", bus->getName() + "-s");
		acalsim::SimPortManager::ConnectPort(dma, bus, dma->getName() + "-m", bus->getName() + "-s");
		// connect channel bus -> dm
		// connect channel bus -> dma
		ChannelPortManager::ConnectPort(bus, dmem, bus->getName() + "-m_dm", dmem->getName() + "-s");
		ChannelPortManager::ConnectPort(bus, dma, bus->getName() + "-m_dma", dma->getName() + "-s");
		// channel bus -> cpu
		ChannelPortManager::ConnectPort(bus, cpu, bus->getName() + "-m_cpu", cpu->getName() + "-s_dm");

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

private:
	Emulator*      isaEmulator;  ///< ISA behavior model for instruction emulation
	CPU*           cpu;          ///< Single-cycle CPU hardware model
	DataMemory*    dmem;         ///< Data memory subsystem model
	AXIBus*        bus;          // bus support
	DMAController* dma;          ///< DMA controller for data transfer
	CFU*           cfu;
};

#endif  // SOC_INCLUDE_SOC_HH_
