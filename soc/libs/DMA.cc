#include "DMA.hh"

#include <cmath>

#include "Bus.hh"
#include "DataMemory.hh"
#include "TraceRecord.hh"
#define BUFFER_CAPACITY 256
class BusMemWriteRespPacket;
class BusMemReadRespPacket;
/**
 * Handle CPU writes to MMIO registers via the AXI Bus
 */
void DMAController::writeMMIO(acalsim::Tick _when, XBarMemWriteReqPayload* _memReqPkt) {
	// LABELED_INFO(this->getName()) << "DMA MMIO write at tick " << _when << " to address " <<
	uint32_t addr = _memReqPkt->getAddr() & 0x3F;  // Mask to align with MMIO range
	uint32_t data = _memReqPkt->getData();

	switch (addr) {
		case 0x0:  // ENABLE Register
			if (data & 0x1) {
				this->enabled = true;
				this->done    = false;
				initialized_transaction();
			}
			break;
		case 0x4:  // SOURCE_INFO Register
			this->srcAddr = data;
			break;
		case 0x8:  // DEST_INFO Register
			this->dstAddr = data;
			break;
		case 0xC:  // DMA_SIZE_CFG Register
			this->dmaSizeCfg = data;
			break;
		case 0x14:  // DONE Register (CPU clears)
			this->done = false;
			break;
		default: LABELED_ERROR(this->getName()) << "Invalid MMIO write address!"; break;
	}

	// Send a response packet to acknowledge the MMIO write
	auto                     rc = acalsim::top->getRecycleContainer();
	XBarMemWriteRespPayload* respPkt =
	    rc->acquire<XBarMemWriteRespPayload>(&XBarMemWriteRespPayload::renew, _memReqPkt->getInstr());
	respPkt->setTid(_memReqPkt->getTid());

	rc->recycle(_memReqPkt);
}

/**
 * Handle CPU reads from MMIO registers via the AXI Bus
 */
void DMAController::readMMIO(acalsim::Tick _when, XBarMemReadReqPayload* _memReqPkt) {
	// LABELED_INFO(this->getName()) << "DMA MMIO read at tick " << _when << " from address " << _memReqPkt->getAddr();

	uint32_t addr = _memReqPkt->getAddr() & 0x3F;
	uint32_t data = 0;

	switch (addr) {
		case 0x0:  // ENABLE Register
			data = this->enabled ? 1 : 0;
			break;
		case 0x4:  // SOURCE_INFO Register
			data = this->srcAddr;
			break;
		case 0x8:  // DEST_INFO Register
			data = this->dstAddr;
			break;
		case 0xC:  // DMA_SIZE_CFG Register
			data = this->dmaSizeCfg;
			break;
		case 0x14:  // DONE Register
			data = this->done ? 1 : 0;
			break;
		default: LABELED_ERROR(this->getName()) << "Invalid MMIO read address!"; break;
	}

	// Send a response packet with the MMIO read value
	auto                    rc      = acalsim::top->getRecycleContainer();
	XBarMemReadRespPayload* respPkt = rc->acquire<XBarMemReadRespPayload>(
	    &XBarMemReadRespPayload::renew, _memReqPkt->getInstr(), _memReqPkt->getOP(), data, _memReqPkt->getA1());
	respPkt->setTid(_memReqPkt->getTid());

	rc->recycle(_memReqPkt);
}

/**
 * Initialize the transaction after the CPU sets ENABLE=1 via MMIO
 */
void DMAController::initialized_transaction() {
	this->done         = false;
	this->currentState = DmaState::IDLE;

	// Decode
	uint8_t TW           = (dmaSizeCfg >> 8) & 0xFF;
	uint8_t TH           = dmaSizeCfg & 0xFF;
	uint8_t sourceStride = (dmaSizeCfg >> 24) & 0xFF;
	uint8_t destStride   = (dmaSizeCfg >> 16) & 0xFF;

	this->true_width    = (TW + 1);
	this->true_height   = (TH + 1);
	this->Source_stride = sourceStride;
	this->Dest_stride   = destStride;

	this->totalElements = this->true_width * this->true_height;
	this->totalWords    = ((this->true_width / 4) + (this->true_width % 4 != 0)) * this->true_height;

	LABELED_INFO(this->getName()) << "Starting DMA transaction: width=" << this->true_width
	                              << ", height=" << this->true_height;
	LABELED_INFO(this->getName()) << "DMA stride: src=" << (int)sourceStride << ", dst=" << (int)destStride
	                              << " total Bytes =" << totalElements << " total words = " << totalWords;

	this->wordsTransferred = 0;
	this->bufferIndex      = 0;
	this->wordsToBuffer    = 0;
	this->startTransfer();
}

/**
 * Kick off the DMA transfer. We'll read from srcAddr up to totalWords,
 * chunking data into bufferMemory, then writing out, etc.
 */
void DMAController::startTransfer() {
	if (!this->enabled) return;
	if (this->done) return;  // already finished
	LABELED_INFO(this->getName()) << "Starting DMA transfer...";
	this->currentState = DmaState::READING;
	scheduleReadsForBuffer();
}

/**
 * Schedules up to one burst worth of read requests into `bufferMemory`.
 * Called either the first time in this read phase or again after each read response is processed.
 */
void DMAController::scheduleReadsForBuffer() {
	// 1) Check if we already read enough overall or if buffer is full
	size_t bufferSpace = BUFFER_CAPACITY - bufferIndex;
	size_t wordsLeft   = totalWords - wordsToBuffer;
	size_t wordsToRead = std::min(bufferSpace, wordsLeft);

	/*LABELED_INFO(this->getName()) << "Total Words :" << this->totalWords
	                              << " Total Word to buffer : " << this->wordsToBuffer
	                              << " Words transfered : " << this->wordsTransferred;
	LABELED_INFO(this->getName()) << "Scheduling " << wordsToRead << " words to bufferMemory";*/

	if (wordsToRead == 0) {
		// No more data to read for now. Move on to writes or finish.
		if (this->wordsToBuffer >= this->totalWords) {
			transaction_complete();
		} else {
			// If we have partial data in buffer, start the write phase
			this->currentState = DmaState::WRITING;
			scheduleWritesFromBuffer();
		}
		return;
	}

	// 2) We have something to read. We'll read up to `burst_size_words` in this single burst
	size_t burst_size_words = (size_t)std::pow(2, this->max_burst_len);  // e.g. 4 => 16 bytes
	size_t chunk            = std::min(burst_size_words, wordsToRead);
	if (chunk == 3) {
		chunk = 2;  // resize the chunk to be supported by the burst mode
	}
	// Build read requests for exactly `chunk` words
	auto                           rc  = acalsim::top->getRecycleContainer();
	AXIBus*                        bus = dynamic_cast<AXIBus*>(this->getDownStream("DSBus"));
	std::vector<MemReadReqPacket*> readRequests;
	readRequests.reserve(chunk);

	// We store the starting offset so we know where we put these words in bufferMemory
	size_t startIndex = bufferIndex;

	for (size_t i = 0; i < chunk; i++) {
		size_t   globalWordIndex = wordsTransferred + bufferIndex + i;
		size_t   row             = globalWordIndex / ((this->true_width + 3) / 4);
		size_t   col             = globalWordIndex % ((this->true_width + 3) / 4);
		uint32_t address         = srcAddr + row * Source_stride + col * 4;

		// Per-request callback
		auto perReqCb = [this](MemReadRespPacket* resp) {
			dynamic_cast<AXIBus*>(this->getDownStream("DSBus"))->memReadRespHandler_DMA(resp);
		};

		// We'll store the local buffer index in operand::imm
		operand opnd;
		opnd.imm = (uint32_t)(startIndex + i);

		/*LABELED_INFO(this->getName()) << "Reading from address " << std::hex << address << " to bufferMemory["
		                              << startIndex + i << "]";
		LABELED_INFO(this->getName()) << "Reading from row " << row << " col " << col;*/
		instr             dummyInstr;
		MemReadReqPacket* reqPkt =
		    rc->acquire<MemReadReqPacket>(&MemReadReqPacket::renew, perReqCb, dummyInstr, LW, address, opnd);
		readRequests.push_back(reqPkt);
	}

	if (!(chunk == 4 || chunk == 2 || chunk == 1)) {
		LABELED_ERROR(this->getName()) << "Bus support the size of 1,2,4 words read only!";
	}

	// LABELED_INFO(this->getName()) << "Scheduling " << chunk << " package size " << readRequests.size();
	// Wrap them in a single BusMemReadReqPacket
	auto                 busBurstCb = [this](BusMemReadRespPacket* busResp) { this->handleReadResponse(busResp); };
	BusMemReadReqPacket* busReadPkt =
	    rc->acquire<BusMemReadReqPacket>(&BusMemReadReqPacket::renew, chunk, readRequests, busBurstCb);
	int tid = busReadPkt->getTransactionID();
	busReadPkt->setTransactionID(tid);

	/* LABELED_INFO(this->getName()) << "Starting DMA read datamem to the bufferMemory " << startIndex << " to "
	                              << startIndex + chunk - 1 << " with transaction id " << tid;*/

	auto m_port = this->getMasterPort(this->getName() + "-m");
	if (m_port->isPushReady()) {
		m_port->push(busReadPkt);
	} else {
		this->request_queue.push(busReadPkt);
	}

	// Keep track of how many read bursts are in flight (usually 1 in this approach)
	this->pendingBusReadResponses = 1;
}

/**
 * Once the entire burst completes, the bus calls this method.
 */
void DMAController::handleReadResponse(BusMemReadRespPacket* pkt) {
	// LABELED_INFO(this->getName()) << " receive resp and write to the bufferMemory for tid: " <<
	this->just_get_resp = true;
	auto rc             = acalsim::top->getRecycleContainer();
	auto readResponses  = pkt->getMemReadRespPkt();

	// Insert data into bufferMemory
	for (auto* rresp : readResponses) {
		size_t localIndex = rresp->getA1().imm;  // the operand we stored in scheduleReadsForBuffer
		if (localIndex < BUFFER_CAPACITY) {
			this->bufferMemory[localIndex] = rresp->getData();
		} else {
			LABELED_ERROR(this->getName()) << "Invalid local index in handleReadResponse!";
		}
	}

	int tid = pkt->getTransactionID();

	// We scheduled only 1 BusMemReadReqPacket, so decrement
	this->pendingBusReadResponses--;
	rc->recycle(pkt);

	// Now that this burst is done, we know how many words we consumed
	size_t chunk = readResponses.size();
	this->bufferIndex += chunk;
	this->wordsToBuffer += chunk;  // total in the entire transfer

	// If we still have more data to read into the buffer => schedule next burst
	// Otherwise => move on
	size_t bufferSpace = BUFFER_CAPACITY - bufferIndex;
	size_t wordsLeft   = totalWords - wordsToBuffer;
	if (bufferSpace > 0 && wordsLeft > 0) {
		// LABELED_INFO(this->getName()) << "Scheduling next read burst...";
		// Schedule next read burst
		acalsim::LambdaEvent<void()>* event =
		    new acalsim::LambdaEvent<void()>([this]() { this->scheduleReadsForBuffer(); });
		this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
	} else {
		// LABELED_INFO(this->getName()) << "Scheduling write from the buffer memory to Datamem";
		// Done reading for now -> move to writes or done
		this->printBufferMem();
		this->currentState = DmaState::WRITING;
		acalsim::LambdaEvent<void()>* event =
		    new acalsim::LambdaEvent<void()>([this]() { this->scheduleWritesFromBuffer(); });
		this->scheduleEvent(event, acalsim::top->getGlobalTick() + 1);
	}
}

void DMAController::scheduleWritesFromBuffer() {
	if (bufferIndex == 0) {
		// If no data left in buffer
		if (this->wordsTransferred >= this->totalWords) {
			transaction_complete();
		} else {
			// Move back to read phase if more data remains
			this->currentState = DmaState::READING;
			scheduleReadsForBuffer();
		}
		return;
	}

	bool need_partial_write = this->true_width % 4 != 0;  // If the last word in a row is partial
	int  partial_word_byte  = this->true_width % 4;       // true width in the last word (In byte format)

	auto rc = acalsim::top->getRecycleContainer();

	size_t wordsToWrite            = bufferIndex;
	size_t burst_size_words        = (size_t)std::pow(2, this->max_burst_len);  // e.g. 4 => 16 bytes
	this->pendingBusWriteResponses = 0;

	size_t  offset       = 0;
	size_t  chunkCounter = 0;  // We'll increment this for each burst, so each is scheduled 1 cycle apart
	AXIBus* bus          = dynamic_cast<AXIBus*>(this->getDownStream("DSBus"));

	while (offset < wordsToWrite) {
		// 1) Decide how many words we can send in this burst
		size_t chunk = writeChunkCalculation(offset, wordsToWrite - offset);

		// 3) Create the MemWriteReqPackets for these chunk words
		std::vector<MemWriteReqPacket*> writeRequests;
		writeRequests.reserve(chunk);

		createWriteRequestsForChunk(offset, chunk, writeRequests);

		// 4) Wrap them in a BusMemWriteReqPacket
		auto                  busCb = [this](BusMemWriteRespPacket* wrResp) { this->handleWriteCompletion(wrResp); };
		BusMemWriteReqPacket* busWritePkt = rc->acquire<BusMemWriteReqPacket>(
		    &BusMemWriteReqPacket::renew, (int)burst_size_words, writeRequests, busCb);

		int tid = busWritePkt->getTransactionID();
		busWritePkt->setTransactionID(tid);

		auto m_port = this->getMasterPort(this->getName() + "-m");
		if (m_port->isPushReady()) {
			m_port->push(busWritePkt);
		} else {
			this->request_queue.push(busWritePkt);
		}

		this->pendingBusWriteResponses++;
		offset += chunk;
		chunkCounter++;
	}
	// LABELED_INFO(this->getName()) << "With the request queue being size of " << this->request_queue.size();
	this->wordsTransferred += wordsToWrite;
}

/**
 * Decide how many words from the original chunk can be sent so that the
 * resulting number of subpackets is in {1,2,4}.
 *
 * @param word_offset         (input) Starting offset in the buffer
 * @param original_chunk_size (input) Proposed number of words in this chunk
 * @return actual number of words we can include in this burst
 *         so that subpacketCount is {1,2,4}.
 */
int DMAController::writeChunkCalculation(int word_offset, int original_chunk_size) {
	int subpacketCount = 0;
	int maxValidWords  = 0;  // how many words produce a valid subpacket count
	// We walk word-by-word, building subpacketCount.
	// If we overshoot 4, we stop. If we land exactly on 1,2, or 4, we record that as "valid".

	for (int i = 0; i < original_chunk_size; i++) {
		// Figure out if this word is partial:
		// e.g. row/col calculation
		int globalIndex = this->wordsTransferred + word_offset + i;
		int row = globalIndex / ((this->true_width + 3) / 4);  // e.g. getWordsPerRow() = (true_width + 3)/4 etc.
		int col = globalIndex % ((this->true_width + 3) / 4);

		// Decide how many subpackets this word will generate:
		int thisWordSubpackets = 1;  // default = 1 subpacket (SW)
		if (col == ((this->true_width + 3) / 4) && this->true_width % 4 != 0) {
			// e.g. partial_word_byte == 3 => 2 subpackets (SH+SB)
			// partial_word_byte == 2 => 1 subpacket (SH)
			// partial_word_byte == 1 => 1 subpacket (SB)
			int byteCount = this->true_width % 4;  // e.g. returns 3,2,1
			if (byteCount == 3) {
				thisWordSubpackets = 2;
			} else {
				thisWordSubpackets = 1;
			}
		}

		subpacketCount += thisWordSubpackets;

		// If we ever exceed 4, we stop — we can't produce bursts bigger than 4
		if (subpacketCount > std::pow(2, this->max_burst_len)) { break; }
		// If subpacketCount == 1, 2, or 4 => record i+1 as a valid chunk
		if (subpacketCount == 1 || subpacketCount == 2 || subpacketCount == 4) { maxValidWords = i + 1; }
	}

	// If we never found a subpacketCount in {1,2,4}, maxValidWords might be 0.
	// That means we can't even accept 1 word without producing an invalid subpacket count (like 3).
	// In that edge case, you may decide to clamp to 1 word (which might produce 2 subpackets).
	// Or handle it as an error. Example:
	if (maxValidWords == 0) {
		// Typically, you'd do something like:
		// fallback to 1 word => hopefully that yields 2 subpackets if partial_word_byte=3
		maxValidWords = 1;
	}

	return maxValidWords;
}

void DMAController::masterPortRetry(const std::string& portName) {
	// LABELED_INFO(this->getName()) << ": Master port retry at " << portName;
	auto m_port = this->getMasterPort(this->getName() + "-m");
	if (!this->request_queue.empty() && m_port->isPushReady()) {
		if (auto read_req = dynamic_cast<BusMemReadReqPacket*>(this->request_queue.front())) {
			auto real_req = read_req->getMemReadReqPkt()[0];
			auto tid      = read_req->getTransactionID();
			m_port->push(read_req);
		} else if (auto write_req = dynamic_cast<BusMemWriteReqPacket*>(this->request_queue.front())) {
			auto real_req = write_req->getMemWriteReqPkt()[0];
			auto tid      = write_req->getTransactionID();
			m_port->push(write_req);
		} else {
			CLASS_ERROR << "Not a valid request";
		}
		this->request_queue.pop();
	}
}

/**
 * Create MemWriteReqPackets for exactly `finalChunkSize` words, matching the
 * partial logic used in writeChunkCalculation().
 *
 * @param offset            Starting offset in bufferMemory
 * @param finalChunkSize    The number of words to process in this chunk
 * @param outReqs           Vector to accumulate MemWriteReqPackets
 */
void DMAController::createWriteRequestsForChunk(size_t offset, size_t finalChunkSize,
                                                std::vector<MemWriteReqPacket*>& outReqs) {
	auto rc = acalsim::top->getRecycleContainer();

	for (size_t i = 0; i < finalChunkSize; i++) {
		size_t globalIndex = this->wordsTransferred + offset + i;
		size_t row         = globalIndex / ((this->true_width + 3) / 4);
		size_t col         = globalIndex % ((this->true_width + 3) / 4);

		uint32_t baseAddr = dstAddr + row * this->Dest_stride + (col * 4);
		// LABELED_INFO(this->getName()) << "Base address" << std::hex << dstAddr;
		uint32_t data = bufferMemory[offset + i];

		// Check partial
		if (col == ((this->true_width + 3) / 4) && this->true_width % 4 != 0) {
			int byteCount = this->true_width % 4;  // e.g. 3,2,1
			/* LABELED_INFO(this->getName())
			    << "Create partial write to " << std::hex << baseAddr << " from bufferMemory[" << offset + i << "]"
			    << " data" << std::hex << data;*/
			makePartialWritePackets(baseAddr, data, byteCount, outReqs);
		} else {
			// Normal 4-byte SW
			/* LABELED_INFO(this->getName())
			    << "Create SW to " << std::hex << baseAddr << " from bufferMemory[" << offset + i << "]"
			    << " data " << std::hex << data;*/
			makeFullWritePacket(baseAddr, data, outReqs);
		}
	}
}

/**
 * Helper that adds one SW request to outReqs
 */
void DMAController::makeFullWritePacket(uint32_t address, uint32_t data, std::vector<MemWriteReqPacket*>& outReqs) {
	auto rc       = acalsim::top->getRecycleContainer();
	auto perReqCb = [this](MemWriteRespPacket* wrResp) {
		dynamic_cast<AXIBus*>(this->getDownStream("DSBus"))->memWriteRespHandler_DMA(wrResp);
	};
	instr              dummyInstr;
	MemWriteReqPacket* wrPkt =
	    rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, perReqCb, dummyInstr, SW, address, data);
	outReqs.push_back(wrPkt);
}

/**
 * Helper that adds subpackets for partial writes, e.g. 3 bytes => SH + SB, etc.
 */
void DMAController::makePartialWritePackets(uint32_t address, uint32_t data, int byteCount,
                                            std::vector<MemWriteReqPacket*>& outReqs) {
	auto rc       = acalsim::top->getRecycleContainer();
	auto perReqCb = [this](MemWriteRespPacket* wrResp) {
		dynamic_cast<AXIBus*>(this->getDownStream("DSBus"))->memWriteRespHandler_DMA(wrResp);
	};
	instr dummyInstr;

	if (byteCount == 3) {
		// Lower 2 bytes => SH
		uint32_t           half = data & 0xFFFF;
		MemWriteReqPacket* wrSH =
		    rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, perReqCb, dummyInstr, SH, address, half);
		outReqs.push_back(wrSH);
		// Next 1 byte => SB (the 3rd byte)
		uint32_t           thirdByte = (data >> 16) & 0xFF;
		MemWriteReqPacket* wrSB =
		    rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, perReqCb, dummyInstr, SB, address + 2, thirdByte);
		outReqs.push_back(wrSB);
	} else if (byteCount == 2) {
		// Just SH
		uint32_t           half = data & 0xFFFF;
		MemWriteReqPacket* wrSH =
		    rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, perReqCb, dummyInstr, SH, address, half);
		outReqs.push_back(wrSH);
	} else if (byteCount == 1) {
		// Just SB
		uint32_t           oneByte = data & 0xFF;
		MemWriteReqPacket* wrSB =
		    rc->acquire<MemWriteReqPacket>(&MemWriteReqPacket::renew, perReqCb, dummyInstr, SB, address, oneByte);
		outReqs.push_back(wrSB);
	} else {
		// Fallback or error
		LABELED_ERROR(this->getName()) << "Invalid byteCount in makePartialWritePackets!";
	}
}

/**
 * Called once one entire BusMemWriteReqPacket has completed.
 */
void DMAController::handleWriteCompletion(BusMemWriteRespPacket* pkt) {
	// LABELED_INFO(this->getName()) << "DMA write response received with tid " << pkt->getTransactionID();
	auto rc = acalsim::top->getRecycleContainer();
	rc->recycle(pkt);
	int tid = pkt->getTransactionID();

	this->pendingBusWriteResponses--;
	if (this->pendingBusWriteResponses == 0) {
		// All writes for this buffer are done
		// Clear buffer
		for (size_t i = 0; i < bufferIndex; i++) { bufferMemory[i] = 0; }
		bufferIndex = 0;

		// If we have transferred all data
		if (this->wordsTransferred >= this->totalWords) {
			transaction_complete();
		} else {
			// Otherwise go back to read more data
			this->currentState = DmaState::READING;
			scheduleReadsForBuffer();
		}
	}
}

void DMAController::transaction_complete() {
	this->currentState = DmaState::IDLE;
	this->done         = true;
	this->enabled      = false;
	this->dma_tx_num++;
	this->just_get_resp = false;
	// LABELED_INFO(this->getName()) << "DMA transaction complete!";
}

void DMAController::printBufferMem() const {
	std::ostringstream oss;

	oss << "Register File Snapshot:\n\n";
	for (int i = 0; i < 256; i++) {
		oss << "x" << std::setw(2) << std::setfill('0') << std::dec << i << ":0x";

		oss << std::setw(8) << std::setfill('0') << std::hex << bufferMemory[i] << " ";

		if ((i + 1) % 8 == 0) { oss << "\n"; }
	}

	oss << '\n';

	CLASS_INFO << oss.str();
}
