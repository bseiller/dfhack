#pragma once

#include <unordered_map>
#include <future>

#include "defs.h"
#include "guarded_roaring.h"
#include "inorganics_information.h"
#include "key_position_mapper.h"
#include "roaring.hh"

namespace embark_assist {
    namespace index {

        using embark_assist::roaring::GuardedRoaring;

        struct position {
            int16_t x;
            int16_t y;
        };

        struct waterfall_drop_bucket {
            uint8_t counter = 1;
            // size == 2 because no more than 2 waterfalls can start in one tile and end in two different tiles (east and south tile)
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

        struct find_context {
            const uint16_t embark_size_x;
            const uint16_t embark_size_y;

            const uint16_t embark_size;

            const size_t size_factor_x = 16;
            const size_t size_factor_y = 16;

            //max_number_of_embark_variants = size_factor_x * size_factor_y;
            //// from a certain embark size (x,y = 9) the region size (16x16) is the limiting factor for how many different embark variants there can be
            //// the smaller number is the correct one in any case
            // size_t max_number_of_embark_variants = std::min(max_number_of_embark_variants, (size_t)embark_size);
            const size_t max_number_of_embark_variants;

            std::vector<Roaring> embarks;
            uint32_t embark_tile_key_buffer[256];
            uint32_t number_of_significant_ids = 0;
            uint32_t number_of_iterations = 0;
            uint32_t number_of_matches = 0;
            uint32_t number_of_matched_worldtiles = 0;

            int16_t previous_x = -1;
            int16_t previous_y = -1;

            embark_tile_tracker start_tile_tracker;

            std::chrono::duration<double> totalElapsed = std::chrono::seconds(0);
            std::chrono::duration<double> queryPlanCreatedElapsed = std::chrono::seconds(0);
            std::chrono::duration<double> queryElapsed = std::chrono::seconds(0);
            std::chrono::duration<double> embarkVariantsCreated = std::chrono::seconds(0);

            find_context(const embark_assist::defs::finders &finder) : embark_size_x(finder.x_dim), embark_size_y(finder.y_dim), 
                embark_size(embark_size_x * embark_size_y), size_factor_x(16 - embark_size_x + 1), size_factor_y(16 - embark_size_y + 1),
                max_number_of_embark_variants(std::min(size_factor_x * size_factor_y, (size_t)embark_size)),
                embarks(max_number_of_embark_variants) {
            }
        };

        class query_plan_interface {
            public:
                virtual ~query_plan_interface() {};
                virtual bool execute(const Roaring &embark_candidate) const = 0;

                // TODO: make this a smart iterator instead of a vector - which would save some memory...
                virtual const std::vector<uint32_t>& get_most_significant_ids() const = 0;
                virtual void set_most_significant_ids(const std::vector<uint32_t>* ids) = 0;
                virtual void get_most_significant_ids(const uint32_t world_offset, std::vector<uint32_t>& keys) const = 0;
                virtual void sort_queries() const = 0;
        };

        class surveyed_world_tiles_positions;

        class Index final : public embark_assist::defs::index_interface {
        private:
            static const uint32_t NUMBER_OF_EMBARK_TILES = 16 * 16;
            static const uint32_t NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL = NUMBER_OF_EMBARK_TILES * 256 - 1;
            bool was_optimized_in_current_feature_shell = false;
            df::world *world;
            embark_assist::defs::match_results &match_results;
            const embark_assist::defs::finders &finder;
            mutable const query_plan_interface *iterative_query_plan = nullptr;
            find_context *find_context = nullptr;
            mutable std::mutex find_single_mutex;
            std::ofstream find_single_world_tiles_file{ "./data/init/embark_assistant_indexes/find_processed_world_tiles.txt" };
            std::ofstream processed_single_world_tiles_file{ "./data/init/embark_assistant_indexes/processed_world_tiles.txt" };
            uint16_t max_inorganic = 0;
            uint32_t capacity;

            embark_assist::index::key_position_mapper::KeyPositionMapper *keyMapper = nullptr;

            mutable uint32_t entryCounter = 0;
            uint16_t feature_set_counter = 0;

            uint32_t maxKeyValue = 0;
            uint32_t previous_key = -1;

            mutable std::future<void> find_result;
            //std::unique_ptr<std::future<void>> test;
            surveyed_world_tiles_positions *completely_surveyed_positions = nullptr;

            // following 4 are for debugging - remove for release
            GuardedRoaring uniqueKeys;
            // TODO: remove, just here for debugging
            mutable embark_assist::defs::match_results iterative_match_results_comparison;
            mutable embark_assist::defs::match_results match_results_comparison;
            std::atomic_uint32_t processed_world_tiles_with_sync{ 0 };
            std::atomic_uint32_t processed_world_all{ 0 };

            GuardedRoaring hasAquifer;
            GuardedRoaring hasNoAquifer;
            GuardedRoaring hasClay;
            GuardedRoaring hasCoal;
            GuardedRoaring hasFlux;
            GuardedRoaring hasRiver;
            GuardedRoaring hasSand;
            GuardedRoaring is_unflat_by_incursion;
            GuardedRoaring has_blood_rain;
            std::vector<uint8_t> mapped_elevations;
            std::vector<GuardedRoaring> soil;
            std::vector<GuardedRoaring> freezing;
            std::vector<GuardedRoaring> syndrome_rain;
            std::vector<GuardedRoaring> reanimation_thralling;
            std::vector<GuardedRoaring> river_size;
            std::vector<GuardedRoaring> magma_level;
            std::vector<GuardedRoaring> adamantine_level;
            std::array<GuardedRoaring, 3> savagery_level;
            std::array<GuardedRoaring, 3> evilness_level;
            std::vector<GuardedRoaring> biome;
            std::array<GuardedRoaring, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> region_type;
            std::array<unordered_map<uint32_t, waterfall_drop_bucket*>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> waterfall_drops;
            std::array<uint64_t, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> number_of_waterfall_drops;
            GuardedRoaring no_waterfall;

            std::vector<GuardedRoaring*> metals;
            std::vector<GuardedRoaring*> economics;
            std::vector<GuardedRoaring*> minerals;

            // std::vector<Roaring*> inorganics;

            // following 2 are only for debugging - remove for release
            std::vector<uint32_t> keys_in_order;
            std::vector <position> positions;

            // helper for easier reset, debug and output of over all indices
            std::vector<GuardedRoaring*> static_indices;

            const embark_assist::inorganics::inorganics_information &inorganics_info;

            const std::list<uint16_t>& metal_indices;
            const std::list<uint16_t>& economic_indices;
            const std::list<uint16_t>& mineral_indices;

            const std::unordered_map<uint16_t, std::string>& metal_names;
            const std::unordered_map<uint16_t, std::string> &economic_names;
            const std::unordered_map<uint16_t, std::string> &mineral_names;

            mutable std::chrono::duration<double> innerElapsed_seconds = std::chrono::seconds(0);
            mutable std::chrono::duration<double> index_adding_seconds = std::chrono::seconds(0);
            mutable std::chrono::duration<double> inorganics_processing_seconds = std::chrono::seconds(0);
            mutable std::chrono::duration<double> vector_elevation_query_seconds = std::chrono::seconds(0);

            // Roaring& getInorganicsIndex(std::vector<Roaring*> &indexMap, uint16_t metal_index);
            void init_inorganic_indices();
            // void addInorganic(std::vector<uint16_t, Roaring*> &indexMap, uint16_t metal_index);
            std::string getInorganicName(const uint16_t index, const std::unordered_map<uint16_t, std::string> &ingorganicNames, std::string name) const;
            void writeIndexToDisk(const GuardedRoaring &roaring, const std::string prefix) const;
            void writeCoordsToDisk(const GuardedRoaring &roaring, const std::string prefix) const;
            void write_matches_to_file(const std::string prefix, const embark_assist::defs::match_results &matches, df::world *world) const;
            void output_embark_matches(std::ofstream &myfile, const uint16_t x, const uint16_t y, const embark_assist::defs::matches &match) const;
            void output_embark_tiles(std::ofstream &myfile, const uint16_t x, const uint16_t y, uint16_t start_i, uint16_t start_k) const;
            void compare_matches(const std::string prefix, df::world *world, const embark_assist::defs::match_results &match_results_matcher, const embark_assist::defs::match_results &match_results_index) const;
            uint16_t calculate_embark_variants(const uint32_t position_id, const uint16_t embark_size_x, const uint16_t embark_size_y, std::vector<Roaring> &embarks, uint32_t buffer[], embark_tile_tracker &tracker) const;
            void find_single_world_tile_matches(const int16_t x, const int16_t y) const;
            void iterate_most_significant_keys(embark_assist::index::find_context &find_context, embark_assist::defs::match_results &match_results_matcher, const query_plan_interface &query_plan, const std::vector<uint32_t> &keys) const;
            const embark_assist::index::query_plan_interface* embark_assist::index::Index::create_query_plan(const embark_assist::defs::finders &finder, const bool init_most_significant_ids) const;
            // const std::vector<uint32_t>* embark_assist::index::Index::get_keys(const GuardedRoaring &index) const;
            void optimize(Roaring *index, bool shrinkToSize);
            const void outputContents() const;
            const void outputSizes(const string &prefix);
        public:
            Index(df::world *world, embark_assist::defs::match_results &match_results, const embark_assist::defs::finders &finder);
            ~Index();
            void setup(uint16_t max_inorganic);
            void init_for_iterative_find();
            void shutdown();
            virtual const bool containsEntries() const final override;
            virtual void add(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, const embark_assist::key_buffer_holder::key_buffer_holder_interface &buffer_holder) final override;
            virtual void add(const embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer_holder) final override;
            virtual void optimize(bool debugOutput) final override;
            virtual void find_all_matches(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results) const final override;
            virtual void check_for_find_single_world_tile_matches(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, const string &prefix) final override;
            virtual void find_matches_in_surveyed_world_tiles() const final override;
            virtual const uint32_t get_key(const int16_t x, const int16_t y) const final override;
            virtual const uint32_t get_key(const int16_t x, const int16_t y, const uint16_t i, const uint16_t k) const final override;
        };

        // FIXME: either move this into a own header file or integrate the code directly into Index...
        class surveyed_world_tiles_positions {
        public:

            surveyed_world_tiles_positions(embark_assist::index::Index *index, uint32_t number_of_world_tiles): index(index), maxEntryCounter(number_of_world_tiles) {
            }

            void emplace(int16_t x, int16_t y) {
                const std::lock_guard<std::mutex> modify_mutex_guard(lock);
                positions.emplace_back(position{ x, y });

                // this is to handle the special case where/when world tile positions have only been fully processed after the last call of 
                // "index.find_matches_in_surveyed_world_tiles();" in the matcher::match_world_tile
                ++entryCounter;
                if (entryCounter == maxEntryCounter) {
                    index->find_matches_in_surveyed_world_tiles();
                }
            }

            void get_positions(std::vector<position> &buffer) {
                const std::lock_guard<std::mutex> modify_mutex_guard(lock);
                if (!positions.empty()) {
                    positions.swap(buffer);
                }
            }

        private:
            std::mutex lock;
            std::vector<position> positions;

            uint32_t entryCounter = 0;
            uint32_t maxEntryCounter = 0;

            embark_assist::index::Index *index;
        };
    }

}