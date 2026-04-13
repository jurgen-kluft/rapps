#include "cube/c_cube.h"

#include "rcore/c_gpio.h"
#include "rwifi/c_node.h"
#include "rwifi/c_wifi.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_packet.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"
#include "rcore/c_task.h"

#include "lib_sc7a20h/c_sc7a20h.h"

namespace ncore
{
    struct state_app_t
    {
        npacket::packet_t gSensorPacket;  // Sensor packet for sending data
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        ntask::result_t process(state_t* state)
        {
            return ntask::RESULT_OK;
        }

        ntask::periodic_t periodic_process(100);

        void main_program(ntask::scheduler_t* exec, state_t* state)
        {
            if (ntask::is_first_call(exec))
            {
                ntask::init_periodic(exec, periodic_process);
            }

            // Process sensor data
        }
        ntask::program_t gMainProgram(main_program);

        state_task_t gAppTask;

        void presetup(state_t* state)
        {
            nsensors::initSC7A20H();            // Initialize sensor
        }

        void setup(state_t* state)
        {
            ntask::set_main(state, &gAppTask, &gMainProgram);
            nnode::initialize(state, &gAppTask);
        }

        void tick(state_t* state) { ntask::tick(state, &gAppTask); }

    }  // namespace napp
}  // namespace ncore
