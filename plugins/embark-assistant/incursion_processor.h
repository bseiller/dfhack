#pragma once

#include "basic_key_buffer_holder.h"
#include "defs.h"
// only for debuggin
#include "roaring.hh"

namespace embark_assist {
    namespace incursion {
        class incursion_processor {
        private:
            const static int size_of_internal_incursion_buffer = 
            // during the internal incursion processing there are 2 edges (east + south) and the south-east corner for 15 * 15 tiles, 
                15 * 15 * (2 + 3)
            // + the east edges for 15 tiles (southmost row) and the south edges for 15 tiles (eastmost column)
                + 15 + 15
            // + there can be more corners in certain cases => maximal case NORTH-WEST + 15 corners (north east corners of the northmost row) + 15 corners (south-west corners of the westmost column)
                + 15 + 15;
            typedef embark_assist::key_buffer_holder::basic_key_buffer_holder<size_of_internal_incursion_buffer> internal_incursion_buffer;
            //embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &internal_buffer;
            embark_assist::key_buffer_holder::key_buffer_holder_basic_interface *internal_buffer;

            const static int size_of_external_incursion_buffer = 
                // during the external incursion processing there are 2 edges (east + south), 
                15 + 15 
                // and the south - east corner for east column 15 tiles * 3 corners
                + 15 * 3 
                // and the south - east corner for south row 14 tiles * 3 corners ( 1 tile is already processed in east col
                + 14 * 3
                // and in maximal case NORTH-WEST the north-east corner of the north row + the south-west corner of the west col
                // both only with once other sink/target tile
                + 1 + 1;

            typedef embark_assist::key_buffer_holder::basic_key_buffer_holder<size_of_external_incursion_buffer> external_incursion_buffer;
            //embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &external_buffer;

            // for debugging/performance profiling
            std::chrono::duration<double> internal_elapsed = std::chrono::seconds(0);
            std::chrono::duration<double> update_and_check_external_elapsed = std::chrono::seconds(0);
            // FIXME: debug code, to be removed
            //Roaring aquifer;
            //Roaring unflat;
            //Roaring soil0;
            //Roaring clay;
            //Roaring savagery0;
            //Roaring evilness0;
            //Roaring biome0;

            void retrieve_position_and_required_number_of_surveyed_neighbours(const int16_t x, const int16_t y, embark_assist::defs::region_tile_position &world_position, 
                uint8_t &required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing,
                uint8_t &required_number_of_contiguous_incursion_processed_world_tiles_for_iterative_search);
            void retrieve_min_max_offsets(const embark_assist::defs::region_tile_position &world_position, int8_t &min_x, int8_t &max_x, int8_t &min_y, int8_t &max_y);

            void fill_buffer(
                embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer,
                const embark_assist::defs::mid_level_tile_basic &source, const bool is_flat,
                const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_key);

            void fill_buffer(
                embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer,
                const embark_assist::defs::mid_level_tile_basic &source, const bool are_flat[3],
                const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_keys[3]);

            void process_eastern_edge(const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results, 
                    const embark_assist::defs::mid_level_tile_basic &western_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
                const embark_assist::defs::region_tile_datum &western_rtd, const embark_assist::defs::region_tile_datum &eastern_rtd, 
                const uint32_t current_target_key, const uint32_t eastern_target_key, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_southern_edge(const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::mid_level_tile_basic &current_northern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile, 
                const embark_assist::defs::region_tile_datum &northern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd, 
                const uint32_t northern_target_key, const uint32_t southern_target_key, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_southern_east_corner(
                const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
                const embark_assist::defs::mid_level_tile_basic &southern_eastern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
                const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &eastern_rtd,
                const embark_assist::defs::region_tile_datum &south_eastern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd,
                const uint32_t current_tile_key, const uint32_t eastern_tile_key, const uint32_t south_eastern_tile_key, const uint32_t southern_tile_key,
                embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_north_east_corner(
                const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
                const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &eastern_rtd,
                const uint32_t current_tile_key, const uint32_t eastern_tile_key,
                embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_south_west_corner(
                const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, const embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
                const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &southern_rtd,
                const uint32_t current_tile_key, const uint32_t southern_tile_key,
                embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);

            void process_external_incursions(const int16_t x, const int16_t y, embark_assist::defs::world_tile_data *survey_results, embark_assist::defs::index_interface &index);
            void update_and_check_external_incursion_counters_of_neighbouring_world_tiles(const int16_t x, const int16_t y, embark_assist::defs::world_tile_data *survey_results, embark_assist::defs::index_interface &index);
            void update_and_check_external_incursion_counters_of_neighbouring_world_tile(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, embark_assist::defs::index_interface &index);

            void process_external_incursions_for_middle_tile(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, 
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_external_incursions_for_north_tile(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_external_incursions_for_east_tile(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_external_incursions_for_south_tile(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_external_incursions_for_west_tile(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);

            void process_external_incursions_for_eastern_column(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &eastern_neighbour_rtd,
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);
            void process_external_incursions_for_southern_row(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &southern_neighbour_rtd,
                const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);

            void incursion_processor::process_additional_internal_incursions_for_north_tile(
                const uint16_t x, const uint16_t y,
                embark_assist::defs::world_tile_data *survey_results, const embark_assist::defs::region_tile_datum &current_rtd,
                embark_assist::defs::mid_level_tiles *mlt,
                embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);

            void incursion_processor::process_additional_internal_incursions_for_west_tile(
                const uint16_t x, const uint16_t y,
                embark_assist::defs::world_tile_data *survey_results, const embark_assist::defs::region_tile_datum &current_rtd,
                embark_assist::defs::mid_level_tiles *mlt,
                embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer);

            //void embark_assist::incursion::incursion_processor::increment(embark_assist::defs::region_tile_datum &neighbour);
        public:
            incursion_processor();
            ~incursion_processor();
            void process_internal_incursions(const uint32_t world_offset, const int16_t x, const int16_t y, embark_assist::defs::world_tile_data &survey_results, embark_assist::defs::mid_level_tiles &mlt, embark_assist::defs::index_interface &index);
            void update_and_check_survey_counters_of_neighbouring_world_tiles(const int16_t x, const int16_t y, embark_assist::defs::world_tile_data &survey_results, embark_assist::defs::index_interface &index);
            void init_incursion_context(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd);
        };
    }
}