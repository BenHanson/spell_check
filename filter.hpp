#pragma once

#include "types.hpp"

#include <lexertl/state_machine.hpp>

#include <string>

lexertl::state_machine build_filter_lexer(const char* pathname,
    config_parser& config);
