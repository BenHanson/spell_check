#pragma once

#include <lexertl/state_machine.hpp>

#include <string>

lexertl::state_machine build_filter_lexer(const std::string& pathname);
