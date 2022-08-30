#include "xr_engine/utils/global_utils.h"

namespace xre {
    void check(bool result, const std::string &error_message)
    {
        if (!result)
        {
            std::cerr << "[Error] " << error_message << " Aborting.\n";
            // Completely halt the program
            // TODO maybe recoverable ?
            exit(1);
        }
    }
}