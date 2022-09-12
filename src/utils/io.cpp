#include "vr_engine/utils/io.h"

#include <cstdio>
#include <stdexcept>

namespace vre
{
    void *load_binary_file(const char *path, size_t *size)
    {
        // Open file
        FILE *file = fopen(path, "rb");
        if (!file)
        {
            throw std::runtime_error("Failed to open file \"" + std::string(path) + "\"");
        }

        // Get file size
        fseek(file, 0, SEEK_END);
        *size = ftell(file);

        // Go back to the beginning of the file
        rewind(file);

        // Allocate memory
        void *data = new char[*size];

        // Read file contents into buffer
        size_t read_count = fread(data, 1, *size, file);
        if (read_count != *size)
        {
            throw std::runtime_error("Failed to read file \"" + std::string(path) + "\"");
        }

        // Close file
        fclose(file);

        return data;
    }
} // namespace vre