#include "types.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <lexertl/iterator.hpp>
#include <lexertl/memory_file.hpp>
#include <lexertl/state_machine.hpp>

#include <algorithm>
#include <exception>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

static void check_range(const char* pathname, const char* start,
    const char* first, const char* second, const data_t& data)
{
    // Lex a range
    lexertl::citerator iter(first, second, data._word_sm);
    // Re-use memory of temporary string
    std::string lhs;

    for (; iter->id != 0; ++iter)
    {
        if (iter->length() < 2)
            continue;

        lhs = data._icase ?
            boost::to_lower_copy(iter->str()) :
            iter->str();

        if (const auto hit_iter =
            std::ranges::lower_bound(data._dict_indexes, lhs);
            hit_iter == data._dict_indexes.end() || *hit_iter != lhs)
        {
            // Word not found in dictionaries
            std::cout << pathname << '(' <<
                1 + std::count(start, iter->first, '\n') << "): ";

            std::cout << iter->view() << '\n';
        }
    }
}

static void check_spell(const char* pathname,
    const char* first, const char* second, const data_t& data)
{
    if (data._filter_sm.empty())
    {
        check_range(pathname, first, first, second, data);
    }
    else
    {
        // Lex the file
        lexertl::citerator iter(first, second, data._filter_sm);

        for (; iter->id; ++iter)
        {
            check_range(pathname, first, iter->first, iter->second, data);
        }
    }
}

int main(int argc, const char* argv[])
{
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
    {
        std::cout << "Usage: spell_check [pathname...]\n"
            "[(--recurse|-r)]\n"
            "[(--word-regex|-w) <regex>]\n"
            "[(--filter|-f) <pathname to flex style lexer spec>]\n"
            "((--dictionary|-d) <pathname to whitespace separated word list>)+\n";
        return argc == 1;
    }

    try
    {
        data_t data;

        data.create(std::span<const char*>(argv, argc));
        data.process([](const char* pathname, const data_t& gubbins)
            {
                lexertl::memory_file mf(pathname);

                check_spell(pathname, mf.data(), mf.data() + mf.size(),
                    gubbins);
            });

        if (data._pathnames.empty())
        {
            // Read from cin
            std::ostringstream ss;
            std::string cin;

            ss << std::cin.rdbuf();
            cin = ss.str();
            check_spell("stdin", cin.c_str(), cin.c_str() + cin.size(), data);
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
