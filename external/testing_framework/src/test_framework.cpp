/**
 * @file Simple lightweight testing framework for C or C++ projects, inspired by Google Tests API. Extension for C++.
 * @author Martin Danhier
 */

#include "test_framework/test_framework.hpp"

#include <exception>
#include <string>

bool tf_assert_throws(tf_context *context, size_t line_number, const char *file, const tf_callback& fn, bool recoverable)
{
    // Run the test
    bool caught = false;
    try
    {
        fn();
    }
    catch (std::exception &e)
    {
        caught = true;
    }

    if (!caught)
    {
        // Equivalent in C++
        std::string message = recoverable ? "Condition" : "Assertion";
        message += " failed. Expected the given function to throw an exception.";
        message += recoverable ? "" : " Unable to continue execution.";

        return tf_assert_common(context, line_number, file, false, tf_dynamic_msg(message.c_str()), recoverable);
    }
    return true;
}

bool tf_assert_no_throws(tf_context *context, size_t line_number, const char *file, const tf_callback& fn, bool recoverable)
{
    // Run the test
    try
    {
        fn();
    }
    catch (std::exception &e)
    {
        // Equivalent in C++
        std::string message = recoverable ? "Condition" : "Assertion";
        message += " failed. Caught unexpected exception: \"";
        message += e.what();
        message += "\".";
        message += recoverable ? "" : " Unable to continue execution.";

        return tf_assert_common(context, line_number, file, false, tf_dynamic_msg(message.c_str()), recoverable);
    }
    return true;
}