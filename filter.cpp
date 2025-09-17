#include "filter.hpp"
#include "types.hpp"

#include <lexertl/generator.hpp>
#include <lexertl/memory_file.hpp>
#include <lexertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/generator.hpp>
#include <parsertl/rules.hpp>

#include <cstdlib>
#include <format>
#include <iostream>
#include <string>

static void build_config_parser(config_parser& config)
{
    parsertl::rules grules;
    lexertl::rules lrules;

    grules.token("Charset ExitState Macro MacroName NL Number Repeat "
        "StartState String");

    grules.push("start", "file");
    grules.push("file", "rx_macros '%%' rx_rules '%%'");

    // Token regex macros
    grules.push("rx_macros", "%empty");
    config._actions[grules.push("rx_macros",
        "rx_macros MacroName regex")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string name = token.str();
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();

            state._lrules.insert_macro(name.c_str(), regex.c_str());
        };

    // Tokens
    grules.push("rx_rules", "%empty");
    config._actions[grules.push("rx_rules", "rx_rules regex Number")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = token.str();
            const std::string number = state._results.dollar(2, parser._gsm,
                state._productions).str();

            state._lrules.push(regex, atoi(number.c_str()) & 0xffff);
        };
    config._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);

            state._lrules.push(std::string(start_state.first + 1,
                start_state.second - 1).c_str(), regex,
                std::string(exit_state.first + 1,
                    exit_state.second - 1).c_str());
        };
    config._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState Number")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);
            const std::string number = state._results.dollar(4, parser._gsm,
                state._productions).str();

            state._lrules.push(std::string(start_state.first + 1,
                start_state.second - 1).c_str(),
                regex, atoi(number.c_str()) & 0xffff,
                std::string(exit_state.first + 1,
                    exit_state.second - 1).c_str());
        };
    config._actions[grules.push("rx_rules",
        "rx_rules regex 'skip()'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& token = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = token.str();

            state._lrules.push(regex, lexertl::rules::skip());
        };
    config._actions[grules.push("rx_rules",
        "rx_rules StartState regex ExitState 'skip()'")] =
        [](config_state& state, const config_parser& parser)
        {
            const auto& start_state = state._results.dollar(1, parser._gsm,
                state._productions);
            const std::string regex = state._results.dollar(2, parser._gsm,
                state._productions).str();
            const auto& exit_state = state._results.dollar(3, parser._gsm,
                state._productions);

            state._lrules.push(std::string(start_state.first + 1,
                start_state.second - 1).c_str(),
                regex, lexertl::rules::skip(),
                std::string(exit_state.first + 1,
                    exit_state.second - 1).c_str());
        };

    // Regex
    grules.push("regex", "rx "
        "| '^' rx "
        "| rx '$' "
        "| '^' rx '$'");
    grules.push("rx", "sequence "
        "| rx '|' sequence");
    grules.push("sequence", "item "
        "| sequence item");
    grules.push("item", "atom "
        "| atom repeat");
    grules.push("atom", "Charset "
        "| Macro "
        "| String "
        "| '(' rx ')'");
    grules.push("repeat", "'?' "
        "| '\?\?' "
        "| '*' "
        "| '*?' "
        "| '+' "
        "| '+?' "
        "| Repeat");

    std::string warnings;

    parsertl::generator::build(grules, config._gsm, &warnings);

    if (!warnings.empty())
    {
        std::cerr << std::format("Config parser warnings: {}", warnings);
    }

    lrules.push_state("REGEX");
    lrules.push_state("RULE");
    lrules.push_state("ID");
    lrules.insert_macro("c_comment", R"("/*"([^*]|\*+[^*/])*\*+\/)");
    lrules.insert_macro("control_char", "c[@A-Za-z]");
    lrules.insert_macro("hex", "x[0-9A-Fa-f]+");
    lrules.insert_macro("escape", R"(\\([^0-9cx]|\d{1,3}|{hex}|{control_char}))");
    lrules.insert_macro("macro_name", R"([A-Z_a-z][-\w]*)");
    lrules.insert_macro("nl", "\r?\n");
    lrules.insert_macro("posix_name", "alnum|alpha|blank|cntrl|digit|graph|"
        "lower|print|punct|space|upper|xdigit");
    lrules.insert_macro("posix", R"(\[:{posix_name}:\])");
    lrules.insert_macro("spc_tab", "[ \t]+");
    lrules.insert_macro("state_name", R"([A-Z_a-z]\w*)");

    lrules.push("INITIAL", "{spc_tab}", lexertl::rules::skip(), ".");

    lrules.push("INITIAL", "{c_comment}", lexertl::rules::skip(), ".");
    // Bison supports single line comments
    lrules.push("INITIAL", R"("//".*)", lexertl::rules::skip(), ".");
    lrules.push("ID", R"([1-9]\d*)", grules.token_id("Number"), ".");

    lrules.push("INITIAL,RULE", "%%", grules.token_id("'%%'"), "RULE");
    lrules.push("INITIAL", "{macro_name}", grules.token_id("MacroName"), "REGEX");
    lrules.push("INITIAL,REGEX", "{nl}", lexertl::rules::skip(), "INITIAL");

    lrules.push("REGEX", "{spc_tab}", lexertl::rules::skip(), ".");
    lrules.push("RULE", "^{spc_tab}({c_comment}({spc_tab}|{c_comment})*)?",
        lexertl::rules::skip(), ".");
    lrules.push("RULE", R"(^<(\*|{state_name}(,{state_name})*)>)",
        grules.token_id("StartState"), ".");
    lrules.push("REGEX,RULE", R"(\^)", grules.token_id("'^'"), ".");
    lrules.push("REGEX,RULE", R"(\$)", grules.token_id("'$'"), ".");
    lrules.push("REGEX,RULE", R"(\|)", grules.token_id("'|'"), ".");
    lrules.push("REGEX,RULE", R"(\((\?(-?[is])*:)?)", grules.token_id("'('"), ".");
    lrules.push("REGEX,RULE", R"(\))", grules.token_id("')'"), ".");
    lrules.push("REGEX,RULE", R"(\?)", grules.token_id("'?'"), ".");
    lrules.push("REGEX,RULE", R"(\?\?)", grules.token_id("'\?\?'"), ".");
    lrules.push("REGEX,RULE", R"(\*)", grules.token_id("'*'"), ".");
    lrules.push("REGEX,RULE", R"(\*\?)", grules.token_id("'*?'"), ".");
    lrules.push("REGEX,RULE", R"(\+)", grules.token_id("'+'"), ".");
    lrules.push("REGEX,RULE", R"(\+\?)", grules.token_id("'+?'"), ".");
    lrules.push("REGEX,RULE", R"({escape}|(\[^?({escape}|{posix}|[^\\\]])*\])|\S)",
        grules.token_id("Charset"), ".");
    lrules.push("REGEX,RULE", R"(\{{macro_name}\})", grules.token_id("Macro"), ".");
    lrules.push("REGEX,RULE", R"(\{\d+(,(\d+)?)?\}[?]?)",
        grules.token_id("Repeat"), ".");
    lrules.push("REGEX,RULE", R"(\"(\\.|[^\\\r\n"])*\")",
        grules.token_id("String"), ".");

    lrules.push("RULE,ID", "{spc_tab}({c_comment}({spc_tab}|{c_comment})*)?",
        lexertl::rules::skip(), "ID");
    lrules.push("RULE", R"(<([.<]|{state_name}|>{state_name}(:{state_name})?)>)",
        grules.token_id("ExitState"), "ID");
    lrules.push("RULE,ID", "{nl}", lexertl::rules::skip(), "RULE");
    lrules.push("ID", R"(skip\s*\(\s*\))", grules.token_id("'skip()'"), "RULE");
    lexertl::generator::build(lrules, config._lsm);
}

lexertl::state_machine build_filter_lexer(const char* pathname,
    config_parser& config)
{
    lexertl::memory_file mf(pathname);
    config_state  cfg;

    build_config_parser(config);
    return cfg.parse(pathname, config);
}
