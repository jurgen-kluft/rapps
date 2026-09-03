#include "magnet/c_magnet.h"

#include "rcore/c_state.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_state.h"
#include "rcore/c_str.h"

#include "rhome/c_home.h"
#include "rhome/c_sensor.h"

#include "rwifi/c_wifi.h"
#include "rwifi/c_wifi_mgr.h"
#include "rwifi/c_udp.h"

#ifndef TARGET_ESP8266
    #define A0 17
#else
    #include "Arduino.h"
#endif

namespace ncore
{
    ngpio::input_pin_t  switch_pin(13);    // GPIO pin connected to switch
    ngpio::output_pin_t poweroff_pin(16);  // GPIO pin connected to end line
    ngpio::analog_pin_t battery_pin(A0);   // GPIO pin connected to battery measurement

    struct state_app_t
    {
        nnet::wifi_config_t  wifi_config;
        nnet::wifi_manager_t wifi_mgr;
        nnet::msg_t          msg;  // Global packet instance to avoid re-allocating memory on each tick
    };
    state_app_t gAppState;

}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        u64 gStartTimeMs;

        // This is where you would set up GPIO pins and other hardware before setup() is called
        void presetup(state_t* state)
        {
            // Initialize poweroff pin and set HIGH
            poweroff_pin.setup();
            poweroff_pin.set_high();
            // Initialize switch pin
            switch_pin.setup();
            // Note: Battery measurement pin is A0, and this is the only analog pin, so no need to set it up

            // Record start time
            gStartTimeMs = ntimer::millis();
        }

        void setup(state_t* state)
        {
            nnet::init_state(state, true);
            nnet::nudp::init_state(state);

            nnet::init_wifi_config(gAppState.wifi_config, WIFI_SSID(), WIFI_PASSWORD());
            nnet::setup(gAppState.wifi_mgr, &gAppState.wifi_config);
        }

        void tick(state_t* state)
        {
            i32 switch_state_cur      = switch_pin.is_high() ? 1 : 0;  // Read switch state
            i32 switch_state_prev     = 1 - switch_state_cur;          // Set previous state to opposite to ensure we enter the loop
            i32 switch_debounce_count = 0;

            // TODO, do we really need the below logic to debounce the switch?

            // Read the switch state again before powering off, and if it is
            // still the same continue the power off sequence. Otherwise, handle
            // the switch state change by sending another ESPNOW packet.
            while (switch_state_cur != switch_state_prev && switch_debounce_count <= 3)
            {
                // delay for 20 ms to debounce the switch
                ntimer::delay(20);
                switch_state_prev = switch_state_cur;
                switch_state_cur  = switch_pin.is_high() ? 1 : 0;  // Read switch state again
                switch_debounce_count++;
            }

            const s32 battery_level = (battery_pin.read() * 42) / 1023;  // Percentage (0-100 %)
            const s32 RSSI          = nnet::get_RSSI(state);             // WiFi signal strength
            const u64 boottime      = ntimer::millis() - gStartTimeMs;   // Time since boot until we send the data
            u8 const* mac           = state->MACAddress;                 // Get MAC address from state

            nnet::msg_init(gAppState.msg, nnet::MSG_TYPE_SENSOR_DATA, mac);
            {
                msg_write_sensor(gAppState.msg, SENSOR_ID_SWITCH1, switch_state_cur);     // Open/Close
                msg_write_sensor(gAppState.msg, SENSOR_ID_BATTERY, battery_level);        // Battery level
                msg_write_sensor(gAppState.msg, SENSOR_ID_RSSI, RSSI);                    // WiFi signal strength
                msg_write_sensor(gAppState.msg, SENSOR_ID_PERF1, boottime);               // Performance metric 1 (boot time in ms, max 65 seconds)
                msg_write_sensor(gAppState.msg, SENSOR_ID_PERF2, switch_debounce_count);  // Performance metric 1 (debounce count)
            }
            nnet::msg_final(gAppState.msg);

            u16 check_wifi_connect_count = 0;
            while (!nnet::connected(state) && check_wifi_connect_count < 5)
            {
                ntimer::delay(20);
                check_wifi_connect_count++;
            }

            if (nnet::is_connected(gAppState.wifi_mgr))
            {
                const IPAddress_t server_ip   = IPAddress_t::from(SENSOR_SERVER_IP());
                const u16         server_port = SENSOR_SERVER_UDPPORT();
                nnet::nudp::open(state, server_port);  // Open UDP port for sending data
                nnet::nudp::send_to(state, server_port, gAppState.msg.Data, gAppState.msg.Size, server_ip, server_port);
            }

            poweroff_pin.set_low();
            nlog::println("sensor message has been sent, turning OFF device!");
            ntimer::delay(5000);  // Delay for 5 seconds
        }

    }  // namespace napp
}  // namespace ncore
