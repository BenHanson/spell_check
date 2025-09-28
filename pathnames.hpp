#pragma once

#include <wildcardtl/wildcard.hpp>

#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

using string_vector = std::vector<std::string>;

enum class directories
{
    read, recurse, skip
};

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
    wildcards _exclude;
    std::map<std::string, wildcards, std::less<>> _path_wcs;

    [[nodiscard]] bool empty() const;
    void create(const string_vector& pns, const directories dir);
    void add_pathname(std::string pn, const directories dir);
    std::string_view normalise(const std::string& pathname);
    bool process_file(const std::string& pathname, const wildcards& wcs) const;
};
