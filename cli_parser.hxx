#ifndef CLI_PARSER_HXX_
#define CLI_PARSER_HXX_

#include <string>
#include <vector>
#include <set>

namespace cli_parser {
    using namespace std;

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
                std::cout << "Located input: " << opt << std::endl;
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
