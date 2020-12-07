#pragma once
//#include <map>

#include "DataDefs.h"
#include "df/coord2d.h"

#include "defs.h"

using namespace DFHack;

namespace embark_assist {
    namespace survey {
        extern std::chrono::duration<double> elapsed_init_seconds;
        extern std::chrono::duration<double> elapsed_big_loop_seconds;
        extern std::chrono::duration<double> elapsed_workaround_seconds;
        extern std::chrono::duration<double> elapsed_tile_summary_seconds;
        extern std::chrono::duration<double> elapsed_waterfall_and_biomes_seconds;
        extern std::chrono::duration<double> elapsed_survey_seconds;
        extern std::chrono::duration<double> elapsed_survey_total_seconds;
        extern uint32_t number_of_waterfalls;

        extern uint32_t number_of_layer_cache_misses;
        extern uint32_t number_of_layer_cache_hits;

        enum class output_coordinates {
            SHOW_COORDINATES,
            DONT_SHOW_COORDINATES
        };

        void setup(uint16_t max_inorganic);

        df::coord2d get_last_pos();

        void initiate(embark_assist::defs::mid_level_tiles *mlt);

        void clear_results(embark_assist::defs::match_results *match_results);

        void high_level_world_survey(embark_assist::defs::geo_data *geo_summary,
            embark_assist::defs::world_tile_data *survey_results);

        void survey_mid_level_tile(embark_assist::defs::geo_data *geo_summary,
            embark_assist::defs::world_tile_data *survey_results,
            embark_assist::defs::mid_level_tiles *mlt,
            embark_assist::defs::index_interface &index,
            const output_coordinates output_coordinates);

        df::coord2d apply_offset(uint16_t x, uint16_t y, int8_t offset);

        df::world_region_type region_type_of(const embark_assist::defs::world_tile_data *survey_results,
            int16_t x,
            int16_t y,
            int8_t i,
            int8_t k);

        //  Returns the direction from which data should be read using DF's
        //  0 - 8 range direction indication.
        //  "corner_location" uses the 0 - 8 notation to indicate the reader's
        //  relation to the data read. Only some values are legal, as the set of
        //  relations is limited by how the corners are defined.
        //  x, y, i, k are the world tile/mid level tile coordinates of the
        //  tile the results should be applied to.
        //  Deals with references outside of the world map by returning "yield"
        //  results, but requires all world tiles affected by the corner to have
        //  been surveyed.
        //
        uint8_t translate_corner(const embark_assist::defs::world_tile_data *survey_results,
            uint8_t corner_location,
            uint16_t x,
            uint16_t y,
            uint8_t i,
            uint8_t k);

        //  Same logic and restrictions as for translate_corner.
        //
        uint8_t translate_ns_edge(embark_assist::defs::world_tile_data *survey_results,
            bool own_edge,
            uint16_t x,
            uint16_t y,
            uint8_t i,
            uint8_t k);

        //  Same logic and restrictions as for translate_corner.
        //
        uint8_t translate_ew_edge(embark_assist::defs::world_tile_data *survey_results,
            bool own_edge,
            uint16_t x,
            uint16_t y,
            uint8_t i,
            uint8_t k);

        void survey_region_sites(embark_assist::defs::site_lists *site_list);

        void survey_embark(embark_assist::defs::mid_level_tiles *mlt,
            embark_assist::defs::world_tile_data *survey_results,
            embark_assist::defs::site_infos *site_info,
            bool use_cache);

        void shutdown();
    }
}