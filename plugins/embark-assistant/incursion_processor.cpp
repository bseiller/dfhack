
#include "DataDefs.h"
#include "df/inorganic_raw.h"
#include "df/global_objects.h"
#include "df/world.h"
#include "df/world_data.h"
#include "df/world_region.h"
#include "df/world_region_details.h"
#include "df/world_region_feature.h"
#include "df/world_region_type.h"

#include "incursion_processor.h"
#include "survey.h"

using df::global::world;

void embark_assist::incursion::incursion_processor::fill_buffer(
        embark_assist::defs::key_buffer_holder_basic_interface &buffer, const embark_assist::defs::mid_level_tile_basic &source, 
        const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_key) {

    if (source.aquifer) {
        buffer.add_aquifer(target_key);
    }

    if (source.clay) {
        buffer.add_clay(target_key);
    }

    if (source.sand) {
        buffer.add_sand(target_key);
    }

    buffer.add_soil_depth(target_key, source.soil_depth);
    buffer.add_savagery_level(target_key, source.savagery_level);
    buffer.add_evilness_level(target_key, source.evilness_level);

    // needs additional processing!
    const int16_t biome_id = rtd.biome[source.biome_offset];
    if (biome_id < 0 || biome_id > 50) {
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("invalid biome_id %d\n", biome_id);
    }
    buffer.add_biome(target_key, biome_id);

    // TODO: find a home for this "conversion"/extraction, that can also be found in survey.cpp => embark_assist::survey::survey_mid_level_tile
    const df::world_region_type region_type = world->world_data->regions[rtd.biome_index[source.biome_offset]]->type;
    const int16_t region_type_id = region_type;
    if (region_type_id < 0 || region_type_id > 10) {
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("invalid region_type_id %d\n", region_type_id);
    }
    buffer.add_region_type(target_key, region_type_id);
}

void embark_assist::incursion::incursion_processor::fill_buffer(
        embark_assist::defs::key_buffer_holder_basic_interface  &buffer, const embark_assist::defs::mid_level_tile_basic &source, 
        const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_keys[3]) {
    // TODO: haha this is very naive code and could be optimized by adapting the method fill_buffer to take an array of keys and a length for the array/number of arguments...
    // but for now it works fine and fast enough...
    fill_buffer(buffer, source, rtd, target_keys[0]);
    fill_buffer(buffer, source, rtd, target_keys[1]);
    fill_buffer(buffer, source, rtd, target_keys[2]);
}

void embark_assist::incursion::incursion_processor::process_eastern_edge(
        const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::mid_level_tile_basic &current_western_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
        const embark_assist::defs::region_tile_datum &western_rtd, const embark_assist::defs::region_tile_datum eastern_rtd, 
        const uint32_t western_target_key, const uint32_t eastern_target_key, embark_assist::defs::key_buffer_holder_basic_interface &buffer) {

    // easter edge to right/eastern neighbour
    const uint8_t eastern_edge = embark_assist::survey::translate_ew_edge(survey_results, false, x, y, i, k);
    if (eastern_edge == 4) {
        // the current tile provides the data => the incursion goes eastward into the next tile
        fill_buffer(buffer, current_western_tile, western_rtd, eastern_target_key);
    }
    else if (eastern_edge == 5) {
        // the eastern tile provides the data => the incursion goes westward into the current tile
        fill_buffer(buffer, eastern_tile, eastern_rtd, western_target_key);
    }
    else {
        // ERROR!
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("embark_assist::incursion::incursion_processor::process_eastern_edge - invalid return value for eastern_edge %d\n", eastern_edge);
    }
}

void embark_assist::incursion::incursion_processor::process_southern_edge(
        const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::mid_level_tile_basic &current_northern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
        const embark_assist::defs::region_tile_datum &northern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd, 
        const uint32_t northern_target_key, const uint32_t southern_target_key, embark_assist::defs::key_buffer_holder_basic_interface &buffer) {

    // southern edge to bottom/southern neighbour
    const uint8_t southern_edge = embark_assist::survey::translate_ns_edge(survey_results, false, x, y, i, k);
    if (southern_edge == 4) {
        // the current tile provides the data => the incursion goes southward into the next tile
        fill_buffer(buffer, current_northern_tile, northern_rtd, southern_target_key);
    }
    else if (southern_edge == 7) {
        // the southern tile provides the data => the incursion goes northward into the current tile
        fill_buffer(buffer, southern_tile, southern_rtd, northern_target_key);
    }
    else {
        // ERROR!
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("embark_assist::incursion::incursion_processor::process_southern_edge - invalid return value for southern_edge %d\n", southern_edge);
    }
}

void embark_assist::incursion::incursion_processor::process_southern_east_corner(
        const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile, 
        const embark_assist::defs::mid_level_tile_basic &southern_eastern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
        const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum eastern_rtd, 
        const embark_assist::defs::region_tile_datum south_eastern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd,
        const uint32_t current_tile_key, const uint32_t eastern_tile_key, const uint32_t south_eastern_tile_key, const uint32_t southern_tile_key, 
        embark_assist::defs::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::mid_level_tile_basic *source = nullptr;
    const embark_assist::defs::region_tile_datum *source_rtd;
    uint32_t target_keys[3];

    const uint8_t south_east_corner = embark_assist::survey::translate_corner(survey_results, 8, x, y, i, k);
    if (south_east_corner == 4) {
        // the current tile provides the data
        source = &current_tile;
        source_rtd = &current_rtd;
        // order of keys: eastern (i+1), south-eastern(k+1, i+1), southern (k+1)
        target_keys[0] = eastern_tile_key;
        target_keys[1] = south_eastern_tile_key;
        target_keys[2] = southern_tile_key;
    }
    else if (south_east_corner == 5) {
        // the eastern tile provides the data
        source = &eastern_tile;
        source_rtd = &eastern_rtd;
        // order of keys: current(+0/+0), south-eastern(k+1, i+1), southern (k+1)
        target_keys[0] = current_tile_key;
        target_keys[1] = south_eastern_tile_key;
        target_keys[2] = southern_tile_key;
    }
    else if (south_east_corner == 7) {
        // the southern tile provides the data
        source = &southern_tile;
        source_rtd = &southern_rtd;
        // order of keys: current(+0/+0), eastern (i+1), south-eastern(k+1, i+1)
        target_keys[0] = current_tile_key;
        target_keys[1] = eastern_tile_key;
        target_keys[2] = south_eastern_tile_key;
    }
    else if (south_east_corner == 8) {
        // the southern-eastern tile provides the data
        source = &southern_eastern_tile;
        source_rtd = &south_eastern_rtd;
        // order of keys: current(+0/+0), eastern (i+1), southern (k+1)
        target_keys[0] = current_tile_key;
        target_keys[1] = eastern_tile_key;
        target_keys[2] = southern_tile_key;
    }
    else {
        // ERROR
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("embark_assist::incursion::incursion_processor::process_southern_east_corner - invalid return value for south_east_corner %d\n", south_east_corner);
        return;
    }
    fill_buffer(buffer, *source, *source_rtd, target_keys);
}

// for debugging
//void print_biome_x(const uint16_t x, const uint16_t y, const uint16_t dimension) {
//    std::stringstream ss;
//    ss << "biome_x_" << unsigned(x) << "_" << unsigned(y) << "_dim" << unsigned(dimension);
//    const std::string prefix = ss.str();
//    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);
//
//    for (uint8_t k = 0; k < dimension; k++) {
//        for (uint8_t i = 0; i < dimension; i++) {
//            int8_t edge = world->world_data->region_details[0]->edges.biome_x[i][k];
//            myfile << unsigned(edge) << ";";
//        }
//        myfile << "\n";
//    }
//    myfile.close();
//}

void embark_assist::incursion::incursion_processor::process_internal_incursions(
        const uint32_t world_offset, const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, 
        embark_assist::defs::mid_level_tiles *mlt, embark_assist::defs::index_interface &index) {

    // for debugging
    /*if ((x == 15 && y == 0) || (x == 16 && y == 0) || (x == 15 && y == 1)) {
        print_biome_x(x, y, 16);
        print_biome_x(x, y, 17);
    }*/

    // TODO: make sure we don't want to run this async/threaded/concurrent, or we need to remove the class member "internal_buffer" and replace it with a local instance, so ever call get its own instance
    // embark_assist::defs::key_buffer_holder_basic_interface &buffer = internal_incursion_buffer();
    embark_assist::defs::key_buffer_holder_basic_interface &buffer = *internal_buffer;
    buffer.reset();

    color_ostream_proxy out(Core::getInstance().getConsole());
    const auto start = std::chrono::steady_clock::now();
    const embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];
    for (uint8_t i = 0; i < 15; i++) {
        for (uint8_t k = 0; k < 15; k++) {

            // is the western and the northern tile both
            const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(i)[k];
            const uint32_t current_tile_key = world_offset + k * 16 + i;

            // easter edge to right/eastern neighbour
            const embark_assist::defs::mid_level_tile_basic &eastern_tile = mlt->at(i + 1)[k];
            const uint32_t eastern_tile_key = current_tile_key + 1;
            process_eastern_edge(x, y, i, k, survey_results, current_tile, eastern_tile, rtd, rtd, current_tile_key, eastern_tile_key, buffer);

            // southern edge to bottom/southern neighbour
            const embark_assist::defs::mid_level_tile_basic &southern_tile = mlt->at(i)[k + 1];
            const uint32_t southern_tile_key = world_offset + (k + 1) * 16 + i;
            process_southern_edge(x, y, i, k, survey_results, current_tile, southern_tile, rtd, rtd, current_tile_key, southern_tile_key, buffer);

            const uint32_t south_eastern_tile_key = southern_tile_key + 1;
            const embark_assist::defs::mid_level_tile_basic &south_eastern_tile = mlt->at(i + 1)[k + 1];
            // south-east corner
            process_southern_east_corner(x, y, i, k, survey_results, 
                current_tile, eastern_tile, south_eastern_tile, southern_tile, 
                rtd, rtd, rtd, rtd, 
                current_tile_key, eastern_tile_key, south_eastern_tile_key, southern_tile_key, buffer);

            
            //const uint8_t south_east_corner = embark_assist::survey::translate_corner(survey_results, 8, x, y, i, k);
            //if (south_east_corner == 4) {
            //    // the current tile provides the data
            //    const embark_assist::defs::mid_level_tile_basic &source = mlt->at(i)[k];
            //    // order of keys: eastern (i+1), south-eastern(k+1, i+1), southern (k+1)
            //    const uint32_t target_keys[3] = { eastern_tile_key, southern_tile_key + 1, southern_tile_key };
            //    fill_buffer(buffer, source, rtd, target_keys);
            //}
            //else if (south_east_corner == 5) {
            //    // the eastern tile provides the data
            //    const embark_assist::defs::mid_level_tile_basic &source = mlt->at(i + 1)[k];
            //    // order of keys: current(+0/+0), south-eastern(k+1, i+1), southern (k+1)
            //    const uint32_t target_keys[3] = { current_tile_key, southern_tile_key + 1, southern_tile_key };
            //    fill_buffer(buffer, source, rtd, target_keys);
            //}
            //else if (south_east_corner == 7) {
            //    // the southern tile provides the data
            //    const embark_assist::defs::mid_level_tile_basic &source = mlt->at(i)[k + 1];
            //    // order of keys: current(+0/+0), eastern (i+1), south-eastern(k+1, i+1)
            //    const uint32_t target_keys[3] = { current_tile_key, eastern_tile_key, southern_tile_key + 1 };
            //    fill_buffer(buffer, source, rtd, target_keys);
            //}
            //else if (south_east_corner == 8) {
            //    // the southern-eastern tile provides the data
            //    const embark_assist::defs::mid_level_tile_basic &source = mlt->at(i + 1)[k + 1];
            //    // order of keys: current(+0/+0), eastern (i+1), southern (k+1)
            //    const uint32_t target_keys[3] = { current_tile_key, eastern_tile_key, southern_tile_key };
            //    fill_buffer(buffer, source, rtd, target_keys);
            //}
            //else {
            //    // ERROR
            //    out.print("embark_assist::incursion::incursion_processor::process_internal_incursions - invalid return value for south_east_corner %d\n", south_east_corner);
            //}
        }
    }

    // southern edges of the eastern most column
    for (uint8_t k = 0; k < 15; k++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(15)[k];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = mlt->at(15)[k + 1];
        const uint32_t current_tile_key = world_offset + k * 16 + 15;
        const uint32_t southern_tile_key = world_offset + (k + 1) * 16 + 15;
        process_southern_edge(x, y, 15, k, survey_results, current_tile, southern_tile, rtd, rtd, current_tile_key, southern_tile_key, buffer);
    }

    // eastern edges of the southern most row
    for (uint8_t i = 0; i < 15; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(i)[15];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = mlt->at(i + 1)[15];
        const uint32_t current_tile_key = world_offset + 15 * 16 + i;
        const uint32_t eastern_tile_key = current_tile_key + 1;
        process_eastern_edge(x, y, i, 15, survey_results, current_tile, eastern_tile, rtd, rtd, current_tile_key, eastern_tile_key, buffer);
    }

    // add all data to the index
    index.add(buffer);

    const auto end = std::chrono::steady_clock::now();
    internal_elapsed += end - start;
}

void embark_assist::incursion::incursion_processor::retrieve_position_and_required_number_of_surveyed_neighbours(
        const uint16_t x, const uint16_t y, embark_assist::defs::region_tile_position &world_position, 
        uint8_t &required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing) {

    world_position = embark_assist::defs::region_tile_position::middle;
    required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 9;
    if (x == 0) {
        if (y == 0) {
            world_position = embark_assist::defs::region_tile_position::north_west;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
        }
        else if (y == world->worldgen.worldgen_parms.dim_y - 1) {
            world_position = embark_assist::defs::region_tile_position::south_west;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
        }
        else {
            world_position = embark_assist::defs::region_tile_position::west;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
        }
    }
    else if (x == world->worldgen.worldgen_parms.dim_x - 1) {
        if (y == 0) {
            world_position = embark_assist::defs::region_tile_position::north_east;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
        }
        else if (y == world->worldgen.worldgen_parms.dim_y - 1) {
            world_position = embark_assist::defs::region_tile_position::south_east;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
        }
        else {
            world_position = embark_assist::defs::region_tile_position::east;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
        }
    }
    else {
        if (y == 0) {
            world_position = embark_assist::defs::region_tile_position::north;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
        }
        else if (y == world->worldgen.worldgen_parms.dim_y - 1) {
            world_position = embark_assist::defs::region_tile_position::south;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
        }
        else {
            world_position = embark_assist::defs::region_tile_position::middle;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 9;
        }
    }
}

void  embark_assist::incursion::incursion_processor::retrieve_min_max_offsets(
        const embark_assist::defs::region_tile_position &world_position, int8_t &min_x, 
        int8_t &max_x, int8_t &min_y, int8_t &max_y) {

    // middle is the "default"/most common case
    min_x = 1;
    max_x = 1;
    min_y = 1;
    max_y = 1;

    switch (world_position) {
        case embark_assist::defs::region_tile_position::north_west :
            min_x = 0;
            min_y = 0;
            break;
        case embark_assist::defs::region_tile_position::north :
            min_y = 0;
            break;
        case embark_assist::defs::region_tile_position::north_east :
            max_x = 0;
            min_y = 0;
            break;
        case embark_assist::defs::region_tile_position::east :
            max_x = 0;
            break;
        case embark_assist::defs::region_tile_position::south_east :
            max_x = 0;
            max_y = 0;
            break;
        case embark_assist::defs::region_tile_position::south :
            max_y = 0;
            break;
        case embark_assist::defs::region_tile_position::south_west :
            min_x = 0;
            max_y = 0;
            break;
        case embark_assist::defs::region_tile_position::west:
            min_x = 0;
            break;
    }
}

void embark_assist::incursion::incursion_processor::update_and_check_survey_counters_of_neighbouring_world_tiles(
        const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, 
        embark_assist::defs::index_interface &index) {

    const auto start = std::chrono::steady_clock::now();

    embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];

    embark_assist::defs::region_tile_position world_position;
    uint8_t required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing;
    retrieve_position_and_required_number_of_surveyed_neighbours(x, y, world_position, required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing);
    rtd.world_position = world_position;
    rtd.required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing;

    int8_t min_x, max_x, min_y, max_y;
    retrieve_min_max_offsets(world_position, min_x, max_x, min_y, max_y);

    for (uint16_t pos_x = x - min_x; pos_x <= x + max_x; pos_x++) {
        for (uint16_t pos_y = y - min_y; pos_y <= y + max_y; pos_y++) {
            embark_assist::defs::region_tile_datum &neighbour = survey_results->at(pos_x)[pos_y];
            neighbour.number_of_contiguous_surveyed_world_tiles++;
            if (neighbour.number_of_contiguous_surveyed_world_tiles == neighbour.required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing) {
                process_external_incursions(pos_x, pos_y, survey_results, index);
            }
        }
    }

    const auto end = std::chrono::steady_clock::now();
    update_and_check_external_elapsed += end - start;
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_middle_tile(
        const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::index_interface &index, embark_assist::defs::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::region_tile_datum &current_rtd = survey_results->at(x)[y];

    // process eastern edge to east world tile neighbour
    const embark_assist::defs::region_tile_datum &eastern_neighbour_rtd = survey_results->at(x + 1)[y];
    for (uint8_t k = 0; k < 15; k++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.east_column[k];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = eastern_neighbour_rtd.west_column[k];
        const uint32_t current_tile_key = index.get_key(x, y, 15, k);
        const uint32_t eastern_tile_key = index.get_key(x + 1, y, 0, k);
        process_eastern_edge(x, y, 15, k, survey_results, current_tile, eastern_tile, current_rtd, eastern_neighbour_rtd, current_tile_key, eastern_tile_key, buffer);
    }

    // process south-east corners of eastern column 
    for (uint8_t k = 0; k < 14; k++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.east_column[k];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = eastern_neighbour_rtd.west_column[k];
        const embark_assist::defs::mid_level_tile_basic &south_eastern_tile = eastern_neighbour_rtd.west_column[k + 1];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = current_rtd.east_column[k + 1];

        const uint32_t current_tile_key = index.get_key(x, y, 15, k);
        const uint32_t eastern_tile_key = index.get_key(x + 1, y, 0, k);
        const uint32_t south_eastern_tile_key = index.get_key(x + 1, y, 0, k + 1);
        const uint32_t southern_tile_key = index.get_key(x, y, 15, k + 1);

        process_southern_east_corner(x, y, 15, k, survey_results,
            current_tile, eastern_tile, south_eastern_tile, southern_tile,
            current_rtd, eastern_neighbour_rtd, eastern_neighbour_rtd, current_rtd,
            current_tile_key, eastern_tile_key, south_eastern_tile_key, southern_tile_key, buffer);
    }

    const embark_assist::defs::region_tile_datum &south_rtd = survey_results->at(x)[y + 1];

    // special case, as the south-eastern neighbour is another world tile than the eastern neighbor for sure
    // also: leave the "superfluous" brackets, we define a block, to keep the additional variables in a local scope
    {
        const embark_assist::defs::region_tile_datum &south_eastern_rtd = survey_results->at(x + 1)[y + 1];

        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.east_column[15];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = eastern_neighbour_rtd.west_column[15];
        const embark_assist::defs::mid_level_tile_basic &south_eastern_tile = south_eastern_rtd.west_column[0];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = south_rtd.east_column[0];

        const uint32_t current_tile_key = index.get_key(x, y, 15, 15);
        const uint32_t eastern_tile_key = index.get_key(x + 1, y, 0, 15);
        const uint32_t south_eastern_tile_key = index.get_key(x + 1, y + 1, 0, 0);
        const uint32_t southern_tile_key = index.get_key(x, y + 1, 15, 0);

        process_southern_east_corner(x, y, 15, 15, survey_results,
            current_tile, eastern_tile, south_eastern_tile, southern_tile,
            current_rtd, eastern_neighbour_rtd, south_eastern_rtd, south_rtd,
            current_tile_key, eastern_tile_key, south_eastern_tile_key, southern_tile_key, buffer);
    }

    // process southern edge to south world tile neighbour
    const embark_assist::defs::region_tile_datum &southern_neighbour_rtd = survey_results->at(x)[y + 1];
    for (uint8_t i = 0; i < 15; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.south_row[i];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = southern_neighbour_rtd.north_row[i];
        const uint32_t current_tile_key = index.get_key(x, y, i, 15);
        const uint32_t southern_tile_key = index.get_key(x, y + 1, i, 0);
        process_southern_edge(
            x, y, i, 15, survey_results, current_tile, southern_tile, 
            current_rtd, southern_neighbour_rtd, current_tile_key, southern_tile_key, buffer);
    }

    // process south-east corners of southern row
    for (uint8_t i = 0; i < 14; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.south_row[i];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = current_rtd.south_row[i + 1];
        const embark_assist::defs::mid_level_tile_basic &south_eastern_tile = south_rtd.north_row[i + 1];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = south_rtd.north_row[i];

        const uint32_t current_tile_key = index.get_key(x, y, i, 15);
        const uint32_t eastern_tile_key = index.get_key(x, y, i + 1, 15);
        const uint32_t south_eastern_tile_key = index.get_key(x, y + 1, i + 1, 0);
        const uint32_t southern_tile_key = index.get_key(x, y + 1, i, 0);

        process_southern_east_corner(x, y, i, 15, survey_results,
            current_tile, eastern_tile, south_eastern_tile, southern_tile,
            current_rtd, current_rtd, south_rtd, south_rtd,
            current_tile_key, eastern_tile_key, south_eastern_tile_key, southern_tile_key, buffer);
    }

    // no need to process the south east corner of i = 15, k = 15, we allready did that while processing the eastern row above
}

void embark_assist::incursion::incursion_processor::process_external_incursions(
        const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results, 
        embark_assist::defs::index_interface &index) {

    embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];

    // TODO: test if moving this into the state of survey brings possible performance gains
    external_incursion_buffer buffer;
    buffer.reset();

    switch (rtd.world_position) {
    case embark_assist::defs::region_tile_position::middle:
        process_external_incursions_for_middle_tile(x,y, survey_results, index, buffer);
        break;
    }

    index.add(buffer);
    rtd.totally_processed = true;
}

embark_assist::incursion::incursion_processor::incursion_processor(): internal_buffer(new internal_incursion_buffer()) {

}

embark_assist::incursion::incursion_processor::~incursion_processor() {
    delete internal_buffer;
}
