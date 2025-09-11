#include "types.hpp"
#include "spellc_error.hpp"

#include <lexertl/generator.hpp>
#include <lexertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/enums.hpp>
#include <parsertl/lookup.hpp>

#include <algorithm>
#include <exception>
#include <format>
#include <string>

extern config_parser g_config_parser;

lexertl::state_machine config_state::parse(const std::string& config_pathname)
{
    lexertl::state_machine ret;
    lexertl::memory_file mf;
    lexertl::citerator iter;
    lexertl::citerator end;

    mf.open(config_pathname.c_str());

    if (!mf.data())
        throw spellc_error(std::format("Unable to open {}", config_pathname));

    iter = lexertl::citerator(mf.data(), mf.data() + mf.size(),
        g_config_parser._lsm);
    _results.reset(iter->id, g_config_parser._gsm);

    while (_results.entry.action != parsertl::action::error &&
        _results.entry.action != parsertl::action::accept)
    {
        if (_results.entry.action == parsertl::action::reduce)
        {
            auto i = g_config_parser._actions.find(_results.entry.param);

            if (i != g_config_parser._actions.end())
            {
                try
                {
                    i->second(*this, g_config_parser);
                }
                catch (const std::exception& e)
                {
                    const auto idx =
                        _results.production_size(g_config_parser._gsm,
                            _results.entry.param) - 1;
                    const auto& token =
                        _results.dollar(idx, g_config_parser._gsm,
                            _productions);
                    const std::size_t line =
                        1 + std::count(mf.data(), token.first, '\n');

                    // Column makes no sense here as we are
                    // already at the end of the line
                    throw spellc_error(std::format("{}({}): {}",
                        config_pathname,
                        line,
                        e.what()));
                }
            }
        }

        parsertl::lookup(iter, g_config_parser._gsm, _results,
            _productions);
    }

    if (_results.entry.action == parsertl::action::error)
    {
        const std::size_t line =
            1 + std::count(mf.data(), iter->first, '\n');
        const char endl[] = { '\n' };
        const std::size_t column = iter->first - std::find_end(mf.data(),
            iter->first, endl, endl + 1);

        throw spellc_error(std::format("{}({}:{}): Parse error",
            config_pathname,
            line,
            column));
    }

    mf.close();

    //_lrules.push("(?s:.)", lexertl::rules::skip());
    lexertl::generator::build(_lrules, ret);
    return ret;
}
