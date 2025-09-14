#include "pathnames.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

static bool is_windows()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void pathnames::create(const string_vector& pns)
{
    for (const auto& pn : pns)
    {
        add_pathname(pn);
    }
}

void pathnames::add_pathname(std::string pn)
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
            if (_recurse)
                pn.insert(negate ? 1 : 0, std::string(1, '*') +
                    static_cast<char>(fs::path::preferred_separator));
            else
                pn.insert(negate ? 1 : 0, path +
                    static_cast<char>(fs::path::preferred_separator));
        }
    }
    else if (_recurse && !((!negate && wc_idx == 0) ||
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

bool pathnames::process_file(const char* pathname,
    const wildcards& wcs) const
{
    bool process = !wcs._negative.empty();

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

    return process;
}
