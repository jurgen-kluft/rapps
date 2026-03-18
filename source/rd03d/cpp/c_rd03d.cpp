#include "rd03d/c_rd03d.h"

#include "rcore/c_gpio.h"
#include "rwifi/c_node.h"
#include "rwifi/c_wifi.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_packet.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"
#include "rcore/c_task.h"

#include "rsensors/c_rd03d.h"

#define ENABLE_RD03D

namespace ncore
{
    struct rd03d_data_t
    {
        u64 DetectionBits[3];
        u8  Detected[3];
        u8  LastSendDetected[3];

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
        npacket::sensorpacket_t gSensorPacket;  // Sensor packet for sending data
        rd03d_data_t            gCurrentRd03d;
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        ntask::result_t process_rd03d(state_t* state)
        {
#ifdef ENABLE_RD03D
            if (nsensors::nrd03d::update())
            {
                for (s8 i = 0; i < 3; ++i)
                {
                    nsensors::nrd03d::target_t tgt;
                    if (nsensors::nrd03d::getTarget(i, tgt))
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
                gAppState.gSensorPacket.begin(state->MACAddress);

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
                        gAppState.gSensorPacket.write(npacket::nsensorid::ID_PRESENCE1 + i, detected);
                    }
                }

                if (gAppState.gSensorPacket.count() > 0)
                {
                    gAppState.gSensorPacket.write(npacket::nsensorid::ID_RSSI, nwifi::get_RSSI(state) & 0xFFFF);
                    gAppState.gSensorPacket.finalize();

                    nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
                }
            }
#endif
            return ntask::RESULT_OK;
        }

        ntask::periodic_t periodic_process_rd03d(100);

        void main_program(ntask::scheduler_t* exec, state_t* state)
        {
            if (ntask::is_first_call(exec))
            {
                ntask::init_periodic(exec, periodic_process_rd03d);
            }

            // Process sensor data
#ifdef ENABLE_RD03D
            if (ntask::periodic(exec, periodic_process_rd03d))
            {
                ntask::call(exec, process_rd03d);
            }
#endif
        }
        ntask::program_t gMainProgram(main_program);

        state_task_t gAppTask;

        void presetup(state_t* state)
        {
            // Initialize RD03D sensor with rx and tx pin
            nsensors::nrd03d::begin(20, 21);
        }

        void setup(state_t* state)
        {
            ntask::set_main(state, &gAppTask, &gMainProgram);
            nnode::initialize(state, &gAppTask);
        }

        void tick(state_t* state) { ntask::tick(state, &gAppTask); }

    }  // namespace napp
}  // namespace ncore
