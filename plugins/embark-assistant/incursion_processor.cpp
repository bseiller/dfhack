
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

#define edge_root_folder "faulty_edges/"
#define edge_root_folder_path index_folder_name edge_root_folder

using df::global::world;

int embark_assist::key_buffer_holder::basic_key_buffer_holder <embark_assist::incursion::incursion_processor::size_of_external_incursion_buffer>::max_index = 0;
int embark_assist::key_buffer_holder::basic_key_buffer_holder <embark_assist::incursion::incursion_processor::size_of_internal_incursion_buffer>::max_index = 0;
int embark_assist::key_buffer_holder::basic_key_buffer_holder <256>::max_index = 0;
//
//namespace embark_assist {
//    namespace incursion {
//        //namespace incursion_processor {
//            struct position {
//                int16_t x;
//                int16_t y;
//            };
//        //}
//    }
//}

// FIXME: for debugging only
void log_incursion_source_target(const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, const std::string incursion_case, const uint32_t current_key, const uint32_t source_key, const uint32_t target_key) {
    if (x == world->world_data->world_width - 1 && (i == 14 || i == 15) || y == world->world_data->world_height - 1 && (k == 14 || k == 15)) {

        const std::string prefix = "edge_incursion_graph_data.csv";
        auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out | std::ofstream::app);

        myfile
            << x << ";"
            << y << ";"
            << i << ";"
            << k << ";"
            << current_key << ";"
            << source_key << ";"
            << incursion_case << ";"
            << target_key << ";\n"
            ;
    }
}

// FIXME: for debugging only
void write_array2(std::ofstream &file, int8_t values[16][16]) {
    for (int8_t y = 0; y < 16; y++) {
        for (int8_t x = 0; x < 16; x++) {
            file << std::to_string(values[x][y]) << ";";
        }
        file << "\n";
    }
}

// FIXME: for debugging only
void write_biomes_edges_to_file(const uint32_t target_key, df::world_region_details::T_edges edges) {
    const std::string x_string = std::to_string(target_key);

    std::ofstream biome_corner = std::ofstream(edge_root_folder_path + x_string + "_biome_corner.csv", std::ios::out);
    write_array2(biome_corner, edges.biome_corner);
    biome_corner.close();

    std::ofstream biome_x = std::ofstream(edge_root_folder_path + x_string + "_biome_x.csv", std::ios::out);
    write_array2(biome_x, edges.biome_x);
    biome_x.close();

    std::ofstream biome_y = std::ofstream(edge_root_folder_path + x_string + "_biome_y.csv", std::ios::out);
    write_array2(biome_y, edges.biome_y);
    biome_y.close();
}

// FIXME: for debugging only
void write_savagery_2_incursions_to_file(const uint32_t target_key) {
    const std::string x_string = "";

    std::ofstream file = std::ofstream(index_folder_name + x_string + "_savagery_2_incursions.txt", std::ios::out | std::ios::app);
    file << target_key << "\n";
    file.close();
}

void embark_assist::incursion::incursion_processor::fill_buffer(
        embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer, const embark_assist::defs::mid_level_tile_basic &source,
        const bool is_flat,
        const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_key, const uint32_t source_key) {

    const bool check_index = false;

    //if (source_key == 10134816 || target_key == 10134816 || source_key == 10140191 || target_key == 10140191) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - incursion from %d to %d\n", source_key, target_key);
    //}

    if (!is_flat) {
        buffer.add_unflat(target_key);

        // FIXME: debug code, to be removed
        //if (check_index && !unflat.contains(target_key)) {
        //    color_ostream_proxy out(Core::getInstance().getConsole());
        //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much unflat: %d\n", target_key);
        //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
        //}
    }

    //if (source.aquifer) {
        buffer.add_aquifer(target_key, source.aquifer);
        //if (target_key == 11704880) {
        // if (target_key == 73216) {
        //if (target_key >= 1024 && target_key <= 1279) {
        //if (target_key == 1144) {
        //    auto t = std::chrono::high_resolution_clock::now();
        //    color_ostream_proxy out(Core::getInstance().getConsole());
        //    //out.print("embark_assist::incursion::incursion_processor::fill_buffer - aquifer incursion into tile 11704880 at %lld\n", static_cast<long long int>(t.time_since_epoch().count()));
        //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - aquifer incursion into tile %u at %lld\n", target_key, static_cast<long long int>(t.time_since_epoch().count()));
        //    //auto time_point = std::chrono::system_clock::now();
        //    //auto now_c = std::chrono::system_clock::to_time_t(time_point);
        //    //std::tm now_tm = *std::localtime(&now_c);
        //    //char buff[70];
        //    //if (strftime(buff, sizeof buff, "%c", &now_tm)) {
        //    //    color_ostream_proxy out(Core::getInstance().getConsole());
        //    //    //out.print("embark_assist::incursion::incursion_processor::fill_buffer - aquifer incursion into tile 11704880 at %s", buff);
        //    //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - aquifer incursion into tile 11704880 at %lld", static_cast<long long int>(t.time_since_epoch().count()));
        //    //}
        //}

        // FIXME: debug code, to be removed
        //if (check_index && !aquifer.contains(target_key)) {
        //    color_ostream_proxy out(Core::getInstance().getConsole());
        //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much aquifer: %d\n", target_key);
        //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
        //}
    //}

    if (source.clay) {
        buffer.add_clay(target_key);

        // FIXME: debug code, to be removed
        //if (check_index && !clay.contains(target_key)) {
        //    color_ostream_proxy out(Core::getInstance().getConsole());
        //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much clay: %d\n", target_key);
        //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
        //}
    }

    if (source.sand) {
        buffer.add_sand(target_key);
    }

    buffer.add_soil_depth(target_key, source.soil_depth);
    // FIXME: debug code, to be removed
    //if (check_index && source.soil_depth == 0 && !soil0.contains(target_key)) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much soil0: %d\n", target_key);
    //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
    //}

    if (rtd.min_temperature[source.biome_offset] <= 0) {
        buffer.add_temperatur_freezing(target_key, embark_assist::key_buffer_holder::temperatur::MIN_ZERO_OR_BELOW);
    }
    else {
        buffer.add_temperatur_freezing(target_key, embark_assist::key_buffer_holder::temperatur::MIN_ABOVE_ZERO);
    }

    if (rtd.max_temperature[source.biome_offset] <= 0) {
        buffer.add_temperatur_freezing(target_key, embark_assist::key_buffer_holder::temperatur::MAX_ZERO_OR_BELOW);
    }
    else {
        buffer.add_temperatur_freezing(target_key, embark_assist::key_buffer_holder::temperatur::MAX_ABOVE_ZERO);
    }

    const bool blood_rain = rtd.blood_rain[source.biome_offset];
    if (blood_rain) {
        buffer.add_blood_rain(target_key);
    }

    buffer.add_savagery_level(target_key, source.savagery_level);
    //if (source.savagery_level == 2 && target_key >= 71936 && target_key <= 72191) {
    //    write_savagery_2_incursions_to_file(target_key);
    //}
    // FIXME: debug code, to be removed
    //if (check_index && source.savagery_level == 0 && !savagery0.contains(target_key)) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much savagery0: %d\n", target_key);
    //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
    //}
    buffer.add_evilness_level(target_key, source.evilness_level);
    // FIXME: debug code, to be removed
    //if (check_index && source.evilness_level == 0 && !evilness0.contains(target_key)) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much evilness0: %d\n", target_key);
    //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
    //}

    // needs additional processing!
    const int16_t biome_id = rtd.biome[source.biome_offset];
    if (biome_id < 0 || biome_id > 50) {
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("invalid biome_id %d\n", biome_id);
    }
    buffer.add_biome(target_key, biome_id);
    // FIXME: debug code, to be removed
    //if (check_index && biome_id == 0 && !biome0.contains(target_key)) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("embark_assist::incursion::incursion_processor::fill_buffer - to much biome0: %d\n", target_key);
    //    write_biomes_edges_to_file(target_key, world->world_data->region_details[0]->edges);
    //}

    // TODO: find a home for this "conversion"/extraction, that can also be found in survey.cpp => embark_assist::survey::survey_mid_level_tile, around line 1463
    const df::world_region_type region_type = world->world_data->regions[rtd.biome_index[source.biome_offset]]->type;
    const int16_t region_type_id = region_type;
    if (region_type_id < 0 || region_type_id > 10) {
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("invalid region_type_id %d\n", region_type_id);
    }
    buffer.add_region_type(target_key, region_type_id);
}

void embark_assist::incursion::incursion_processor::fill_buffer(
        embark_assist::key_buffer_holder::key_buffer_holder_basic_interface  &buffer, const embark_assist::defs::mid_level_tile_basic &source, 
        const bool are_flat[3],
        const embark_assist::defs::region_tile_datum &rtd, const uint32_t target_keys[3], const uint32_t source_key) {
    // FIXME: haha this is very naive solution and could be optimized by adapting the above method fill_buffer to take an array of keys and a length for the array/number of arguments...
    // but for now it works fine and fast enough...
    fill_buffer(buffer, source, are_flat[0], rtd, target_keys[0], source_key);
    fill_buffer(buffer, source, are_flat[1], rtd, target_keys[1], source_key);
    fill_buffer(buffer, source, are_flat[2], rtd, target_keys[2], source_key);
}

void embark_assist::incursion::incursion_processor::process_eastern_edge(
        const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::mid_level_tile_basic &current_western_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
        const embark_assist::defs::region_tile_datum &western_rtd, const embark_assist::defs::region_tile_datum &eastern_rtd, 
        const uint32_t western_target_key, const uint32_t eastern_target_key, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    // easter edge to right/eastern neighbour
    const uint8_t eastern_edge = embark_assist::survey::translate_ew_edge(survey_results, false, x, y, i, k);
    const bool is_flat = current_western_tile.elevation == eastern_tile.elevation;
    if (eastern_edge == 4) {
        // the current tile provides the data => the incursion goes eastward into the next tile
        fill_buffer(buffer, current_western_tile, is_flat, western_rtd, eastern_target_key, western_target_key);
       // log_incursion_source_target(x, y, i, k, "eastern_edge", western_target_key, western_target_key, eastern_target_key);
    }
    else if (eastern_edge == 5) {
        // the eastern tile provides the data => the incursion goes westward into the current tile
        fill_buffer(buffer, eastern_tile, is_flat, eastern_rtd, western_target_key, eastern_target_key);
      //  log_incursion_source_target(x, y, i, k, "eastern_edge", western_target_key, eastern_target_key, western_target_key);
    }
    else {
        // ERROR!
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.printerr("embark_assist::incursion::incursion_processor::process_eastern_edge - invalid return value for eastern_edge %d\n", eastern_edge);
    }
}

void embark_assist::incursion::incursion_processor::process_southern_edge(
        const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::mid_level_tile_basic &current_northern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
        const embark_assist::defs::region_tile_datum &northern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd, 
        const uint32_t northern_target_key, const uint32_t southern_target_key, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    // southern edge to bottom/southern neighbour
    const uint8_t southern_edge = embark_assist::survey::translate_ns_edge(survey_results, false, x, y, i, k);
    const bool is_flat = current_northern_tile.elevation == southern_tile.elevation;
    if (southern_edge == 4) {
        // the current tile provides the data => the incursion goes southward into the next tile
        fill_buffer(buffer, current_northern_tile, is_flat, northern_rtd, southern_target_key, northern_target_key);
      //  log_incursion_source_target(x, y, i, k, "southern_edge", northern_target_key, northern_target_key, southern_target_key);
    }
    else if (southern_edge == 7) {
        // the southern tile provides the data => the incursion goes northward into the current tile
        fill_buffer(buffer, southern_tile, is_flat, southern_rtd, northern_target_key, southern_target_key);
      //  log_incursion_source_target(x, y, i, k, "southern_edge", northern_target_key, southern_target_key, northern_target_key);
    }
    else {
        // ERROR!
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.printerr("embark_assist::incursion::incursion_processor::process_southern_edge - invalid return value for southern_edge %d\n", southern_edge);
    }
}

void embark_assist::incursion::incursion_processor::process_southern_east_corner(
        const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile, 
        const embark_assist::defs::mid_level_tile_basic &southern_eastern_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
        const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &eastern_rtd, 
        const embark_assist::defs::region_tile_datum &south_eastern_rtd, const embark_assist::defs::region_tile_datum &southern_rtd,
        const uint32_t current_tile_key, const uint32_t eastern_tile_key, const uint32_t south_eastern_tile_key, const uint32_t southern_tile_key, 
        embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::mid_level_tile_basic *source = nullptr;
    const embark_assist::defs::region_tile_datum *source_rtd;
    uint32_t source_key;
    uint32_t target_keys[3];
    bool are_flat[3];

    //if (current_tile_key == 10140191) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("embark_assist::incursion::incursion_processor::process_southern_east_corner - current_tile_key %d x:%02d/y:%02d, i:%02d/k:%02d\n", current_tile_key, x, y, i, k);
    //}

    const uint8_t south_east_corner = embark_assist::survey::translate_corner(survey_results, 8, x, y, i, k);
    if (south_east_corner == 4) {
        // the current tile provides the data
        source = &current_tile;
        source_rtd = &current_rtd;
        source_key = current_tile_key;
        // order of keys: eastern (i+1), south-eastern(k+1, i+1), southern (k+1)
        target_keys[0] = eastern_tile_key;
        target_keys[1] = south_eastern_tile_key;
        target_keys[2] = southern_tile_key;

        are_flat[0] = current_tile.elevation == eastern_tile.elevation;
        are_flat[1] = current_tile.elevation == southern_eastern_tile.elevation;
        are_flat[2] = current_tile.elevation == southern_tile.elevation;
    }
    else if (south_east_corner == 5) {
        // the eastern tile provides the data
        source = &eastern_tile;
        source_rtd = &eastern_rtd;
        source_key = eastern_tile_key;
        // order of keys: current(+0/+0), south-eastern(k+1, i+1), southern (k+1)
        target_keys[0] = current_tile_key;
        target_keys[1] = south_eastern_tile_key;
        target_keys[2] = southern_tile_key;

        are_flat[0] = eastern_tile.elevation == current_tile.elevation;
        are_flat[1] = eastern_tile.elevation == southern_eastern_tile.elevation;
        are_flat[2] = eastern_tile.elevation == southern_tile.elevation;
    }
    else if (south_east_corner == 7) {
        // the southern tile provides the data
        source = &southern_tile;
        source_rtd = &southern_rtd;
        source_key = southern_tile_key;
        // order of keys: current(+0/+0), eastern (i+1), south-eastern(k+1, i+1)
        target_keys[0] = current_tile_key;
        target_keys[1] = eastern_tile_key;
        target_keys[2] = south_eastern_tile_key;

        are_flat[0] = southern_tile.elevation == current_tile.elevation;
        are_flat[1] = southern_tile.elevation == eastern_tile.elevation;
        are_flat[2] = southern_tile.elevation == southern_eastern_tile.elevation;
    }
    else if (south_east_corner == 8) {
        // the southern-eastern tile provides the data
        source = &southern_eastern_tile;
        source_rtd = &south_eastern_rtd;
        source_key = south_eastern_tile_key;
        // order of keys: current(+0/+0), eastern (i+1), southern (k+1)
        target_keys[0] = current_tile_key;
        target_keys[1] = eastern_tile_key;
        target_keys[2] = southern_tile_key;

        are_flat[0] = southern_eastern_tile.elevation == current_tile.elevation;
        are_flat[1] = southern_eastern_tile.elevation == eastern_tile.elevation;
        are_flat[2] = southern_eastern_tile.elevation == southern_tile.elevation;
    }
    else {
        // ERROR!
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.printerr("embark_assist::incursion::incursion_processor::process_southern_east_corner - invalid return value for south_east_corner %d\n", south_east_corner);
        return;
    }
    fill_buffer(buffer, *source, are_flat, *source_rtd, target_keys, source_key);

    //if (x == world->world_data->world_width - 1 && (i == 14 || i == 15)  || y == world->world_data->world_height - 1 && (k == 14 || k == 15)) {
    //    const std::string prefix = "edge_incursion_graph_data.csv";
    //    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out | std::ofstream::app);

    //    myfile
    //        << x << ";"
    //        << y << ";"
    //        << i << ";"
    //        << k << ";"
    //        << current_tile_key << ";"
    //        << source_key << ";"
    //        << "south-east corner;"
    //        << target_keys[0] << ";"
    //        << target_keys[2] << ";"
    //        << target_keys[1] << ";\n"
    //        ;
    //}
}

void embark_assist::incursion::incursion_processor::process_north_east_corner(
    const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &eastern_tile,
    const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &eastern_rtd,
    const uint32_t current_tile_key, const uint32_t eastern_tile_key, 
    embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::mid_level_tile_basic *source = nullptr;
    const embark_assist::defs::region_tile_datum *source_rtd;
    uint32_t target_key = 0;
    uint32_t source_key = 0;

    const uint8_t north_east_corner = embark_assist::survey::translate_corner(survey_results, 5, x, y, i, k);
    const bool is_flat = current_tile.elevation == eastern_tile.elevation;
    if (north_east_corner == 4) {
        // the current (western) tile provides the data
        source = &current_tile;
        source_rtd = &current_rtd;
        target_key = eastern_tile_key;
        source_key = current_tile_key;
    }
    else if (north_east_corner == 5) {
        // the eastern tile provides the data
        source = &eastern_tile;
        source_rtd = &eastern_rtd;
        target_key = current_tile_key;
        source_key = eastern_tile_key;
    }

    fill_buffer(buffer, *source, is_flat, *source_rtd, target_key, source_key);
}

void embark_assist::incursion::incursion_processor::process_south_west_corner(
    const uint16_t x, const uint16_t y, const uint16_t i, const uint16_t k, const embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::mid_level_tile_basic &current_tile, const embark_assist::defs::mid_level_tile_basic &southern_tile,
    const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &southern_rtd,
    const uint32_t current_tile_key, const uint32_t southern_tile_key,
    embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::mid_level_tile_basic *source = nullptr;
    const embark_assist::defs::region_tile_datum *source_rtd;
    uint32_t target_key = 0;
    uint32_t source_key = 0;

    const uint8_t south_west_corner = embark_assist::survey::translate_corner(survey_results, 7, x, y, i, k);
    const bool is_flat = current_tile.elevation == southern_tile.elevation;
    if (south_west_corner == 4) {
        // the current (western) tile provides the data
        source = &current_tile;
        source_rtd = &current_rtd;
        target_key = southern_tile_key;
        source_key = current_tile_key;
    }
    else if (south_west_corner == 7) {
        // the southern tile provides the data
        source = &southern_tile;
        source_rtd = &southern_rtd;
        target_key = current_tile_key;
        source_key = southern_tile_key;
    }

    fill_buffer(buffer, *source, is_flat, *source_rtd, target_key, source_key);
}

// FIXME only for debugging - remove for release
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

// FIXME only for debugging - remove for release
void print_edges(const uint16_t x, const uint16_t y) {
    std::stringstream ss;
    ss << "split_x_" << unsigned(x) << "_" << unsigned(y);
    const std::string prefix = ss.str();
    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);

    for (uint8_t k = 0; k < 17; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            df::coord2d coord = world->world_data->region_details[0]->edges.split_x[i][k];
            //myfile << unsigned(i) << ";" << unsigned(k) << ";"  << unsigned(coord.x) << ";" << unsigned(coord.y) << ";";
            myfile << "(" << signed(coord.x) << "," << signed(coord.y) << ");";
        }
        myfile << "\n";
    }
    myfile.close();

    std::stringstream ss2;
    ss2 << "split_y_" << unsigned(x) << "_" << unsigned(y);
    const std::string prefix2 = ss2.str();
    auto myfile2 = std::ofstream(index_folder_name + prefix2, std::ios::out);

    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 17; i++) {
            df::coord2d coord = world->world_data->region_details[0]->edges.split_y[i][k];
            //myfile2 << unsigned(i) << ";" << unsigned(k) << ";" << unsigned(coord.x) << ";" << unsigned(coord.y) << ";";
            myfile2 << "(" << signed(coord.x) << "," << signed(coord.y) << ");";
        }
        myfile2 << "\n";
    }
    myfile2.close();
}

void embark_assist::incursion::incursion_processor::init_incursion_context(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd) {
    embark_assist::defs::region_tile_position world_position;
    uint8_t required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing;
    uint8_t required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search;
    retrieve_position_and_required_number_of_surveyed_neighbours(x, y, world_position, required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing, required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search);
    // FIXME: do it like this
    //retrieve_position_and_required_number_of_surveyed_neighbours(x, y, rtd.world_position, rtd.required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing, rtd.required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search);
    rtd.world_position = world_position;
    rtd.required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing;
    rtd.required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search;
}

void embark_assist::incursion::incursion_processor::process_internal_incursions(
        const uint32_t world_offset, const int16_t x, const int16_t y, embark_assist::defs::world_tile_data &survey_results,
        embark_assist::defs::mid_level_tiles &mlt_ref, embark_assist::defs::index_interface &index) {

    embark_assist::defs::mid_level_tiles *mlt = &mlt_ref;

    // FIXME only for debugging - remove for release
    /*if ((x == 15 && y == 0) || (x == 16 && y == 0) || (x == 15 && y == 1)) {
        print_biome_x(x, y, 16);
        print_biome_x(x, y, 17);
    }*/

    // FIXME only for debugging of world edge corner incursions- remove for release
    //if ((x == 256 && y == 246)) {
    ////if ((x == 4 || x == 5) && y == 5) {
    //    print_edges(x, y);
    //}

    // TODO: make sure we don't want to run this async/threaded/concurrent, otherwise we need to remove the class member "internal_buffer" and replace it with a local instance, so every call get its own instance
    embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer = internal_incursion_buffer();

    color_ostream_proxy out(Core::getInstance().getConsole());
    const auto start = std::chrono::steady_clock::now();
    embark_assist::defs::region_tile_datum &rtd = survey_results.at(x)[y];

    for (uint8_t i = 0; i < 15; i++) {
        for (uint8_t k = 0; k < 15; k++) {

            // is the western and the northern tile both
            const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(i)[k];
            const uint32_t current_tile_key = world_offset + k * 16 + i;

            // easter edge to right/eastern neighbour
            const embark_assist::defs::mid_level_tile_basic &eastern_tile = mlt->at(i + 1)[k];
            const uint32_t eastern_tile_key = current_tile_key + 1;
            process_eastern_edge(x, y, i, k, &survey_results, current_tile, eastern_tile, rtd, rtd, current_tile_key, eastern_tile_key, buffer);

            // southern edge to bottom/southern neighbour
            const embark_assist::defs::mid_level_tile_basic &southern_tile = mlt->at(i)[k + 1];
            const uint32_t southern_tile_key = world_offset + (k + 1) * 16 + i;
            process_southern_edge(x, y, i, k, &survey_results, current_tile, southern_tile, rtd, rtd, current_tile_key, southern_tile_key, buffer);

            const uint32_t south_eastern_tile_key = southern_tile_key + 1;
            const embark_assist::defs::mid_level_tile_basic &south_eastern_tile = mlt->at(i + 1)[k + 1];
            // south-east corner
            process_southern_east_corner(x, y, i, k, &survey_results,
                current_tile, eastern_tile, south_eastern_tile, southern_tile, 
                rtd, rtd, rtd, rtd, 
                current_tile_key, eastern_tile_key, south_eastern_tile_key, southern_tile_key, buffer);
        }
    }

    // southern edges of the eastern most column
    for (uint8_t k = 0; k < 15; k++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(15)[k];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = mlt->at(15)[k + 1];
        const uint32_t current_tile_key = world_offset + k * 16 + 15;
        const uint32_t southern_tile_key = world_offset + (k + 1) * 16 + 15;
        process_southern_edge(x, y, 15, k, &survey_results, current_tile, southern_tile, rtd, rtd, current_tile_key, southern_tile_key, buffer);
    }

    // eastern edges of the southern most row
    for (uint8_t i = 0; i < 15; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(i)[15];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = mlt->at(i + 1)[15];
        const uint32_t current_tile_key = world_offset + 15 * 16 + i;
        const uint32_t eastern_tile_key = current_tile_key + 1;
        process_eastern_edge(x, y, i, 15, &survey_results, current_tile, eastern_tile, rtd, rtd, current_tile_key, eastern_tile_key, buffer);
    }

    // cases sorted by frequency
    switch (rtd.world_position)
    {
    case embark_assist::defs::region_tile_position::north:
        process_additional_internal_incursions_for_north_tile(x, y, &survey_results, rtd, mlt, index, buffer);
        break;

    case embark_assist::defs::region_tile_position::west:
        process_additional_internal_incursions_for_west_tile(x, y, &survey_results, rtd, mlt, index, buffer);
        break;

    case embark_assist::defs::region_tile_position::north_west:
        process_additional_internal_incursions_for_north_tile(x, y, &survey_results, rtd, mlt, index, buffer);
        process_additional_internal_incursions_for_west_tile(x, y, &survey_results, rtd, mlt, index, buffer);
        break;

    // it seems for the other cases there is nothing to do here, or is there?

        default:
        break;
    }

    // add all data to the index
    index.add(buffer);
    index.check_for_find_single_world_tile_matches(x, y, rtd, "internal incursion");

    const auto end = std::chrono::steady_clock::now();
    internal_elapsed += end - start;

    //out.print("%llu - internal end of of async \n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void embark_assist::incursion::incursion_processor::process_additional_internal_incursions_for_north_tile(
    const uint16_t x, const uint16_t y, 
    embark_assist::defs::world_tile_data *survey_results, const embark_assist::defs::region_tile_datum &current_rtd, 
    embark_assist::defs::mid_level_tiles *mlt,
    embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {
    // additional processing 
    // process north east corners of northern row
    for (uint8_t i = 0; i < 15; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(i)[0];
        const uint32_t current_tile_key = index.get_key(x, y, i, 0);

        const embark_assist::defs::mid_level_tile_basic &eastern_tile = mlt->at(i + 1)[0];
        const uint32_t eastern_tile_key = index.get_key(x, y, i + 1, 0);

        process_north_east_corner(x, y, i, 0, survey_results,
            current_tile, eastern_tile, current_rtd, current_rtd,
            current_tile_key, eastern_tile_key, buffer);
    }
}

void embark_assist::incursion::incursion_processor::process_additional_internal_incursions_for_west_tile(
    const uint16_t x, const uint16_t y,
    embark_assist::defs::world_tile_data *survey_results, const embark_assist::defs::region_tile_datum &current_rtd,
    embark_assist::defs::mid_level_tiles *mlt,
    embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {
    // additional processing 
    // process south west corners of western row
    for (uint8_t k = 0; k < 15; k++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = mlt->at(0)[k];
        const uint32_t current_tile_key = index.get_key(x, y, 0, k);

        const embark_assist::defs::mid_level_tile_basic &southern_tile = mlt->at(0)[k + 1];
        const uint32_t southern_tile_key = index.get_key(x, y, 0, k + 1);

        process_south_west_corner(x, y, 0, k, survey_results,
            current_tile, southern_tile, current_rtd, current_rtd,
            current_tile_key, southern_tile_key, buffer);
    }
}

void embark_assist::incursion::incursion_processor::retrieve_position_and_required_number_of_surveyed_neighbours(
        const int16_t x, const int16_t y, embark_assist::defs::region_tile_position &world_position,
        uint8_t &required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing,
        uint8_t &required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search) {

    world_position = embark_assist::defs::region_tile_position::middle;
    required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 9;
    if (x == 0) {
        if (y == 0) {
            world_position = embark_assist::defs::region_tile_position::north_west;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 0;
        }
        else if (y == world->worldgen.worldgen_parms.dim_y - 1) {
            world_position = embark_assist::defs::region_tile_position::south_west;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 1;
        }
        else {
            world_position = embark_assist::defs::region_tile_position::west;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 1;
        }
    }
    else if (x == world->worldgen.worldgen_parms.dim_x - 1) {
        if (y == 0) {
            world_position = embark_assist::defs::region_tile_position::north_east;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 1;
        }
        else if (y == world->worldgen.worldgen_parms.dim_y - 1) {
            world_position = embark_assist::defs::region_tile_position::south_east;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 4;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 3;
        }
        else {
            world_position = embark_assist::defs::region_tile_position::east;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 3;
        }
    }
    else {
        if (y == 0) {
            world_position = embark_assist::defs::region_tile_position::north;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 1;
        }
        else if (y == world->worldgen.worldgen_parms.dim_y - 1) {
            world_position = embark_assist::defs::region_tile_position::south;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 6;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 3;
        }
        else {
            world_position = embark_assist::defs::region_tile_position::middle;
            required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing = 9;
            required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search = 3;
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
        const int16_t x, const int16_t y, embark_assist::defs::world_tile_data &survey_results, embark_assist::defs::index_interface &index) {

    color_ostream_proxy out(Core::getInstance().getConsole());

    const auto start = std::chrono::steady_clock::now();

    const embark_assist::defs::region_tile_datum &rtd = survey_results.at(x)[y];

    int8_t min_x, max_x, min_y, max_y;
    retrieve_min_max_offsets(rtd.world_position, min_x, max_x, min_y, max_y);

    //uint8_t position_index = 0;
    //std::array <position, 9> positions;

    for (uint16_t pos_x = x - min_x; pos_x <= x + max_x; pos_x++) {
        for (uint16_t pos_y = y - min_y; pos_y <= y + max_y; pos_y++) {
            embark_assist::defs::region_tile_datum &neighbour = survey_results.at(pos_x)[pos_y];
            ++neighbour.number_of_contiguous_surveyed_world_tiles;
            if (neighbour.number_of_contiguous_surveyed_world_tiles == neighbour.required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing) {
                process_external_incursions(pos_x, pos_y, &survey_results, index);
                //positions[position_index].x = pos_x;
                //positions[position_index++].y = pos_y;
            }
        }
    }

    //for (uint8_t current_index = 0; current_index < position_index; current_index++) {
    //    const position &pos = positions[current_index];
    //    embark_assist::defs::region_tile_datum &neighbour = survey_results.at(pos.x)[pos.y];
    //    index.check_for_find_single_world_tile_matches(pos.x, pos.y, neighbour);
    //}

    const auto end = std::chrono::steady_clock::now();
    update_and_check_external_elapsed += end - start;

    //out.print("%llu - internal end of external incursions async \n\n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_eastern_column(
    const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &eastern_neighbour_rtd,
    const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    for (uint8_t k = 0; k < 16; k++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.east_column[k];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = eastern_neighbour_rtd.west_column[k];
        const uint32_t current_tile_key = index.get_key(x, y, 15, k);
        const uint32_t eastern_tile_key = index.get_key(x + 1, y, 0, k);
        process_eastern_edge(x, y, 15, k, survey_results, current_tile, eastern_tile, current_rtd, eastern_neighbour_rtd, current_tile_key, eastern_tile_key, buffer);
    }

    // process south-east corners of eastern column 
    for (uint8_t k = 0; k < 15; k++) {
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
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_southern_row(
    const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::region_tile_datum &current_rtd, const embark_assist::defs::region_tile_datum &south_rtd,
    const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    for (uint8_t i = 0; i < 16; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.south_row[i];
        const embark_assist::defs::mid_level_tile_basic &southern_tile = south_rtd.north_row[i];
        const uint32_t current_tile_key = index.get_key(x, y, i, 15);
        const uint32_t southern_tile_key = index.get_key(x, y + 1, i, 0);
        process_southern_edge(
            x, y, i, 15, survey_results, current_tile, southern_tile,
            current_rtd, south_rtd, current_tile_key, southern_tile_key, buffer);
    }

    // process south-east corners of southern row
    for (uint8_t i = 0; i < 15; i++) {
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
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_middle_tile(
        const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
        const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::region_tile_datum &current_rtd = survey_results->at(x)[y];

    // process eastern edge to east region/world tile neighbour
    const embark_assist::defs::region_tile_datum &eastern_neighbour_rtd = survey_results->at(x + 1)[y];
    process_external_incursions_for_eastern_column(x, y, survey_results, current_rtd, eastern_neighbour_rtd, index, buffer);

    const embark_assist::defs::region_tile_datum &south_rtd = survey_results->at(x)[y + 1];

    // special case, as the south-eastern neighbour is different world tile than the eastern neighbor for sure
    // also BEWARE: never remove the "superfluous" brackets, defining a block, to keep the additional variables in a local scope
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
    process_external_incursions_for_southern_row(x, y, survey_results, current_rtd, south_rtd, index, buffer);

    // no need to process the south east corner of i = 15, k = 15 (as seen above for eastern col), we already did that while processing south-east corners of the the eastern column above
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_north_tile(
    const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::region_tile_datum &current_rtd = survey_results->at(x)[y];

    // last north-east corner i = 15, k = 0
    const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.north_row[15];
    const uint32_t current_tile_key = index.get_key(x, y, 15, 0);

    const embark_assist::defs::region_tile_datum &eastern_rtd = survey_results->at(x + 1)[y];
    const embark_assist::defs::mid_level_tile_basic &eastern_tile = eastern_rtd.north_row[0];
    const uint32_t eastern_tile_key = index.get_key(x + 1, y, 0, 0);

    process_north_east_corner(x, y, 15, 0, survey_results,
        current_tile, eastern_tile, current_rtd, eastern_rtd,
        current_tile_key, eastern_tile_key, buffer);
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_east_tile(
    const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    // processing the south-east corners of the eastern-column (process_external_incursions_for_eastern_column) does not make any sense here
    // as translate_corner always returns 4 (=> self reference) for the south-east corner (corner_location = 8) at the eastern edge of the world

    // only south edge and south-east-corners of south row
    const embark_assist::defs::region_tile_datum &current_rtd = survey_results->at(x)[y];
    const embark_assist::defs::region_tile_datum &south_rtd = survey_results->at(x)[y + 1];
    process_external_incursions_for_southern_row(x, y, survey_results, current_rtd, south_rtd, index, buffer);
}

// can only be used for the south-most region/world tiles
void embark_assist::incursion::incursion_processor::process_external_incursions_for_south_tile(
    const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {
    // only east edge and south-east-corners of east column
    const embark_assist::defs::region_tile_datum &current_rtd = survey_results->at(x)[y];
    const embark_assist::defs::region_tile_datum &eastern_neighbour_rtd = survey_results->at(x + 1)[y];
    process_external_incursions_for_eastern_column(x, y, survey_results, current_rtd, eastern_neighbour_rtd, index, buffer);

    // processing south-east corners of southern row
    // not processing i = 15 && k = 15 as it is done in process_external_incursions_for_eastern_column
    // translate_corner always returns 5 for this case (south-east corners at the southern edge of the world)
    // so this actually always results in an incursion from the eastern neighbour to the current tile 
    for (uint8_t i = 0; i < 15; i++) {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.south_row[i];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = current_rtd.south_row[i + 1];
        const bool is_flat = current_tile.elevation == eastern_tile.elevation;
        const uint32_t current_tile_key = index.get_key(x, y, i, 15);
        fill_buffer(buffer, eastern_tile, is_flat, current_rtd, current_tile_key, current_tile_key + 1);
    }

    // special case, as the eastern neighbour is actually another region_tile_datum
    // also BEWARE: never remove the "superfluous" brackets, it is defining a block to keep the additional variables in a local scope
    {
        const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.south_row[15];
        const embark_assist::defs::mid_level_tile_basic &eastern_tile = eastern_neighbour_rtd.south_row[0];
        const bool is_flat = current_tile.elevation == eastern_tile.elevation;
        const uint32_t current_tile_key = index.get_key(x, y, 15, 15);
        const uint32_t easter_tile_key = index.get_key(x + 1, y, 0, 15);
        fill_buffer(buffer, eastern_tile, is_flat, eastern_neighbour_rtd, current_tile_key, easter_tile_key);
    }
}

void embark_assist::incursion::incursion_processor::process_external_incursions_for_west_tile(
    const uint16_t x, const uint16_t y, embark_assist::defs::world_tile_data *survey_results,
    const embark_assist::defs::index_interface &index, embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer) {

    const embark_assist::defs::region_tile_datum &current_rtd = survey_results->at(x)[y];

    // last south-west corner i = 0, k = 15
    const embark_assist::defs::mid_level_tile_basic &current_tile = current_rtd.west_column[15];
    const uint32_t current_tile_key = index.get_key(x, y, 0, 15);

    const embark_assist::defs::region_tile_datum &southern_rtd = survey_results->at(x)[y + 1];
    const embark_assist::defs::mid_level_tile_basic &eastern_tile = southern_rtd.west_column[0];
    const uint32_t eastern_tile_key = index.get_key(x, y + 1, 0, 0);

    process_south_west_corner(x, y, 0, 15, survey_results,
        current_tile, eastern_tile, current_rtd, southern_rtd,
        current_tile_key, eastern_tile_key, buffer);
}

void embark_assist::incursion::incursion_processor::process_external_incursions(
        const int16_t x, const int16_t y, embark_assist::defs::world_tile_data *survey_results,
        embark_assist::defs::index_interface &index) {

    embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];

    // TODO: test if moving this into the state of survey brings possible performance gains
    // if so we would need to finish the previous processing pass before we start a new one
    external_incursion_buffer buffer;
    // buffer.reset();

    // cases sorted by frequency first, than clockwise from north-west to west
    switch (rtd.world_position) {
    case embark_assist::defs::region_tile_position::middle:
        process_external_incursions_for_middle_tile(x,y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::north:
        // mostly the same as the middle region tile
        process_external_incursions_for_middle_tile(x, y, survey_results, index, buffer);
        process_external_incursions_for_north_tile(x, y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::west:
        // mostly the same as the middle region tile
        process_external_incursions_for_middle_tile(x, y, survey_results, index, buffer);
        process_external_incursions_for_west_tile(x, y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::south:
        // mostly NOT the same as the middle region tile
        process_external_incursions_for_south_tile(x, y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::east:
        // mostly NOT the same as the middle region tile
        process_external_incursions_for_east_tile(x, y, survey_results, index, buffer);
        break;

    // following all the corner cases
    case embark_assist::defs::region_tile_position::north_west:
        // mostly the same as the middle region tile
        process_external_incursions_for_middle_tile(x, y, survey_results, index, buffer);
        process_external_incursions_for_north_tile(x, y, survey_results, index, buffer);
        process_external_incursions_for_west_tile(x, y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::north_east:
        // mostly NOT the same as the middle region tile
        // BE AWARE: the north-east corner of the east column (x = 15, i = 0) can't be processed in this case,
        // the eastern neighbour would have x = world_width and therefore does not exist
        //process_external_incursions_for_north_tile(x, y, survey_results, index, buffer);
        process_external_incursions_for_east_tile(x, y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::south_west:
        // mostly NOT the same as the middle region tile
        process_external_incursions_for_south_tile(x, y, survey_results, index, buffer);
        // BE AWARE: the south-west corner of the west column (x = 0, i = 15) can't be processed in this case,
        // the southern neighbour would have y = world_height and therefore does not exist
        // process_external_incursions_for_west_tile(x, y, survey_results, index, buffer);
        break;
    case embark_assist::defs::region_tile_position::south_east:
        // mostly not the same as the middle region tile
        // actually nothing to do at all
        break;
    }

    index.add(buffer);
    rtd.totally_processed = true;

    // all outgoing (and activly pulled incoming) incursions processed
    index.check_for_find_single_world_tile_matches(x, y, rtd, "actively outgoing external incursion");
    // update relevant neighbors state concerning their passively incoming incursions
    update_and_check_external_incursion_counters_of_neighbouring_world_tiles(x, y, survey_results, index);
}

void embark_assist::incursion::incursion_processor::update_and_check_external_incursion_counters_of_neighbouring_world_tiles(const int16_t x, const int16_t y, embark_assist::defs::world_tile_data *survey_results, embark_assist::defs::index_interface &index) {
    embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];

    switch (rtd.world_position) {
    case embark_assist::defs::region_tile_position::north_west:
        // special case: the north east world tile never gets "pushed" incoming incursions, so it takes care of this itself
        update_and_check_external_incursion_counters_of_neighbouring_world_tile(x, y, survey_results->at(x)[y], index);
    case embark_assist::defs::region_tile_position::north:
    case embark_assist::defs::region_tile_position::middle:
    case embark_assist::defs::region_tile_position::west: {
        embark_assist::defs::region_tile_datum &eastern_neighbour = survey_results->at(x + 1)[y];
        update_and_check_external_incursion_counters_of_neighbouring_world_tile(x + 1, y, eastern_neighbour, index);

        embark_assist::defs::region_tile_datum &south_eastern_neighbour = survey_results->at(x + 1)[y + 1];
        update_and_check_external_incursion_counters_of_neighbouring_world_tile(x + 1, y + 1, south_eastern_neighbour, index);

        // debugging
        //if (x == 0 && y + 1 == 4) {
        //    color_ostream_proxy out(Core::getInstance().getConsole());
        //    auto t = std::chrono::high_resolution_clock::now();
        //    out.print("embark_assist::incursion::incursion_processor::update_and_check_external_incursion_counters_of_neighbouring_world_tiles: x == 0 && y == 4 at %lld\n", static_cast<long long int>(t.time_since_epoch().count()));
        //}
        embark_assist::defs::region_tile_datum &southern_neighbour = survey_results->at(x)[y + 1];
        update_and_check_external_incursion_counters_of_neighbouring_world_tile(x, y + 1, southern_neighbour, index);
    }
        break;

    case embark_assist::defs::region_tile_position::north_east:
    case embark_assist::defs::region_tile_position::east: {
        embark_assist::defs::region_tile_datum &southern_neighbour = survey_results->at(x)[y + 1];
        update_and_check_external_incursion_counters_of_neighbouring_world_tile(x, y + 1, southern_neighbour, index);
    }
        break;

    case embark_assist::defs::region_tile_position::south_west:
    case embark_assist::defs::region_tile_position::south: {
        embark_assist::defs::region_tile_datum &eastern_neighbour = survey_results->at(x + 1)[y];
        update_and_check_external_incursion_counters_of_neighbouring_world_tile(x + 1, y, eastern_neighbour, index);
    }
        break;

    case embark_assist::defs::region_tile_position::south_east:
        // actually nothing to do at all
        break;
    }
}

void embark_assist::incursion::incursion_processor::update_and_check_external_incursion_counters_of_neighbouring_world_tile(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, embark_assist::defs::index_interface &index) {
    ++rtd.number_of_neighboring_incursion_processed_world_tiles;
    if (rtd.number_of_neighboring_incursion_processed_world_tiles >= rtd.required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search) {
        // all passively incoming incursions processed
        index.check_for_find_single_world_tile_matches(x, y, rtd, "passively incoming external incursion");
    }
}

embark_assist::incursion::incursion_processor::incursion_processor(): internal_buffer(new internal_incursion_buffer()) {
    // FIXME: debug code, to be removed
    /*{
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/0_hasAquifier.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        aquifer = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }

    {
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/358_is_unflat_by_incursion.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        unflat = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }

    {
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/7_soil_level_0.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        soil0 = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }

    {
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/3_hasClay.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        clay = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }

    {
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/26_savagery_level_0.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        savagery0 = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }

    {
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/29_evilness_level_0.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        evilness0 = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }

    {
        std::string filePath = "e:/games/df/PeridexisErrant's Starter Pack 0.44.12-r06/Dwarf Fortress 0.44.12/data/init/embark_assistant_indexes/_new_matching_code_region4_all_asnyc_incursions_but_no_overrun_run7/287_biome_0.index";
        std::ifstream indexFile(filePath, std::ios::in | std::ios::binary);

        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(indexFile), {});
        biome0 = std::move(Roaring::readSafe((const char*)buffer.data(), buffer.size()));

        indexFile.close();
    }*/
}

embark_assist::incursion::incursion_processor::~incursion_processor() {
    delete internal_buffer;

    // all the following is only for debugging
    color_ostream_proxy out(Core::getInstance().getConsole());

    out.print("internal incursions took %f seconds\n", internal_elapsed);
    out.print("external incursions + checking took %f seconds\n", update_and_check_external_elapsed);

    out.print("normal survey - max index size for N=%d: %d\n", 256, embark_assist::key_buffer_holder::basic_key_buffer_holder<256>::max_index);
    out.print("internal - max index size for N=%d: %d\n", embark_assist::incursion::incursion_processor::size_of_internal_incursion_buffer, embark_assist::key_buffer_holder::basic_key_buffer_holder<embark_assist::incursion::incursion_processor::size_of_internal_incursion_buffer>::max_index);
    out.print("external - max index size for N=%d: %d\n", embark_assist::incursion::incursion_processor::size_of_external_incursion_buffer, embark_assist::key_buffer_holder::basic_key_buffer_holder<embark_assist::incursion::incursion_processor::size_of_external_incursion_buffer>::max_index);
}
