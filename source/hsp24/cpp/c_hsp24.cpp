#include "hsp24/c_hsp24.h"

#include "rcore/c_app.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"

#include "rwifi/c_wifi.h"

#include "rhome/c_home.h"
#include "rhome/c_sensor.h"

#include "lib_hsp24/c_hsp24.h"

#define ENABLE_HSP24

namespace ncore
{
    struct hsp24_data_t
    {
        u64 DetectionBits;
        u8  Detected;
        u8  LastSendDetected;

        void reset()
        {
            DetectionBits    = 0;
            Detected         = 4;  // Unknown state
            LastSendDetected = 8;  // Impossible state
        }
    };

    struct state_app_t
    {
        nnet::msg_t                gSensorPacket;  // Sensor packet for sending data
        hsp24_data_t               gCurrentHsp24;
        nsensors::nseeed::hsp24_t* gSensor;
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        void read_presence(state_t* state)
        {
#ifdef ENABLE_HSP24
            nsensors::nseeed::RadarStatus status;
            if (nsensors::nseeed::getStatus(gAppState.gSensor, status) == nsensors::nseeed::Success)
            {
                const u64 detectionBit                = isTargetDetected(status.targetStatus) ? 1 : 0;
                gAppState.gCurrentHsp24.DetectionBits = (gAppState.gCurrentHsp24.DetectionBits << 1) | detectionBit;

                // Draw a graph, going from 0 to 1 (up-flank), then from 1 to 3 which means up stays up, then when
                // going from 3 to 1 (down-flank) and finally from 1 to 0.

                //  PRESENCE                    3---------------------------|
                //                              |                           |
                //  PRESENCE               1----|                           1---|
                //                         |                                    |
                //  ABSENCE      0 --------|                                    0 ---------
                //

                u8         detected = gAppState.gCurrentHsp24.Detected;
                const bool dseen    = (gAppState.gCurrentHsp24.DetectionBits != 0);
                if (dseen)
                {
                    // Too transition from no-presence to presence we must have seen 3 detections in a row (300 ms)
                    detected = ((detected << 1) | 1);
                }
                else
                {
                    const bool dnone = gAppState.gCurrentHsp24.DetectionBits == 0;
                    if (dnone)
                    {
                        // To transition from presence to no-presence we must have seen 32 no-detections in a row (~3 seconds)
                        detected = ((detected << 1) | 0);
                    }
                }
                gAppState.gCurrentHsp24.Detected = detected;

                if (detected == 0x80)
                {
                    nlog::printf("Status: PRESENCE 1 -> 0 (distance: %d)\n", va_t(status.detectionDistance));
                }
                else if (detected == 0x01)
                {
                    nlog::printf("Status: PRESENCE 0 -> 1 (distance: %d)\n", va_t(status.detectionDistance));
                }
                else if (detected != 0x0)
                {
                    nlog::printf("Status: PRESENCE (distance: %d)\n", va_t(status.detectionDistance));
                }
                else
                {
                    nlog::printf("Status: ABSENCE\n");
                }
            }
#endif
        }

        void send_presence(state_t* state)
        {
#ifdef ENABLE_HSP24
            u8 detected = gAppState.gCurrentHsp24.Detected;
            if (detected == 0x80)
                detected = 2;
            else if (detected == 0x01)
                detected = 1;
            else if (detected != 0x0)
                detected = 3;

            if (gAppState.gCurrentHsp24.LastSendDetected != detected)
            {
                gAppState.gCurrentHsp24.LastSendDetected = detected;

                // Write a custom (binary-format) network message
                // gAppState.gSensorPacket.begin(state->MACAddress);
                // gAppState.gSensorPacket.write(nnet::nsensorid::ID_PRESENCE1, detected & 3);
                // gAppState.gSensorPacket.write(nnet::nsensorid::ID_RSSI, nwifi::get_RSSI(state));
                // gAppState.gSensorPacket.finalize();

                // nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
            }
#endif
        }

        ntimer::periodic_task_t periodic_read_presence; // (100);
        ntimer::periodic_task_t periodic_send_presence; // (50 + 3);

        void presetup(state_t* state)
        {
            // Initialize RD03D sensor with rx and tx pin
            gAppState.gSensor = nsensors::nseeed::create_hsp24(ncore::nserialx::reader(ncore::nserialx::SERIAL1));
            nserialx::begin(nserialx::SERIAL1, nbaud::Rate9600, nconfig::MODE_8N1, 4, 5);
        }

        void setup(state_t* state)
        {
            // ...
        }

        void tick(state_t* state) 
        { 
            // ...
        }

    }  // namespace napp
}  // namespace ncore
