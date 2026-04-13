#include "humanpresence/c_humanpresence.h"

#include "rcore/c_app.h"
#include "rwifi/c_wifi.h"
#include "rwifi/c_node.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_packet.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"
#include "rcore/c_task.h"

#include "rsensors/c_hmmd.h"

namespace ncore
{
    struct state_app_t
    {
        u64               gLastSensorReadTimeInMillis = 0;
        npacket::packet_t gSensorPacket;                // Sensor packet for sending data
        u16               gSequence           = 0;      // Sequence number for the packet
        const u8          kVersion            = 1;      // Version number for the packet
        s16               gLastDistanceInCm   = 32768;  // Last distance value read from the sensor
        s8                gLastPresence       = 0;      // Last presence value read from the sensor
        u64               gLastPresenceStream = 0;      // Last presence value read from the sensor
        s8                gLastPresence0      = 64;
        s8                gLastPresence1      = 0;
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        ntask::result_t func_read(state_t* state)
        {
            // Read the HMMD sensor data
            s8  presence     = gAppState.gLastPresence;
            u16 distanceInCm = gAppState.gLastDistanceInCm;
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

                    npacket::sensor_block_t sensors;
                    sensors.begin(&gAppState.gSensorPacket);

                    npacket::sensor_value_t presenceSensor = {npacket::nsensorid::ID_PRESENCE1, (u16)presence};
                    sensors.write(&gAppState.gSensorPacket, presenceSensor);

                    if (distanceInCm > 0 && distanceInCm < 3200)
                    {
                        gAppState.gLastDistanceInCm = distanceInCm;
                        npacket::sensor_value_t distanceSensor = {npacket::nsensorid::ID_DISTANCE1, distanceInCm};
                        sensors.write(&gAppState.gSensorPacket, distanceSensor);
                    }

                    if (sensors.finalize() > 0)
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
                        npacket::packet_set_mac(gAppState.gSensorPacket, state->MACAddress);
                        nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
                    }
                }
            }
            return ntask::RESULT_OK;
        }

        ntask::periodic_t gReadPeriodic(100);  // Every 100 ms
        void              main_program(ntask::scheduler_t* exec, state_t* state)
        {
            if (ntask::is_first_call(exec))
            {
                ntask::init_periodic(exec, gReadPeriodic);
            }
            else
            {
                if (ntask::periodic(exec, gReadPeriodic))
                {
                    ntask::call(exec, func_read);
                }
            }
        }
        ntask::program_t gMainProgram(main_program);
        state_task_t     gAppTask;

        void presetup(state_t* state)
        {
            // This is where you would set up your hardware, peripherals, etc.
        }

        void setup(state_t* state)
        {
            // Initialize the sensors
            const u8 rx = 15;            // RX pin for HMMD
            const u8 tx = 16;            // TX pin for HMMD
            nsensors::initHMMD(rx, tx);  // Initialize the HMMD sensor

            ntask::set_main(state, &gAppTask, &gMainProgram);
            nnode::initialize(state, &gAppTask);

            nlog::println("Setup done...");
        }

        void tick(state_t* state) { ntask::tick(state, &gAppTask); }

    }  // namespace napp
}  // namespace ncore
