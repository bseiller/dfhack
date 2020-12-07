#pragma once

#include <array>
#include <string>
#include <vector>

#include "DataDefs.h"

#include "df/biome_type.h"
#include "df/world_region_type.h"

using namespace std;
using std::array;
using std::ostringstream;
using std::string;
using std::vector;

#define fileresult_file_name "./data/init/embark_assistant_fileresult.txt"
#define index_folder_name "./data/init/embark_assistant_indexes/"
#define index_folder_log_file_name "./data/init/embark_assistant_indexes/index.log"
#define rivers_file_name "./data/init/embark_assistant_indexes/rivers"
#define river_anomalies_file_name "./data/init/embark_assistant_indexes/rivers_anomalies"
#define invalid_rivers_file_name "./data/init/embark_assistant_indexes/invalid_rivers"

namespace embark_assist {
    namespace defs {

        static const uint8_t SOIL_DEPTH_LEVELS = 5;
        static const uint8_t ARRAY_SIZE_FOR_BIOMES = ENUM_LAST_ITEM(biome_type) + 1;
        static const uint8_t ARRAY_SIZE_FOR_REGION_TYPES = ENUM_LAST_ITEM(world_region_type) + 1;
        static const uint8_t ARRAY_SIZE_FOR_RIVER_SIZES = 6;
        // 1-9 drops + everything > 9 => == 9, index 0 is never used
        static const uint8_t ARRAY_SIZE_FOR_WATERFALL_DROPS = 10;

        //  Survey types
        //
        enum class river_sizes {
            None,
            Brook,
            Stream,
            Minor,
            Medium,
            Major
        };

        // only contains those attributes that are being used during incursion processing
        // leads to a significantly smaller memory footprint => 10 Bytes
        // also ordered members by size, which again might lead to a smaller memory footprint, by better alignment
        struct mid_level_tile_basic {
            int16_t elevation;

            bool aquifer = false;
            bool clay = false;
            bool sand = false;

            int8_t soil_depth;
            int8_t biome_offset;
            uint8_t savagery_level;  // 0 - 2
            uint8_t evilness_level;  // 0 - 2
        };

        // contains all attributes, used for regular survey/matching => 120 Bytes
        // also ordered members by size, which again might lead to a smaller memory footprint, by better alignment
        struct mid_level_tile : public mid_level_tile_basic {
            std::vector<bool> metals;
            std::vector<bool> economics;
            std::vector<bool> minerals;
            int16_t river_elevation = 100;

            int8_t adamantine_level;  // -1 = none, 0 .. 3 = cavern 1 .. magma sea. Currently not used beyond present/absent.
            int8_t magma_level;  // -1 = none, 0 .. 3 = cavern 3 .. surface/volcano
            int8_t offset;

            bool flux = false;
            bool coal = false;

            bool river_present = false;
        };

        typedef std::array<std::array<mid_level_tile, 16>, 16> mid_level_tiles;

        enum class region_tile_position {
            north_west,
            north,
            north_east,
            west,
            middle,
            east,
            south_west,
            south,
            south_east
        };

        struct region_tile_datum {
            bool surveyed = false;
            uint16_t aquifer_count = 0;
            uint16_t clay_count = 0;
            uint16_t sand_count = 0;
            uint16_t flux_count = 0;
            uint16_t coal_count = 0;
            uint8_t min_region_soil = 10;
            uint8_t max_region_soil = 0;
            uint8_t max_waterfall = 0;
            river_sizes river_size;
            int16_t biome_index[10];  // Indexed through biome_offset; -1 = null, Index of region, [0] not used
            int16_t biome[10];        // Indexed through biome_offset; -1 = null, df::biome_type, [0] not used
            uint8_t biome_count;
            int16_t min_temperature[10];  // Indexed through biome_offset; -30000 = null, Urists - 10000, [0] not used
            int16_t max_temperature[10];  // Indexed through biome_offset; -30000 = null, Urists - 10000, [0] not used
            bool blood_rain[10];
            bool blood_rain_possible;
            bool blood_rain_full;
            bool permanent_syndrome_rain[10];
            bool permanent_syndrome_rain_possible;
            bool permanent_syndrome_rain_full;
            bool temporary_syndrome_rain[10];
            bool temporary_syndrome_rain_possible;
            bool temporary_syndrome_rain_full;
            bool reanimating[10];
            bool reanimating_possible;
            bool reanimating_full;
            bool thralling[10];
            bool thralling_possible;
            bool thralling_full;
            uint16_t savagery_count[3];
            uint16_t evilness_count[3];
            std::vector<bool> metals;
            std::vector<bool> economics;
            std::vector<bool> minerals;
            // using mid_level_tile_basic instead of mid_level_tile reduces the base size of region_tile_datum from 7984 Bytes to 944 Bytes
            // for a world with 257x257 this leads to memory saving of about 450 MB
            mid_level_tile_basic north_row[16];
            mid_level_tile_basic south_row[16];
            mid_level_tile_basic west_column[16];
            mid_level_tile_basic east_column[16];
            //uint8_t north_corner_selection[16]; //  0 - 3. For some reason DF stores everything needed for incursion
            //uint8_t west_corner_selection[16];  //  detection in 17:th row/colunm data in the region details except
                                                //  this info, so we have to go to neighboring world tiles to fetch it.
            df::world_region_type region_type[16][16];  //  Required for incursion override detection. We could store only the
                                                //  edges, but storing it for every tile allows for a unified fetching
                                                //  logic.

            region_tile_position world_position = region_tile_position::middle;
            uint8_t number_of_contiguous_surveyed_world_tiles = 0;
            uint8_t required_number_of_contiguous_surveyed_world_tiles_for_incursion_processing;
            uint8_t number_of_neighboring_incursion_processed_world_tiles = 0;
            uint8_t required_number_of_neighboring_incursion_processed_world_tiles_for_iterative_search;
            bool totally_processed = false;
            std::unique_ptr<std::atomic_uint8_t> process_counter{ new std::atomic_uint8_t(0) };
            //uint8_t northern_row_biome_x[16]; /*!< 0=Reference is N, 1=Reference is current tile (adopted by S edge to the N) */
            //uint8_t western_column_biome_y[16]; /*!< 0=Reference is W, 1=Reference is current tile (Adopted by E edge to the W) */

            // retaining the whole biome_edge and corner data in region_tile_datum to make asnychronous/concurrent accessing/processing possible
            // accessing world_data->region_details[0]->edges.biome_x/y/corner would lead to invalid data as the world cursor will have moved on
            // and world_data->region_details[0] now contains the data of another world tile
            int8_t biome_corner[16][16]; /*!< 0=Reference is NW, 1=Reference is N, 2=Reference is W, 3=Reference is current tile */
            int8_t biome_x[16][16]; /*!< 0=Reference is N, 1=Reference is current tile (adopted by S edge to the N) */
            int8_t biome_y[16][16]; /*!< 0=Reference is W, 1=Reference is current tile (Adopted by E edge to the W) */
        };

        struct geo_datum {
            uint8_t soil_size = 0;
            bool top_soil_only = true;
            bool top_soil_aquifer_only = true;
            bool aquifer_absent = true;
            bool clay_absent = true;
            bool sand_absent = true;
            bool flux_absent = true;
            bool coal_absent = true;
            std::vector<bool> possible_metals;
            std::vector<bool> possible_economics;
            std::vector<bool> possible_minerals;
        };

        typedef std::vector<geo_datum> geo_data;

        struct sites {
            uint8_t x;
            uint8_t y;
            char type;
        };

        struct site_infos {
            bool incursions_processed;
            bool aquifer;
            bool aquifer_full;
            uint8_t min_soil;
            uint8_t max_soil;
            bool flat;
            uint8_t max_waterfall;
            bool clay;
            bool sand;
            bool flux;
            bool coal;
            bool blood_rain;
            bool permanent_syndrome_rain;
            bool temporary_syndrome_rain;
            bool reanimating;
            bool thralling;
            std::vector<uint16_t> metals;
            std::vector<uint16_t> economics;
            std::vector<uint16_t> minerals;
            //  Could add savagery, evilness, and biomes, but DF provides those easily.
        };

        typedef std::vector<sites> site_lists;

        typedef std::vector<std::vector<region_tile_datum>> world_tile_data;

        typedef bool mlt_matches[16][16];
        //  An embark region match is indicated by marking the top left corner
        //  tile as a match. Thus, the bottom and right side won't show matches
        //  unless the appropriate dimension has a width of 1.

        struct matches {
            bool preliminary_match;
            bool contains_match;
            mlt_matches mlt_match;
        };

        typedef std::vector<std::vector<matches>> match_results;

        //  matcher types
        //
        enum class evil_savagery_values : int8_t {
            NA = -1,
            All,
            Present,
            Absent
        };

        enum class evil_savagery_ranges : int8_t {
            Low,
            Medium,
            High
        };

        enum class aquifer_ranges : int8_t {
            NA = -1,
            All,
            Present,
            Partial,
            Not_All,
            Absent
        };

        enum class river_ranges : int8_t {
            NA = -1,
            None,
            Brook,
            Stream,
            Minor,
            Medium,
            Major
        };

//  For possible future use. That's the level of data actually collected.
//        enum class adamantine_ranges : int8_t {
//            NA = -1,
//            Cavern_1,
//            Cavern_2,
//            Cavern_3,
//            Magma_Sea
//        };

        enum class magma_ranges : int8_t {
            NA = -1,
            Cavern_3,
            Cavern_2,
            Cavern_1,
            Volcano
        };

        enum class yes_no_ranges : int8_t {
            NA = -1,
            Yes,
            No
        };

        enum class all_present_ranges : int8_t {
            All,
            Present
        };
        enum class present_absent_ranges : int8_t {
            NA = -1,
            Present,
            Absent
        };

        enum class soil_ranges : int8_t {
            NA = -1,
            None,
            Very_Shallow,
            Shallow,
            Deep,
            Very_Deep
        };

        enum class syndrome_rain_ranges : int8_t {
            NA = -1,
            Any,
            Permanent,
            Temporary,
            Not_Permanent,
            None
        };

        enum class reanimation_ranges : int8_t {
            NA = -1,
            Both,
            Any,
            Thralling,
            Reanimation,
            Not_Thralling,
            None
        };

        enum class freezing_ranges : int8_t {
            NA = -1,
            Permanent,
            At_Least_Partial,
            Partial,
            At_Most_Partial,
            Never
        };

        struct finders {
            uint16_t x_dim;
            uint16_t y_dim;
            evil_savagery_values savagery[static_cast<int8_t>(evil_savagery_ranges::High) + 1];
            evil_savagery_values evilness[static_cast<int8_t>(evil_savagery_ranges::High) + 1];
            aquifer_ranges aquifer;
            river_ranges min_river;
            river_ranges max_river;
            int8_t min_waterfall; // N/A(-1), Absent, 1-9
            yes_no_ranges flat;
            present_absent_ranges clay;
            present_absent_ranges sand;
            present_absent_ranges flux;
            present_absent_ranges coal;
            soil_ranges soil_min;
            all_present_ranges soil_min_everywhere;
            soil_ranges soil_max;
            freezing_ranges freezing;
            yes_no_ranges blood_rain;  //  Will probably blow up with the magic release arcs...
            syndrome_rain_ranges syndrome_rain;
            reanimation_ranges reanimation;
            int8_t spire_count_min; // N/A(-1), 0-9
            int8_t spire_count_max; // N/A(-1), 0-9
            magma_ranges magma_min;
            magma_ranges magma_max;
            int8_t biome_count_min; // N/A(-1), 1-9
            int8_t biome_count_max; // N/A(-1), 1-9
            int8_t region_type_1;   // N/A(-1), df::world_region_type
            int8_t region_type_2;   // N/A(-1), df::world_region_type
            int8_t region_type_3;   // N/A(-1), df::world_region_type
            int8_t biome_1;         // N/A(-1), df::biome_type
            int8_t biome_2;         // N/A(-1), df::biome_type
            int8_t biome_3;         // N/A(-1), df::biome_type
            int16_t metal_1;        // N/A(-1), 0-max_inorganic;
            int16_t metal_2;        // N/A(-1), 0-max_inorganic;
            int16_t metal_3;        // N/A(-1), 0-max_inorganic;
            int16_t economic_1;     // N/A(-1), 0-max_inorganic;
            int16_t economic_2;     // N/A(-1), 0-max_inorganic;
            int16_t economic_3;     // N/A(-1), 0-max_inorganic;
            int16_t mineral_1;      // N/A(-1), 0-max_inorganic;
            int16_t mineral_2;      // N/A(-1), 0-max_inorganic;
            int16_t mineral_3;      // N/A(-1), 0-max_inorganic;
        };

        struct match_iterators {
            bool active;
            uint16_t x;  //  x position of focus when iteration started so we can return it.
            uint16_t y;  //  y
            uint16_t i;
            uint16_t k;
            bool x_right;
            bool y_down;
            bool inhibit_x_turn;
            bool inhibit_y_turn;
            uint16_t target_location_x;
            uint16_t target_location_y;
            uint16_t count;
            finders finder;
        };

        typedef void(*find_callbacks) (const embark_assist::defs::finders &finder);

        class key_buffer_holder_basic_interface {
        public:
            virtual void add_unflat(const uint32_t key) = 0;
            virtual void get_unflat_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_aquifer(const uint32_t key, const bool has_aquifer) = 0;
            virtual void get_aquifer_buffer(uint16_t &index, const uint32_t *&buffer, const bool has_aquifer) const = 0;
            virtual void add_clay(const uint32_t key) = 0;
            virtual void get_clay_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_sand(const uint32_t key) = 0;
            virtual void get_sand_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_soil_depth(const uint32_t key, const int8_t soil_depth) = 0;
            virtual void get_soil_depth_buffers(const std::array<uint16_t, SOIL_DEPTH_LEVELS> *&indices, const std::array<uint32_t *, SOIL_DEPTH_LEVELS> *&buffers) const = 0;
            virtual void add_savagery_level(const uint32_t key, const uint8_t savagery_level) = 0;
            virtual void get_savagery_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const = 0;
            virtual void add_evilness_level(const uint32_t key, const uint8_t evilness_level) = 0;
            virtual void get_evilness_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const = 0;
            virtual void add_biome(const uint32_t key, const int16_t biome) = 0;
            virtual void get_biome_buffers(const std::array<int16_t, ARRAY_SIZE_FOR_BIOMES> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_BIOMES> *&buffers) const = 0;
            virtual void add_region_type(const uint32_t key, const int8_t region_type) = 0;
            virtual void get_region_type_buffers(const std::array<int16_t, ARRAY_SIZE_FOR_REGION_TYPES> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_REGION_TYPES> *&buffers) const = 0;
            virtual void reset() = 0;
            virtual ~key_buffer_holder_basic_interface() {}
        };

        class key_buffer_holder_interface : public virtual key_buffer_holder_basic_interface {
        public:
            virtual void get_coal_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void get_flux_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void get_adamantine_level_buffers(const std::array<uint16_t, 4> *&indices, const std::array<uint32_t *, 4> *&buffers) const = 0;
            virtual void get_magma_level_buffers(const std::array<uint16_t, 4> *&indices, const std::array<uint32_t *, 4> *&buffers) const = 0;
            virtual void get_metal_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t*> *&buffers) const = 0;
            virtual void get_economic_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t*> *&buffers) const = 0;
            virtual void get_mineral_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t*> *&buffers) const = 0;
            virtual void get_river_size_buffers(const std::array<uint16_t, ARRAY_SIZE_FOR_RIVER_SIZES> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_RIVER_SIZES> *&buffers) const = 0;
            virtual void get_waterfall_drop_buffers(const std::array<uint16_t, ARRAY_SIZE_FOR_WATERFALL_DROPS> *&indices, const std::array<std::array<std::pair<uint32_t, uint32_t>, 480>, ARRAY_SIZE_FOR_WATERFALL_DROPS> *&buffers) const = 0;
            virtual void get_no_waterfall_buffers(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void get_mapped_elevation_buffer(uint16_t &index, const uint8_t *&buffer, uint32_t &initial_offset) const = 0;
        };

        class index_interface {
            public:
                virtual const bool containsEntries() const = 0;
                virtual void add(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, const embark_assist::defs::key_buffer_holder_interface &buffer_holder) = 0;
                virtual void add(const embark_assist::defs::key_buffer_holder_basic_interface &buffer_holder) = 0;
                virtual void optimize(bool debugOutput) = 0;
                virtual void find_all_matches(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results) const = 0;
                virtual void check_for_find_single_world_tile_matches(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, const string &prefix) = 0;
                virtual void find_matches_in_surveyed_world_tiles() const = 0;
                virtual const uint32_t get_key(const int16_t x, const int16_t y) const = 0;
                virtual const uint32_t get_key(const int16_t x, const int16_t y, const uint16_t i, const uint16_t k) const = 0;
                virtual ~index_interface(){}
        };
    }
}