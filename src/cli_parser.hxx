#ifndef CLI_PARSER_HXX_
#define CLI_PARSER_HXX_

#include <string>
#include <vector>
#include <set>
#include <list>
#include <typeindex>
#include <algorithm>

namespace cli_parser {
    using namespace std;

    // Option. Represent one single option with required type. 
    // If required arg's type is `tuple<>` then the option 
    // doesn't expect for a value.
    template<typename T>
    struct Option {
        using req_type = T;
        string short_name;
        string full_name;
        string description;
    };

    // GenOption. Represents all possible Options.
    using GenOption = variant<
        Option<tuple<>>,
        Option<int>,
        Option<float>,
        Option<char>,
        Option<string>>;

    // Value. Represents all option's values that could be parsed.
    using Value = variant<
        tuple<>,
        int,
        float, 
        char, 
        string>;

    // matcher 
    //
    // name     – string that will be copared with short and full name of the option.
    // return   – function that answers if given GenOpt has provided name.
    function<bool(GenOption)> matcher(string name) {
        return [name](GenOption option) {
            return visit([name](auto &&option) {
                return (name == option.short_name ||
                        name == option.full_name);
            }, option);
        };
    }
    
    // parse_value
    //
    // T        – type of current Option. Used to infer type of argument to parse. 
    // narg     – current number of argument being processing.
    // argv     – list of arguments. 
    // return.1 – extracted value of variant type.
    // return.2 – flag if narg was advanced in order to get the value.
    template<typename T>
    tuple<Value, bool> parse_value(uint32_t narg, char **argv) {
        if constexpr (is_same<typename T::req_type, tuple<>>::value) return {Value(tuple<>()), false};  // Options doesn't expect for an arument.
        else if constexpr (is_same<typename T::req_type, string>::value) return {argv[narg + 1], true}; // Argument is a string.
        else {
            string value_str = argv[narg + 1];
            stringstream ss(value_str);
            typename T::req_type value;
            ss >> value;
            return {Value(value), true};
        }
    }

    // ParsedResult. Returns as the final result of argument parsing.
    //
    // options      – list of parsed options (name of option and it's value).
    // positional   – received arguments that not an options.
    struct ParsedResult {
        list<pair<string, Value>> options;
        list<string> positional;
    };

    // parse
    //
    // argc     – number of cl arguments.
    // argv     – list of arguments.
    // options  – list of options parametrizes by the type of values it requires. `tuple<>` means
    //            this option isn't require value at all.
    ParsedResult parse(uint32_t argc, char **argv, list<GenOption> options) {
        ParsedResult result;
        for (uint32_t narg = 1; narg < argc; ++narg) {
            auto name = argv[narg];

            auto option_it = find_if(options.begin(), options.end(), matcher(name));
            if (option_it == options.end()) {
                result.positional.push_back(name);
                continue;
            }

            auto value = visit([narg, argv](auto option) mutable { 
                    auto [value, advance_narg] = parse_value<decltype(option)>(narg, argv);
                    if (advance_narg) ++narg;
                    return value;
            }, *option_it);

            result.options.emplace_back(name, move(value));
        }
        return result;
    }
};

#endif//CLI_PRASER_HXX_
