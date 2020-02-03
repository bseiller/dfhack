#pragma once

#include <unordered_map>

#include "DataDefs.h"
//#include "df/biome_type.h"
//#include "df/world_region_type.h"

#include "defs.h"
#include "key_position_mapper.h"
#include "roaring.hh"

namespace embark_assist {
    namespace index {

        struct position {
            int16_t x;
            int16_t y;
        };

        struct embark_tile_tracker {
            uint16_t x = 65535;
            uint16_t y = 65535;
            bool was_already_used_as_start_position[16][16];
        };

        class query_plan_interface {
            public:
                virtual ~query_plan_interface() {};
                virtual bool execute(const Roaring &embark_candidate) const = 0;

                // TODO: make this a smart iterator instead of a vector - which would save same memory...
                virtual const std::vector<uint32_t>& get_most_significant_ids() const = 0;
                virtual void set_most_significant_ids(const std::vector<uint32_t>* ids) = 0;
        };

        class Index final : public embark_assist::defs::index_interface {
        private:
            static const uint32_t numberEmbarkTiles = 16 * 16;
            static const uint32_t NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL = numberEmbarkTiles * 256 - 1;
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
            std::array<Roaring, 5> soil;
            std::vector<Roaring> soil2 = std::vector<Roaring>(5, Roaring(roaring_bitmap_create_with_capacity(capacity)));
            std::array<Roaring, 6> river_size;
            std::array<Roaring, 4> magma_level;
            std::array<Roaring, 4> adamantine_level;
            std::array<Roaring, 3> savagery_level;
            std::array<Roaring, 3> evilness_level;
            //std::array<Roaring, ENUM_LAST_ITEM(biome_type) + 1> biomes;
            //std::array<Roaring, ENUM_LAST_ITEM(world_region_type) + 1> regions;

            std::vector<Roaring*> metals;
            std::vector<Roaring*> economics;
            std::vector<Roaring*> minerals;

            std::vector<Roaring*> inorganics;

            std::unordered_map<uint16_t, std::string> metalNames;
            std::unordered_map<uint16_t, std::string> economicNames;
            std::unordered_map<uint16_t, std::string> mineralNames;

            // following 3 are only for debugging - remove again
            std::vector<uint32_t> keys_in_order;
            std::vector<uint32_t> keys_in_order2;
            std::vector <position> positions;

            // helper for easier reset, debug and output of over all indices
            std::vector<Roaring*> static_indices;

            std::chrono::duration<double> innerElapsed_seconds = std::chrono::seconds(0);

            uint32_t aquiferBuffer[256];
            uint32_t clayBuffer[256];
            uint32_t coalBuffer[256];
            uint32_t riverBuffer[256];
            uint32_t sandBuffer[256];
            uint32_t fluxBuffer[256];

            std::vector<uint32_t*> metalBuffer;
            std::vector<uint16_t> metalBufferIndex;
            std::list<uint16_t> metalIndexes;

            std::vector<uint32_t*> economicBuffer;
            std::vector<uint16_t> economicBufferIndex;
            std::list<uint16_t> economicIndexes;

            std::vector<uint32_t*> mineralBuffer;
            std::vector<uint16_t> mineralBufferIndex;
            std::list<uint16_t> mineralIndexes;

            // Roaring& getInorganicsIndex(std::vector<Roaring*> &indexMap, uint16_t metal_index);
            void init_inorganic_index();
            // void addInorganic(std::vector<uint16_t, Roaring*> &indexMap, uint16_t metal_index);
            void initInorganicNames();
            std::string getInorganicName(const uint16_t index, const std::unordered_map<uint16_t, std::string> &ingorganicNames, std::string name) const;
            void embark_assist::index::Index::writeIndexToDisk(const Roaring &roaring, const std::string prefix) const;
            uint16_t calculate_embark_variants(const uint32_t position_id, const uint16_t embark_size_x, const uint16_t embark_size_y, std::vector<Roaring> &embarks, uint32_t buffer[], embark_tile_tracker &tracker) const;
            const embark_assist::index::query_plan_interface* embark_assist::index::Index::create_query_plan(const embark_assist::defs::finders &finder) const;
            const std::vector<uint32_t>* embark_assist::index::Index::get_keys(const Roaring &index) const;
            void optimize(Roaring *index, bool shrinkToSize);
        public:
            Index();
            Index(df::world *world);
            ~Index();
            void setup(df::world *world, uint16_t max_inorganic);
            void shutdown();
            virtual const bool containsEntries() const final override;
            virtual const uint32_t createKey(int16_t x, int16_t y, uint8_t i, uint8_t k) const final override;
            virtual void add(uint32_t key, const embark_assist::defs::mid_level_tile &data) final override;
            virtual void add(const int16_t x, const int16_t y, const embark_assist::defs::region_tile_datum &rtd, const embark_assist::defs::mid_level_tiles *mlt) final override;
            virtual void optimize(bool debugOutput) final override;
            virtual const void outputContents() const final override;
            virtual const void outputSizes(const string &prefix) final override;
            virtual void find(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results) const final override;
        };
    }
}