# XR Engine

First attempt of engine designed specifically for VR.

Objectives of this project:

- Learn OpenXR API
- Find a good architecture for XR, and the differences with "flat" engines

## Configure development environment

1. Clone this repository with the `--recurse-submodules` flag.
    - If you forgot the flag, you can run `git submodule update --init` instead.
2. Install Vulkan SDK. Make sure to have the latest version, since vk_mem_alloc is often up-to-date.
3. Install SDL2
4. Install a C++ compiler (on Linux, clang and ninja are recommended).

## Enable GDB pretty print

To enable GDB pretty print, add this line to your `~/.gdbinit` file:

```gdb
source <repo_dir>/gdb_formatter.py
```

where you replace `<repo_dir>` by the directory where you cloned this repository.

This will allow GDB to use custom pretty printers for types.

## Build

On Linux:

- `cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -GNinja`
- `ninja -C build tests` to build tests. If `tests` is not specified, only the main library is built.
- `cd ./build/tests`
- Run one of the test executables, e.g `./core-deferred`
