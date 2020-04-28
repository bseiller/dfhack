#include <modules/Gui.h>

#include "df/coord2d.h"
#include "df/inorganic_raw.h"
#include "df/dfhack_material_category.h"
#include "df/interface_key.h"
#include "df/viewscreen.h"
#include "df/viewscreen_choose_start_sitest.h"
#include "df/world.h"
#include "df/world_raws.h"

#include "finder_ui.h"
#include "help_ui.h"
#include "overlay.h"
#include "screen.h"
#include "survey.h"

#include <chrono>
#include <ctime>

using df::global::world;

namespace embark_assist {
    namespace overlay {
        DFHack::Plugin *plugin_self;
        const Screen::Pen empty_pen = Screen::Pen('\0', COLOR_YELLOW, COLOR_BLACK, false);
        const Screen::Pen yellow_x_pen = Screen::Pen('X', COLOR_BLACK, COLOR_YELLOW, false);
        const Screen::Pen green_x_pen = Screen::Pen('X', COLOR_BLACK, COLOR_GREEN, false);

        struct display_strings {
            Screen::Pen pen;
            std::string text;
        };

        typedef Screen::Pen *pen_column;

        struct states {
            int blink_count = 0;
            bool show = true;

            bool matching = false;
            bool match_active = false;

            embark_update_callbacks embark_update;
            match_callbacks match_callback;
            clear_match_callbacks clear_match_callback;
            embark_assist::defs::find_callbacks find_callback;
            shutdown_callbacks shutdown_callback;

            Screen::Pen site_grid[16][16];
            uint8_t current_site_grid = 0;

            std::vector<display_strings> embark_info;

            Screen::Pen local_match_grid[16][16];

            pen_column *world_match_grid = nullptr;
            uint16_t match_count = 0;

            uint16_t max_inorganic;

            bool fileresult = false;
            uint8_t fileresult_pass = 0;

            std::chrono::time_point<std::chrono::system_clock> start;
        };

        static states *state = nullptr;

        //====================================================================

        //  Logic for sizing the World map to the right.
        df::coord2d  world_dimension_size(uint16_t map_size, uint16_t region_size) {
            uint16_t factor = (map_size - 1 + region_size - 1) / region_size;
            uint16_t result = (map_size + ((factor - 1) / 2)) / factor;
            if (result > region_size) { result = region_size; }

            return{ result, factor};
        }

        //====================================================================

        class ViewscreenOverlay : public df::viewscreen_choose_start_sitest
        {
        public:
            typedef df::viewscreen_choose_start_sitest interpose_base;

            void send_key(const df::interface_key &key)
            {
                std::set< df::interface_key > keys;
                keys.insert(key);
                this->feed(&keys);
            }

            DEFINE_VMETHOD_INTERPOSE(void, feed, (std::set<df::interface_key> *input))
            {
//                color_ostream_proxy out(Core::getInstance().getConsole());
                if (input->count(df::interface_key::CUSTOM_Q)) {
                    state->shutdown_callback();
                    return;

                }
                else if (input->count(df::interface_key::SETUP_LOCAL_X_MUP) ||
                    input->count(df::interface_key::SETUP_LOCAL_X_MDOWN) ||
                    input->count(df::interface_key::SETUP_LOCAL_Y_MUP) ||
                    input->count(df::interface_key::SETUP_LOCAL_Y_MDOWN) ||
                    input->count(df::interface_key::SETUP_LOCAL_X_UP) ||
                    input->count(df::interface_key::SETUP_LOCAL_X_DOWN) ||
                    input->count(df::interface_key::SETUP_LOCAL_Y_UP) ||
                    input->count(df::interface_key::SETUP_LOCAL_Y_DOWN)) {
                    INTERPOSE_NEXT(feed)(input);
                    state->embark_update();
                }
                else if (input->count(df::interface_key::CUSTOM_C)) {
                    if (state->matching) {
                        state->matching = false;
                    }
                    else {
                        state->match_active = false;
                        state->clear_match_callback();
                    }
                }
                else if (input->count(df::interface_key::CUSTOM_F)) {
                    if (!state->match_active && !state->matching) {
                        embark_assist::finder_ui::init(embark_assist::overlay::plugin_self, state->find_callback, state->max_inorganic, false);
                    }
                }
                else if (input->count(df::interface_key::CUSTOM_I)) {
                    embark_assist::help_ui::init(embark_assist::overlay::plugin_self);
                }
                else {
                    INTERPOSE_NEXT(feed)(input);
                }
            }

            //====================================================================

            DEFINE_VMETHOD_INTERPOSE(void, render, ())
            {
                INTERPOSE_NEXT(render)();
//                color_ostream_proxy out(Core::getInstance().getConsole());
                auto current_screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
                int16_t x = current_screen->location.region_pos.x;
                int16_t y = current_screen->location.region_pos.y;
                auto width = Screen::getWindowSize().x;
                auto height = Screen::getWindowSize().y;

                state->blink_count++;
                if (state->blink_count == 35) {
                    state->blink_count = 0;
                    state->show = !state->show;
                }

                if (state->matching) state->show = true;

                Screen::drawBorder("  Embark Assistant  ");

                Screen::Pen pen_lr(' ', COLOR_LIGHTRED);
                Screen::Pen pen_w(' ', COLOR_WHITE);
                Screen::Pen pen_g(' ', COLOR_GREY);

                Screen::paintString(pen_lr, width - 28, 20, DFHack::Screen::getKeyDisplay(df::interface_key::CUSTOM_I).c_str(), false);
                Screen::paintString(pen_w, width - 27, 20, ": Embark Assistant Info", false);
                Screen::paintString(pen_lr, width - 28, 21, DFHack::Screen::getKeyDisplay(df::interface_key::CUSTOM_F).c_str(), false);
                Screen::paintString(pen_w, width - 27, 21, ": Find Embark ", false);
                Screen::paintString(pen_lr, width - 28, 22, DFHack::Screen::getKeyDisplay(df::interface_key::CUSTOM_C).c_str(), false);
                Screen::paintString(pen_w, width - 27, 22, ": Cancel/Clear Find", false);
                Screen::paintString(pen_lr, width - 28, 23, DFHack::Screen::getKeyDisplay(df::interface_key::CUSTOM_Q).c_str(), false);
                Screen::paintString(pen_w, width - 27, 23, ": Quit Embark Assistant", false);
                Screen::paintString(pen_w, width - 28, 25, "Matching World Tiles:", false);
                Screen::paintString(empty_pen, width - 6, 25, to_string(state->match_count), false);
                Screen::paintString(pen_g, width - 28, 26, "(Those on the Region Map)", false);

                if (height > 25) {  //  Mask the vanilla DF find help as it's overridden.
                    Screen::paintString(pen_w, 50, height - 2, "                          ", false);
                }

                for (uint8_t i = 0; i < 16; i++) {
                    for (uint8_t k = 0; k < 16; k++) {
                        if (state->site_grid[i][k].ch) {
                            Screen::paintTile(state->site_grid[i][k], i + 1, k + 2);
                        }
                    }
                }

                for (size_t i = 0; i < state->embark_info.size(); i++) {
                    embark_assist::screen::paintString(state->embark_info[i].pen, 1, i + 19, state->embark_info[i].text, false);
                }

                if (state->show) {
                    int16_t left_x = x - (width / 2 - 7 - 18 + 1) / 2;
                    int16_t right_x;
                    int16_t top_y = y - (height - 8 - 2 + 1) / 2;
                    int16_t bottom_y;

                    if (left_x < 0) { left_x = 0; }

                    if (top_y < 0) { top_y = 0; }

                    right_x = left_x + width / 2 - 7 - 18;
                    bottom_y = top_y + height - 8 - 2;

                    if (right_x >= world->worldgen.worldgen_parms.dim_x) {
                        right_x = world->worldgen.worldgen_parms.dim_x - 1;
                        left_x = right_x - (width / 2 - 7 - 18);
                    }

                    if (bottom_y >= world->worldgen.worldgen_parms.dim_y) {
                        bottom_y = world->worldgen.worldgen_parms.dim_y - 1;
                        top_y = bottom_y - (height - 8 - 2);
                    }

                    if (left_x < 0) { left_x = 0; }

                    if (top_y < 0) { top_y = 0; }


                    for (uint16_t i = left_x; i <= right_x; i++) {
                        for (uint16_t k = top_y; k <= bottom_y; k++) {
                            if (state->world_match_grid[i][k].ch) {
                                Screen::paintTile(state->world_match_grid[i][k], i - left_x + 18, k - top_y + 2);
                            }
                        }
                    }

                    for (uint8_t i = 0; i < 16; i++) {
                        for (uint8_t k = 0; k < 16; k++) {
                            if (state->local_match_grid[i][k].ch) {
                                Screen::paintTile(state->local_match_grid[i][k], i + 1, k + 2);
                            }
                        }
                    }

                    df::coord2d size_factor_x = world_dimension_size(world->worldgen.worldgen_parms.dim_x, width / 2 - 24);
                    df::coord2d size_factor_y = world_dimension_size(world->worldgen.worldgen_parms.dim_y, height - 9);

                    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
                        for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
                            if (state->world_match_grid[i][k].ch) {
                                Screen::paintTile(state->world_match_grid[i][k], width / 2 - 5 + min(size_factor_x.x - 1, i / size_factor_x.y), 2 + min(size_factor_y.x - 1, k / size_factor_y.y));
                            }
                        }
                    }
                }

                if (state->matching) {
                    embark_assist::overlay::state->match_callback();
                }
            }
        };

        IMPLEMENT_VMETHOD_INTERPOSE(embark_assist::overlay::ViewscreenOverlay, feed);
        IMPLEMENT_VMETHOD_INTERPOSE(embark_assist::overlay::ViewscreenOverlay, render);
    }
}

//====================================================================

bool embark_assist::overlay::setup(DFHack::Plugin *plugin_self,
    embark_update_callbacks embark_update_callback,
    match_callbacks match_callback,
    clear_match_callbacks clear_match_callback,
    embark_assist::defs::find_callbacks find_callback,
    shutdown_callbacks shutdown_callback,
    uint16_t max_inorganic)
{
//    color_ostream_proxy out(Core::getInstance().getConsole());
    state = new(states);

    embark_assist::overlay::plugin_self = plugin_self;
    embark_assist::overlay::state->embark_update = embark_update_callback;
    embark_assist::overlay::state->match_callback = match_callback;
    embark_assist::overlay::state->clear_match_callback = clear_match_callback;
    embark_assist::overlay::state->find_callback = find_callback;
    embark_assist::overlay::state->shutdown_callback = shutdown_callback;
    embark_assist::overlay::state->max_inorganic = max_inorganic;
    embark_assist::overlay::state->match_active = false;

    state->world_match_grid = new pen_column[world->worldgen.worldgen_parms.dim_x];
    if (!state->world_match_grid) {
        return false;  //  Out of memory
    }

    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        state->world_match_grid[i] = new Screen::Pen[world->worldgen.worldgen_parms.dim_y];
        if (!state->world_match_grid[i]) {  //  Out of memory.
            return false;
        }
    }

    clear_match_results();

    return INTERPOSE_HOOK(embark_assist::overlay::ViewscreenOverlay, feed).apply(true) &&
        INTERPOSE_HOOK(embark_assist::overlay::ViewscreenOverlay, render).apply(true);
}

//====================================================================

void embark_assist::overlay::set_sites(embark_assist::defs::site_lists *site_list) {
    for (uint8_t i = 0; i < 16; i++) {
        for (uint8_t k = 0; k < 16; k++) {
            state->site_grid[i][k] = empty_pen;
        }
    }

    for (uint16_t i = 0; i < site_list->size(); i++) {
        state->site_grid[site_list->at(i).x][site_list->at(i).y].ch = site_list->at(i).type;
    }
}

//====================================================================

void embark_assist::overlay::initiate_match() {
    embark_assist::overlay::state->matching = true;

    color_ostream_proxy out(Core::getInstance().getConsole());

    const auto start = std::chrono::system_clock::now();
    embark_assist::overlay::state->start = start;
    const std::time_t start_time = std::chrono::system_clock::to_time_t(start);
    out.print("embark_assist::overlay::initiate_match started at: %s\n", std::ctime(&start_time));
}

//====================================================================

void init_stream(std::ostringstream &oss, std::chrono::duration<double> duration) {
    auto d(duration.count());
    oss << std::setfill('0')
        << std::setw(2)
        << (int)(d / 60) // format minutes
        << ":"
        << std::setw(2)
        << (int)std::fmod(d, 60) // format seconds
        << ":"
        << std::setw(3) // set width of milliseconds field
        << (int)std::fmod(d * 1000, 1000);
}

//====================================================================

void format_and_output_duration(const char *format, std::chrono::duration<double> &duration) {
    color_ostream_proxy out(Core::getInstance().getConsole());
    std::ostringstream oss;
    init_stream(oss, duration);

    out.print(format, oss.str(), duration.count());

    duration = std::chrono::seconds(0);
}

//====================================================================

void embark_assist::overlay::match_progress(uint16_t count, embark_assist::defs::match_results *match_results, bool done) {
    color_ostream_proxy out(Core::getInstance().getConsole());
    state->matching = !done;
    state->match_count = count;

    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
            if (match_results->at(i).at(k).preliminary_match) {
                state->world_match_grid[i][k] = yellow_x_pen;

            } else if (match_results->at(i).at(k).contains_match) {
                state->world_match_grid[i][k] = green_x_pen;
            }
            else {
                state->world_match_grid[i][k] = empty_pen;
            }
        }
    }

    if (done) {
        const auto end = std::chrono::system_clock::now();
        const std::chrono::duration<double> elapsed_seconds = end - state->start;
        const std::time_t end_time = std::chrono::system_clock::to_time_t(end);

        std::ostringstream oss;
        init_stream(oss, elapsed_seconds);
        out.print("embark_assist::overlay::match_progress: finished search at: %s - elapsed time formatted: %s (m:s:ms) - %f seconds \n", std::ctime(&end_time), oss.str(), elapsed_seconds.count());
        
        format_and_output_duration("embark_assist::survey::survey_mid_level_tile - elapsed_init took: %s (m:s:ms) - %f seconds \n", embark_assist::survey::elapsed_init_seconds);
        format_and_output_duration("embark_assist::survey::survey_mid_level_tile - elapsed_big_loop took: %s (m:s:ms) - %f seconds \n", embark_assist::survey::elapsed_big_loop_seconds);
        format_and_output_duration("embark_assist::survey::survey_mid_level_tile - elapsed_river_workaround took: %s (m:s:ms) - %f seconds \n", embark_assist::survey::elapsed_workaround_seconds);
        format_and_output_duration("embark_assist::survey::survey_mid_level_tile - elapsed_tile_summary took: %s (m:s:ms) - %f seconds \n", embark_assist::survey::elapsed_tile_summary_seconds);
        format_and_output_duration("embark_assist::survey::survey_mid_level_tile - elapsed_waterfall_and_biomes took: %s (m:s:ms) - %f seconds \n", embark_assist::survey::elapsed_waterfall_and_biomes_seconds);

        format_and_output_duration("embark_assist::survey::survey_mid_level_tile took: %s (m:s:ms) - %f seconds \n", embark_assist::survey::elapsed_survey_seconds);
        format_and_output_duration("embark_assist::survey::survey_mid_level_tile took: %s (m:s:ms) - %f seconds including index.add total \n", embark_assist::survey::elapsed_survey_total_seconds);

        out.print("number of found waterfalls: %d\n", embark_assist::survey::number_of_waterfalls);
        out.print("number of layer_cache misses: %d\n", embark_assist::survey::number_of_layer_cache_misses);
        out.print("number of layer_cache hits: %d\n", embark_assist::survey::number_of_layer_cache_hits);
        embark_assist::survey::number_of_waterfalls = 0;
    }

    if (done && state->fileresult) {
        state->fileresult_pass++;
        if (state->fileresult_pass == 1) {
            embark_assist::finder_ui::init(embark_assist::overlay::plugin_self, state->find_callback, state->max_inorganic, true);
        }
        else {
            FILE* outfile = fopen(fileresult_file_name, "w");
            fprintf(outfile, "%i\n", count);
            fclose(outfile);
        }
    }
}

//====================================================================

void embark_assist::overlay::set_embark(embark_assist::defs::site_infos *site_info) {
    state->embark_info.clear();

    if (!site_info->incursions_processed) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_LIGHTRED), "Incomp. Survey" });
    }

    if (site_info->sand) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_YELLOW), "Sand" });
    }

    if (site_info->clay) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_RED), "Clay" });
    }

    if (site_info->coal) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_GREY), "Coal" });
    }

    state->embark_info.push_back({ Screen::Pen(' ', COLOR_BROWN), "Soil " + std::to_string(site_info->min_soil) + " - " + std::to_string(site_info->max_soil) });

    if (site_info->flat) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_BROWN), "Flat" });
    }

    if (site_info->aquifer) {
        if (site_info->aquifer_full) {
            state->embark_info.push_back({ Screen::Pen(' ', COLOR_LIGHTBLUE), "Full Aquifer" });

        }
        else {
            state->embark_info.push_back({ Screen::Pen(' ', COLOR_LIGHTBLUE), "Part. Aquifer" });
        }
    }

    if (site_info->max_waterfall > 0) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_LIGHTBLUE), "Waterfall " + std::to_string(site_info->max_waterfall) });
    }

    if (site_info->blood_rain ||
        site_info->permanent_syndrome_rain ||
        site_info->temporary_syndrome_rain ||
        site_info->reanimating ||
        site_info->thralling) {
        std::string blood_rain;
        std::string permanent_syndrome_rain;
        std::string temporary_syndrome_rain;
        std::string reanimating;
        std::string thralling;

        if (site_info->blood_rain) {
            blood_rain = "BR ";
        }
        else {
            blood_rain = "   ";
        }

        if (site_info->permanent_syndrome_rain) {
            permanent_syndrome_rain = "PS ";
        }
        else {
            permanent_syndrome_rain = "   ";
        }

        if (site_info->temporary_syndrome_rain) {
            temporary_syndrome_rain = "TS ";
        }
        else {
            permanent_syndrome_rain = "   ";
        }

        if (site_info->reanimating) {
            reanimating = "Re ";
        }
        else {
            reanimating = "   ";
        }

        if (site_info->thralling) {
            thralling = "Th";
        }
        else {
            thralling = "  ";
        }

        state->embark_info.push_back({ Screen::Pen(' ', COLOR_LIGHTRED), blood_rain + temporary_syndrome_rain + permanent_syndrome_rain + reanimating + thralling });
    }

    if (site_info->flux) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_WHITE), "Flux" });
    }

    for (auto const& i : site_info->metals) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_GREY), world->raws.inorganics[i]->id });
    }

    for (auto const& i : site_info->economics) {
        state->embark_info.push_back({ Screen::Pen(' ', COLOR_WHITE), world->raws.inorganics[i]->id });
    }
}

//====================================================================

void embark_assist::overlay::set_mid_level_tile_match(embark_assist::defs::mlt_matches mlt_matches) {
    for (uint8_t i = 0; i < 16; i++) {
        for (uint8_t k = 0; k < 16; k++) {
            if (mlt_matches[i][k]) {
                state->local_match_grid[i][k] = green_x_pen;

            }
            else {
                state->local_match_grid[i][k] = empty_pen;
            }
        }
    }
}

//====================================================================

void embark_assist::overlay::clear_match_results() {
    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
            state->world_match_grid[i][k] = empty_pen;
        }
    }

    for (uint8_t i = 0; i < 16; i++) {
        for (uint8_t k = 0; k < 16; k++) {
            state->local_match_grid[i][k] = empty_pen;
        }
    }
}

//====================================================================

void embark_assist::overlay::fileresult() {
    //  Have to search twice, as the first pass cannot be complete due to mutual dependencies.
    state->fileresult = true;
    embark_assist::finder_ui::init(embark_assist::overlay::plugin_self, state->find_callback, state->max_inorganic, true);
}

//====================================================================

void embark_assist::overlay::shutdown() {
    if (state &&
        state->world_match_grid) {
        INTERPOSE_HOOK(ViewscreenOverlay, render).remove();
        INTERPOSE_HOOK(ViewscreenOverlay, feed).remove();

        for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
            delete[] state->world_match_grid[i];
        }

        delete[] state->world_match_grid;
    }

    if (state) {
        state->embark_info.clear();
        delete state;
        state = nullptr;
    }
}
