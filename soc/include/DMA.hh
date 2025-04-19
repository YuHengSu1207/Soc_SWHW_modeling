#ifndef SOC_INCLUDE_DMA_HH_
#define SOC_INCLUDE_DMA_HH_

#include <string>
#include <vector>

#include "ACALSim.hh"
#include "DataMemory.hh"
#include "MMIOUtil.hh"
#include "packet/XBarPacket.hh"

class DMAController : public acalsim::CPPSimBase, public MMIOUTIL {
public:
	DMAController(std::string _name) : acalsim::CPPSimBase(_name) {
		LABELED_INFO(this->getName()) << "Constructing...";
		this->registerSimPort();
	};

	virtual ~DMAController() {}

	void init() override {
		this->m_reg = this->getPipeRegister("bus-m");
		;
	}

	void registerSimPort() { this->s_port = this->addSlavePort("bus-s", 1); }

	void cleanup() override { ; };

	void step() override {
		for (auto s_port : this->s_ports_) {
			if (s_port.second->isPopValid()) {
				auto packet = s_port.second->pop();
				// read req handling
				if (auto ReadReqPkt = dynamic_cast<XBarMemReadReqPacket*>(packet)) {
					assert(ReadReqPkt->getPayloads().size() == ReadReqPkt->getBurstSize() &&
					       ReadReqPkt->getBurstSize() == 1);
					auto                          payload = ReadReqPkt->getPayloads();
					acalsim::LambdaEvent<void()>* event   = new acalsim::LambdaEvent<void()>(
                        [this, payload]() { this->readMMIO(acalsim::top->getGlobalTick(), payload[0]); });
					this->scheduleEvent(event, acalsim::top->getGlobalTick());
				}
				// Write req handling
				if (auto WriteReq = dynamic_cast<XBarMemWriteReqPacket*>(packet)) {
					assert(WriteReq->getPayloads().size() == WriteReq->getBurstSize() && WriteReq->getBurstSize() == 1);
					auto                          payload = WriteReq->getPayloads();
					acalsim::LambdaEvent<void()>* event   = new acalsim::LambdaEvent<void()>(
                        [this, payload]() { this->writeMMIO(acalsim::top->getGlobalTick(), payload[0]); });
					this->scheduleEvent(event, acalsim::top->getGlobalTick());
				}
				this->accept(acalsim::top->getGlobalTick(), *packet);
				// expect to get the response at
				acalsim::LambdaEvent<void()>* event =
				    new acalsim::LambdaEvent<void()>([this]() { this->trySendResponse(); });
				this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
			}
		}
	}

	void trySendResponse() { std::cout << "[TODO]"; };

	void masterPortRetry(const std::string& portName) final;

	// MMIO read/write from CPU
	void writeMMIO(acalsim::Tick _when, XBarMemWriteReqPayload* _memReqPkt);
	void readMMIO(acalsim::Tick _when, XBarMemReadReqPayload* _memReqPkt);

	/*
	Response handler
	*/
	void handleReadResponse(XBarMemReadRespPacket* pkt);
	void handleWriteCompletion(XBarMemWriteRespPacket* pkt);
	// Called after writing ENABLE=1 in your MMIO register
	void initialized_transaction();

private:
	// Core internal methods
	void startTransfer();  // Start row-by-row read+write iteration

	/*
	Read and write scheduling
	*/
	void scheduleReadsForBuffer();
	void scheduleWritesFromBuffer();
	/*
	Partial write utility
	*/
	void createWriteRequestsForChunk(size_t offset, size_t chunk, std::vector<XBarMemWriteReqPayload*>& outReqs);
	int  writeChunkCalculation(int word_offset, int original_chunk_size);
	void makeFullWritePacket(uint32_t address, uint32_t data, std::vector<XBarMemWriteReqPayload*>& outReqs);
	void makePartialWritePackets(uint32_t address, uint32_t data, int byteCount,
	                             std::vector<XBarMemWriteReqPayload*>& outReqs);

	// Called when everything is done
	void transaction_complete();

	void printBufferMem() const;

private:
	acalsim::SimPipeRegister* m_reg;  // from addPRMasterPort("bus-m", ...)

	// DMA registers
	bool     enabled;
	bool     done;
	uint32_t srcAddr;
	uint32_t dstAddr;
	uint32_t dmaSizeCfg;  // Bits: [31:24] SourceStride, [23:16] DestStride, [15:8] TW, [7:0] TH

	// Geometry
	int true_width;     // TW + 1
	int true_height;    // TH + 1
	int Source_stride;  // number of bytes in one row
	int Dest_stride;    // number of bytes in one row
	int totalElements;  // total number of bytes to copy (true_width * true_height)

	// Tracking
	int      wordsToBuffer;
	int      wordsTransferred;          // Number of words transferred so far
	int      totalWords;                // Total number of words to transfer
	int      max_burst_len = 2;         ///< e.g. 2 => burst_size = 2^2 = 4
	int      bufferIndex;               // Index into bufferMemory
	int      pendingBusReadResponses;   // Number of outstanding read responses
	int      pendingBusWriteResponses;  // Number of outstanding write responses
	uint32_t bufferMemory[256];         ///< Storage for one iteration (just an example size)
	bool     just_get_resp = false;
	int      dma_tx_num    = 0;  // Number of DMA transactions
	// State: (for conceptual clarity)
	enum class DmaState { IDLE, READING, WRITING } currentState;
	// assembled response packets waiting for pipe‑reg
	std::queue<acalsim::SimPacket*> CommandQ;
	acalsim::SlavePort*             s_port;
};

#endif  // SOC_INCLUDE_DMA_HH_
