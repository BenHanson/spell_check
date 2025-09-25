#pragma once

#include <wildcardtl/wildcard.hpp>

#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

using string_vector = std::vector<std::string>;

struct wildcards
{
    struct wildcard
    {
        wildcardtl::wildcard _wc;
        std::string _pathname;
    };

    std::vector<wildcard> _positive;
    std::vector<wildcard> _negative;
};

struct pathnames
{
    bool _recurse = false;
    std::map<std::string, wildcards, std::less<>> _path_wcs;

    [[nodiscard]] bool empty() const;
    void create(const string_vector& pns);
    void add_pathname(std::string pn);
    bool process_file(const char* pathname, const wildcards& wcs) const;
};
