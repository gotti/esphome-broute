#include "echonet_lite.h"
#include <cstddef>

bool
echonet_lite::Codec::decode_packet(const std::byte* data, size_t data_len, Packet& out) {
	constexpr const size_t p_min = 12;
	static_assert(offsetof(Packet, properties) == p_min);
	if (data_len < 12) {
		return false;
	}
	std::copy(data, data + p_min, reinterpret_cast<std::byte*>(&out));
	if (out.ehd1 != EHD1 || out.ehd2 != EHD2_Format1) {
		// currently EHD2_Format2 is not supported
		return false;
	}
	out.tid = (std::to_integer<uint8_t>(data[2]) << 8) + std::to_integer<uint8_t>(data[3]);
	for (int pos = p_min, n = 0; n < out.opc; n++) {
		if (data_len < pos + 2) {
			return false;
		}
		Property& prop = out.properties[n];
		prop.epc = std::to_integer<uint8_t>(data[pos++]);
		prop.pdc = std::to_integer<uint8_t>(data[pos++]);
		prop.offset = pos;
		if (data_len < pos + prop.pdc) {
			return false;
		}
		pos += prop.pdc;
	}
	return true;
}
