#include "filter.hpp"
#include "output.hpp"
#include "types.hpp"
#include "pathnames.hpp"
#include "spellc_error.hpp"
#include "utils.hpp"

#include <lexertl/generator.hpp>
#include <lexertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <lexertl/rules.hpp>
#include <lexertl/state_machine.hpp>
#include <parsertl/enums.hpp>
#include <parsertl/lookup.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static std::vector<std::string> split(const char* str, const char c)
{
    std::vector<std::string> ret;
    const char* first = str;
    std::size_t count = 0;

    for (; *str; ++str)
    {
        if (*str == c)
        {
            count = str - first;

            if (count > 0)
                ret.emplace_back(first, count);

            first = str + 1;
        }
    }

    count = str - first;

    if (count > 0)
        ret.emplace_back(first, count);

    return ret;
}

static lexertl::state_machine build_word_lexer(const char* word_rx)
{
    lexertl::rules rules;
    lexertl::state_machine ret;

    rules.push(word_rx, 1);
    rules.push("(?s:.)", lexertl::rules::skip());
    lexertl::generator::build(rules, ret);
    return ret;
}

static void add_pathname(const char* first, const char* second, wildcards& wcs)
{
    const std::string pathname(*first == '!' ? first + 1 : first, second);

    if (*first == '!')
    {
        // Not using emplace_back() for compatibility with Macintosh
        wcs._negative.push_back({ wildcardtl::wildcard
            { pathname, is_windows() },
            // If pathname does not include a wildcard
            // store it as a plain string for error reporting.
            pathname.find_first_of("*?[") == std::string::npos ?
            pathname :
            std::string() });
    }
    else
    {
        // Not using emplace_back() for compatibility with Macintosh
        wcs._positive.push_back({ wildcardtl::wildcard
            { pathname, is_windows() },
            // If pathname does not include a wildcard
            // store it as a plain string for error reporting.
            pathname.find_first_of("*?[") == std::string::npos ?
            pathname :
            std::string() });
    }
}

[[nodiscard]] static bool build_indexes(const mf_vector& dictionaries,
    sv_vector& dict_indexes)
{
    using namespace lexertl;
    bool icase = true;
    std::size_t count = 0;
    rules index_rules;
    state_machine sm;
    enum class token { lower = 1, upper };

    index_rules.push("[a-z]([-']?[a-z])*", *token::lower);
    index_rules.push("[A-Z]([-']?[A-Za-z])*", *token::upper);
    index_rules.push(R"(\s+)", lexertl::rules::skip());
    generator::build(index_rules, sm);

    for (const auto& dict : dictionaries)
    {
        count += std::count(dict.data(), dict.data() + dict.size(), '\n');
    }

    dict_indexes.reserve(count);

    for (const auto& dict : dictionaries)
    {
        lexertl::citerator iter(dict.data(), dict.data() + dict.size(), sm);

        for (; iter->id; ++iter)
        {
            using namespace lexertl;

            switch (iter->id)
            {
            case +token::lower:
                // As expected
                break;
            case +token::upper:
                // Seen capital letters, so make case sensitive
                icase = false;
                break;
            default:
                throw spellc_error(std::format("Unexpected char '{}' "
                    "in dictionaries", *iter->first));
                break;
            }

            dict_indexes.push_back(iter->view());
        }
    }

    std::ranges::sort(dict_indexes);
    return icase;
}

lexertl::state_machine config_state::parse(const char* pathname,
    const config_parser& config)
{
    lexertl::state_machine ret;
    lexertl::memory_file mf;
    lexertl::citerator iter;
    lexertl::citerator end;

    mf.open(pathname);

    if (!mf.data())
        throw spellc_error(std::format("Unable to open {}", pathname));

    iter = lexertl::citerator(mf.data(), mf.data() + mf.size(),
        config._lsm);
    _results.reset(iter->id, config._gsm);

    while (_results.entry.action != parsertl::action::error &&
        _results.entry.action != parsertl::action::accept)
    {
        if (_results.entry.action == parsertl::action::reduce)
        {
            auto i = config._actions.find(_results.entry.param);

            if (i != config._actions.end())
            {
                try
                {
                    i->second(*this, config);
                }
                catch (const std::exception& e)
                {
                    const auto idx =
                        _results.production_size(config._gsm,
                            _results.entry.param) - 1;
                    const auto& token =
                        _results.dollar(idx, config._gsm,
                            _productions);
                    const std::size_t line =
                        1 + std::count(mf.data(), token.first, '\n');

                    // Column makes no sense here as we are
                    // already at the end of the line
                    throw spellc_error(std::format("{}({}): {}",
                        pathname,
                        line,
                        e.what()));
                }
            }
        }

        parsertl::lookup(iter, config._gsm, _results,
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
            pathname,
            line,
            column));
    }

    mf.close();

    //_lrules.push("(?s:.)", lexertl::rules::skip());
    lexertl::generator::build(_lrules, ret);
    return ret;
}

void data_t::create(const std::span<const char*>& params)
{
    string_vector dictionary_pathnames;
    std::string filter_pathname;
    const char* word_rx = nullptr;
    string_vector pns;

    for (std::size_t i = 1, size = params.size(); i < size; ++i)
    {
        const std::string_view param(params[i]);

        if (param[0] == '-')
        {
            if (param == "-d" || param == "--dictionary")
            {
                ++i;

                if (i == size)
                    throw spellc_error("--dictionary is not "
                        "followed by pathname");

                // Dictionary to load
                dictionary_pathnames.emplace_back(params[i]);
            }
            else if (param == "--exclude")
            {
                ++i;

                if (i == size)
                    throw spellc_error("--exclude is not "
                        "followed by pathname");

                const std::string_view value(params[i]);
                const char* first = value.data();
                const char* end = nullptr;
                const char* second = first + value.size();

                do
                {
                    const auto idx = value.find_first_of(';', first - value.data());

                    end = idx == std::string_view::npos ?
                        second :
                        value.data() + idx;
                    add_pathname(first, end, _pathnames._exclude);

                    if (end != second)
                        ++end;

                    first = end;
                } while (end != second);
            }
            else if (param == "--exclude-dir")
            {
                ++i;

                if (i == size)
                    throw spellc_error("--exclude-dir is not "
                        "followed by pathname");

                auto pathnames = split(params[i], ';');

                for (auto& p : pathnames)
                {
                    namespace fs = std::filesystem;

                    while (!p.empty() && p.ends_with(fs::path::preferred_separator))
                        p.pop_back();

                    if (p[0] == '!')
                    {
                        const std::string pathname(&p[1], std::to_address(p.end()));

                        // Not using emplace_back() for compatibility with Macintosh
                        _exclude_dirs._negative.push_back({ wildcardtl::wildcard
                            { pathname, is_windows() },
                            // If pathname does not include a wildcard
                            // store it as a plain string for error reporting.
                            pathname.find_first_of("*?[") == std::string::npos ?
                            pathname :
                            std::string() });
                    }
                    else
                    {
                        const std::string pathname(p);

                        // Not using emplace_back() for compatibility with Macintosh
                        _exclude_dirs._positive.push_back({ wildcardtl::wildcard
                            { pathname, is_windows() },
                            // If pathname does not include a wildcard
                            // store it as a plain string for error reporting.
                            pathname.find_first_of("*?[") == std::string::npos ?
                            pathname :
                            std::string() });
                    }
                }
            }
            else if (param == "-f" || param == "--filter")
            {
                ++i;

                if (i == size)
                    throw spellc_error("--filter is not "
                        "followed by pathname");

                if (!filter_pathname.empty())
                    throw spellc_error("Cannot set "
                        "--filter more than once");

                // Lexer to load
                filter_pathname = params[i];
            }
            else if (param == "--include")
            {
                ++i;

                if (i == size)
                    throw spellc_error("--include is not "
                        "followed by pathname");

                const std::string_view value(params[i]);
                const char* first = value.data();
                const char* end = nullptr;
                const char* second = first + value.size();

                do
                {
                    const auto idx = value.find_first_of(';', first - value.data());

                    end = idx == std::string_view::npos ?
                        second :
                        value.data() + idx;
                    add_pathname(first, end, _include);

                    if (end != second)
                        ++end;

                    first = end;
                } while (end != second);
            }
            else if (param == "-r" || param == "--recurse")
            {
                _directories = directories::recurse;
            }
            else if (param == "-w" || param == "--word-regex")
            {
                ++i;

                if (i == size)
                    throw spellc_error("--word-regex is not "
                        "followed by pathname");

                if (word_rx != nullptr)
                    throw spellc_error("Cannot set "
                        "--word_regex more than once");

                word_rx = params[i];
            }
            else
                throw spellc_error(std::format("Unknown switch {}", param));
        }
        else
        {
            string_vector input_pathnames = split(params[i], ';');

            // Input file to load
            pns.insert(pns.end(),
                input_pathnames.begin(), input_pathnames.end());
        }
    }

    if (word_rx == nullptr)
        // Word can be capitalised, all lower case or all upper case
        word_rx = "[A-Za-z]([-']?[a-z])*|[A-Z]([-']?[A-Z])*";

    _word_sm = build_word_lexer(word_rx);

    if (!filter_pathname.empty())
        _filter_sm = build_filter_lexer(filter_pathname.c_str(),
            _config_parser);

    namespace fs = std::filesystem;

    if (dictionary_pathnames.empty())
        throw spellc_error("No dictionaries specified!");

    _dictionaries = mf_vector(dictionary_pathnames.size());

    for (std::size_t idx = 0, size = dictionary_pathnames.size();
        idx < size; ++idx)
    {
        // Load a dictionary
        _dictionaries[idx].open(dictionary_pathnames[idx].c_str());

        if (_dictionaries[idx].data() == nullptr)
            throw spellc_error(std::format("Failed to open {}",
                dictionary_pathnames[idx]));
    }

    _icase = build_indexes(_dictionaries, _dict_indexes);
    _pathnames.create(pns, _directories);
}

void data_t::process(void(*func)(const char* pathname,
    const data_t& data))
{
    std::queue<std::pair<std::string, const wildcards*>> queue;

    for (const auto& [path, wcs] : _pathnames._path_wcs)
    {
        queue.emplace(path, &wcs);
    }

    for (; !queue.empty(); queue.pop())
    {
        namespace fs = std::filesystem;
        const auto& [path, wcs] = queue.front();
        std::error_code err;
        bool processed = false;

        for (auto iter = fs::directory_iterator(path,
            fs::directory_options::skip_permission_denied, err),
            end = fs::directory_iterator(); iter != end; ++iter)
        {
            const auto& p = iter->path();

            // Don't throw if there is a Unicode pathname
            const std::string pathname = reinterpret_cast<const char*>
                (p.u8string().c_str());

            if (!fs::is_directory(p) ||
                _directories == directories::read)
            {
                if (!_pathnames.process_file(pathname.c_str(), *wcs))
                    continue;
            }

            if (fs::is_directory(p))
            {
                switch (_directories)
                {
                case directories::read:
                    if (!_no_messages)
                    {
                        output_text_nl(std::cerr, is_a_tty(stderr),
                            _wa_text,
                            std::format("{}{}: Is a directory",
                                sc_text(),
                                _pathnames.normalise(p.string())));
                    }

                    break;
                case directories::recurse:
                    if (!(fs::is_symlink(p) && !_follow_symlinks) &&
                        include_dir(pathname.substr(pathname.
                            rfind(fs::path::preferred_separator) + 1)))
                    {
                        queue.emplace(pathname, wcs);
                    }

                    break;
                case directories::skip:
                    // Do nothing
                    break;
                }
            }
            else
            {
                // Skip zero length files
                if (fs::file_size(p) == 0)
                {
                    continue;
                }

                if (include_file(pathname.substr(pathname.
                    rfind(fs::path::preferred_separator) + 1)))
                {
                    func(pathname.c_str(), *this);
                    processed = true;
                }
            }
        }

        if (!processed && _directories != directories::recurse &&
            !_no_messages)
        {
            for (const auto& wildcard : wcs->_positive)
            {
                if (!wildcard._pathname.empty())
                {
                    output_text_nl(std::cerr, is_a_tty(stderr),
                        _wa_text,
                        std::format("{}{}: No such file or directory",
                            sc_text(),
                            _pathnames.normalise(wildcard._pathname)));
                }
            }
        }
    }
}

bool data_t::include_dir(const std::string& path)
{
    return (_exclude_dirs._negative.empty() ||
        std::ranges::any_of(_exclude_dirs._negative,
            [&path](const auto& pn)
            {
                return pn._wc.match(path);
            })) &&
        std::ranges::none_of(_exclude_dirs._positive,
            [&path](const auto& pn)
            {
                return pn._wc.match(path);
            });
}

bool data_t::include_file(const std::string& path)
{
    return (_include._positive.empty() ||
        std::ranges::any_of(_include._positive,
            [&path](const auto& pn)
            {
                return pn._wc.match(path);
            })) &&
        std::ranges::none_of(_include._negative,
            [&path](const auto& pn)
            {
                return pn._wc.match(path);
            });
}
