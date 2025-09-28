#include "pathnames.hpp"
#include "utils.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

bool pathnames::empty() const
{
    return _path_wcs.empty();
}

void pathnames::create(const string_vector& pns, const directories dir)
{
    for (const auto& pn : pns)
    {
        add_pathname(dir == directories::recurse ?
            (pn + static_cast<char>(fs::path::preferred_separator)) + '*' : pn,
            dir);
    }
}

void pathnames::add_pathname(std::string pn, const directories dir)
{
    const std::size_t wc_idx = pn.find_first_of("*?[");
    const std::size_t sep_idx = pn.rfind(fs::path::preferred_separator,
        wc_idx);
    const bool negate = pn[0] == '!';
    const std::string path = sep_idx == std::string::npos ? "." :
        pn.substr(negate ? 1 : 0, sep_idx + (negate ? 0 : 1));
    auto& wcs = _path_wcs[path];

    if (sep_idx == std::string::npos)
    {
        if (!((!negate && wc_idx == 0) || (negate && wc_idx == 1)))
        {
            if (dir == directories::recurse)
                pn.insert(negate ? 1 : 0, std::string(1, '*') +
                    static_cast<char>(fs::path::preferred_separator));
            else
                pn.insert(negate ? 1 : 0, path +
                    static_cast<char>(fs::path::preferred_separator));
        }
    }
    else if (dir == directories::recurse && !((!negate && wc_idx == 0) ||
        (negate && wc_idx == 1)))
    {
        pn = std::string(1, '*') + pn.substr(sep_idx);

        if (negate)
            pn = '!' + pn;
    }

    if (negate)
        // Not using emplace_back() for compatibility with Macintosh
        wcs._negative.push_back({ wildcardtl::wildcard{ pn, is_windows() },
            wc_idx == std::string::npos ?
            pn :
            std::string() });
    else
        // Not using emplace_back() for compatibility with Macintosh
        wcs._positive.push_back({ wildcardtl::wildcard{ pn, is_windows() },
            wc_idx == std::string::npos ?
            pn :
            std::string() });
}

std::string_view pathnames::normalise(const std::string& pathname)
{
    namespace fs = std::filesystem;
    std::string_view pn = (pathname[0] == '.' &&
        pathname[1] == fs::path::preferred_separator) ?
        pathname.c_str() + 2 :
        pathname.c_str();

    return pn;
}

bool pathnames::process_file(const std::string& pathname,
    const wildcards& wcs) const
{
    bool process = false;
    const char* filename = pathname.c_str() +
        pathname.rfind(fs::path::preferred_separator) + 1;
    bool skip = !_exclude._negative.empty();

    for (const auto& pn : _exclude._negative)
    {
        if (!pn._wc.match(filename))
        {
            skip = false;
            break;
        }
    }

    if (!skip)
    {
        for (const auto& pn : _exclude._positive)
        {
            if (pn._wc.match(filename))
            {
                skip = true;
                break;
            }
        }
    }

    if (!skip)
    {
        process = !wcs._negative.empty();

        for (const auto& pn : wcs._negative)
        {
            if (!pn._wc.match(pathname))
            {
                process = false;
                break;
            }
        }

        if (!process)
        {
            for (const auto& pn : wcs._positive)
            {
                if (pn._wc.match(pathname))
                {
                    process = true;
                    break;
                }
            }
        }
    }

    return process;
}
