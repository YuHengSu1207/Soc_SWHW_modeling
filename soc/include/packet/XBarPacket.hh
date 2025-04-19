#ifndef SOC_INCLUDE_XBAR_PACKET_HH_
#define SOC_INCLUDE_XBAR_PACKET_HH_
#include "ACALSim.hh"

// forward class
class XBarMemReadReqPayload;
class XBarMemWriteReqPayload;
class XBarMemReadRespPayload;
class XBarMemWriteRespPayload;

class BurstModeBusPacket {
public:
	/** @brief Constructor that initializes burst mode and assigns a unique TransactionID */
	explicit BurstModeBusPacket(int burstMode = 0, std::string caller = "IDK")
	    : burstSize(pow(2, burstMode)), burstLength(burstMode), TransactionID(0), Caller(caller) {}

	int         getBurstLen() const { return burstLength; }
	int         getBurstSize() const { return burstSize; }
	int         getTransactionID() const { return TransactionID; }
	void        setTransactionID(int tid) { TransactionID = tid; }
	std::string getCallerName() const { return Caller; }
	static int  generateTransactionID() {
        static std::atomic<int> transactionCounter{0};  // Atomic counter for thread safety
        return transactionCounter++;
	}

protected:
	std::string Caller;
	int         TransactionID;
	int         burstLength;
	int         burstSize;
};

template <typename PayloadType>
class XBarMemPacket : public acalsim::crossbar::CrossBarPacket, public BurstModeBusPacket {
public:
	XBarMemPacket() : acalsim::crossbar::CrossBarPacket(0, 0), BurstModeBusPacket(0) {}

	XBarMemPacket(int burstMode, const std::vector<PayloadType*>& payloads, size_t src_idx = 0, size_t dst_idx = 0)
	    : acalsim::crossbar::CrossBarPacket(src_idx, dst_idx), BurstModeBusPacket(burstMode), payloadList(payloads) {}

	~XBarMemPacket() override = default;

	void renew(int burstMode, const std::vector<PayloadType*>& payloads, size_t src_idx = 0, size_t dst_idx = 0) {
		this->acalsim::crossbar::CrossBarPacket::renew(src_idx, dst_idx);
		this->burstLength   = burstMode;
		this->burstSize     = std::pow(2, burstMode);
		this->payloadList   = payloads;
		this->TransactionID = generateTransactionID();
	}

	std::vector<PayloadType*> getPayloads() const { return payloadList; }

	void visit(acalsim::Tick when, acalsim::SimModule& module) override {}
	void visit(acalsim::Tick when, acalsim::SimBase& simulator) override;

private:
	std::vector<PayloadType*> payloadList;
};

class XBarMemReadReqPacket : public XBarMemPacket<XBarMemReadReqPayload> {
public:
	using XBarMemPacket<XBarMemReadReqPayload>::XBarMemPacket;

	void visit(acalsim::Tick when, acalsim::SimBase& simulator) override;
};

class XBarMemWriteReqPacket : public XBarMemPacket<XBarMemWriteReqPayload> {
public:
	using XBarMemPacket<XBarMemWriteReqPayload>::XBarMemPacket;

	void visit(acalsim::Tick when, acalsim::SimBase& simulator) override;
};

class XBarMemReadRespPacket : public XBarMemPacket<XBarMemReadRespPayload> {
public:
	using XBarMemPacket<XBarMemReadRespPayload>::XBarMemPacket;

	void visit(acalsim::Tick when, acalsim::SimBase& simulator) override;
};

class XBarMemWriteRespPacket : public XBarMemPacket<XBarMemWriteRespPayload> {
public:
	using XBarMemPacket<XBarMemWriteRespPayload>::XBarMemPacket;

	void visit(acalsim::Tick when, acalsim::SimBase& simulator) override;
};

class XBarMemReadReqPayload : public acalsim::RecyclableObject {
public:
	XBarMemReadReqPayload(const instr& _i = instr{}, instr_type _op = NOP, uint32_t _addr = 0, operand _a1 = operand{})
	    : tid(-1), i(_i), op(_op), addr(_addr), a1(_a1) {}

	~XBarMemReadReqPayload() override = default;

	void renew(const instr& _i, instr_type _op, uint32_t _addr, operand _a1) {
		this->i    = _i;
		this->op   = _op;
		this->addr = _addr;
		this->a1   = _a1;
	}

	void setTid(int _tid) { this->tid = _tid; }
	int  getTid() const { return this->tid; }
	std::string getCaller() const {return this->Caller;}
	void setCaller(std::string _caller) {this->Caller = _caller;}
	const instr& getInstr() const { return i; }
	instr_type   getOP() const { return op; }
	uint32_t     getAddr() const { return addr; }
	operand      getA1() const { return a1; }

private:
	std::string Caller;
	int        tid;
	instr      i;
	instr_type op;
	uint32_t   addr;
	operand    a1;
};

class XBarMemWriteReqPayload : public acalsim::RecyclableObject {
public:
	XBarMemWriteReqPayload(const instr& _i = instr{}, instr_type _op = NOP, uint32_t _addr = 0, uint32_t _data = 0)
	    : tid(-1), i(_i), op(_op), addr(_addr), data(_data) {}

	~XBarMemWriteReqPayload() override = default;

	void renew(const instr& _i, instr_type _op, uint32_t _addr, uint32_t _data) {
		this->i    = _i;
		this->op   = _op;
		this->addr = _addr;
		this->data = _data;
	}

	void setTid(int _tid) { this->tid = _tid; }
	int  getTid() const { return this->tid; }
	std::string getCaller() const {return this->Caller;}
	void setCaller(std::string _caller) {this->Caller = _caller;}
	const instr& getInstr() const { return i; }
	instr_type   getOP() const { return op; }
	uint32_t     getAddr() const { return addr; }
	uint32_t     getData() const { return data; }

private:
	std::string Caller;
	int        tid;
	instr      i;
	instr_type op;
	uint32_t   addr;
	uint32_t   data;
};

class XBarMemReadRespPayload : public acalsim::RecyclableObject {
public:
	XBarMemReadRespPayload(const instr& _i = instr{}, instr_type _op = instr_type::NOP, uint32_t _data = 0,
	                       operand _a1 = operand{})
	    : tid(-1), i(_i), op(_op), data(_data), a1(_a1) {}

	~XBarMemReadRespPayload() override = default;

	void renew(const instr& _i, instr_type _op, uint32_t _data, operand _a1) {
		this->i    = _i;
		this->op   = _op;
		this->data = _data;
		this->a1   = _a1;
	}

	void setTid(int _tid) { this->tid = _tid; }
	int  getTid() const { return this->tid; }

	const instr& getInstr() const { return i; }
	instr_type   getOP() const { return op; }
	uint32_t     getData() const { return data; }
	operand      getA1() const { return a1; }

private:
	int        tid;
	instr      i;
	instr_type op;
	uint32_t   data;
	operand    a1;
};

class XBarMemWriteRespPayload : public acalsim::RecyclableObject {
public:
	XBarMemWriteRespPayload(const instr& _i = instr{}) : tid(-1), i(_i) {}

	~XBarMemWriteRespPayload() override = default;

	void renew(const instr& _i) { this->i = _i; }

	void setTid(int _tid) { this->tid = _tid; }
	int  getTid() const { return this->tid; }

	const instr& getInstr() const { return i; }

private:
	int   tid;
	instr i;
};

#endif  // SOC_INCLUDE_XBAR_PACKET_HH_