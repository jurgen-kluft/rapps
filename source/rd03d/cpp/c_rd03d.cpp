#include "rd03d/c_rd03d.h"

#include "rcore/c_app.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"

#include "rwifi/c_wifi_mgr.h"
#include "rwifi/c_tcp_client.h"

#include "rhome/c_home.h"
#include "rhome/c_sensor.h"

#include "lib_rd03d/c_rd03d.h"

#define ENABLE_RD03D

namespace ncore
{
    namespace napp
    {
        struct rd03d_data_t
        {
            u64 DetectionBits[3];
            i32 Detected[3];

            void reset()
            {
                for (s8 i = 0; i < 3; ++i)
                {
                    DetectionBits[i] = 0;
                    Detected[i]      = 4;  // Unknown state
                }
            }
        };

        nnet::msg_t                gSensorPacket;  // Sensor packet for sending data
        nsensors::nrd03d::sensor_t gRd03dSensor;
        rd03d_data_t               gCurrentRd03d;
        u8                         gLastPresence;
        nnet::wifi_manager_t*      gWifiManager;
        nnet::config_t             gTcpConfig;
        nnet::tcp_client_t         gTcpClient;
        ntimer::periodic_task_t    gPeriodicProcessRd03d;

        void process_rd03d(void* user)
        {
#ifdef ENABLE_RD03D
            if (nsensors::nrd03d::update(gRd03dSensor))
            {
                for (s8 i = 0; i < 3; ++i)
                {
                    nsensors::nrd03d::target_t tgt;
                    if (nsensors::nrd03d::getTarget(gRd03dSensor, i, tgt))
                    {
                        gCurrentRd03d.DetectionBits[i] = (gCurrentRd03d.DetectionBits[i] << 1) | 1;
                    }
                    else
                    {
                        gCurrentRd03d.DetectionBits[i] = (gCurrentRd03d.DetectionBits[i] << 1) | 0;
                    }

                    u8         detected = gCurrentRd03d.Detected[i];
                    const bool dseen    = (gCurrentRd03d.DetectionBits[i] != 0);
                    if (dseen)
                    {
                        detected = ((detected << 1) | 1);
                    }
                    else
                    {
                        const bool dnone = gCurrentRd03d.DetectionBits[i] == 0;
                        if (dnone)
                        {
                            detected = ((detected << 1) | 0);
                        }
                    }
                    gCurrentRd03d.Detected[i] = detected;

                    if (detected == 0x80)
                    {
                        nlog::printf("Status: PRESENCE 1 -> 0 (distance: %d,%d)\n", va_t(tgt.x), va_t(tgt.y));
                    }
                    else if (detected == 0x01)
                    {
                        nlog::printf("Status: PRESENCE 0 -> 1 (distance: %d,%d)\n", va_t(tgt.x), va_t(tgt.y));
                    }
                    else if (detected != 0x0)
                    {
                        nlog::printf("Status: PRESENCE (distance: %d,%d)\n", va_t(tgt.x), va_t(tgt.y));
                    }
                    else
                    {
                        nlog::printf("Status: ABSENCE\n");
                    }
                }

                // Write a custom (binary-format) network message

                u8 presence = 0;
                for (s8 i = 0; i < 3; ++i)
                {
                    u8 detected = gCurrentRd03d.Detected[i];
                    if (detected == 0x80)
                        detected = 0;  // down flank
                    else if (detected == 0x01)
                        detected = 1;  // up flank
                    else if (detected != 0x00)
                        detected = 1;  // presence

                    presence |= (detected << i);
                }

                if (nnet::is_connected(gWifiManager))
                {
                    if (presence != gLastPresence)
                    {
                        nnet::msg_init(gSensorPacket, nnet::MSG_TYPE_SENSOR_DATA, nnet::get_mac_address(gWifiManager));
                        msg_write_sensor(gSensorPacket, SENSOR_ID_BINARY, presence);
                        msg_write_sensor(gSensorPacket, SENSOR_ID_RSSI, nnet::get_rssi(gWifiManager));
                        if (nnet::send(gTcpClient, gSensorPacket.Data, gSensorPacket.Size))
                        {
                            gLastPresence = presence;
                        }
                    }
                }
            }
#endif
        }

        void wakeup(state_t* state, nwakeup::reason_t reason)
        {
            // Initialize RD03D sensor with rx and tx pin
            nsensors::nrd03d::begin(gRd03dSensor, 20, 21);

            ntimer::init_periodic_task(&gPeriodicProcessRd03d, 100, process_rd03d, nullptr);
        }

        void setup(state_t* state)
        {
            gWifiManager = nnet::create_wifi_mgr(WIFI_SSID(), WIFI_PASSWORD());
            nnet::activate(gWifiManager);

            void* socket = nnet::setup_default(&gTcpConfig);
            nnet::setup(gTcpClient, &gTcpConfig, socket);
            nnet::connect(gTcpClient, SENSOR_SERVER_IP(), SENSOR_SERVER_TCPPORT());
        }

        void tick(state_t* state)
        {
            const u64 now_ms = ntimer::millis();

            ntimer::tick_periodic_task(&gPeriodicProcessRd03d, now_ms);

            nnet::tick(gWifiManager);
            nnet::tick_tcp_client(nnet::get_link_state(gWifiManager), gTcpClient);
        }

    }  // namespace napp
}  // namespace ncore
