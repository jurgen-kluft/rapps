#include "humanpresence/c_humanpresence.h"

#include "rcore/c_app.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"

#include "rwifi/c_wifi.h"
#include "rwifi/c_wifi_mgr.h"
#include "rwifi/c_tcp_client.h"

#include "rhome/c_home.h"
#include "rhome/c_sensor.h"

#include "lib_hmmd/c_hmmd.h"

namespace ncore
{
    struct state_app_t
    {
        u64         gLastSensorReadTimeInMillis = 0;
        nnet::msg_t gSensorPacket;                // Sensor packet for sending data
        u16         gSequence           = 0;      // Sequence number for the packet
        const u8    kVersion            = 1;      // Version number for the packet
        s16         gLastDistanceInCm   = 32768;  // Last distance value read from the sensor
        s8          gLastPresence       = 0;      // Last presence value read from the sensor
        u64         gLastPresenceStream = 0;      // Last presence value read from the sensor
        s8          gLastPresence0      = 64;
        s8          gLastPresence1      = 0;

        nnet::wifi_config_t  gWifiConfig;   // WiFi configuration for connecting to the network
        nnet::wifi_manager_t gWifiManager;  // WiFi manager for handling WiFi connections

        nnet::config_t     gTcpClientConfig;  // TCP client configuration for connecting to the server
        nnet::tcp_client_t gTcpClient;        // TCP client for sending data to the server
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        void func_read(state_t* state)
        {
            // Read the HMMD sensor data
            i32 presence     = gAppState.gLastPresence;
            i32 distanceInCm = gAppState.gLastDistanceInCm;
            if (nsensors::readHMMD2(&presence, &distanceInCm))
            {
#if 0
            nlog::print("Read Presence: ");
            nlog::println(presence == 1 ? "On" : "Off");
            nlog::print("Read Distance: ");
            char  distanceStrBuffer[16];
            str_t distanceStr = str_mutable(distanceStrBuffer, 16);
            to_str(distanceStr, (s32)distanceInCm, 10);
            nlog::print(distanceStr.m_const);
            nlog::println(" cm");
#endif
                if ((gAppState.gLastPresenceStream & 0x8000000000000000) == 0)
                {
                    gAppState.gLastPresence0 -= 1;
                }
                else
                {
                    gAppState.gLastPresence1 -= 1;
                }

                if (presence != 0)
                {
                    gAppState.gLastPresenceStream = (gAppState.gLastPresenceStream << 1) | 1;
                    gAppState.gLastPresence1 += 1;
                }
                else
                {
                    gAppState.gLastPresenceStream = (gAppState.gLastPresenceStream << 1) | 0;
                    gAppState.gLastPresence0 += 1;
                }

                // Presence is true if in the stream of last 64 samples more than 56 samples were 'presence detected'
                presence = (gAppState.gLastPresence1 > 56) ? 1 : 0;

                if (presence != gAppState.gLastPresence)
                {
                    gAppState.gLastPresence = presence;

                    // nnet::sensor_value_t presenceSensor = {nnet::nsensorid::ID_PRESENCE1, (u16)presence};
                    // sensors.write(&gAppState.gSensorPacket, presenceSensor);
                    nnet::msg_init(gAppState.gSensorPacket, nnet::MSG_TYPE_SENSOR_DATA, nnet::get_mac_address(gAppState.gWifiManager));

                    if (distanceInCm > 0 && distanceInCm < 3200)
                    {
                        gAppState.gLastDistanceInCm = distanceInCm;
                        // nnet::sensor_value_t distanceSensor = {nnet::nsensorid::ID_DISTANCE1, distanceInCm};
                        // sensors.write(&gAppState.gSensorPacket, distanceSensor);
                        msg_write_sensor(gAppState.gSensorPacket, SENSOR_ID_DISTANCE1, distanceInCm);
                    }

                    {
#ifdef TARGET_DEBUG
                        nlog::print("Sending presence=");
                        nlog::print(presence == 1 ? "On" : "Off");
                        nlog::print(", distance=");
                        char  distanceStrBuffer[16];
                        str_t distanceStr = str_mutable(distanceStrBuffer, 16);
                        to_str(distanceStr, (s32)distanceInCm, 10);
                        nlog::print(distanceStr.m_const);
                        nlog::println(" cm");
#endif
                        nnet::send(gAppState.gTcpClient, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
                    }
                }
            }
        }

        void presetup(state_t* state)
        {
            // This is where you would set up your hardware, peripherals, etc.
        }

        ntimer::periodic_task_t gSensorReadTask;

        void setup(state_t* state)
        {
            // Initialize the sensors
            const u8 rx = 15;            // RX pin for HMMD
            const u8 tx = 16;            // TX pin for HMMD
            nsensors::initHMMD(rx, tx);  // Initialize the HMMD sensor

            ntimer::init_periodic_task(&gSensorReadTask, 100, func_read, nullptr);  // Set up a periodic task to read the sensor every 100 ms

            nnet::init_wifi_config(gAppState.gWifiConfig, WIFI_SSID(), WIFI_PASSWORD());
            nnet::activate(gAppState.gWifiManager);

            void* socket = nnet::setup_default(&gAppState.gTcpClientConfig);
            nnet::setup(gAppState.gTcpClient, &gAppState.gTcpClientConfig, socket, SENSOR_SERVER_IP(), SENSOR_SERVER_TCPPORT());
            nnet::connect(gAppState.gTcpClient);

            nlog::println("Setup done...");
        }

        void tick(state_t* state)
        {
            const u64 now_ms = ntimer::millis();

            nnet::tick_tcp_client(&gAppState.gWifiManager, gAppState.gTcpClient);
            ntimer::tick_periodic_task(&gSensorReadTask, now_ms);
        }

    }  // namespace napp
}  // namespace ncore
