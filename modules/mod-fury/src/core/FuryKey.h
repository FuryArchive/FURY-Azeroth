#ifndef MOD_FURY_KEY_H
#define MOD_FURY_KEY_H

#include <cctype>
#include <string_view>

namespace Fury
{
[[nodiscard]] inline bool IsCanonicalKey(
    std::string_view key,
    std::size_t maximumLength = 128)
{
    if (key.empty() || key.size() > maximumLength)
        return false;

    for (unsigned char c : key)
    {
        if (std::isalnum(c) ||
            c == '.' ||
            c == '_' ||
            c == '-' ||
            c == ':')
        {
            continue;
        }

        return false;
    }

    return true;
}
}

#endif
