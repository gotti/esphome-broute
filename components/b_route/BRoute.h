#pragma once
#include <esphome/components/sensor/sensor.h>
#include <esphome/components/uart/uart.h>
#include <esphome/core/component.h>
#include <cmath>
#include "bp35cmd.h"
#include "echonet_lite.h"
#include "libbp35.h"

namespace esphome {
namespace b_route {

using echonet_lite::EOJ;

class BRoute : public Component, public uart::UARTDevice, public libbp35::SerialIO {
 public:
	BRoute();
	virtual void loop() override;
	void set_power_sensor(sensor::Sensor* sensor) { power_sensor = sensor; }
	void set_energy_sensor(sensor::Sensor* sensor) { energy_sensor = sensor; }
	void set_power_sensor_interval_sec(uint32_t interval) { power_sensor_interval = interval * 1000; }
	void set_energy_sensor_interval_sec(uint32_t interval) { energy_sensor_interval = interval * 1000; }
	void set_rejoin_miss_count(uint8_t count) { rejoin_miss_count = count; }
	void set_rejoin_timeout_sec(uint32_t sec) { rejoin_timeout = sec * 1000; }
	void set_rescan_timeout_sec(uint32_t sec) { rescan_timeout = sec * 1000; }
	void set_restart_timeout_sec(uint32_t sec) { reboot_timeout = sec * 1000; }
	void set_rbid(const char* id, const char* password) {
		rb_id = id;
		rb_password = password;
	}

	virtual size_t write(char c) override {
		write_byte(c);
		return 1;
	}
	virtual size_t write(const char* str) override {
		size_t len = std::strlen(str);
		write_array(reinterpret_cast<const uint8_t*>(str), len);
		return len;
	}
	virtual size_t write(const char* data, size_t len) override {
		write_array(reinterpret_cast<const uint8_t*>(data), len);
		return len;
	}

	virtual int read() override {
		if (available() < 1) {
			return -1;
		}
		uint8_t b;
		if (read_byte(&b)) {
			return b;
		} else {
			return -1;
		}
	}

 private:
	static constexpr EOJ EOJ_CONTROLLER{0x05, 0xff, 0x01};
	static constexpr EOJ EOJ_LOWV_SMART_METER{0x02, 0x88, 0x01};
	static constexpr uint32_t REQUEST_PROPERTY_INTERVAL = 5'000;
	static constexpr const char* TAG = "b_route";

	enum class initial_value_t { pwd, rbid, panid, channel, ropt, wopt, echo } setting_value = initial_value_t::pwd;
	enum class state_t { init, wait_ver, setting_values, scanning, joining, running, addr_conv, restarting } state = state_t::init;

	libbp35::BP35 bp{*this};
	sensor::Sensor* power_sensor = nullptr;
	sensor::Sensor* energy_sensor = nullptr;
	std::string v6_address;
	std::string channel;
	std::string panid;
	std::string mac;
	const char* rb_password = nullptr;
	const char* rb_id = nullptr;

	int32_t energy_coeff = -1;
	float energy_unit = NAN;
	uint32_t property_requested = 0;
	uint32_t state_timeout = 0;
	uint32_t state_started = 0;
	uint32_t rejoin_timer = 0;
	uint32_t rescan_timer = 0;
	uint32_t reboot_timer = 0;
	uint8_t miss_count = 0;
	uint32_t power_sensor_interval = 30'000;
	uint32_t energy_sensor_interval = 60'000;
	uint32_t rejoin_timeout = 0;
	uint32_t rescan_timeout = 0;
	uint32_t reboot_timeout = 0;
	uint8_t rejoin_miss_count = 0;

	void set_state(state_t state, uint32_t timeout);
	void start_join();
	void start_scan();
	void handle_rxudp(std::string_view);
	void handle_property_response(const std::byte* data, const echonet_lite::Packet& pkt);
	void request_momentary_power();
	void request_integral_energy();
	void request_energy_parameters();
	bool test_nw_info() const;
	bool energy_params_received() const { return std::isfinite(energy_unit) && energy_coeff > 0; }
	libbp35::event_t get_event(libbp35::event_params_t& params);
	virtual void setup() override;
	std::array<std::byte, 255> out_buffer{};
	bool is_measurement_requesting() const {
		return (power_sensor && power_sensor_interval > 0 && power_sensor_interval != esphome::SCHEDULER_DONT_RUN) ||
		       (energy_sensor && energy_sensor_interval > 0 && energy_sensor_interval != esphome::SCHEDULER_DONT_RUN);
	}
	void reset_timers() { rejoin_timer = rescan_timer = reboot_timer = esphome::millis(); }

	template <size_t N>
	bool request_property(std::array<uint8_t, N> props) {
		using namespace libbp35::cmd;
		namespace echo = echonet_lite;
		if (state != state_t::running) {
			return false;
		}
		if (rejoin_miss_count && miss_count >= rejoin_miss_count) {
			ESP_LOGW(TAG, "Data not received for %u times, rejoin to meter", miss_count);
			miss_count = 0;
			start_join();
			return false;
		}
		if (property_requested && millis() - property_requested < REQUEST_PROPERTY_INTERVAL) {
			return false;
		}
		size_t len = echo::Codec::encode_property_get(out_buffer, EOJ_CONTROLLER, EOJ_LOWV_SMART_METER, props);
		if (len > std::size(out_buffer)) {
			ESP_LOGE(TAG, "Get property encode overflow");
			return false;
		}
		bp.send_sk_with_data("SKSENDTO", out_buffer.data(), len, arg::mode(1), arg::str(v6_address), arg::num16(echo::UDP_PORT),
		                     arg::mode(2), arg::flag(false), arg::num16(len));
		property_requested = millis();
		++miss_count;
		return true;
	}

	static const char* state_name(state_t);
};

}  // namespace b_route
}  // namespace esphome
