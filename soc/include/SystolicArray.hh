#ifndef SOC_INCLUDE_SYSTOLICARRAY_HH_
#define SOC_INCLUDE_SYSTOLICARRAY_HH_

#include <queue>
#include <string>
#include <vector>

#include "ACALSim.hh"
#include "DataMemory.hh"
#include "MMIOUtil.hh"
#include "packet/XBarPacket.hh"

#define SA_MEMORY_BASE 0x20000
#define SA_SRAM_SIZE   8000
class SystolicArray : public acalsim::CPPSimBase, public MMIOUTIL {
public:
	explicit SystolicArray(const std::string& name);
	virtual ~SystolicArray();

	// Lifecycle hooks
	void init() override;
	void step() override;
	void masterPortRetry(const std::string& portName) final;

	// MMIO interface
	void readMMIO(acalsim::Tick when, XBarMemReadReqPayload* req);
	void writeMMIO(acalsim::Tick when, XBarMemWriteReqPayload* req);

	// Crossbar response handlers
	void handleReadResponse(XBarMemReadRespPacket* pkt);
	void handleWriteCompletion(XBarMemWriteRespPacket* pkt);

private:
	// Start a new matrix multiplication transaction
	void initialized_transaction();

	/* ---------- helpers for new SRAM path ---------- */
	void      memReadReqHandler(acalsim::Tick when, XBarMemReadReqPayload* p);
	void      memWriteReqHandler(acalsim::Tick when, XBarMemWriteReqPayload* p);
	uint32_t* getSramPtr(uint32_t addr);

	// Internal phases
	void preloadWeights();
	void computeMatrix();
	void writeOutputs();

	// Send any queued packets
	void trySendPacket();

	// Configuration registers
	bool     enabled_ = false;
	bool     done_    = false;
	uint32_t M_ = 0, K_ = 0, N_ = 0;
	uint32_t A_addr_ = 0, B_addr_ = 0, C_addr_ = 0;
	uint32_t strideA_ = 0, strideB_ = 0, strideC_ = 0;

	// Internal SRAM
	uint8_t  A_matrix[2048];
	uint8_t  B_matrix[2048];
	uint16_t C_matrix[2048];  // size M_ * N_
	/* ---------- on‑chip SRAM ---------- */
	uint32_t sram_[SA_SRAM_SIZE]{};

	/* ---------- burst tracking (same idea as DataMemory) ---------- */
	struct BurstTracker {
		int                                   expected = 0;
		std::vector<XBarMemReadRespPayload*>  rbeats;
		std::vector<XBarMemWriteRespPayload*> wbeats;
	};
	std::unordered_map<int /*tid*/, BurstTracker> pending_;

	// Crossbar ports
	acalsim::SimPipeRegister*       m_req_  = nullptr;
	acalsim::SimPipeRegister*       m_resp_ = nullptr;
	std::queue<acalsim::SimPacket*> req_Q_;
	std::queue<acalsim::SimPacket*> resp_Q_;
};

#endif  // SOC_INCLUDE_SYSTOLICARRAY_HH_