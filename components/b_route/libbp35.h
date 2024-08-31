#pragma once
#include <string>
#include <string_view>

namespace libbp35 {

enum class event_t {
	none,
	error,
	ver,
	rxudp,
	pandesc,
	event,
	ok,
	unknown,
};

extern const char* event_str(event_t ev);
extern const char* event_num_str(uint8_t num);

struct event_params_t {
	std::string line;
	std::string_view remain;
	union {
		struct {
			uint8_t num;
		} event;
	};
	void clear() {
		line.clear();
		remain = {};
		event.num = 0;
	}
};

struct rxudp_t {
	uint8_t sender[16];
	uint8_t dest[16];
	uint16_t rport;
	uint16_t lport;
	uint8_t sender_lla[8];
	bool secured;
	uint16_t data_len;
	std::string::size_type data_pos;
};

class SerialIO {
 public:
	virtual size_t write(const char* str) = 0;
	virtual size_t write(char) = 0;
	virtual size_t write(const char* str, size_t len) = 0;
	virtual int read();
};

class BP35 {
 public:
	BP35(SerialIO& stream) : stream(stream) {}

	template <typename... Args>
	void send_sk(std::string_view cmd, Args... args) {
		int a[] = {0, (stream.write(cmd.data(), std::size(cmd)), 0),
		           (stream.write(' '), stream.write(args.data(), std::size(args)), 0)...};
		static_cast<void>(a);
		stream.write("\r\n");
	}

	template <typename... Args>
	void send_prod(std::string_view cmd, Args... args) {
		int a[] = {0, (stream.write(cmd.data(), std::size(cmd)), 0),
		           (stream.write(' '), stream.write(args.data(), std::size(args)), 0)...};
		static_cast<void>(a);
		stream.write("\r");
	}

	template <typename... Args>
	void send_sk_with_data(std::string_view cmd, const std::byte* data, size_t data_len, const Args&... args) {
		int a[] = {0, (stream.write(cmd.data(), std::size(cmd)), 0),
		           (stream.write(' '), stream.write(args.data(), std::size(args)), 0)...};
		static_cast<void>(a);
		stream.write(' ');
		stream.write(reinterpret_cast<const char*>(data), data_len);
	}
	bool read_line(std::string& line, uint32_t timeout);
	event_t get_event(uint32_t timeout, event_params_t& params);

	static bool parse_rxudp(std::string_view remains, rxudp_t& out);

 private:
	SerialIO& stream;
};

}  // namespace libbp35
