#ifndef SOC_INCLUDE_MMIOUTIL_HH_
#define SOC_INCLUDE_MMIOUTIL_HH_

#include "DataMemory.hh"
#include "packet/XBarPacket.hh"

/*  Address map  ---------------------------------------------------------- */
/*  – 0x0000 – 0x7FFF  : Data‑memory (slave‑idx = 0)                       */
/*  – 0xF000 – 0xF03F  : DMA‑MMIO  (slave‑idx = 1)                         */
/*    everything else : map to data‑memory                                 */
class MMIOUTIL {
	/* --- helpers ------------------------------------------------------- */
	constexpr static uint32_t MMIO_BASE = 0xF000;
	constexpr static uint32_t MMIO_END  = 0xF03F;

	static size_t masterIndex(const std::string& caller) {
		if (caller == "cpu") return 0;  // cpu -> Req[0]
		if (caller == "dma") return 1;  // dma -> Req[1]
		return 0;                       // default / unknown
	}
	static size_t slaveIndex(uint32_t addr) {
		return (addr >= MMIO_BASE && addr <= MMIO_END) ? 1 /* DMA */ : 0 /* DM */;
	}

protected:
	/* ----------------------------- READ -------------------------------- */
	std::shared_ptr<XBarMemReadReqPacket> Construct_MemReadpkt(const instr& _i, instr_type _op, uint32_t _addr,
	                                                           operand _a1, const std::string& caller,
	                                                           int burst /* log2(#beats) */ = 1) {
		/* assemble burst payloads */
		std::vector<XBarMemReadReqPayload*> payloads;
		for (int i = 0; i < (1 << burst); ++i) {
			uint32_t addr_i = _addr + static_cast<uint32_t>(i) * 4;  // 4‑byte words
			operand  a1_i   = _a1;
			a1_i.imm += i;

			payloads.push_back(new XBarMemReadReqPayload(_i, _op, addr_i, a1_i));
		}

		size_t src = masterIndex(caller);
		size_t dst = slaveIndex(_addr);

		return std::make_shared<XBarMemReadReqPacket>(burst, payloads, src, dst);
	}

	/* ----------------------------- WRITE ------------------------------- */
	std::shared_ptr<XBarMemWriteReqPacket> Construct_MemWritepkt(const instr& _i, instr_type _op, uint32_t _addr,
	                                                             uint32_t _data, const std::string& caller,
	                                                             int burst /* log2(#beats) */ = 1) {
		std::vector<XBarMemWriteReqPayload*> payloads;
		for (int i = 0; i < (1 << burst); ++i) {
			uint32_t addr_i = _addr + static_cast<uint32_t>(i) * 4;
			uint32_t data_i = _data + static_cast<uint32_t>(i);  // example pattern

			payloads.push_back(new XBarMemWriteReqPayload(_i, _op, addr_i, data_i));
		}

		size_t src = masterIndex(caller);
		size_t dst = slaveIndex(_addr);

		return std::make_shared<XBarMemWriteReqPacket>(burst, payloads, src, dst);
	}
};

#endif /* SOC_INCLUDE_MMIOUTIL_HH_ */