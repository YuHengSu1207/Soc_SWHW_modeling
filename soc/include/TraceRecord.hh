// demo/common.hh

#include <sstream>
#include <string>

#include "ACALSim.hh"

inline std::string convertUint32ToHex(uint32_t _value) {
	std::stringstream ss;
	ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << _value;
	return ss.str();
}

class RequestHandlingRecord : public acalsim::SimTraceRecord {
public:
	RequestHandlingRecord(int _req_id, uint32_t _addr, uint32_t _size, acalsim::Tick _ts)
	    : acalsim::SimTraceRecord(), req_id_(_req_id), addr_(_addr), size_(_size), time_stamp_(_ts) {}

	nlohmann::json toJson() const override {
		nlohmann::json j = nlohmann::json::object();
		j["request id"]  = this->req_id_;
		j["address"]     = convertUint32ToHex(this->addr_);
		j["data size"]   = this->size_;
		j["time"]        = this->time_stamp_;
		return j;
	}

private:
	int           req_id_;
	uint32_t      addr_;
	uint32_t      size_;
	acalsim::Tick time_stamp_;
};

class ResponseHandlingRecord : public acalsim::SimTraceRecord {
public:
	ResponseHandlingRecord(int _req_id, uint32_t _addr, uint32_t _size, acalsim::Tick _ts)
	    : req_id_(_req_id), addr_(_addr), size_(_size), time_stamp_(_ts) {}

	nlohmann::json toJson() const override {
		nlohmann::json j = nlohmann::json::object();
		j["request id"]  = this->req_id_;
		j["address"]     = convertUint32ToHex(this->addr_);
		j["data size"]   = this->size_;
		j["time"]        = this->time_stamp_;
		return j;
	}

private:
	int           req_id_;
	uint32_t      addr_;
	uint32_t      size_;
	acalsim::Tick time_stamp_;
};

class MemRequestInfoRecord : public acalsim::SimTraceRecord {
public:
	MemRequestInfoRecord(int _req_id, uint32_t _addr, uint32_t _size)
	    : acalsim::SimTraceRecord(), req_id_(_req_id), addr_(_addr), size_(_size) {}

	nlohmann::json toJson() const override {
		nlohmann::json info = nlohmann::json::object();
		info["request id"]  = this->req_id_;
		info["address"]     = convertUint32ToHex(this->addr_);
		info["data size"]   = this->size_;

		return info;
	}

private:
	int      req_id_;
	uint32_t addr_;
	uint32_t size_;
};
