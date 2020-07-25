#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <variant>
#include <algorithm>
#include <regex>

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"

#include "cli_parser.hxx"
#include "arial.hpp" // resources_font_ttf, resources_font_ttf_len

#define VERSION "0.1.0"

template<typename T> 
using Maybe = typename std::variant<T, std::string>;
using Dataset = std::vector<std::pair<float, float>>;

template<class... Args>
std::string fmt_error(Args&&... args) {
    const char red_mod[] = "\u001b[31m";
    const char def_mod[] = "\u001b[0m";
    std::stringstream ss;
    ss << red_mod << "Error: " << def_mod;
    (ss << ... << args);
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
    int line_number = 0;
    int col_number = 0;
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

Dataset average(Dataset &&dset, int nneighbours) {
     Dataset result(dset.size());
     int n_2 = nneighbours / 2;
     for (int i = 0; i < (int)dset.size(); ++i) {
         int from  = std::max(0, i - n_2);
         int to    = std::min((int)dset.size() - 1, i + n_2);
         for (int j = from; j <= to; ++j) {
            result[i].second += dset[j].second;
         }
         result[i].first = dset[i].first;
         result[i].second /= to - from + 1;
     }
     return result;
}

struct Graph {
    const int font_size = 12;
    const int panel_height = 50;
    const SDL_Color font_color { 0, 0, 0, 0 };
    const SDL_Color plot_bg { 22, 25, 37, 0 };
    const SDL_Color panel_bg { 253, 252, 254, 0 };
    const std::array<SDL_Color, 6> palette {
        SDL_Color { 35, 87, 135, 0 },
        SDL_Color { 193, 41, 46, 0 },
        SDL_Color { 241, 211, 2, 0 },
        SDL_Color { 224, 119, 125, 0 },
        SDL_Color { 81, 88, 187, 0 },
        SDL_Color { 242, 109, 249, 0 },
    };

    int resolution_x;
    int resolution_y;
    SDL_Renderer *renderer;
    SDL_Window *window;
    TTF_Font *font;

    Graph(int resolution_x, int resolution_y): resolution_x(resolution_x), resolution_y(resolution_y) {
        // Initializer SDL, renderer and window
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << fmt_error("Couldn't SDL: ", SDL_GetError()) << std::endl;
            exit(1);
        }

        SDL_CreateWindowAndRenderer(resolution_x, resolution_y, 0, &window, &renderer);
        if (window == NULL || renderer == NULL) {
            std::cerr << fmt_error("Couldn't create renderer and window: ", SDL_GetError()) << std::endl;
            exit(1);
        }

        // Initialize fonts
        if (TTF_Init() < 0) {
            std::cerr << fmt_error("Couldn't initialize SDL_ttf: ", TTF_GetError()) << std::endl;
            exit(1);
        }
        font = TTF_OpenFontRW(SDL_RWFromConstMem(resources_arial_ttf, resources_arial_ttf_len), 1, font_size);
    }

    template<typename TDsets, typename TLabels> 
    void render_scene(const TDsets &dsets, const TLabels &labels) {
        SDL_Rect plot_rect{0, 0, resolution_x, resolution_y - panel_height};
        render_plot(dsets, plot_rect);
        SDL_Rect panel_rect{0, resolution_y - panel_height, resolution_x, panel_height};
        render_panel(labels, panel_rect);
    }

    template<typename TCont>
    void render_plot(const TCont &dsets, SDL_Rect rect) {
        SDL_SetRenderDrawColor(renderer, plot_bg.r, plot_bg.g, plot_bg.b, plot_bg.a);
        SDL_RenderFillRect(renderer, &rect);
        for (int ndset = 0; ndset < dsets.size(); ++ndset) {
            SDL_SetRenderDrawColor(
                renderer, 
                palette[ndset].r, 
                palette[ndset].g, 
                palette[ndset].b, 
                palette[ndset].a);

            for (int index = 0; index < dsets[ndset].size() - 1; ++index) {
                auto [x1, y1] = dsets[ndset][index];
                auto [x2, y2] = dsets[ndset][index + 1];
                SDL_RenderDrawLine(
                    renderer, 
                    rect.w * x1 + rect.x,
                    rect.h * y1 + rect.y,
                    rect.w * x2 + rect.x,
                    rect.h * y2 + rect.y);
            }
        }
    }

    template<typename TCont>
    void render_panel(const TCont &labels, SDL_Rect rect) {
        SDL_SetRenderDrawColor(renderer, panel_bg.r, panel_bg.g, panel_bg.b, panel_bg.a);
        SDL_RenderFillRect(renderer, &rect);
        int i = 0;
        for (auto &label: labels) {
            const SDL_Color color = palette[i];
            SDL_Rect label_rect {
                rect.x + 100 * (i / 2),
                rect.y + (panel_height / 2) * (i % 2),
                100, 
                panel_height / 2 };
            render_panel_label(label, color, label_rect);
            ++i;
        }
    }

    void render_panel_label(std::string text, SDL_Color color, SDL_Rect rect) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Surface* font_surf = TTF_RenderText_Blended(font, text.c_str(), {0, 0, 0, 0}); 
        SDL_RenderDrawLine(
            renderer, 
            rect.x, 
            rect.y + rect.h / 2,
            rect.x + rect.w - font_surf->w,
            rect.y + rect.h / 2);
        SDL_Texture* font_tex = SDL_CreateTextureFromSurface(renderer, font_surf);
        SDL_Rect text_rect {
            rect.x + rect.w - font_surf->w,
            rect.y + (rect.h - font_surf->h) / 2,
            font_surf->w,
            font_surf->h };
        SDL_RenderCopy(renderer, font_tex, NULL, &text_rect);
        SDL_FreeSurface(font_surf);
        SDL_DestroyTexture(font_tex);
    }

    void present() {
        SDL_RenderPresent(renderer);
        for (SDL_Event event; event.type != SDL_QUIT; SDL_PollEvent(&event)) { /* no-op */ }
    }

    ~Graph() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        TTF_CloseFont(font);
        TTF_Quit();
    }
};

int main(int argc, char **argv) {
    const std::list<cli_parser::GenOption> options = {
        cli_parser::Option<int> {
            .full_name = "--average",
            .short_name = "-a",
            .description = "Every value becomes average between it's Ns neighbours."
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
        },
    };

    auto args = cli_parser::parse(argc, argv, options);
    if (args.occurs("--help", "-h")) {
        std::cout << fmt_help(options, VERSION) << std::endl;
        exit(0);
    }

    // Handle lack of input fnames
    if (!args.positional.size()) {
        std::cerr << fmt_error("You must provide at least one file to graph (see sg --help).") << std::endl;
        exit(1);
    } 

    // Handle separator
    char separator = args.get_value<char>("--sep", "-s").value_or(',');
    
    // Read datasets
    std::vector<Dataset> dsets;
    for (auto fname: args.positional) {
        auto mb_dset = csv_to_dataset(fname, separator);
        if (std::holds_alternative<std::string>(mb_dset)) {
            std::cerr << fmt_error(std::get<std::string>(mb_dset)) << std::endl;
            exit(1);
        }
        dsets.emplace_back(std::move(std::get<Dataset>(mb_dset)));
    }
    dsets = normilize_dsets(std::move(dsets));

    // Handle average option
    int nneighbours = args.get_value<int>("--average", "-a").value_or(1);
    if (nneighbours < 1) {
        std::cerr << fmt_error("Number of neigbours to average must be at least 1, not ", nneighbours) << std::endl;
        exit(1);
    }
    for (auto &dset: dsets) {
        dset = average(std::move(dset), nneighbours);
    }

    // Handle resolution preferences
    int width = args.get_value<int>("--res_x", "-x").value_or(512);
    int height = args.get_value<int>("--res_y", "-y").value_or(512);
    if (width <= 0 || height <= 0) {
        std::cerr << fmt_error("Resolution must be a positive number.") << std::endl;
        exit(1);
    }

    Graph graph(width, height);
    graph.render_scene(dsets, args.positional);
    graph.present();
    return 0;
}
