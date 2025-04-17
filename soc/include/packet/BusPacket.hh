#ifndef SOC_INCLUDE_BUS_PACKET_MEMPACKET_HH_
#define SOC_INCLUDE_BUS_PACKET_MEMPACKET_HH_
#include <cmath>

#include "ACALSim.hh"
#include "Bus.hh"
#include "packet/MemPacket.hh"
/*forward class*/
class MemReadRespPacket;
class MemWriteRespPacket;
class MemReadReqPacket;
class MemWriteReqPacket;
/*forward class*/
class BusMemWriteReqPacket;
class BusMemReadReqPacket;
class BusMemWriteRespPacket;
class BusMemReadRespPacket;

class BurstModeBusPacket {
public:
	/** @brief Constructor that initializes burst mode and assigns a unique TransactionID */
	explicit BurstModeBusPacket(int burstMode = 0)
	    : burstSize(pow(2, burstMode)), burstLength(burstMode), TransactionID(0) {}

	int        getBurstLen() const { return burstLength; }
	int        getBurstSize() const { return burstSize; }
	int        getTransactionID() const { return TransactionID; }
	void       setTransactionID(int tid) { TransactionID = tid; }
	static int generateTransactionID() {
		static std::atomic<int> transactionCounter{0};  // Atomic counter for thread safety
		return transactionCounter++;
	}

protected:
	int TransactionID;
	int burstLength;
	int burstSize;
};

class BusMemReadReqPacket : public BurstModeBusPacket, public acalsim::SimPacket {
public:
	// Default constructor
	BusMemReadReqPacket() : BurstModeBusPacket(1), memReadReqPkt(), callback(nullptr) {}

	BusMemReadReqPacket(int burstMode, const std::vector<MemReadReqPacket*>& memPackets,
	                    std::function<void(BusMemReadRespPacket*)> cb)
	    : BurstModeBusPacket(burstMode), memReadReqPkt(memPackets), callback(std::move(cb)) {}
	std::vector<MemReadReqPacket*> getMemReadReqPkt() const { return memReadReqPkt; }

	void visit(acalsim::Tick _when, acalsim::SimModule& _module) override;
	/** @brief Visit function for simulator interaction */
	void visit(acalsim::Tick _when, acalsim::SimBase& _simulator) override;

	void renew(int burst_size, std::vector<MemReadReqPacket*>& memPackets,
	           std::function<void(BusMemReadRespPacket*)> cb);

private:
	std::vector<MemReadReqPacket*>             memReadReqPkt;
	std::function<void(BusMemReadRespPacket*)> callback;
};

class BusMemWriteReqPacket : public BurstModeBusPacket, public acalsim::SimPacket {
public:
	// Default constructor
	BusMemWriteReqPacket() : BurstModeBusPacket(1), memWriteReqPkt(), callback(nullptr) {}
	BusMemWriteReqPacket(int burstMode, const std::vector<MemWriteReqPacket*>& memPackets,
	                     std::function<void(BusMemWriteRespPacket*)> cb)
	    : BurstModeBusPacket(burstMode), memWriteReqPkt(memPackets), callback(std::move(cb)) {}

	std::vector<MemWriteReqPacket*> getMemWriteReqPkt() const { return memWriteReqPkt; }

	void visit(acalsim::Tick _when, acalsim::SimModule& _module) override;
	/** @brief Visit function for simulator interaction */
	void visit(acalsim::Tick _when, acalsim::SimBase& _simulator) override;

	void renew(int burst_size, std::vector<MemWriteReqPacket*>& memPackets,
	           std::function<void(BusMemWriteRespPacket*)> cb);

private:
	std::vector<MemWriteReqPacket*>             memWriteReqPkt;
	std::function<void(BusMemWriteRespPacket*)> callback;
};

class BusMemReadRespPacket : public BurstModeBusPacket, public acalsim::SimPacket {
public:
	/** @brief Constructor that initializes burst mode */
	explicit BusMemReadRespPacket(int burstMode = 1) : BurstModeBusPacket(burstMode) {}

	std::vector<MemReadRespPacket*> getMemReadRespPkt() const { return memReadRespPkt; }

	void visit(acalsim::Tick _when, acalsim::SimModule& _module) override;
	/** @brief Visit function for simulator interaction */
	void visit(acalsim::Tick _when, acalsim::SimBase& _simulator) override;

	void renew(int burst_size, std::vector<MemReadRespPacket*>& memPackets);

private:
	std::vector<MemReadRespPacket*>         memReadRespPkt;
	std::function<void(MemReadRespPacket*)> callback;
};

class BusMemWriteRespPacket : public BurstModeBusPacket, public acalsim::SimPacket {
public:
	/** @brief Constructor that initializes burst mode */
	explicit BusMemWriteRespPacket(int burstMode = 1) : BurstModeBusPacket(burstMode) {}

	std::vector<MemWriteRespPacket*> getMemWriteRespPkt() const { return memWriteRespPkt; }

	void visit(acalsim::Tick _when, acalsim::SimModule& _module) override;
	/** @brief Visit function for simulator interaction */
	void visit(acalsim::Tick _when, acalsim::SimBase& _simulator) override;

	void renew(int burst_size, std::vector<MemWriteRespPacket*>& memPackets);

private:
	std::vector<MemWriteRespPacket*>         memWriteRespPkt;
	std::function<void(MemWriteRespPacket*)> callback;
};

#endif  // #define SOC_INCLUDE_PACKET_MEMPACKET_HH_
