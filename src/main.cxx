#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <variant>
#include <algorithm>

#include "SDL2/SDL.h"

#include "cli_parser.hxx"

#define VERSION "0.0.2 alpha"

template<typename T> 
using Maybe = typename std::variant<T, std::string>;

using Dataset = std::vector<std::pair<float, float>>;

std::string fmt_error(std::string msg) {
    const char red_mod[] = "\u001b[31m";
    const char def_mod[] = "\u001b[0m";
    std::stringstream ss;
    ss << red_mod << "Error: " << def_mod << msg;
    return ss.str();
}

std::string fmt_help(std::list<cli_parser::GenOption> options, std::string version) {
    std::stringstream ss;
    ss 
        << "simple_graph, version " << version << std::endl
        << cli_parser::help("simple_graph", "[fname ...]", options);
    return ss.str();
}

Maybe<Dataset> csv_to_dataset(std::string fname, char separator) {
    std::ifstream csv_ss(fname);
    if (!csv_ss.is_open()) {
        std::stringstream err_ss;
        err_ss << "Can't read file" << fname;
        return Maybe<Dataset>(err_ss.str());
    }

    Dataset dset;
    uint32_t line_number = 0;
    uint32_t col_number = 0;
    for (std::string line; std::getline(csv_ss, line, '\n'); ++line_number) {
        std::vector<float> columns;
        std::stringstream line_ss(line);
        for (std::string col; std::getline(line_ss, col, separator); ++col_number) {
            try {
                columns.push_back(std::stof(col));
            } catch (std::invalid_argument exc) {
                std::stringstream err_ss;
                err_ss 
                    << "Couldn't parse number at " << line_number << ", " << col_number << "." << std::endl
                    << "Expected float, found \"" << col << "\".";
                return Maybe<Dataset>(err_ss.str());
            }
        } 
        dset.push_back({columns[0], columns[1]});
    }
    return dset;
}

std::vector<Dataset> normilize_dsets(std::vector<Dataset> &&dsets) {
    std::vector<Dataset> result;
    float x_min = 1e9;
    float x_max = -1e9;
    float y_max = -1e9;
    for (auto &dset: dsets) {
        for (auto [x, y]: dset) {
            x_min = fmin(x_min, x);
            x_max = fmax(x_max, x);
            y_max = fmax(y_max, y); 
        }
    }

    std::transform(
            std::make_move_iterator(dsets.begin()),
            std::make_move_iterator(dsets.end()),
            std::back_inserter(result),
            [x_min, x_max, y_max](Dataset &&dset) {
                std::sort(dset.begin(), dset.end());
                for (auto &[x, y]: dset) {
                    x = (x - x_min) / x_max;
                    y = y / y_max;
                } 
                return std::move(dset);
            });

    return result;
}

Dataset average(Dataset &&dset, uint32_t nneigbours) {
     Dataset result(dset.size());
     uint32_t n_2 = nneigbours / 2;
     for (uint32_t i = 0; i < dset.size(); ++i) {
         uint32_t from  = std::max(0u, i - n_2);
         uint32_t to    = std::min(dset.size() - 1, static_cast<unsigned long>(i + n_2));
         for (uint32_t j = from; j <= to; ++j) {
            result[i].second += dset[j].second;
         }
         result[i].first = dset[i].first;
         result[i].second /= to - from + 1;
     }
     return result;
}

struct Graph {
    uint32_t x_res;
    uint32_t y_res;
    SDL_Renderer *renderer;
    SDL_Window *window;

    Graph(uint32_t x_res, uint32_t y_res): x_res(x_res), y_res(y_res) {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer(x_res, y_res, 0, &window, &renderer);
    }

    void render(const std::vector<Dataset> &dsets) {
        const struct { uint8_t r, g, b; } palette[] = {
            {255, 0, 0},
            {0, 255, 0},
            {0, 0, 255},
        };

		SDL_RenderClear(renderer);

        uint32_t icolor = 0;
        for (auto &dset: dsets) {
            auto color = palette[icolor++];
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0);
            for (int i = 0; i < dset.size() - 1; ++i) {
                uint32_t x1 = dset[i].first * x_res;
                uint32_t y1 = dset[i].second * y_res;
                uint32_t x2 = dset[i + 1].first * x_res;
                uint32_t y2 = dset[i + 1].second * y_res;
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            } 
        } 

		SDL_RenderPresent(renderer);		
    }

    void loop() {
        for (SDL_Event event; event.type != SDL_QUIT; SDL_PollEvent(&event)) {
            // no-op
        }
    }

    ~Graph() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

int main(int argc, char **argv) {
    const std::list<cli_parser::GenOption> options = {
        cli_parser::Option<int> {
            .full_name = "--average",
            .short_name = "-a",
            .description = "Every value becomes average between it's 10 neighbours."
        },
        cli_parser::Option<char> { 
            .full_name = "--sep",
            .short_name = "-s",
            .description = "Custom separator between columns in providen csv file (Defualt is ',')."
        },
        cli_parser::Option<std::string> {
            .full_name = "--output",
            .short_name = "-o",
            .description = "Output filename. Available extensions are \".png\". (Default is \"output.png\")."
        },
        cli_parser::Option<std::tuple<>> {
            .full_name = "--help",
            .short_name = "-h",
            .description = "Print help message.",
        }
    };

    auto args = cli_parser::parse(argc, argv, options);

    // Handle -h | --help
    if (std::find_if(args.options.begin(), args.options.end(), 
        [](auto opt) -> bool { return (opt.first == "-h" || opt.first == "--help"); }) != args.options.end()) {
        std::cout << fmt_help(options, VERSION) << std::endl;
        return 0;
    }

    // Handle lack of input fnames
    if (!args.positional.size()) {
        std::cout << fmt_help(options, VERSION) << std::endl;
        return 1;
    } 

    // Handle -s | --separator
    char separator = ',';
    if (auto sep_opt_it = std::find_if(args.options.begin(), args.options.end(), 
        [](auto opt) -> bool { return (opt.first == "-s" || opt.first == "--sep"); });
        sep_opt_it != args.options.end()) {
        separator = std::get<char>(sep_opt_it->second);
    }
    
    // Read datasets
    std::vector<Dataset> dsets;
    for (auto fname: args.positional) {
        auto mb_dset = csv_to_dataset(fname, separator);
        if (std::holds_alternative<std::string>(mb_dset)) {
            std::cerr << fmt_error(std::get<std::string>(mb_dset)) << std::endl;
            return 1;
        } 
        dsets.emplace_back(std::move(std::get<Dataset>(mb_dset)));
    } 

    // Handle -a | --average
    if (auto avg_opt_it = std::find_if(args.options.begin(), args.options.end(), 
        [](auto opt) -> bool { return (opt.first == "-a" || opt.first == "--average"); });
        avg_opt_it != args.options.end()) {
        uint32_t nneigbours = std::get<int>(avg_opt_it->second);
        for (auto &dset: dsets) {
            dset = average(std::move(dset), nneigbours);
        }
    }

    dsets = normilize_dsets(std::move(dsets));

    Graph graph(512, 512);
    graph.render(dsets);
    graph.loop();
    return 0;
}
