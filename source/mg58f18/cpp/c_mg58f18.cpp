#include "mg58f18/c_mg58f18.h"

#include "rcore/c_gpio.h"
#include "rwifi/c_node.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_packet.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"
#include "rcore/c_task.h"

#include "rsensors/c_mg58f18.h"

namespace ncore
{
    struct state_app_t
    {
    };

    state_app_t gAppState;
}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        // the main program to execute sensor reading
        bool gLastDetected = false;
        void main_program(ntask::scheduler_t* exec, state_t* state)
        {
            if (ntask::is_first_call(exec)) {}

            bool detected = nsensors::nmg58f18::is_detecting(5);
            if (gLastDetected != detected)
            {
                gLastDetected = detected;
                nlog::println(detected ? "Presence!" : "No presence.");
            }
            else
            {
                ntimer::delay(50);
            }
        }
        ntask::program_t gMainProgram(main_program);
        state_task_t     gAppTask;

        void presetup(state_t* state) 
        { 
            nsensors::nmg58f18::initialize(20, 21, 5); 
        }

        void setup(state_t* state)
        {
            ntask::set_main(state, &gAppTask, &gMainProgram);
            nnode::initialize(state, &gAppTask);
        }

        void tick(state_t* state) { ntask::tick(state, &gAppTask); }

    }  // namespace napp
}  // namespace ncore
