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

struct Valid8 {
	bool    valid;
	uint8_t data;
};
struct Valid16 {
	bool     valid;
	uint16_t sum;
};

struct PE {
	// weight register
	Valid8 weightReg{false, 0};
	// input pipeline
	Valid8 inReg{false, 0}, fwdIn{false, 0};
	// partial‐sum pipeline
	Valid16 psumReg{false, 0}, fwdPsum{false, 0};
};

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
	void SRAMReadReqHandler(acalsim::Tick when, XBarMemReadReqPayload* p);
	void SRAMWriteReqHandler(acalsim::Tick when, XBarMemWriteReqPayload* p);

	// Program DMA
	void AskDMAtoWrite_matA();
	void AskDMAtoWrite_matB();
	void PokeDMAReady();

	// Internal phases
	void writeOutputs();

	// Core computation
	std::vector<std::vector<PE>> Construct_PE(int Systolic_Array_Size);
	void                         Preload_Weight(std::vector<std::vector<PE>>& pe, int SA_Size, int cycle_cnt);
	void PropagateA_And_MAC(std::vector<std::vector<PE>>& pe, std::vector<std::vector<uint16_t>>& Result,
	                        std::vector<uint16_t>& emitRow, int SA_Size, int cycle);
	void FlushAndEmit(std::vector<std::vector<PE>>& pe, std::vector<std::vector<uint16_t>>& Result,
	                  std::vector<uint16_t>& emitRow, int SA_Size, int cycle);
	void ComputeTile(int SA_Size);

	void ComputeMatrix();

	// Send any queued packets
	void trySendPacket();

	// Print Utility
	std::string instrToString(instr_type _op) const;
	void        DumpMemory() const;
	// Configuration registers
	bool     enabled_ = false;
	bool     done_    = false;
	uint32_t M_ = 0, K_ = 0, N_ = 0;
	uint32_t A_addr_ = 0, B_addr_ = 0, C_addr_ = 0;
	uint32_t A_addr_dm_ = 0, B_addr_dm_ = 0, C_addr_dm_ = 0;
	uint32_t strideA_ = 0, strideB_ = 0, strideC_ = 0;
	int      expected_MatA_size, expected_MatB_size, read_MatA_size, read_MatB_size;
	// Internal SRAM
	uint8_t  A_matrix[64][64];
	uint8_t  B_matrix[64][64];
	uint16_t C_matrix[64][64];  // size M_ * N_

	uint16_t Tile_Result[8][8];
	uint8_t  A_Tile[8][8];
	uint8_t  B_Tile[8][8];

	/* ---------- on‑chip SRAM ---------- */
	uint32_t sram_[SA_SRAM_SIZE]{};

	/* ---------- burst tracking (same idea as DataMemory) ---------- */
	struct BurstTracker {
		int                                   expected = 0;
		std::vector<XBarMemReadRespPayload*>  rbeats;
		std::vector<XBarMemWriteRespPayload*> wbeats;
	};
	std::unordered_map<int /*tid*/, BurstTracker> pending_;

	enum Phase { READ_MAT_A, READ_MAT_B, COMPUTE };
	Phase phase_ = READ_MAT_A;

	// Crossbar ports
	acalsim::SimPipeRegister*       m_req_  = nullptr;
	acalsim::SimPipeRegister*       m_resp_ = nullptr;
	std::queue<acalsim::SimPacket*> req_Q_;
	std::queue<acalsim::SimPacket*> resp_Q_;
};

#endif  // SOC_INCLUDE_SYSTOLICARRAY_HH_