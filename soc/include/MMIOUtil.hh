#ifndef SOC_INCLUDE_MMIOUTIL_HH_
#define SOC_INCLUDE_MMIOUTIL_HH_

#include "ACALSim.hh"
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

	static size_t getIndex(const std::string& name) {
		if (name == "cpu" || name == "dm") return 0;  // cpu, dm (first in index)
		if (name == "dma") return 1;                  // dma -> Req[1]
		return 0;                                     // default / unknown
	}

	static size_t slaveIndex(uint32_t addr) {
		if (addr >= MMIO_BASE && MMIO_END) {
			return 1;
		} else {
			return 0;
		}
	}

protected:
	/* ----------------------------- READ -------------------------------- */
	XBarMemReadReqPacket* Construct_MemReadpkt_non_burst(const instr& _i, instr_type _op, uint32_t _addr, operand _a1,
	                                                     const std::string& caller, int burst /* log2(#beats) */ = 0) {
		/* assemble burst payloads */
		auto                                rc = acalsim::top->getRecycleContainer();
		std::vector<XBarMemReadReqPayload*> payloads;
		XBarMemReadReqPayload*              mem_req_packet =
		    rc->acquire<XBarMemReadReqPayload>(&XBarMemReadReqPayload::renew, _i, _op, _addr, _a1);
		mem_req_packet->setCaller(caller);
		payloads.push_back(mem_req_packet);
		size_t                src      = getIndex(caller);
		size_t                dst      = slaveIndex(_addr);
		bool                  renew_id = true;
		XBarMemReadReqPacket* pkt =
		    rc->acquire<XBarMemReadReqPacket>(&XBarMemReadReqPacket::renew, burst, payloads, src, dst, renew_id);
		pkt->getPayloads()[0]->setTid(pkt->getAutoIncTID());

		return pkt;
	}

	/* ----------------------------- WRITE ------------------------------- */
	XBarMemWriteReqPacket* Construct_MemWritepkt_non_burst(const instr& _i, instr_type _op, uint32_t _addr,
	                                                       uint32_t _data, const std::string& caller,
	                                                       int burst /* log2(#beats) */ = 0) {
		auto                                 rc = acalsim::top->getRecycleContainer();
		std::vector<XBarMemWriteReqPayload*> payloads;

		XBarMemWriteReqPayload* mem_req_packet =
		    rc->acquire<XBarMemWriteReqPayload>(&XBarMemWriteReqPayload::renew, _i, _op, _addr, _data);
		mem_req_packet->setCaller(caller);
		payloads.push_back(mem_req_packet);

		size_t                 src      = getIndex(caller);
		size_t                 dst      = slaveIndex(_addr);
		bool                   renew_id = true;
		XBarMemWriteReqPacket* pkt =
		    rc->acquire<XBarMemWriteReqPacket>(&XBarMemWriteReqPacket::renew, burst, payloads, src, dst, renew_id);
		pkt->getPayloads()[0]->setTid(pkt->getAutoIncTID());

		return pkt;
	}

	XBarMemReadReqPacket* Construct_MemReadpkt_burst(std::string _src, std::vector<XBarMemReadReqPayload*>& payloads) {
		/* assemble burst payloads */
		auto rc           = acalsim::top->getRecycleContainer();
		bool renew_id     = true;
		int  payload_size = payloads.size();
		int  burst        = -1;
		switch (payload_size) {
			case 4: burst = 2; break;
			case 2: burst = 1; break;
			case 1: burst = 0; break;
			default: burst = -1; break;
		}
		for (auto payload : payloads) { payload->setCaller(_src); }
		size_t                src = getIndex(_src);
		size_t                dst = slaveIndex(payloads[0]->getAddr());
		XBarMemReadReqPacket* pkt =
		    rc->acquire<XBarMemReadReqPacket>(&XBarMemReadReqPacket::renew, burst, payloads, src, dst, renew_id);
		for (auto payload : pkt->getPayloads()) { payload->setTid(pkt->getAutoIncTID()); }
		return pkt;
	}

	XBarMemWriteReqPacket* Construct_MemWritepkt_burst(std::string                           _src,
	                                                   std::vector<XBarMemWriteReqPayload*>& payloads) {
		auto rc           = acalsim::top->getRecycleContainer();
		int  payload_size = payloads.size();
		bool renew_id     = true;
		int  burst        = -1;
		switch (payload_size) {
			case 4: burst = 2; break;
			case 2: burst = 1; break;
			case 1: burst = 0; break;
			default: burst = -1; break;
		}
		for (auto payload : payloads) { payload->setCaller(_src); }

		size_t                 src = getIndex(_src);
		size_t                 dst = slaveIndex(payloads[0]->getAddr());
		XBarMemWriteReqPacket* pkt =
		    rc->acquire<XBarMemWriteReqPacket>(&XBarMemWriteReqPacket::renew, burst, payloads, src, dst, renew_id);
		for (auto payload : pkt->getPayloads()) { payload->setTid(pkt->getAutoIncTID()); }
		return pkt;
	}

	/* read‑response */
	XBarMemReadRespPacket* Construct_MemReadRespPkt(const std::vector<XBarMemReadRespPayload*>& beats,
	                                                const std::string& src, const std::string& dst /*DataMem*/) {
		int    burstMode = (beats.size() == 1) ? 0 : static_cast<int>(std::ceil(std::log2(beats.size())));
		auto   rc        = acalsim::top->getRecycleContainer();
		size_t dst_idx   = getIndex(dst);
		size_t src_idx   = getIndex(src);

		return rc->acquire<XBarMemReadRespPacket>(&XBarMemReadRespPacket::renew, burstMode, beats, src_idx, dst_idx,
		                                          false);
	}

	/* write‑response */
	XBarMemWriteRespPacket* Construct_MemWriteRespPkt(const std::vector<XBarMemWriteRespPayload*>& beats,
	                                                  const std::string& src, const std::string& dst) {
		int    burstMode = (beats.size() == 1) ? 0 : static_cast<int>(std::ceil(std::log2(beats.size())));
		auto   rc        = acalsim::top->getRecycleContainer();
		size_t dst_idx   = getIndex(dst);
		size_t src_idx   = getIndex(src);
		return rc->acquire<XBarMemWriteRespPacket>(&XBarMemWriteRespPacket::renew, burstMode, beats, src_idx, dst_idx,
		                                           false);
	}
};

#endif /* SOC_INCLUDE_MMIOUTIL_HH_ */