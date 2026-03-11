#pragma once
#include <initializer_list>
#include <string>
#include <string_view>

namespace bdc::app
{

// Returns the value following any of the given flags, or defaultValue.
// Supports both space-separated form (-c foo, --config foo)
// and equals form (--long-flag=foo) for flags starting with "--".
inline std::string parseArgument(int argc, char** argv,
                                 std::string_view defaultValue,
                                 std::initializer_list<std::string_view> flags)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        for (auto flag : flags)
        {
            if (arg == flag && i + 1 < argc)
                return argv[i + 1];

            if (flag.starts_with("--"))
            {
                // --flag=value form
                auto prefix = std::string{flag} + '=';
                if (arg.starts_with(prefix))
                    return std::string{arg.substr(prefix.size())};
            }
        }
    }
    return std::string{defaultValue};
}

inline std::string parseConfigPath(int argc, char** argv)
{
    return parseArgument(argc, argv, "config.json", {"-c", "--config"});
}

} // namespace bdc::app
