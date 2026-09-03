#include "cube/c_cube.h"

#include "rcore/c_deepsleep.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"

#include "rwifi/c_wifi.h"

#include "lib_sc7a20h/c_sc7a20h.h"

#include "rhome/c_sensor.h"

#include "lib_sc7a20h/c_sc7a20h.h"

namespace ncore
{
    struct state_app_t
    {
        nnet::msg_t         gSensorPacket;  // Sensor packet for sending data
        nsensors::sc7a20h_t gSc7a20h;       // SC7A20H sensor instance
    };
}  // namespace ncore

namespace ncore
{
    state_app_t gAppState;
    namespace napp
    {
        ntimer::periodic_task_t periodic_process;

        const u8 SC7A20H_I2C_ADDRESS = 0x18;  // I2C address of the SC7A20H sensor
        const u8 SC7A20H_INT_PIN     = 4;     // GPIO pin for the SC7A20H interrupt

        void wakeup(state_t* state, nwakeup::reason_t reason)
        {
            nsensors::init(gAppState.gSc7a20h, SC7A20H_I2C_ADDRESS, SC7A20H_INT_PIN);

            if (reason == nwakeup::REASON_EXT0)
            {
                // Figure out what caused the wakeup and handle it accordingly
            }
        }

        void setup(state_t* state) {}

        void tick(state_t* state)
        {
            // ...
        }

    }  // namespace napp
}  // namespace ncore
