#include "xr_engine/core/global.h"

#include <algorithm>
#include <iostream>

namespace xre
{

    std::ostream &operator<<(std::ostream &os, const Version &version)
    {
#ifdef _MSC_VER
        os << version.major << "." << version.minor << "." << version.patch;
#else
        os << std::to_string(version.major) << "." << std::to_string(version.minor) << "." << std::to_string(version.patch);
#endif
        return os;
    }
} // namespace xre