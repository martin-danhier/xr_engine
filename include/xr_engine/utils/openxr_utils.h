#pragma once

#include <string>
#include <openxr/openxr.h>

namespace xre {
    struct Version;

    void xr_check(XrResult result, const std::string &error_message = "");
    Version make_version(const XrVersion &version);
}