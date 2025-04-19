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

	static size_t getIndex(const std::string& name) {
		if (name == "cpu") return 0;  // cpu -> Req[0] / Resp[0]
		if (name == "dma") return 1;  // dma -> Req[1] / Resp[1]
		if (name == "dm") return 2;
		return 0;                       // default / unknown
	}
	
	static size_t slaveIndex(uint32_t addr) {
		if(addr >= MMIO_BASE && MMIO_END){
			return 1;
		}
		else{
			return 0;
		}
	}

protected:
	/* ----------------------------- READ -------------------------------- */
	std::shared_ptr<XBarMemReadReqPacket> Construct_MemReadpkt_non_burst(const instr& _i, instr_type _op, uint32_t _addr,
	                                                           operand _a1, const std::string& caller,
	                                                           int burst /* log2(#beats) */ = 0) {
		/* assemble burst payloads */
		std::vector<XBarMemReadReqPayload*> payloads;
		auto mem_req_packet = new XBarMemReadReqPayload(_i, _op, _addr, _a1);
		mem_req_packet->setCaller(caller);
		payloads.push_back(mem_req_packet);
		size_t src = getIndex(caller);
		size_t dst = slaveIndex(_addr);
		return std::make_shared<XBarMemReadReqPacket>(burst, payloads, src, dst);
	}

	std::shared_ptr<XBarMemReadReqPacket> Construct_MemReadpkt_burst(std::string _src,  std::vector<XBarMemReadReqPayload*>& payloads) {
		/* assemble burst payloads */
		int burst = int(log(payloads.size()));
		for(auto payload : payloads){
			payload->setCaller(_src);
		}
		size_t src = getIndex(_src);
		size_t dst = slaveIndex(payloads[0]->getAddr());
		return std::make_shared<XBarMemReadReqPacket>(burst, payloads, src, dst);
	}

	std::shared_ptr<XBarMemWriteReqPacket> Construct_MemWritepkt_burst(std::string _src, std::vector<XBarMemWriteReqPayload*>& payloads) {
		/* assemble burst payloads */
		int burst = int(log(payloads.size()));
		for(auto payload : payloads){
			payload->setCaller(_src);
		}
		size_t src = getIndex(_src);
		size_t dst = slaveIndex(payloads[0]->getAddr());
		return std::make_shared<XBarMemWriteReqPacket>(burst, payloads, src, dst);

	}

	/* ----------------------------- WRITE ------------------------------- */
	std::shared_ptr<XBarMemWriteReqPacket> Construct_MemWritepkt_non_burst(const instr& _i, instr_type _op, uint32_t _addr,
	                                                             uint32_t _data, const std::string& caller,
	                                                             int burst /* log2(#beats) */ = 0) {
		std::vector<XBarMemWriteReqPayload*> payloads;
		auto mem_req_packet = new XBarMemWriteReqPayload(_i, _op, _addr, _data);
		mem_req_packet->setCaller(caller);
		payloads.push_back(mem_req_packet);
		size_t src = getIndex(caller);
		size_t dst = slaveIndex(_addr);
		return std::make_shared<XBarMemWriteReqPacket>(burst, payloads, src, dst);
	}

	 /* read‑response */
	 std::shared_ptr<XBarMemReadRespPacket>
	 Construct_MemReadRespPkt(const std::vector<XBarMemReadRespPayload*>& beats,
							  const std::string&                          src,
							  const std::string&                          dst /*DataMem*/){

	    int burstMode = (beats.size() == 1) ? 0 : static_cast<int>(std::ceil(std::log2(beats.size())));
 
		size_t dst_idx = getIndex(dst);
		size_t src_idx = getIndex(src);

		return std::make_shared<XBarMemReadRespPacket>(burstMode,
														beats,
														src_idx,
														dst_idx);
	 }
 
	 /* write‑response */
	 std::shared_ptr<XBarMemWriteRespPacket>
	 Construct_MemWriteRespPkt(const std::vector<XBarMemWriteRespPayload*>& beats,
							   const std::string&                          src,
							   const std::string&                          dst){
		 int burstMode = (beats.size() == 1) ? 0
											 : static_cast<int>(std::ceil(std::log2(beats.size())));
 
		 size_t dst_idx = getIndex(dst);
		 size_t src_idx = getIndex(src);

		 return std::make_shared<XBarMemWriteRespPacket>(burstMode,
														 beats,
														 src_idx,
														 dst_idx);
	 }
};

#endif /* SOC_INCLUDE_MMIOUTIL_HH_ */