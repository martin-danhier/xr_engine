# Config inspired by https://github.com/krux02/minimal_cmake_example

cmake_minimum_required(VERSION 3.16)
project(railguard)

##################################################################
###                          SETTINGS                          ###
##################################################################

# Add settings flags here
set(settings
        # Window framework: one of {WINDOW_SDL2}
        WINDOW_SDL2

        # Rendering framework: one of {RENDERER_VULKAN}
        RENDERER_VULKAN

        # XR framework: one of {XR_OPENXR}
        XR_OPENXR
        )

# Enable/disable sanitizers
set(enable_asan 0)
set(enable_ubsan 0)

# Enable/disable interactivity
# Setting this to 0 will add timers to ensure that no test is blocked in a loop, waiting for user input.
# Useful for CI, where the value is always overridden to 0 automatically.
set(interactive 1)

# Define C and C++ versions
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Shaders location
set(shaders_directory "${PROJECT_SOURCE_DIR}/tests/resources/shaders")
set(spirv_directory "${CMAKE_BINARY_DIR}/tests/resources/shaders")

# Enable tests
enable_testing()

##################################################################
###                          SOURCES                           ###
##################################################################

# Add source files for main lib
set(railguard_lib_sources

        )

# Add header directories for main lib
set(railguard_lib_header_dir
        include
        )

# Test resources
set(test_resources

        )

##################################################################
###                       APPLY SETTINGS                       ###
##################################################################

# Don't change this directly

# Debug vs Release
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(
            DEBUG
    )
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(
            NDEBUG
    )
endif ()

# Add dependencies flags
foreach (flag ${settings})
    add_compile_definitions(${flag})
    set(${flag} "true")
endforeach ()

# Vulkan specific
if (DEFINED RENDERER_VULKAN)
    add_compile_definitions(VK_NO_PROTOTYPES)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(
                USE_VK_VALIDATION_LAYERS
        )
    endif ()
endif ()

# On a CI, override the value of interactive with 0
# We know that we are on a CI if the CI environment variable is set
if (DEFINED ENV{CI})
    message(STATUS "Continuous integration detected, disabling interactive mode")
    set(interactive 0)
    # Also disable sanitizers
    set(enable_asan 0)
    set(enable_ubsan 0)
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
    set(is_msvc 1)
else ()
    set(is_msvc 0)
endif ()
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" AND WIN32)
    set(is_mingw 1)
else ()
    set(is_mingw 0)
endif ()

# Enable sanitizers if variable is 1
if (${enable_asan} STREQUAL "1" AND NOT ${is_mingw})
    message(STATUS "Enabling address sanitizer")

    # MSVC syntax is a bit different
    if (${is_msvc})
        set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    else ()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    endif ()
endif ()

if (${enable_ubsan} STREQUAL "1" AND NOT ${is_msvc} AND NOT ${is_mingw})
    message(STATUS "Enabling undefined behavior sanitizer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif ()

# Enable or not interactivity
if (${interactive} STREQUAL "1")
    add_compile_definitions(
            INTERACTIVE
    )
else ()
    add_compile_definitions(
            NO_INTERACTIVE
    )
endif ()

##################################################################
###                         CREATE LIB                         ###
##################################################################

# Create main lib
add_library(railguard_lib STATIC ${railguard_lib_sources})
target_sources(railguard_lib PRIVATE ${railguard_lib_sources})
target_include_directories(railguard_lib PUBLIC ${railguard_lib_header_dir})

##################################################################
###                         DEPENDENCIES                       ###
##################################################################

# Find Vulkan if needed
if (DEFINED RENDERER_VULKAN)
    find_package(Vulkan REQUIRED)
endif ()

# Load external script
add_subdirectory(external)

# Link dependencies
if (${interactive} STREQUAL "0")
    # Link pthread on linux
    if (UNIX)
        target_link_libraries(railguard_lib PUBLIC pthread)
    endif ()
endif ()

if (DEFINED RENDERER_VULKAN)
    target_link_libraries(railguard_lib PUBLIC
            Vulkan::Vulkan
            volk
            )
endif ()

if (DEFINED WINDOW_SDL2)
    target_link_libraries(railguard_lib PUBLIC
            sdl2
            )
endif ()

##################################################################
###                           TESTING                          ###
##################################################################

# Macro inspired by https://bertvandenbroucke.netlify.app/2019/12/12/unit-testing-with-ctest/

# Add a new unit test
# A new target with the test sources is constructed, and a CTest test with the
# same name is created. The new test is also added to the global list of test
# contained in the check target
macro(add_unit_test)
    # Define macro arguments
    set(options PARALLEL)
    set(oneValueArgs FILE)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "" ${ARGN})

    # Remove extension of TEST_FILE.
    set(TEST_NAME ${TEST_FILE})
    string(REGEX REPLACE ".cpp$" "" TEST_NAME ${TEST_NAME})
    # Also replace \ by /
    string(REGEX REPLACE "/" "-" TEST_NAME ${TEST_NAME})

    # Compile test
    message(STATUS "Generating test \"${TEST_NAME}\"")
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL tests/${TEST_FILE})
    target_sources(${TEST_NAME} PRIVATE tests/${TEST_FILE})

    # Set directory for executable
    set_target_properties(${TEST_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
            COMPILE_DEFINITIONS UNIT_TESTS
            )

    # Link project lib and testing framework
    target_link_libraries(${TEST_NAME} railguard_lib testing_framework)

    # Register test
    add_test(NAME ${TEST_NAME}
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/tests
            COMMAND ${TEST_NAME})

    # Add test to list
    set(TEST_NAMES ${TEST_NAMES} ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES
            ENVIRONMENT "TEST_FILE=tests/${TEST_FILE};TEST_LINE=0"
            )
endmacro(add_unit_test)

# Get all c++ files in the tests directory, recursively
file(GLOB_RECURSE test_files
        "tests/*.cpp"
        )

# For each one, add it
foreach (test_file ${test_files})
    # Get local path (remove prefix project source)
    string(REGEX REPLACE "^${PROJECT_SOURCE_DIR}/tests/" "" test_file ${test_file})

    add_unit_test(FILE ${test_file})
endforeach (test_file)

# Save target
add_custom_target(
        tests
        DEPENDS ${TEST_NAMES}
)

# Copy test resources
foreach (test_resource ${test_resources})
    # Get local path (remove prefix project source)
    get_filename_component(file_path ${test_resource} PATH)
    file(COPY ${test_resource} DESTINATION ${PROJECT_BINARY_DIR}/${file_path})
endforeach ()

##################################################################
###                     SHADER COMPILATION                     ###
##################################################################

# Shaders need to be compiled if the rendering engine is Vulkan
if (DEFINED RENDERER_VULKAN)

    # Get compiler
    find_program(glsl_validator glslangValidator HINTS
            /usr/bin
            /usr/local/bin
            $ENV{VULKAN_SDK}/Bin/
            $ENV{VULKAN_SDK}/Bin32/
            ${PROJECT_SOURCE_DIR}/glslangValidator/bin/ # For GitHub Actions
            )

    # Error if not found
    if (glsl_validator STREQUAL "")
        message(FATAL_ERROR "Could not find glslangValidator")
    else()
        message(STATUS "Using glslangValidator: ${glsl_validator}")
    endif()

    # Find shader files
    file(GLOB_RECURSE glsl_source_files
            "${shaders_directory}/*.frag"
            "${shaders_directory}/*.vert"
            "${shaders_directory}/*.comp"
            )

    # Iterate each shader
    foreach(glsl ${glsl_source_files})
        get_filename_component(file_name ${glsl} NAME)

        # Get relative path of shader in shaders directory
        # Then place the spv file at the same location relatively
        get_filename_component(file_path ${glsl} PATH)

        string(REGEX REPLACE "^${shaders_directory}/" "" file_path ${file_path})
        message(STATUS "Registering shader \"${file_path}/${file_name}\"")

        set(spirv "${spirv_directory}/${file_path}/${file_name}.spv")

        # Execute glslang command to compile that specific shader
        add_custom_command(
                OUTPUT ${spirv}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${spirv_directory}/${file_path}"
                COMMAND ${glsl_validator} -V ${glsl} -o ${spirv}
                DEPENDS ${glsl})
        list(APPEND spirv_binary_files ${spirv})
    endforeach(glsl)

    # Link it to the main lib
    add_custom_target(shaders DEPENDS ${spirv_binary_files})
    add_dependencies(railguard_lib shaders)
endif()