#ifndef SOC_INCLUDE_DMA_HH_
#define SOC_INCLUDE_DMA_HH_

#include <string>
#include <vector>

#include "ACALSim.hh"
#include "Bus.hh"
#include "DataMemory.hh"
#include "event/BusTransactionEvent.hh"
#include "event/MemReqEvent.hh"
#include "packet/BusPacket.hh"
#include "packet/MemPacket.hh"

class BusMemReadRespPacket;
class BusMemWriteRespPacket;

class DMAController : public acalsim::CPPSimBase {
public:
	DMAController(std::string _name) : acalsim::CPPSimBase(_name) {
		LABELED_INFO(this->getName()) << "Constructing...";
		this->addMasterPort(_name + "-m");
		// this->addSlavePort(_name + "-s", 1);
	};

	virtual ~DMAController() {}

	void init() override { ; }
	void cleanup() override { ; };

	void step() override {
		for (auto s_port : this->s_ports_) {
			if (s_port.second->isPopValid()) {
				auto packet = s_port.second->pop();
				this->accept(acalsim::top->getGlobalTick(), *packet);
			}
		}
	}

	void masterPortRetry(const std::string& portName) final;

	// MMIO read/write from CPU
	void writeMMIO(acalsim::Tick _when, MemWriteReqPacket* _memReqPkt);
	void readMMIO(acalsim::Tick _when, MemReadReqPacket* _memReqPkt);

	/*
	Response handler
	*/
	void handleReadResponse(BusMemReadRespPacket* pkt);
	void handleWriteCompletion(BusMemWriteRespPacket* pkt);
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
	void createWriteRequestsForChunk(size_t offset, size_t chunk, std::vector<MemWriteReqPacket*>& outReqs);
	int  writeChunkCalculation(int word_offset, int original_chunk_size);
	void makeFullWritePacket(uint32_t address, uint32_t data, std::vector<MemWriteReqPacket*>& outReqs);
	void makePartialWritePackets(uint32_t address, uint32_t data, int byteCount,
	                             std::vector<MemWriteReqPacket*>& outReqs);

	// Called when everything is done
	void transaction_complete();

	void printBufferMem() const;

private:
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
	std::queue<acalsim::SimPacket*> request_queue;
};

#endif  // SOC_INCLUDE_DMA_HH_
