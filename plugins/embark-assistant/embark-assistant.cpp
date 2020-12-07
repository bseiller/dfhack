#include <time.h>

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "modules/Gui.h"
#include "modules/Screen.h"
#include "../uicommon.h"

#include "DataDefs.h"
#include "df/coord2d.h"
#include "df/inorganic_flags.h"
#include "df/inorganic_raw.h"
#include "df/interfacest.h"
#include "df/viewscreen.h"
#include "df/viewscreen_choose_start_sitest.h"
#include "df/world.h"
#include "df/world_data.h"
#include "df/world_geo_biome.h"
#include "df/world_raws.h"

#include "defs.h"
#include "embark-assistant.h"
#include "finder_ui.h"
#include "matcher.h"
#include "overlay.h"
#include "survey.h"
#include "index.h"

DFHACK_PLUGIN("embark-assistant");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

using namespace DFHack;
using namespace df::enums;
using namespace Gui;

REQUIRE_GLOBAL(world);

namespace embark_assist {
    namespace main {
        struct states {
            embark_assist::defs::geo_data geo_summary;
            embark_assist::defs::world_tile_data survey_results;
            embark_assist::defs::site_lists region_sites;
            embark_assist::defs::site_infos site_info;
            embark_assist::defs::match_results match_results;
            embark_assist::defs::match_iterators match_iterator;
            uint16_t max_inorganic;
            //embark_assist::index::Index index = embark_assist::index::Index(world, match_results, match_iterator.finder);
            embark_assist::index::Index index{ world, match_results, match_iterator.finder };
            //embark_assist::index::Index *index;
            bool testing = false;
            embark_assist::defs::mid_level_tiles mlt;
        };

        static states *state = nullptr;

        void embark_update ();
        void shutdown();

        //===============================================================================

        void embark_update() {
            // not updating the embark screen/data during an active find/match/search phase
            // which leads to better performance and probably prevents nasty hard to track errors
            if (state != nullptr && state->match_iterator.active) {
                return;
            }

            auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
            embark_assist::defs::mid_level_tiles &mlt = state->mlt;
            //embark_assist::survey::initiate(&mlt);

            embark_assist::survey::survey_mid_level_tile(&state->geo_summary,
                &state->survey_results,
                &mlt,
                state->index,
                embark_assist::survey::output_coordinates::SHOW_COORDINATES);

            embark_assist::survey::survey_embark(&mlt,
                &state->survey_results,
                &state->site_info,
                false);
            embark_assist::overlay::set_embark(&state->site_info);

            embark_assist::survey::survey_region_sites(&state->region_sites);
            embark_assist::overlay::set_sites(&state->region_sites);

            embark_assist::overlay::set_mid_level_tile_match(state->match_results.at(screen->location.region_pos.x).at(screen->location.region_pos.y).mlt_match);
        }

        //===============================================================================

        // FIXME: for debugging, remove for release
        void check_all_processed(embark_assist::defs::world_tile_data &survey_results) {
            const std::string prefix = "world_tiles_without_incursion_processing";
            auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);
            myfile << "x\ty\trequired_count\tactual_count\n";
            uint16_t count = 0;
            for (uint16_t x = 0; x < world->worldgen.worldgen_parms.dim_x; x++) {
                for (uint16_t y = 0; y < world->worldgen.worldgen_parms.dim_y; y++) {
                    embark_assist::defs::region_tile_datum &rtd = survey_results.at(x)[y];
                    if (!rtd.totally_processed) {
                        myfile << x <<"\t" << y << "\t" << unsigned(rtd.required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing) << "\t" << unsigned(rtd.number_of_contiguous_surveyed_world_tiles) << "\n";
                        ++count;
                    }
                }
            }
            myfile << "number of unprocessed tiles: " << count << "\n";
            myfile.close();
        }

        void match() {
            color_ostream_proxy out(Core::getInstance().getConsole());

            // FIXME: debug code remove again
            const bool isTesting = true;

            if (!isTesting && state->index.containsEntries()) {
                // should prevent subsequent survey phases, once the first survey phase is complete and the index is primed
                out.print("embark_assistant::match: index already contains all entries - directly searching/finding matches\n");
                // embark_assist::main::clear_match();
                embark_assist::main::state->match_iterator.active = false;
                embark_assist::main::state->testing = false;
                state->index.find_all_matches(state->match_iterator.finder, state->match_results);
                embark_assist::overlay::match_progress(0, &state->match_results, true);
            } else {
                uint16_t count = embark_assist::matcher::find(&state->match_iterator,
                    &state->geo_summary,
                    &state->survey_results,
                    state->index,
                    &state->match_results);

                // FIXME: remove the following if block containing "find_all_matches" as we don't need it anymore once the iterative search during the survey phase is properly implemented
                if (isTesting && !state->match_iterator.active) {
                    state->index.find_all_matches(state->match_iterator.finder, state->match_results);
                }
                embark_assist::overlay::match_progress(count, &state->match_results, !state->match_iterator.active);
            }

            if (!state->match_iterator.active) {
                state->index.optimize(true);
                // FIXME: for debugging, remove for release
                check_all_processed(state->survey_results);
                auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
                embark_assist::overlay::set_mid_level_tile_match(state->match_results.at(screen->location.region_pos.x).at(screen->location.region_pos.y).mlt_match);
            }

            // testing == true means the code loops over all available embark profiles and writes the results/deltas between matcher and index results into a separate file
            if (state->testing) {
                // as long as there is another profile advance to next profile file name
                // FIXME: use next profile file name from encapsulated logic/class/iterator
                embark_assist::overlay::testing(nullptr);
            }
        }

        //===============================================================================

        void clear_match() {
//            color_ostream_proxy out(Core::getInstance().getConsole());
            if (state->match_iterator.active) {
                embark_assist::matcher::move_cursor(state->match_iterator.x, state->match_iterator.y);
            }
            embark_assist::survey::clear_results(&state->match_results);
            embark_assist::overlay::clear_match_results();
            embark_assist::main::state->match_iterator.active = false;
            embark_assist::main::state->testing = false;
        }

        //===============================================================================

        void find(const embark_assist::defs::finders &finder) {
//            color_ostream_proxy out(Core::getInstance().getConsole());

            state->match_iterator.x = embark_assist::survey::get_last_pos().x;
            state->match_iterator.y = embark_assist::survey::get_last_pos().y;
            state->match_iterator.finder = finder;
            state->index.init_for_iterative_find();
            embark_assist::overlay::initiate_match();
        }

        //===============================================================================

        void shutdown() {
//            color_ostream_proxy out(Core::getInstance().getConsole());
            embark_assist::survey::shutdown();
            embark_assist::matcher::shutdown();
            embark_assist::finder_ui::shutdown();
            embark_assist::overlay::shutdown();
            state->index.shutdown();
            delete state;
            state = nullptr;
        }
    }
}

//=======================================================================================

command_result embark_assistant (color_ostream &out, std::vector <std::string> & parameters);

//=======================================================================================

struct start_site_hook : df::viewscreen_choose_start_sitest {
    typedef df::viewscreen_choose_start_sitest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, render, ())
    {
        INTERPOSE_NEXT(render)();
        if (embark_assist::main::state)
            return;
        auto dims = Screen::getWindowSize();
        int x = 60;
        int y = dims.y - 2;
        OutputString(COLOR_LIGHTRED, x, y, " " + Screen::getKeyDisplay(interface_key::CUSTOM_A));
        OutputString(COLOR_WHITE, x, y, ": Embark ");
        OutputString(COLOR_WHITE, x, y, dims.x > 82 ? "Assistant" : "Asst.");
    }

    DEFINE_VMETHOD_INTERPOSE(void, feed, (std::set<df::interface_key> *input))
    {
        if (!embark_assist::main::state && input->count(interface_key::CUSTOM_A))
        {
            Core::getInstance().setHotkeyCmd("embark-assistant");
            return;
        }
        INTERPOSE_NEXT(feed)(input);
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(start_site_hook, render);
IMPLEMENT_VMETHOD_INTERPOSE(start_site_hook, feed);

//=======================================================================================

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "embark-assistant", "Embark site selection support.",
        embark_assistant, false, /* false means that the command can be used from non-interactive user interface */
        // Extended help string. Used by CR_WRONG_USAGE and the help command:
        "  This command starts the embark-assist plugin that provides embark site\n"
        "  selection help. It has to be called while the pre-embark screen is\n"
        "  displayed and shows extended (and correct(?)) resource information for\n"
        "  the embark rectangle as well as normally undisplayed sites in the\n"
        "  current embark region. It also has a site selection tool with more\n"
        "  options than DF's vanilla search tool. For detailed help invoke the\n"
        "  in game info screen. Prefers 46 lines to display properly.\n"
    ));
    return CR_OK;
}

//=======================================================================================

DFhackCExport command_result plugin_shutdown (color_ostream &out)
{
    if (embark_assist::main::state) {
        embark_assist::main::shutdown();
    }
    return CR_OK;
}

//=======================================================================================

DFhackCExport command_result plugin_enable (color_ostream &out, bool enable)
{
    if (is_enabled != enable)
    {
        if (!INTERPOSE_HOOK(start_site_hook, render).apply(enable) ||
            !INTERPOSE_HOOK(start_site_hook, feed).apply(enable))
        {
            return CR_FAILURE;
        }
        is_enabled = enable;
    }
    return CR_OK;
}

//=======================================================================================

DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    switch (event) {
      case DFHack::SC_UNKNOWN:
          break;

      case DFHack::SC_WORLD_LOADED:
          break;

      case DFHack::SC_WORLD_UNLOADED:
      case DFHack::SC_MAP_LOADED:
          if (embark_assist::main::state) {
              embark_assist::main::shutdown();
          }
          break;

      case DFHack::SC_MAP_UNLOADED:
          break;

      case DFHack::SC_VIEWSCREEN_CHANGED:
          break;

      case DFHack::SC_CORE_INITIALIZED:
          break;

      case DFHack::SC_BEGIN_UNLOAD:
          break;

      case DFHack::SC_PAUSED:
          break;

      case DFHack::SC_UNPAUSED:
          break;
    }
    return CR_OK;
}


//=======================================================================================

command_result embark_assistant(color_ostream &out, std::vector <std::string> & parameters)
{
//#ifdef _MSC_VER
//    std::cout << std::to_string(_MSC_VER) << std::endl;
//#endif
//    std::cout << __cplusplus << std::endl;

    bool fileresult = false;
    bool testing = false;

    if (parameters.size() == 1 &&
        parameters[0] == "fileresult") {
        remove(fileresult_file_name);
        fileresult = true;
    } if (parameters.size() == 1 &&
        parameters[0] == "testing") {
        testing = true;
    } else
    if (!parameters.empty())
        return CR_WRONG_USAGE;

    CoreSuspender suspend;

    auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
    if (!screen) {
        out.printerr("This plugin works only in the embark site selection phase.\n");
        return CR_WRONG_USAGE;
    }

    df::world_data *world_data = world->world_data;

    if (embark_assist::main::state) {
        out.printerr("You can't invoke the embark assistant while it's already active.\n");
        return CR_WRONG_USAGE;
    }

    embark_assist::main::state = new embark_assist::main::states;

    embark_assist::main::state->match_iterator.active = false;
    embark_assist::main::state->testing = testing;

    //  Find the end of the normal inorganic definitions.
    embark_assist::main::state->max_inorganic = 0;
    for (uint16_t i = world->raws.inorganics.size() - 1; i >= 0 ; i--) {
        if (!world->raws.inorganics[i]->flags.is_set(df::inorganic_flags::GENERATED)) {
            embark_assist::main::state->max_inorganic = i;
            break;
        }
    }
    embark_assist::main::state->max_inorganic++;  //  To allow it to be used as size() replacement

    if (!embark_assist::overlay::setup(plugin_self,
        embark_assist::main::embark_update,
        embark_assist::main::match,
        embark_assist::main::clear_match,
        embark_assist::main::find,
        embark_assist::main::shutdown,
        embark_assist::main::state->max_inorganic)) {
        return CR_FAILURE;
    }

    embark_assist::survey::setup(embark_assist::main::state->max_inorganic);
    embark_assist::matcher::setup();
    embark_assist::main::state->index.setup(embark_assist::main::state->max_inorganic);
    embark_assist::main::state->geo_summary.resize(world_data->geo_biomes.size());
    embark_assist::main::state->survey_results.resize(world->worldgen.worldgen_parms.dim_x);

    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        embark_assist::main::state->survey_results[i].resize(world->worldgen.worldgen_parms.dim_y);

        for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
            embark_assist::main::state->survey_results[i][k].surveyed = false;
            embark_assist::main::state->survey_results[i][k].aquifer_count = 0;
            embark_assist::main::state->survey_results[i][k].clay_count = 0;
            embark_assist::main::state->survey_results[i][k].sand_count = 0;
            embark_assist::main::state->survey_results[i][k].flux_count = 0;
            embark_assist::main::state->survey_results[i][k].min_region_soil = 10;
            embark_assist::main::state->survey_results[i][k].max_region_soil = 0;
            embark_assist::main::state->survey_results[i][k].max_waterfall = 0;
            embark_assist::main::state->survey_results[i][k].river_size = embark_assist::defs::river_sizes::None;

            for (uint8_t l = 1; l < 10; l++) {
                embark_assist::main::state->survey_results[i][k].biome_index[l] = -1;
                embark_assist::main::state->survey_results[i][k].biome[l] = -1;
                embark_assist::main::state->survey_results[i][k].blood_rain[l] = false;
                embark_assist::main::state->survey_results[i][k].permanent_syndrome_rain[l] = false;
                embark_assist::main::state->survey_results[i][k].temporary_syndrome_rain[l] = false;
                embark_assist::main::state->survey_results[i][k].reanimating[l] = false;
                embark_assist::main::state->survey_results[i][k].thralling[l] = false;
            }

            for (uint8_t l = 0; l < 2; l++) {
                embark_assist::main::state->survey_results[i][k].savagery_count[l] = 0;
                embark_assist::main::state->survey_results[i][k].evilness_count[l] = 0;
            }
            embark_assist::main::state->survey_results[i][k].metals.resize(embark_assist::main::state->max_inorganic);
            embark_assist::main::state->survey_results[i][k].economics.resize(embark_assist::main::state->max_inorganic);
            embark_assist::main::state->survey_results[i][k].minerals.resize(embark_assist::main::state->max_inorganic);
        }
    }

    embark_assist::survey::high_level_world_survey(&embark_assist::main::state->geo_summary,
        &embark_assist::main::state->survey_results);

    embark_assist::main::state->match_results.resize(world->worldgen.worldgen_parms.dim_x);

    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        embark_assist::main::state->match_results[i].resize(world->worldgen.worldgen_parms.dim_y);
    }

    embark_assist::survey::clear_results(&embark_assist::main::state->match_results);
    embark_assist::survey::survey_region_sites(&embark_assist::main::state->region_sites);
    embark_assist::overlay::set_sites(&embark_assist::main::state->region_sites);

    embark_assist::defs::mid_level_tiles &mlt = embark_assist::main::state->mlt;
    //embark_assist::survey::initiate(&mlt);*/
    embark_assist::survey::survey_mid_level_tile(&embark_assist::main::state->geo_summary, &embark_assist::main::state->survey_results, &mlt, embark_assist::main::state->index, embark_assist::survey::output_coordinates::SHOW_COORDINATES);
    embark_assist::survey::survey_embark(&mlt, &embark_assist::main::state->survey_results, &embark_assist::main::state->site_info, false);
    embark_assist::overlay::set_embark(&embark_assist::main::state->site_info);

    if (fileresult) {
        embark_assist::overlay::fileresult();
    } else if (testing) {
        // FIXME: use first profile file name => encapsulate logic of retrieving all test profile file names
        embark_assist::overlay::testing(nullptr);
    }

    return CR_OK;
}
