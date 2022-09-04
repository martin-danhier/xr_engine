#include "vr_engine/core/engine.h"

#include <test_framework/test_framework.hpp>
#include <vr_engine/core/global.h>

using namespace vre;

TEST
{
    Settings settings {
        ApplicationInfo {
            "Test Application",
            {0, 1, 0},
        },
    };

    Engine engine;

    ASSERT_NO_THROWS(engine = Engine(settings));

    engine.run_main_loop();
}