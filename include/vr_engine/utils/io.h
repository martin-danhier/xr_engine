#pragma once

#include <cstddef>

namespace vre {
    void *load_binary_file(const char *path, size_t *size);
}