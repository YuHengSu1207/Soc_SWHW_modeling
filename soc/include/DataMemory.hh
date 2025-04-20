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

#ifndef SOC_INCLUDE_DATAMEMORY_HH_
#define SOC_INCLUDE_DATAMEMORY_HH_

#include <queue>
#include <string>
#include <unordered_map>

#include "ACALSim.hh"
#include "BaseMemory.hh"
#include "DataStruct.hh"
#include "MMIOUtil.hh"
#include "packet/XBarPacket.hh"

/**
 * @class DataMemory
 * @brief Data memory module for the system
 * @details Implements a memory module that handles both read and write operations
 *          Inherits from SimModule for simulation functionality and BaseMemory
 *          for basic memory operations
 */
class DataMemory : public acalsim::CPPSimBase, public BaseMemory, public MMIOUTIL {
public:
	/**
	 * @brief Constructor for DataMemory
	 * @param _name Name identifier for the memory module
	 * @param _size Size of the memory in bytes
	 */
	DataMemory(std::string _name, size_t _size) : acalsim::CPPSimBase(_name), BaseMemory(_size) {
		LABELED_INFO(_name) << " Constructing";
		/*     (CrossBarBuilder already connects PR \"Resp\",0 to dmem‘s *slave* side     */
		/*      so we expose a *master* for the opposite direction)                      */
		this->registerSimPort();
	}
	void registerSimPort() { this->addSlavePort("bus-s", 1); }
	/**
	 * @brief Virtual destructor
	 */
	virtual ~DataMemory() {}

	void init() override { this->m_reg = this->getPipeRegister("bus-m"); };
	void cleanup() override { ; };

	void step() override {
		if (!respQ_.empty()) { this->trySendResponse(); }

		for (auto s_port : this->s_ports_) {
			CLASS_INFO << s_port.first;
			if (s_port.second->isPopValid()) {
				CLASS_INFO << "Is pop valid";
				int  burst_size = -1;
				auto packet     = s_port.second->pop();
				// read req handling
				if (auto ReadReqPkt = dynamic_cast<XBarMemReadReqPacket*>(packet)) {
					assert(ReadReqPkt->getPayloads().size() == ReadReqPkt->getBurstSize());
					auto payload = ReadReqPkt->getPayloads();
					burst_size   = payload.size();
					CLASS_INFO << "[DMEM] : pop a read packet : " << ReadReqPkt->getAutoIncTID()
					           << " with burst size : " << burst_size;
					this->pending_[ReadReqPkt->getAutoIncTID()].expected = payload.size();
					this->memReadReqHandler(acalsim::top->getGlobalTick(),
					                        payload[0]);  // handle the first read immediately.
					if (burst_size != 1) {
						for (int i = 1; i < ReadReqPkt->getBurstSize(); i++) {
							auto                          payload = ReadReqPkt->getPayloads();
							acalsim::LambdaEvent<void()>* event =
							    new acalsim::LambdaEvent<void()>([this, i, payload]() {
								    this->memReadReqHandler(acalsim::top->getGlobalTick() + i, payload[i]);
							    });
							this->scheduleEvent(event, acalsim::top->getGlobalTick() + i);
						}
					}
				}
				// Write req handling
				if (auto WriteReq = dynamic_cast<XBarMemWriteReqPacket*>(packet)) {
					assert(WriteReq->getPayloads().size() == WriteReq->getBurstSize());
					auto payload = WriteReq->getPayloads();
					burst_size   = payload.size();
					CLASS_INFO << "[DMEM] : pop a write packet: " << WriteReq->getAutoIncTID() << " with size "
					           << burst_size;
					this->pending_[WriteReq->getAutoIncTID()].expected = payload.size();
					this->memWriteReqHandler(acalsim::top->getGlobalTick(), payload[0]);
					if (burst_size != 1) {
						for (int i = 1; i < WriteReq->getBurstSize(); i++) {
							auto                          payload = WriteReq->getPayloads();
							acalsim::LambdaEvent<void()>* event =
							    new acalsim::LambdaEvent<void()>([this, i, payload]() {
								    this->memWriteReqHandler(acalsim::top->getGlobalTick() + i, payload[i]);
							    });
							this->scheduleEvent(event, acalsim::top->getGlobalTick() + i);
						}
					}
				}
				// expect to get the response at
				acalsim::LambdaEvent<void()>* event =
				    new acalsim::LambdaEvent<void()>([this]() { this->trySendResponse(); });
				this->scheduleEvent(event, acalsim::top->getGlobalTick() + burst_size + 1);
			}
		}
	}

	void masterPortRetry(const std::string& portName) final;

	/**
	 * @brief Handles memory read request packets
	 * @param _when Simulation time tick when the request was received
	 * @param _memReqPkt Pointer to the memory read request packet
	 * @details Processes incoming read requests and generates appropriate responses
	 */
	void memReadReqHandler(acalsim::Tick _when, XBarMemReadReqPayload* _memReqPkt);

	/**
	 * @brief Handles memory write request packets
	 * @param _when Simulation time tick when the request was received
	 * @param _memReqPkt Pointer to the memory write request packet
	 * @details Processes incoming write requests and updates memory contents
	 */
	void memWriteReqHandler(acalsim::Tick _when, XBarMemWriteReqPayload* _memReqPkt);

private:
	/* ---------- internal helpers ------------ */
	void trySendResponse();  // pushes one packet if pipe‑reg ready

	struct BurstTracker {
		int                                   expected;  // burstLen
		std::vector<XBarMemReadRespPayload*>  rbeats;
		std::vector<XBarMemWriteRespPayload*> wbeats;
	};

	/* ---------- state ------------ */

	acalsim::SimPipeRegister* m_reg;  // from addPRMasterPort("bus-m", ...)
	// tid -> tracker
	std::unordered_map<int /*tid*/, BurstTracker> pending_;

	// assembled response packets waiting for pipe‑reg
	std::queue<acalsim::SimPacket*> respQ_;
};

#endif