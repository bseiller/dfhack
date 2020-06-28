#pragma once

#include "basic_key_buffer_holder.h"
#include "defs.h"

namespace embark_assist {
    namespace incursion {
        class incursion_processor {
        private:
            // during the internal incursion processing there are 2 edges (east + south) and the south-east corner for 15 * 15 tiles, 
            // + the east edge for 15 tiles (southmost row) and the south edge for 15 tiles (eastmost column)
            const static int size_of_internal_incursion_buffer = 15 * 15 * (2 + 3) + 15 + 15;
            typedef embark_assist::key_buffer_holder::basic_key_buffer_holder<size_of_internal_incursion_buffer> internal_incursion_buffer;
            //embark_assist::defs::key_buffer_holder_basic_interface &internal_buffer;
            embark_assist::defs::key_buffer_holder_basic_interface *internal_buffer;

            // max size of the buffers for a world tile in the middle => 8 neighbours, 4 relevant edges and corners
            const static int size_of_external_incursion_buffer = 15 * 15 * (2 + 3) + 15 + 15;

            typedef embark_assist::key_buffer_holder::basic_key_buffer_holder<size_of_external_incursion_buffer> external_incursion_buffer;
            //embark_assist::defs::key_buffer_holder_basic_interface &external_buffer;

            // for debugging/performance profiling
            std::chrono::duration<double> internal_elapsed = std::chrono::seconds(0);
            std::chrono::duration<double> update_and_check_external_elapsed = std::chrono::seconds(0);

            void retrieve_position_and_required_number_of_surveyed_neighbours(const uint16_t x, const uint16_t y, embark_assist::defs::region_tile_position &world_position, uint8_t &required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing);
            void retrieve_min_max_offsets(const embark_assist::defs::region_tile_position &world_position, int8_t &min_x, int8_t &max_x, int8_t &min_y, int8_t &max_y);
            void fill_buffer(embark_assist::defs::key_buffer_holder_basic_interface  &buffer, const embark_assist::defs::mid_level_tile_basic &source, const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_key);
            void fill_buffer(embark_assist::defs::key_buffer_holder_basic_interface  &buffer, const embark_assist::defs::mid_level_tile_basic &source, const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_keys[3]);
            void process_eastern_edge(const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results, 
                    const embark_assist::defs::mid_level_tile_basic &western_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
                const embark_assist::defs::region_tile_datum &western_rtd, const embark_assist::defs::region_tile_datum eastern_rtd, 
                const uint32_t current_target_key, const uint32_t eastern_target_key, embark_assist::defs::key_buffer_holder_basic_interface &buffer);
            void process_southern_edge(const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::mid_level_tile_basic &current_northern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile, 
                const embark_assist::defs::region_tile_datum &northern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd, 
                const uint32_t northern_target_key, const uint32_t southern_target_key, embark_assist::defs::key_buffer_holder_basic_interface &buffer);
            void process_southern_east_corner(
                const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
                const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
                const embark_assist::defs::mid_level_tile_basic &southern_eastern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
                const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum eastern_rtd,
                const embark_assist::defs::region_tile_datum south_eastern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd,
                const uint32_t current_tile_key, const uint32_t eastern_tile_key, const uint32_t south_eastern_tile_key, const uint32_t southern_tile_key,
                embark_assist::defs::key_buffer_holder_basic_interface &buffer);
            void process_external_incursions(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, embark_assist::defs::index_interface &index);
            void process_external_incursions_for_middle_tile(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, 
                const embark_assist::defs::index_interface &index, embark_assist::defs::key_buffer_holder_basic_interface &buffer);
        public:
            incursion_processor();
            ~incursion_processor();
            void process_internal_incursions(const uint32_t world_offset, const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, embark_assist::defs::mid_level_tiles *mlt, embark_assist::defs::index_interface &index);
            void update_and_check_survey_counters_of_neighbouring_world_tiles(const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, embark_assist::defs::index_interface &index);
        };
    }
}