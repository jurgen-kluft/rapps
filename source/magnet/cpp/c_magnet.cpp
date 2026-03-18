#include "magnet/c_magnet.h"

#include "rcore/c_state.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_state.h"
#include "rcore/c_str.h"
#include "rcore/c_packet.h"

#include "rwifi/c_wifi.h"
#include "rwifi/c_udp.h"

#ifndef TARGET_ESP8266
    #define A0 17
#else
    #include "Arduino.h"
#endif

namespace ncore
{
    struct state_app_t
    {
    };
    state_app_t gAppState;

    npacket::packet_t   packet;            // Global packet instance to avoid re-allocating memory on each tick
    ngpio::input_pin_t  switch_pin(13);    // GPIO pin connected to switch
    ngpio::output_pin_t poweroff_pin(16);  // GPIO pin connected to end line
    ngpio::analog_pin_t battery_pin(A0);   // GPIO pin connected to battery measurement
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
            nwifi::init_state(state, true);
            nudp::init_state(state);
            nwifi::connect(state);  // Connect to WiFi using credentials from state
        }

        void tick(state_t* state)
        {
            s8  switch_state_cur      = switch_pin.is_high() ? 1 : 0;  // Read switch state
            s8  switch_state_prev     = 1 - switch_state_cur;          // Set previous state to opposite to ensure we enter the loop
            u16 switch_debounce_count = 0;

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
            const s32 RSSI          = nwifi::get_RSSI(state);            // WiFi signal strength
            const u64 boottime      = ntimer::millis() - gStartTimeMs;   // Time since boot until we send the data
            u8 const* mac           = state->MACAddress;                 // Get MAC address from state

            npacket::sensor_block_t sensors;
            sensors.begin(&packet);
            {
                npacket::sensor_value_t switch_state_value{npacket::ID_SWITCH1, (u16)switch_state_cur};
                npacket::sensor_value_t battery_level_value{npacket::ID_BATTERY, (u16)battery_level};
                npacket::sensor_value_t rssi_value{npacket::ID_RSSI, (u16)RSSI};
                npacket::sensor_value_t perf1_value{npacket::ID_PERF1, (u16)boottime};
                npacket::sensor_value_t perf2_value{npacket::ID_PERF2, switch_debounce_count};

                sensors.write(&packet, switch_state_value);   // Open/Close
                sensors.write(&packet, battery_level_value);  // Battery level
                sensors.write(&packet, rssi_value);           // WiFi signal strength
                sensors.write(&packet, perf1_value);          // Performance metric 1 (boot time in ms, max 65 seconds)
                sensors.write(&packet, perf2_value);          // Performance metric 1 (debounce count)
            }
            sensors.finalize(&packet);

            u16 check_wifi_connect_count = 0;
            while (!nwifi::connected(state) && check_wifi_connect_count < 5)
            {
                ntimer::delay(20);
                check_wifi_connect_count++;
            }

            if (nwifi::connected(state))
            {
                packet->begin(state->MACAddress);  // Initialize packet with MAC address from state
                const IPAddress_t server_ip   = IPAddress_t::from(SERVER_IP);
                const u16         server_port = SERVER_UDPPORT;
                nudp::open(state, server_port);  // Open UDP port for sending data
                nudp::send_to(state, server_port, packet.Data, packet.Size, server_ip, server_port);
            }

            poweroff_pin.set_low();
            nlog::println("packet has been sent, turning OFF device!");
            ntimer::delay(5000);  // Delay for 5 seconds
        }

    }  // namespace napp
}  // namespace ncore
