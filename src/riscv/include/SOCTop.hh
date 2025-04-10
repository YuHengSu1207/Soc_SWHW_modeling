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
#ifndef RISCV_INCLUDE_SOCTOP_HH_
#define RISCV_INCLUDE_SOCTOP_HH_

#include <string>

#include "ACALSim.hh"
#include "EXEStage.hh"
#include "Emulator.hh"
#include "IDStage.hh"
#include "IFStage.hh"
#include "MEMStage.hh"
#include "SOC.hh"
#include "SystemConfig.hh"
#include "TopPipeRegisterManager.hh"
#include "WBStage.hh"

/**
 * @class SOCTop
 * @brief Top-level System-on-Chip simulation class
 * @details Manages the complete SoC simulation environment, including configuration
 *          registration, CLI argument handling, and trace control. Inherits from
 *          STSim template class specialized for SOC type.
 */
class SOCTop : public acalsim::SimTop {
public:
	/**
	 * @brief Constructor for SOCTop simulation environment
	 * @param _name Name of the simulation instance (default: "SOCTop")
	 * @param _configFile Path to configuration file (default: empty string)
	 * @details Initializes the simulation and sets up trace container with
	 *          default path "soc/trace"
	 */
	SOCTop(const std::string _name = "SOCTop", const std::string _configFile = "")
	    : acalsim::SimTop(_name, _configFile) {
		this->traceCntr.run(0, &acalsim::SimTraceContainer::setFilePath, "trace", "soc/trace");
	}
	virtual ~SOCTop() {}

	/**
	 * @brief Registers configuration objects for the simulation
	 * @details Creates and registers two configuration objects:
	 *          1. EmulatorConfig: Configuration for the CPU emulator
	 *          2. SOCConfig: Configuration for SOC timing parameters
	 * @override Overrides base class method
	 */
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
		this->soc  = new SOC("top-level soc");
		this->sIF  = new IFStage("IF stage model");
		this->sID  = new IDStage("ID stage model");
		this->sEXE = new EXEStage("EXE stage model");
		this->sMEM = new MEMStage("MEM stage modell");
		this->sWB  = new WBStage("WB stage model");

		this->addSimulator(this->soc);
		this->addSimulator(this->sIF);
		this->addSimulator(this->sID);
		this->addSimulator(this->sEXE);
		this->addSimulator(this->sMEM);
		this->addSimulator(this->sWB);

		// Create SimPort connection between SOC(functional modeling) and sIF(timing model)
		// SOC only sends an instruction to the IF stage only when there is no backpressue
		/* SOC -> sIF */
		this->soc->addMasterPort("sIF-m");
		this->sIF->addSlavePort("soc-s", 1);
		// connect SimPort
		acalsim::SimPortManager::ConnectPort(this->soc, this->sIF, "sIF-m", "soc-s");
	}

	void registerPipeRegisters() override {
		// SimPipeRegister Setup
		// IF ->prIF2EXE->EXE->prEXE2WB->WB

		acalsim::SimPipeRegister* prIF2ID   = new acalsim::SimPipeRegister("prIF2ID");
		acalsim::SimPipeRegister* prID2EXE  = new acalsim::SimPipeRegister("prID2EXE");
		acalsim::SimPipeRegister* prEXE2MEM = new acalsim::SimPipeRegister("prEXE2MEM");
		acalsim::SimPipeRegister* prMEM2WB  = new acalsim::SimPipeRegister("prMEM2WB");

		this->pipeRegisterManager = new TopPipeRegisterManager("Top-Level Pipe Register Manager");
		// Add to manager
		pipeRegisterManager->addPipeRegister(prIF2ID);
		pipeRegisterManager->addPipeRegister(prID2EXE);
		pipeRegisterManager->addPipeRegister(prEXE2MEM);
		pipeRegisterManager->addPipeRegister(prMEM2WB);

		this->sIF->addPRMasterPort("prIF2ID-in", prIF2ID);
		this->sID->addPRSlavePort("prIF2ID-out", prIF2ID);

		this->sID->addPRMasterPort("prID2EXE-in", prID2EXE);
		this->sEXE->addPRSlavePort("prID2EXE-out", prID2EXE);

		this->sEXE->addPRMasterPort("prEXE2MEM-in", prEXE2MEM);
		this->sMEM->addPRSlavePort("prEXE2MEM-out", prEXE2MEM);

		this->sMEM->addPRMasterPort("prMEM2WB-in", prMEM2WB);
		this->sWB->addPRSlavePort("prMEM2WB-out", prMEM2WB);
	}

private:
	SOC*      soc;
	Emulator* isaEmulator;
	IFStage*  sIF;
	EXEStage* sEXE;
	WBStage*  sWB;
	IDStage*  sID;
	MEMStage* sMEM;
};

#endif
