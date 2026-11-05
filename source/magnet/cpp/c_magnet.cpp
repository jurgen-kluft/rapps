#include "magnet/c_magnet.h"

#include "rcore/c_app.h"
#include "rcore/c_state.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_str.h"

#include "rhome/c_home.h"
#include "rhome/c_sensor.h"

#include "rwifi/c_wifi_mgr.h"
#include "rwifi/c_udp_client.h"

#ifndef TARGET_ESP8266
#    define A0 17
#else
#    include "Arduino.h"
#endif

namespace ncore
{
    ngpio::input_pin_t  switch_pin(13);
    ngpio::output_pin_t poweroff_pin(16);
    ngpio::analog_pin_t battery_pin(A0);

    struct state_app_t
    {
        nnet::wifi_manager_t* wifi_mgr;
        nnet::udp_config_t    udp_config;
        nnet::udp_client_t    udp_client;
        nnet::msg_t           msg;
        u64                   connect_deadline_ms;
        bool                  completed;
    };
    state_app_t gAppState;

    namespace napp
    {
        u64 gStartTimeMs;

        void wakeup(state_t* state, nwakeup::reason_t reason)
        {
            poweroff_pin.setup();
            poweroff_pin.set_high();
            switch_pin.setup();
            gStartTimeMs = ntimer::millis();
        }

        void setup(state_t* state)
        {
            gAppState.wifi_mgr = nnet::create_wifi_mgr(WIFI_SSID(), WIFI_PASSWORD());
            nnet::activate(gAppState.wifi_mgr);

            void* udp_socket = nnet::setup_default(&gAppState.udp_config);
            nnet::setup(gAppState.udp_client, &gAppState.udp_config, udp_socket);
            nnet::bind(gAppState.udp_client, SENSOR_SERVER_UDPPORT());

            gAppState.connect_deadline_ms = ntimer::millis() + 10000;
            gAppState.completed           = false;
        }

        void tick(state_t* state)
        {
            if (gAppState.completed)
                return;

            nnet::tick(gAppState.wifi_mgr);
            nnet::tick_udp_client(nnet::get_link_state(gAppState.wifi_mgr), gAppState.udp_client);

            const u64 now_ms = ntimer::millis();
            if (nnet::is_connected(gAppState.wifi_mgr) && nnet::is_bound(gAppState.udp_client))
            {
                i32 switch_state_cur      = switch_pin.is_high() ? 1 : 0;
                i32 switch_state_prev     = 1 - switch_state_cur;
                i32 switch_debounce_count = 0;
                while (switch_state_cur != switch_state_prev && switch_debounce_count <= 3)
                {
                    ntimer::delay(20);
                    switch_state_prev = switch_state_cur;
                    switch_state_cur  = switch_pin.is_high() ? 1 : 0;
                    switch_debounce_count++;
                }

                const s32 battery_level = (battery_pin.read() * 42) / 1023;
                const s32 rssi          = nnet::get_rssi(gAppState.wifi_mgr);
                const u64 boottime      = now_ms - gStartTimeMs;
                u8 const* mac           = nnet::get_mac_address(gAppState.wifi_mgr);

                nnet::msg_init(gAppState.msg, nnet::MSG_TYPE_SENSOR_DATA, mac);
                msg_write_sensor(gAppState.msg, SENSOR_ID_BINARY, switch_state_cur);
                msg_write_sensor(gAppState.msg, SENSOR_ID_BATTERY, battery_level);
                msg_write_sensor(gAppState.msg, SENSOR_ID_RSSI, rssi);
                msg_write_sensor(gAppState.msg, SENSOR_ID_DURATION, boottime);
                msg_write_sensor(gAppState.msg, SENSOR_ID_FREQUENCY, switch_debounce_count);
                nnet::msg_final(gAppState.msg);

                const nnet::udp_endpoint_t endpoint = {SENSOR_SERVER_IP(), SENSOR_SERVER_UDPPORT()};
                if (nnet::send_to(gAppState.udp_client, endpoint, gAppState.msg.Data, gAppState.msg.Size))
                {
                    gAppState.completed = true;
                    poweroff_pin.set_low();
                    nlog::println("sensor message has been sent, turning OFF device!");
                    return;
                }
            }

            if (now_ms >= gAppState.connect_deadline_ms)
            {
                gAppState.completed = true;
                poweroff_pin.set_low();
                nlog::println("sensor message was not sent before the connection deadline, turning OFF device!");
            }
        }

    }  // namespace napp
}  // namespace ncore
