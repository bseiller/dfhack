#pragma once

#include <unordered_map>

//#include "DataDefs.h"
//#include "df/biome_type.h"
//#include "df/world_region_type.h"

#include "defs.h"
#include "inorganics_information.h"
#include "key_position_mapper.h"
#include "roaring.hh"

namespace embark_assist {
    namespace index {

        struct position {
            int16_t x;
            int16_t y;
        };

        struct waterfall_drop_bucket {
            uint8_t counter = 1;
            std::array<uint32_t, 2> keys;

            waterfall_drop_bucket(const uint32_t first_key) {
                keys[0] = first_key;
            };

            void add(const uint32_t key) {
                keys[++counter - 1] = key;
            }
        };

        struct embark_tile_tracker {
            // x within the world/world tile x
            uint16_t x = 65535;
            // y within the world/world tile y
            uint16_t y = 65535;
            // embark tile x/y
            bool was_already_used_as_start_position[16][16];
        };

        class query_plan_interface {
            public:
                virtual ~query_plan_interface() {};
                virtual bool execute(const Roaring &embark_candidate) const = 0;

                // TODO: make this a smart iterator instead of a vector - which would save same memory...
                virtual const std::vector<uint32_t>& get_most_significant_ids() const = 0;
                virtual void set_most_significant_ids(const std::vector<uint32_t>* ids) = 0;

                // FIXME: add method to query only for a specific whole world tile at a time...
        };

        class Index final : public embark_assist::defs::index_interface {
        public:
            
        private:
            static const uint32_t NUMBER_OF_EMBARK_TILES = 16 * 16;
            static const uint32_t NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL = NUMBER_OF_EMBARK_TILES * 256 - 1;
            bool was_optimized_in_current_feature_shell = false;
            df::world *world;
            uint16_t max_inorganic = 0;
            uint32_t capacity;

            embark_assist::index::key_position_mapper::KeyPositionMapper *keyMapper;

            uint32_t entryCounter = 0;
            uint16_t feature_set_counter = 0;
            
            uint32_t maxKeyValue = 0;
            uint32_t previous_key = -1;
            
            // following 1 is for debugging - remove again
            Roaring uniqueKeys;

            Roaring hasAquifer;
            Roaring hasClay;
            Roaring hasCoal;
            Roaring hasFlux;
            Roaring hasRiver;
            Roaring hasSand;
            Roaring is_unflat_by_incursion;
            std::array<Roaring, embark_assist::defs::SOIL_DEPTH_LEVELS> soil;
            std::array<Roaring, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> river_size;
            std::array<Roaring, 4> magma_level;
            std::array<Roaring, 4> adamantine_level;
            std::array<Roaring, 3> savagery_level;
            std::array<Roaring, 3> evilness_level;
            std::array<Roaring, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> biome;
            std::array<Roaring, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> region_type;
            std::array<unordered_map<uint32_t, waterfall_drop_bucket*>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> waterfall_drops;
            Roaring no_waterfall;

            std::vector<Roaring*> metals;
            std::vector<Roaring*> economics;
            std::vector<Roaring*> minerals;

            std::vector<Roaring*> inorganics;

            // following 2 are only for debugging - remove again
            std::vector<uint32_t> keys_in_order;
            std::vector <position> positions;

            // helper for easier reset, debug and output of over all indices
            std::vector<Roaring*> static_indices;

            const embark_assist::inorganics::inorganics_information &inorganics_info;

            const std::list<uint16_t>& metal_indices;
            const std::list<uint16_t>& economic_indices;
            const std::list<uint16_t>& mineral_indices;

            const std::unordered_map<uint16_t, std::string>& metal_names;
            const std::unordered_map<uint16_t, std::string> &economic_names;
            const std::unordered_map<uint16_t, std::string> &mineral_names;

            std::chrono::duration<double> innerElapsed_seconds = std::chrono::seconds(0);
            std::chrono::duration<double> index_adding_seconds = std::chrono::seconds(0);
            std::chrono::duration<double> inorganics_processing_seconds = std::chrono::seconds(0);

            // Roaring& getInorganicsIndex(std::vector<Roaring*> &indexMap, uint16_t metal_index);
            void init_inorganic_indices();
            // void addInorganic(std::vector<uint16_t, Roaring*> &indexMap, uint16_t metal_index);
            std::string getInorganicName(const uint16_t index, const std::unordered_map<uint16_t, std::string> &ingorganicNames, std::string name) const;
            void embark_assist::index::Index::writeIndexToDisk(const Roaring &roaring, const std::string prefix) const;
            uint16_t calculate_embark_variants(const uint32_t position_id, const uint16_t embark_size_x, const uint16_t embark_size_y, std::vector<Roaring> &embarks, uint32_t buffer[], embark_tile_tracker &tracker) const;
            const embark_assist::index::query_plan_interface* embark_assist::index::Index::create_query_plan(const embark_assist::defs::finders &finder) const;
            const std::vector<uint32_t>* embark_assist::index::Index::get_keys(const Roaring &index) const;
            void add_buffers_to_indices(const embark_assist::defs::key_buffer_holder_basic_interface &buffer_holder2);
            void process_incursions(const int16_t x, const int16_t y, const uint32_t world_offset, const embark_assist::defs::region_tile_datum &rtd, const embark_assist::defs::mid_level_tiles *mlt);
            void optimize(Roaring *index, bool shrinkToSize);
            const void outputContents() const;
            const void outputSizes(const string &prefix);
        public:
            Index(df::world *world);
            ~Index();
            void setup(uint16_t max_inorganic);
            void shutdown();
            virtual const bool containsEntries() const final override;
            virtual void add(const int16_t x, const int16_t y, const embark_assist::defs::region_tile_datum &rtd, const embark_assist::defs::mid_level_tiles *mlt, const embark_assist::defs::key_buffer_holder_interface &buffer_holder) final override;
            virtual void optimize(bool debugOutput) final override;
            // FIXME: add find variant that allows for a search only in a specific world tile which is needed during survey iteration phase
            virtual void find(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results) const final override;
            virtual const uint32_t get_key(const int16_t x, const int16_t y) const final override;
        };
    }
}