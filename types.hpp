#pragma once

#include "pathnames.hpp"

#include <lexertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <lexertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/match_results.hpp>
#include <parsertl/state_machine.hpp>
#include <parsertl/token.hpp>

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using token = parsertl::token<lexertl::criterator>;

// Forward declare, defined below
struct config_parser;

struct config_state
{
    lexertl::rules _lrules;
    parsertl::match_results _results;
    token::token_vector _productions;

    lexertl::state_machine parse(const char* pathname,
        const config_parser& config);
};

using config_actions_map = std::map<uint16_t, void(*)(config_state& state,
    const config_parser& parser)>;

struct config_parser
{
    parsertl::state_machine _gsm;
    lexertl::state_machine _lsm;
    config_actions_map _actions;
};

using mf_vector = std::vector<lexertl::memory_file>;
using string_vector = std::vector<std::string>;
using sv_vector = std::vector<std::string_view>;

struct data_t
{
    directories _directories = directories::read;
    bool _no_messages = false;
    bool _icase = false;
    wildcards _include;
    wildcards _exclude_dirs;
    bool _follow_symlinks = false;
    config_parser _config_parser;
    mf_vector _dictionaries;
    sv_vector _dict_indexes;
    lexertl::state_machine _word_sm;
    lexertl::state_machine _filter_sm;
    pathnames _pathnames;
    static inline const char _wa_text[] = "\x1b[38;5;229m";

    void create(const std::span<const char*>& params);
    void process(void(*func)(const char* pathname, const data_t& data));

    bool include_dir(const std::string& path);
    bool include_file(const std::string& path);
};
