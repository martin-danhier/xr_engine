#include "xr_engine/core/global.h"

#include <algorithm>
#include <iostream>

namespace xre
{

    std::ostream &operator<<(std::ostream &os, const Version &version)
    {
        os << std::to_string(version.major) << "." << std::to_string(version.minor) << "." << std::to_string(version.patch);
        return os;
    }
} // namespace xre