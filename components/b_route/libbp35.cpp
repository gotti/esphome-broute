#include "libbp35.h"
#include <esphome/core/application.h>
#include <esphome/core/hal.h>
#include "bp35cmd.h"

using namespace libbp35::cmd;
namespace libbp35 {

const char*
event_num_str(uint8_t num) {
	switch (num) {
		case 0x01:
			return "rcvNS";
		case 0x02:
			return "rcvNA";
		case 0x05:
			return "recvECHO";
		case 0x1f:
			return "doneEDscan";
		case 0x20:
			return "recvBCN";
		case 0x21:
			return "sentUDP";
		case 0x22:
			return "doneAScan";
		case 0x24:
			return "failedPANAconn";
		case 0x25:
			return "donePANAconn";
		case 0x26:
			return "recvDISC";
		case 0x27:
			return "donePANAdisc";
		case 0x28:
			return "timeoutPANDdisc";
		case 0x29:
			return "expiredSession";
		case 0x32:
			return "limitRate";
		case 0x33:
			return "canceledLimit";
		default:
			return "UNKNOWN";
	}
}

const char*
event_str(event_t ev) {
	switch (ev) {
		case event_t::error:
			return "error";
		case event_t::event:
			return "event";
		case event_t::none:
			return "none";
		case event_t::ok:
			return "ok";
		case event_t::pandesc:
			return "pandesc";
		case event_t::rxudp:
			return "rxudp";
		case event_t::ver:
			return "ver";
		default:
		case event_t::unknown:
			return "unknown";
	}
}

bool
BP35::read_line(std::string& line, uint32_t timeout) {
	auto start = esphome::millis();
	bool read1 = false;
	while (esphome::millis() - start < timeout) {
		int c = stream.read();
		if (c == '\n')
			continue;
		if (c < 0) {
			if (!read1) {
				return false;
			}
			esphome::delay(1);
			continue;
		}
		read1 = true;
		while (c >= 0 && c != '\r') {
			if (c != '\n') {
				line += static_cast<char>(c);
			}
			c = stream.read();
		}
		if (c == '\r') {
			if (line.size() == 0) {
				return false;
			}
			return true;
		}
	}
	return false;
}

bool
BP35::parse_rxudp(std::string_view remain, rxudp_t& out) {
	auto pos = std::cbegin(remain);
	auto end = std::cend(remain);
	if (!arg::get_ipv6(pos, end, out.sender) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse sender address in RXUDP: %s", remain.data());
		return false;
	}
	if (!arg::get_ipv6(pos, end, out.dest) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse destination address in RXUDP: %s", remain.data());
		return false;
	}
	if (!arg::get_num16(pos, end, out.rport) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse remote port in RXUDP: %s", remain.data());
		return false;
	}
	if (!arg::get_num16(pos, end, out.lport) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse local port in RXUDP: %s", remain.data());
		return false;
	}
	if (!arg::get_mac(pos, end, out.sender_lla) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse sender LLA in RXUDP: %s", remain.data());
		return false;
	}
	if (!arg::get_flag(pos, end, out.secured) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse secured flag in RXUDP: %s", remain.data());
		return false;
	}
	if (!arg::get_num16(pos, end, out.data_len) || !arg::skip_sep(pos, end)) {
		ESP_LOGE(TAG, "Failed to parse data length in RXUDP: %s", remain.data());
		return false;
	}
	out.data_pos = std::distance(std::cbegin(remain), pos);

	return true;
}

event_t
BP35::get_event(uint32_t timeout, event_params_t& params) {
	params.clear();
	std::string line;
	for (auto remain = timeout; remain > 0;) {
		line.clear();
		auto started = esphome::millis();
		if (!read_line(line, remain)) {
			return event_t::none;
		}
		if (line.rfind("SK", 0) == 0) {
			auto elapsed = esphome::millis() - started;
			if (remain > elapsed) {
				remain -= elapsed;
			} else {
				return event_t::none;
			}
			continue;
		}
		break;
	}
	params.line = std::move(line);
	if (params.line == "OK") {
		return event_t::ok;
	}
	if (params.line.rfind("OK ", 0) == 0) {
		params.remain = std::string_view{params.line}.substr(3);
		return event_t::ok;
	}
	if (params.line.rfind("EVER ", 0) == 0) {
		params.remain = std::string_view{params.line}.substr(5);
		return event_t::ver;
	}
	if (params.line.rfind("EVENT ", 0) == 0) {
		params.remain = std::string_view{params.line}.substr(6);
		auto beg = std::cbegin(params.remain);
		if (!arg::get_num8(beg, std::cend(params.remain), params.event.num)) {
			params.event.num = 0;
		}
		return event_t::event;
	}
	if (params.line.rfind("ERXUDP ", 0) == 0) {
		params.remain = std::string_view{params.line}.substr(7);
		return event_t::rxudp;
	}
	if (params.line.rfind("EPANDESC ", 0) == 0) {
		params.remain = std::string_view{params.line}.substr(9);
		return event_t::pandesc;
	}
	return event_t::unknown;
}

}  // namespace libbp35
