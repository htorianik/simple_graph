#ifndef CLI_PARSER_HXX_
#define CLI_PARSER_HXX_

#include <string> // std::string
#include <list> // std::list
#include <algorithm> // std::find_if
#include <typeindex> // typeid
#include <sstream> // std::stringstream
#include <unordered_map> // std::unordered_map
#include <type_traits> // std::is_same
#include <tuple> // std::tuple
#include <functional> // std::function
#include <variant> // std::variant

namespace cli_parser {
    // Option. Represents one single option with required type. 
    // If required arg's type is `std::tuple<>` then the option 
    // doesn't expect for a value.
    template<typename T>
    struct Option {
        using req_type = T;
        std::string short_name;
        std::string full_name;
        std::string description;
    };

    // GenOption. Represents all possible Options.
    using GenOption = std::variant<
        Option<std::tuple<>>,
        Option<int>,
        Option<float>,
        Option<char>,
        Option<std::string>>;

    // Value. Represents all option's values that could be parsed.
    using Value = std::variant<
        std::tuple<>,
        int,
        float, 
        char, 
        std::string>;

    // Returns function that answers if either short or long name of the GetOption equals to given.
    std::function<bool(GenOption)> matcher(std::string name) {
        return [name](GenOption option) {
            return visit([name](auto &&option) {
                return (name == option.short_name ||
                        name == option.full_name);
            }, option);
        };
    }
    
    // T        – type of current Option. Used to infer type of the argument to parse. 
    // narg     – current number of argument being processing.
    // argv     – list of arguments. 
    // return.1 – extracted value of std::variant type.
    // return.2 – flag if narg was advanced in order to get the value.
    template<typename T>
    std::tuple<Value, bool> parse_value(uint32_t narg, char **argv) {
        if constexpr (std::is_same<typename T::req_type, std::tuple<>>::value) return {Value(std::tuple<>()), false};
        else if constexpr (std::is_same<typename T::req_type, std::string>::value) return {argv[narg + 1], true};
        else if constexpr (std::is_same<typename T::req_type, char>::value) return {argv[narg + 1][0], true};
        else {
            std::string value_str = argv[narg + 1];
            std::stringstream ss(value_str);
            typename T::req_type value;
            ss >> value;
            return {Value(value), true};
        }
    }

    // ParsedResult. Returns as the final result of the argument parsing.
    // options      – list of parsed options (name of option and it's value).
    // positional   – received arguments that not an options.
    struct ParsedResult {
        std::list<std::pair<std::string, Value>> options;
        std::list<std::string> positional;

        static inline std::function<bool(std::pair<std::string, Value>)> match_fs_name(std::string full_name, std::string short_name) {
            return [full_name, short_name](std::pair<std::string, Value> kv) -> bool {
                return (kv.first == full_name || kv.first == short_name);
            };
        }

        bool occurs(std::string full_name, std::string short_name = "") const {
            return std::find_if(options.begin(), options.end(), match_fs_name(full_name, short_name)) != options.end();
        }

        template<typename T>
        std::optional<T> get_value(std::string full_name, std::string short_name = "") const {
            auto option_it = std::find_if(options.begin(), options.end(), match_fs_name(full_name, short_name));
            if (option_it != options.end() && holds_alternative<T>(option_it->second)) {
                return get<T>(option_it->second);
            } else {
                return std::nullopt;
            }
        }
    };

    // argc     – number of command line arguments.
    // argv     – list of arguments.
    // options  – list of options parametrizes by the type of values it requires. `std::tuple<>` means
    //            this option doesn't require value at all.
    ParsedResult parse(uint32_t argc, char **argv, const std::list<GenOption> &options) {
        ParsedResult result;
        for (uint32_t narg = 1; narg < argc; ++narg) {
            auto name = argv[narg];

            auto option_it = std::find_if(options.begin(), options.end(), matcher(name));
            if (option_it == options.end()) {
                result.positional.push_back(name);
                continue;
            }

            auto [value, advance_narg] = visit([narg, argv](auto option) mutable { return parse_value<decltype(option)>(narg, argv); }, *option_it);
            if (advance_narg) ++narg;

            result.options.emplace_back(name, move(value));
        }
        return result;
    }

    template<typename T>
    std::string help_opt(T option) {
        std::unordered_map<std::type_index, std::string> type_aliases = {
            { typeid(int), "integer" },
            { typeid(char), "char" },
            { typeid(float), "float" },
            { typeid(std::string), "std::string" },
        };
        std::stringstream ss;
        ss << "[" << option.short_name;
        if constexpr (!std::is_same<typename T::req_type, std::tuple<>>::value) {
            ss << " <" << type_aliases[typeid(typename T::req_type)] << ">";
        }
        ss << "]";
        return ss.str();
    }

    std::string help(std::string util_name, std::string additional, const std::list<GenOption> &options) {
        const uint32_t max_len = 80;
        uint32_t cur_line_len = 0;
        std::stringstream ss;
        ss << "usage: " << util_name << " " << additional << " ";
        for (auto option: options) {
            std::string option_help = visit([](auto opt) { return help_opt(opt); }, option);
            if (cur_line_len + option_help.length() >= max_len) {
                ss << "\n\t";
                cur_line_len = 0;
            }
            ss << option_help << " ";
            cur_line_len += option_help.length() + 1;
        }
        return ss.str();
    }
};

#endif//CLI_PRASER_HXX_
