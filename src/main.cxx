#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <variant>
#include <algorithm>

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"

#include "cli_parser.hxx"
#include "arial.hpp" // resources_arial_ttf, resources_arial_ttf_len

#define VERSION "0.0.2"

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
        err_ss << "Can't read file \"" << fname << "\"";
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
    TTF_Font *arial;

    Graph(uint32_t x_res, uint32_t y_res): x_res(x_res), y_res(y_res) {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer(x_res, y_res, 0, &window, &renderer);
        TTF_Init();
        arial = TTF_OpenFontRW(SDL_RWFromConstMem(resources_arial_ttf, resources_arial_ttf_len), 1, 12);
    }

    void render_annotation(uint32_t offset_x, uint32_t offset_y, SDL_Color color, std::string text) {
        const uint32_t line_width = 40;
        const uint32_t gap_after_line = 10;
        SDL_Surface* font_surf = TTF_RenderText_Blended(arial, text.c_str(), {0, 0, 0, 0}); 
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(
                renderer, 
                offset_x, 
                offset_y + font_surf->h / 2,
                offset_x + line_width,
                offset_y + font_surf->h / 2);

        SDL_Texture* font_tex = SDL_CreateTextureFromSurface(renderer, font_surf);
        SDL_Rect rect;
        rect.x = offset_x + line_width + gap_after_line;
        rect.y = offset_y;
        rect.w = font_surf->w; 
        rect.h = font_surf->h;
        SDL_RenderCopy(renderer, font_tex, NULL, &rect);
        SDL_FreeSurface(font_surf);
        SDL_DestroyTexture(font_tex);
    }

    template<typename TDsetsContainer, typename TAnnotationsContainer>
    void render(const TDsetsContainer &dsets, const TAnnotationsContainer &annotations) {
        const struct { uint8_t r, g, b; } 
            background = { 22,   25,  37},
            info_panel = { 253, 255, 252},
            palette[] = {
                { 35,  87, 135},
                {193,  41,  46},
                {241, 211,   2},
                {224, 119, 125},
                { 81,  88, 187},
                {242, 109, 249},
            };

        // Render canvas
        const uint32_t canvas_x_res = x_res;
        const uint32_t canvas_y_res = y_res - 50;
        SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, 0);
		SDL_RenderClear(renderer);
        uint32_t icolor = 0;
        for (auto &dset: dsets) {
            const auto color = palette[icolor++];
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0);
            for (int i = 0; i < dset.size() - 1; ++i) {
                const uint32_t x1 = dset[i].first * canvas_x_res;
                const uint32_t y1 = dset[i].second * canvas_y_res;
                const uint32_t x2 = dset[i + 1].first * canvas_x_res;
                const uint32_t y2 = dset[i + 1].second * canvas_y_res;
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            } 
        } 

        // Render info_panel
        SDL_Rect panel_rect;
        panel_rect.x = 0;
        panel_rect.y = canvas_y_res;
        panel_rect.w = x_res;
        panel_rect.h = y_res - canvas_y_res;
        SDL_SetRenderDrawColor(renderer, info_panel.r, info_panel.g, info_panel.b, 0);
        SDL_RenderFillRect(renderer, (SDL_Rect*)&panel_rect);  
        icolor = 0;

        uint32_t counter = 0;
        for (auto &annotation: annotations) {
            const uint32_t offset_x = 20 + 150 * (counter / 2);
            const uint32_t offset_y = canvas_y_res + 7 + 20 * (counter % 2);
            const auto color = palette[counter];
            render_annotation(offset_x, offset_y, {color.r, color.g, color.b}, annotation);
            ++counter;
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
        TTF_Quit();
    }
};

int main(int argc, char **argv) {
    const std::list<cli_parser::GenOption> options = {
        cli_parser::Option<int> {
            .full_name = "--average",
            .short_name = "-a",
            .description = "Every value becomes average between it's 10 neighbours."
        },
        cli_parser::Option<int> {
            .full_name = "--res_x", 
            .short_name = "-x",
            .description = "Width of window in pixels (Default is 512)."
        },
        cli_parser::Option<int> {
            .full_name = "--res_y",
            .short_name = "-y",
            .description = "Height of window in pixels (Default is 512)."
        },
        cli_parser::Option<char> { 
            .full_name = "--sep",
            .short_name = "-s",
            .description = "Custom separator between columns in providen csv file (Defualt is ',')."
        },
        cli_parser::Option<std::tuple<>> {
            .full_name = "--help",
            .short_name = "-h",
            .description = "Print help message.",
        }
    };

    auto args = cli_parser::parse(argc, argv, options);
    if (args.occurs("--help", "-h")) {
        std::cout << fmt_help(options, VERSION) << std::endl;
        return 0;
    }

    // Handle lack of input fnames
    if (!args.positional.size()) {
        std::cerr << fmt_error("You must provide at least one file to graph (see sg --help).") << std::endl;
        return 1;
    } 

    // Handle separator
    char separator = ',';
    if (auto sep_opt = args.get_value<char>("--sep", "-s"); sep_opt.has_value()) separator = sep_opt.value();
    
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
    dsets = normilize_dsets(std::move(dsets));

    // Handle average option
    if (auto avg_opt = args.get_value<int>("--average", "-a"); avg_opt.has_value()) {
        uint32_t nneigbours = avg_opt.value();
        for (auto &dset: dsets) {
            dset = average(std::move(dset), nneigbours);
        }
    }

    // Handle resolution preferences
    uint32_t width = 512;
    uint32_t height = 512;
    if (auto res_x_opt = args.get_value<int>("--res_x", "-x"); res_x_opt.has_value()) width = res_x_opt.value();
    if (auto res_y_opt = args.get_value<int>("--res_y", "-y"); res_y_opt.has_value()) height = res_y_opt.value();
    Graph graph(width, height);
    graph.render(dsets, args.positional);
    graph.loop();
    return 0;
}
