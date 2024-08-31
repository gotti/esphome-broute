#include "util.h"
#include <cstddef>
#include <cstring>

namespace util {

int8_t
nibble(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

char
hexchar(int b, bool upper) {
	if (b >= 0 && b <= 9) {
		return '0' + b;
	}
	if (b >= 10 && b <= 15) {
		return (upper ? 'A' : 'a') + (b - 10);
	}
	return 0;
}

#if __GNUG__ <= 7
static const auto WHITESPACES = std::string_view(" \t\r\n");
#else
static constexpr auto WHITESPACES = std::string_view(" \t\r\n");
#endif

std::string_view
ltrim_sv(std::string_view str) {
	auto pos = str.find_first_not_of(WHITESPACES);
	return pos == str.npos ? std::string_view{} : std::string_view{str}.substr(pos);
}

std::string_view
rtrim_sv(std::string_view str) {
	auto pos = str.find_last_not_of(WHITESPACES);
	return pos == str.npos ? std::string_view{} : std::string_view{str}.substr(0, pos + 1);
}

std::string_view
trim_sv(std::string_view str) {
	return rtrim_sv(ltrim_sv(str));
}

}  // namespace util
