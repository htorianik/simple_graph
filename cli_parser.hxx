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

    template<typename T>
    struct Option {
        using req_type = T;
        string short_name;
        string full_name;
        string description;
    };

    using GenOption = variant<
        Option<tuple<>>,
        Option<int>,
        Option<float>,
        Option<char>,
        Option<string>>;

    using Value = variant<
        tuple<>,
        int,
        float, 
        char, 
        string>;

    // matcher 
    //
    // name         – string that will be copared with short and full name of the option.
    // return       – function that answers if given GenOpt has provided name.
    function<bool(GenOption)> matcher(string name) {
        return [name](GenOption option) {
            return visit([name](auto &&option) {
                return (name == option.short_name ||
                        name == option.full_name);
            }, option);
        };
    }

    template<typename T>
    tuple<Value, bool> parse_value(uint32_t narg, char **argv) {
        if constexpr (is_same<typename T::req_type, tuple<>>::value) {
            return {Value(tuple<>()), false};
        } else 
        if constexpr (is_same<typename T::req_type, string>::value) {
            return {argv[narg + 1], true};
        } else {
            string value_str = argv[narg + 1];
            stringstream ss(value_str);
            typename T::req_type value;
            ss >> value;
            return {Value(value), true};
        }
    }

    // parse
    //
    // argc         – 
    // argv         –
    // options      – 
    list<pair<string, Value>> parse(uint32_t argc, char **argv, list<GenOption> options) {
        list<pair<string, Value>> result;
        for (uint32_t narg = 1; narg < argc; ++narg) {
            auto name = argv[narg];
            auto option_it = find_if(options.begin(), options.end(), matcher(name));
            if (option_it == options.end()) continue;
            auto value = visit([narg, argv](auto option) mutable { 
                    auto [value, advance_narg] = parse_value<decltype(option)>(narg, argv);
                    if (advance_narg) ++narg;
                    return value;
            }, *option_it);
            result.emplace_back(name, move(value));
        }
        return result;
    }

    struct Args {
        bool unrecognised = false;
        bool miss_value = false;
        vector<string> inputs;
        vector<string> options;
        vector<pair<string, string>> key_values;
    };

    Args parse(uint32_t argc, char **argv, set<string> available, set<string> single) {
        Args args;
        for (uint32_t narg = 1; narg < argc; ++narg) {
            auto opt = argv[narg];
            if (opt[0] != '-') {
                args.inputs.push_back(opt);
            } else if (!available.count(opt)) {
                args.unrecognised = true;
            } else if (single.count(opt)) {
                args.options.push_back(opt);
            } else if (narg == argc - 1) {
                args.miss_value = true;
            } else {
                args.key_values.push_back({opt, argv[++narg]});
            }
        }
        return args;
    }
};

#endif//CLI_PRASER_HXX_
