#pragma once

#include <lexertl/iterator.hpp>
#include <lexertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/match_results.hpp>
#include <parsertl/state_machine.hpp>
#include <parsertl/token.hpp>

#include <cstdint>
#include <map>
#include <string>

using token = parsertl::token<lexertl::criterator>;

struct config_state
{
    lexertl::rules _lrules;
    parsertl::match_results _results;
    token::token_vector _productions;

    lexertl::state_machine parse(const std::string& config_pathname);
};

struct config_parser;

using config_actions_map = std::map<uint16_t, void(*)(config_state& state,
    const config_parser& parser)>;

struct config_parser
{
    parsertl::state_machine _gsm;
    lexertl::state_machine _lsm;
    config_actions_map _actions;
};
