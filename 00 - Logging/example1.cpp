#include <iostream>
#include <format>

#include "log.hpp"

namespace logging = squawkbus::logging;

// LOGGER_FORMAT="{time:%Y-%m-%d %X} {level:9} {name:6} {function} ({file}, {line:3}) {message}" ./example1

int main()
{
    // LOGGER_LEVEL=TRACE ./example1
    logging::critical("A critical message");
    logging::error("A error message");
    logging::warning("A warning message");
    logging::info("A info message");
    logging::debug("A debug message");
    logging::trace("A trace message");

    // LOGGER_LEVEL_foo=TRACE ./example1
    auto log = logging::logger("foo");
    log.critical("A critical message");
    log.error("A error message");
    log.warning("A warning message");
    log.info("A info message");
    log.debug("A debug message");
    log.trace("A trace message");

    return 0;
}