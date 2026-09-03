#include "rd03d/c_rd03d.h"

#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"

#include "rwifi/c_wifi.h"
#include "rwifi/c_wifi_mgr.h"
#include "rwifi/c_tcp_client.h"

#include "rhome/c_home.h"
#include "rhome/c_sensor.h"

#include "lib_rd03d/c_rd03d.h"

#define ENABLE_RD03D

namespace ncore
{
    struct rd03d_data_t
    {
        u64 DetectionBits[3];
        i32 Detected[3];
        i32 LastSendDetected[3];

        void reset()
        {
            for (s8 i = 0; i < 3; ++i)
            {
                DetectionBits[i]    = 0;
                Detected[i]         = 4;  // Unknown state
                LastSendDetected[i] = 8;
            }
        }
    };

    struct state_app_t
    {
        nnet::msg_t                gSensorPacket;  // Sensor packet for sending data
        nsensors::nrd03d::sensor_t gRd03dSensor;
        rd03d_data_t               gCurrentRd03d;

        nnet::wifi_manager_t gWifiManager;
        nnet::wifi_config_t  gWifiConfig;

        nnet::config_t gTcpConfig;
        nnet::tcp_client_t gTcpClient;
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        void process_rd03d(void* user)
        {
#ifdef ENABLE_RD03D
            if (nsensors::nrd03d::update(gAppState.gRd03dSensor))
            {
                for (s8 i = 0; i < 3; ++i)
                {
                    nsensors::nrd03d::target_t tgt;
                    if (nsensors::nrd03d::getTarget(gAppState.gRd03dSensor, i, tgt))
                    {
                        gAppState.gCurrentRd03d.DetectionBits[i] = (gAppState.gCurrentRd03d.DetectionBits[i] << 1) | 1;
                    }
                    else
                    {
                        gAppState.gCurrentRd03d.DetectionBits[i] = (gAppState.gCurrentRd03d.DetectionBits[i] << 1) | 0;
                    }

                    u8         detected = gAppState.gCurrentRd03d.Detected[i];
                    const bool dseen    = (gAppState.gCurrentRd03d.DetectionBits[i] != 0);
                    if (dseen)
                    {
                        // Too transition from no-presence to presence we must have seen 3 detections in a row (300 ms)
                        detected = ((detected << 1) | 1);
                    }
                    else
                    {
                        const bool dnone = gAppState.gCurrentRd03d.DetectionBits[i] == 0;
                        if (dnone)
                        {
                            // To transition from presence to no-presence we must have seen 32 no-detections in a row (~3 seconds)
                            detected = ((detected << 1) | 0);
                        }
                    }
                    gAppState.gCurrentRd03d.Detected[i] = detected;

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

                nnet::msg_init(gAppState.gSensorPacket, nnet::MSG_TYPE_SENSOR_DATA, nnet::get_mac_address(gAppState.gWifiManager));

                for (s8 i = 0; i < 3; ++i)
                {
                    u8 detected = gAppState.gCurrentRd03d.Detected[i];
                    if (detected == 0x80)
                        detected = 2;
                    else if (detected == 0x01)
                        detected = 1;
                    else if (detected != 0x00)
                        detected = 3;

                    if (gAppState.gCurrentRd03d.LastSendDetected[i] != detected)
                    {
                        gAppState.gCurrentRd03d.LastSendDetected[i] = detected;
                        msg_write_sensor(gAppState.gSensorPacket, SENSOR_ID_DISTANCE1 + i, detected);
                    }
                }

                msg_write_sensor(gAppState.gSensorPacket, SENSOR_ID_RSSI, nnet::get_rssi(gAppState.gWifiManager));
                nnet::send(gAppState.gTcpClient, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
            }
#endif
        }

        ntimer::periodic_task_t periodic_process_rd03d;

        void presetup(state_t* state)
        {
            // Initialize RD03D sensor with rx and tx pin
            nsensors::nrd03d::begin(gAppState.gRd03dSensor, 20, 21);

            ntimer::init_periodic_task(&periodic_process_rd03d, 100, process_rd03d, nullptr);
        }

        void setup(state_t* state)
        {
            nnet::init_wifi_config(gAppState.gWifiConfig, WIFI_SSID(), WIFI_PASSWORD());
            nnet::setup(gAppState.gWifiManager, &gAppState.gWifiConfig);

            void* socket = nnet::setup_default(&gAppState.gTcpConfig);
            nnet::setup(gAppState.gTcpClient, &gAppState.gTcpConfig, socket, SENSOR_SERVER_IP(), SENSOR_SERVER_TCPPORT());
        }

        void tick(state_t* state) 
        { 
            const u64 now_ms = ntimer::millis();

            ntimer::tick_periodic_task(&periodic_process_rd03d, now_ms);

            nnet::tick_tcp_client(&gAppState.gWifiManager, gAppState.gTcpClient);
        }

    }  // namespace napp
}  // namespace ncore
