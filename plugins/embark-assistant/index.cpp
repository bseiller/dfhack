#include <stdio.h>
#include "Core.h"
#include <Console.h>
#include <future>

// #include "ColorText.h"
#include "DataDefs.h"
#include "df/inorganic_raw.h"
#include "df/world.h"
#include "df/world_data.h"
#include "df/world_region.h"
#include "df/world_region_details.h"
#include "df/world_region_feature.h"
#include "df/world_region_type.h"

#include "basic_key_buffer_holder.h"
#include "defs.h"
#include "index.h"
#include "inorganics_information.h"
#include "query.h"
// TODO/FIXME: remove for release, just used to get embark_assist::survey::clear_results(&match_results)
#include "survey.h"

using namespace DFHack;

namespace embark_assist {
    namespace index {
        class query_plan : public query_plan_interface {
        public:
            mutable std::vector<const embark_assist::query::query_interface*> queries;

            // TODO: make this a smart iterator instead of a vector - which would save some memory...
            const std::vector<uint32_t>& get_most_significant_ids() const final {
                return *most_significant_ids;
            }

            void get_most_significant_ids(const uint32_t world_offset, std::vector<uint32_t>& keys) const final {
                keys.clear();
                if (!queries.empty()) {
                    queries[0]->get_world_tile_keys(world_offset, keys);
                }
            }

            void set_most_significant_ids(const std::vector<uint32_t>* ids) final {
                most_significant_ids = ids;
            }

            bool execute(const Roaring &embark_candidates) const final {
                bool result = true;
                for (const auto query : queries) {
                    result = query->run(embark_candidates);
                    if (!result) {
                        break;
                    }
                }
                return result;
            };

            ~query_plan() {
                delete most_significant_ids;
                for (const auto query : queries) {
                    delete query;
                }
                queries.clear();
            }

            void sort_queries() const final {
                std::sort(queries.begin(), queries.end(), [](const embark_assist::query::query_interface* a, const embark_assist::query::query_interface* b) -> bool {
                    return a->get_number_of_entries() < b->get_number_of_entries();
                });
            }

        private:
            // FIXME: make this an iterator
            const std::vector<uint32_t>* most_significant_ids;
        };

        void set_capacity_and_add_to_static_indices(GuardedRoaring &index, const uint32_t capacity, std::vector<GuardedRoaring*> static_indices) {
            // sadly the copy constructor of Roaring does not copy the value for allocation_size, so we have to do this manually...
            ra_init_with_capacity(&index.roaring.high_low_container, capacity);
            static_indices.push_back(&index);
        }

        void create_and_add_present_query(const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_present_query(index);
            result->queries.push_back(q);
        }

        void create_and_add_absent_query(const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_absent_query(index);
            result->queries.push_back(q);
        }

        void create_and_add_all_query(const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_all_query(index);
            result->queries.push_back(q);
        }

        void create_and_add_all_query(const GuardedRoaring &index, const std::vector<const GuardedRoaring*> &exclusion_indices, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_multiple_exclusions_all_query(index, exclusion_indices);
            result->queries.push_back(q);
        }

        void create_and_add_partial_query(const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_partial_query(index);
            result->queries.push_back(q);
        }

        void create_and_add_not_all_query(const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_not_all_query(index);
            result->queries.push_back(q);
        }

        void create_savagery_evilness_queries(
            const embark_assist::defs::evil_savagery_values evil_savagery_values[4], const std::array<GuardedRoaring, 3> &index_array,
            const uint8_t savageryEvilnessLevel, embark_assist::index::query_plan *result) {
            const embark_assist::defs::evil_savagery_values finderValue = evil_savagery_values[savageryEvilnessLevel];
            if (finderValue != embark_assist::defs::evil_savagery_values::NA) {
                const GuardedRoaring &index = index_array[savageryEvilnessLevel];
                if (finderValue == embark_assist::defs::evil_savagery_values::Present) {
                    create_and_add_present_query(index, result);
                }
                else if (finderValue == embark_assist::defs::evil_savagery_values::Absent) {
                    create_and_add_absent_query(index, result);
                }
                else if (finderValue == embark_assist::defs::evil_savagery_values::All) {
                    std::vector<const GuardedRoaring*> exclusion_indices;
                    for (const GuardedRoaring &exclusion_index : index_array) {
                        // if the current exclusion_index is not the primary index we added it to the exclusion_indices
                        if (&exclusion_index != &index) {
                            exclusion_indices.push_back(&exclusion_index);
                        }
                    }
                    create_and_add_all_query(index, exclusion_indices, result);
                }
            }
        }

        void create_and_add_present_or_absent_query(const embark_assist::defs::present_absent_ranges &range, const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            if (range == embark_assist::defs::present_absent_ranges::Present) {
                create_and_add_present_query(index, result);
            }
            else if (range == embark_assist::defs::present_absent_ranges::Absent) {
                create_and_add_absent_query(index, result);
            }
        }

        void create_and_add_present_or_absent_query(const embark_assist::defs::yes_no_ranges &range, const GuardedRoaring &index, embark_assist::index::query_plan *result) {
            if (range == embark_assist::defs::yes_no_ranges::Yes) {
                create_and_add_present_query(index, result);
            }
            else if (range == embark_assist::defs::yes_no_ranges::No) {
                create_and_add_absent_query(index, result);
            }
        }

        void create_and_add_region_type_query(int8_t region_type_index, std::array<GuardedRoaring, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> region_types, embark_assist::index::query_plan *result) {
            if (region_type_index != -1) {
                create_and_add_present_query(region_types[region_type_index], result);
            }
        }

        void create_and_add_biome_type_query(int8_t biome_type_index, std::array<GuardedRoaring, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> biomes, embark_assist::index::query_plan *result) {
            if (biome_type_index != -1) {
                create_and_add_present_query(biomes[biome_type_index], result);
            }
        }

        void create_and_add_inorganics_query(int16_t inorganics_index, const std::vector<GuardedRoaring*> inorganics_indices, embark_assist::index::query_plan *result) {
            if (inorganics_index != -1) {
                create_and_add_present_query(*inorganics_indices[inorganics_index], result);
            }
        }
    }
}

embark_assist::index::Index::Index(df::world *world, embark_assist::defs::match_results &match_results, const embark_assist::defs::finders &finder)
    : match_results(match_results),
    finder(finder),
    capacity(std::ceil(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * NUMBER_OF_EMBARK_TILES / NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL)),
    hasAquifer(roaring_bitmap_create_with_capacity(capacity)),
    hasNoAquifer(roaring_bitmap_create_with_capacity(capacity)),
    hasClay(roaring_bitmap_create_with_capacity(capacity)),
    hasCoal(roaring_bitmap_create_with_capacity(capacity)),
    hasFlux(roaring_bitmap_create_with_capacity(capacity)),
    hasRiver(roaring_bitmap_create_with_capacity(capacity)),
    hasSand(roaring_bitmap_create_with_capacity(capacity)),
    has_blood_rain(roaring_bitmap_create_with_capacity(capacity)),
    is_unflat_by_incursion(roaring_bitmap_create_with_capacity(capacity)),
    no_waterfall(roaring_bitmap_create_with_capacity(capacity)),
    uniqueKeys(roaring_bitmap_create_with_capacity(capacity)),
    inorganics_info(embark_assist::inorganics::inorganics_information::get_instance()),
    metal_indices(inorganics_info.get_metal_indices()),
    economic_indices(inorganics_info.get_economic_indices()),
    mineral_indices(inorganics_info.get_mineral_indices()),
    metal_names(inorganics_info.get_metal_names()),
    economic_names(inorganics_info.get_economic_names()),
    mineral_names(inorganics_info.get_mineral_names()),
    soil(embark_assist::defs::SOIL_DEPTH_LEVELS),
    freezing(embark_assist::key_buffer_holder::FREEZING_ARRAY_LENGTH),
    river_size(embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES),
    magma_level(4),
    adamantine_level(4),
    biome(embark_assist::defs::ARRAY_SIZE_FOR_BIOMES) {

    this->world = world;

    maxKeyValue = world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * NUMBER_OF_EMBARK_TILES;
    embark_assist::query::abstract_query::set_world_size(maxKeyValue);

    keys_in_order.reserve(maxKeyValue);
    mapped_elevations.reserve(maxKeyValue);
    mapped_elevations.resize(maxKeyValue);

    positions.reserve(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y);

    keyMapper = new embark_assist::index::key_position_mapper::KeyPositionMapper(world->world_data->world_width, world->world_data->world_height);
    completely_surveyed_positions = new surveyed_world_tiles_positions(this, world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y);

    number_of_waterfall_drops.assign(0);

    static_indices.push_back(&uniqueKeys);
    static_indices.push_back(&hasCoal);
    static_indices.push_back(&hasFlux);
    static_indices.push_back(&hasRiver);
    static_indices.push_back(&no_waterfall);

    static_indices.push_back(&hasAquifer);
    static_indices.push_back(&hasNoAquifer);
    static_indices.push_back(&hasClay);
    static_indices.push_back(&hasSand);
    static_indices.push_back(&has_blood_rain);
    static_indices.push_back(&is_unflat_by_incursion);

    for (auto& index : soil) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : freezing) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : river_size) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : magma_level) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : adamantine_level) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : savagery_level) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : evilness_level) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : biome) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    for (auto& index : region_type) {
        set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    }

    // FIXME: remove, just here for debugging
    match_results_comparison.resize(world->worldgen.worldgen_parms.dim_x);
    iterative_match_results_comparison.resize(world->worldgen.worldgen_parms.dim_x);
    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        match_results_comparison[i].resize(world->worldgen.worldgen_parms.dim_y);
        iterative_match_results_comparison[i].resize(world->worldgen.worldgen_parms.dim_y);
    }
}

embark_assist::index::Index::~Index() {
    this->world = nullptr;
}

void embark_assist::index::Index::setup(const uint16_t max_inorganic) {
    this->max_inorganic = max_inorganic;

    init_inorganic_indices();
}

const uint32_t embark_assist::index::Index::get_key(const int16_t x, const int16_t y) const {
    return keyMapper->key_of(x, y, 0, 0);
}

const uint32_t embark_assist::index::Index::get_key(const int16_t x, const int16_t y, const uint16_t i, const uint16_t k) const {
    return keyMapper->key_of(x, y, i, k);
}

void embark_assist::index::Index::add(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, const embark_assist::key_buffer_holder::key_buffer_holder_interface &buffer_holder) {
    color_ostream_proxy out(Core::getInstance().getConsole());
    const auto innerStartTime = std::chrono::steady_clock::now();

    const uint32_t world_offset = keyMapper->key_of(x, y, 0, 0);
    if (previous_key == world_offset) {
        return;
    }
    previous_key = world_offset;

    //positions.push_back({ x,y });

    //const auto adding_start = std::chrono::steady_clock::now();

    uint16_t coalBufferIndex(0);
    const uint32_t *coalBuffer;
    buffer_holder.get_coal_buffer(coalBufferIndex, coalBuffer);
    if (coalBufferIndex > 0) {
        hasCoal.addManyGuarded(coalBufferIndex, coalBuffer);
    }

    uint16_t fluxBufferIndex(0);
    const uint32_t *fluxBuffer;
    buffer_holder.get_flux_buffer(fluxBufferIndex, fluxBuffer);
    if (fluxBufferIndex > 0) {
        hasFlux.addManyGuarded(fluxBufferIndex, fluxBuffer);
    }

    const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> * river_indices;
    const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> * river_buffers;
    buffer_holder.get_river_size_buffers(river_indices, river_buffers);
    for (int i = 0; i < river_indices->size(); i++) {
        if (river_indices->at(i) > 0) {
            river_size[i].addManyGuarded(river_indices->at(i), river_buffers->at(i));
        }
    }

    const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> * waterfall_indices;
    const std::array<std::array<std::pair<uint32_t, uint32_t>, 480>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS > * waterfall_buffers;
    buffer_holder.get_waterfall_drop_buffers(waterfall_indices, waterfall_buffers);
    for (int depth = 1; depth < waterfall_indices->size(); depth++) {
        const uint16_t buffer_index_position = waterfall_indices->at(depth);
        if (buffer_index_position > 0) {
            for (int index = 0; index < buffer_index_position; index++) {
                number_of_waterfall_drops[depth]++;
                const std::pair<uint32_t, uint32_t> &keys = waterfall_buffers->at(depth)[index];
                //waterfall_drops[depth].insert(waterfall_buffers->at(depth)[index].first, waterfall_buffers->at(depth)[index].second);
                //waterfall_drops[depth].insert(waterfall_buffers->at(depth)[index]);

                // keys.first contains the tile id of the starting tile of the waterfall (north or west from the ending tile)
                // keys.second contains the tile id of the ending tile of the waterfall (east or south from the starting tile)
                if (waterfall_drops[depth].count(keys.first) == 0) {
                    //waterfall_drops[depth][keys.first] = { 1, {keys.second, 0, 0, 0} };
                    // creating new bucket with first waterfall tile "target" key
                    waterfall_drops[depth][keys.first] = new waterfall_drop_bucket(keys.second);
                }
                else {
                    // adding second waterfall tile "target" key to existing bucket
                    waterfall_drops[depth][keys.first]->add(keys.second);
                }
            }
        }
    }

    uint16_t no_waterfall_index(0);
    const uint32_t *no_waterfall_buffer;
    buffer_holder.get_no_waterfall_buffers(no_waterfall_index, no_waterfall_buffer);
    if (no_waterfall_index > 0) {
        no_waterfall.addManyGuarded(no_waterfall_index, no_waterfall_buffer);
    }

    const std::array<uint16_t, 4> * magma_indices;
    const std::array<uint32_t *, 4> * magma_buffers;
    buffer_holder.get_magma_level_buffers(magma_indices, magma_buffers);
    for (int i = 0; i < magma_indices->size(); i++) {
        if (magma_indices->at(i) > 0) {
            magma_level[i].addManyGuarded(magma_indices->at(i), magma_buffers->at(i));
        }
    }

    const std::array<uint16_t, 4> * adamantine_indices;
    const std::array<uint32_t *, 4> * adamantine_buffers;
    buffer_holder.get_adamantine_level_buffers(adamantine_indices, adamantine_buffers);
    for (int i = 0; i < adamantine_indices->size(); i++) {
        if (adamantine_indices->at(i) > 0) {
            adamantine_level[i].addManyGuarded(adamantine_indices->at(i), adamantine_buffers->at(i));
        }
    }

    const std::vector<uint16_t> * metal_buffer_indices;
    const std::vector<uint32_t *> * metal_buffers;
    buffer_holder.get_metal_buffers(metal_buffer_indices, metal_buffers);
    for (auto metalIndexOffset : this->metal_indices) {
        if (metal_buffer_indices->at(metalIndexOffset) > 0) {
            metals[metalIndexOffset]->addManyGuarded(metal_buffer_indices->at(metalIndexOffset), metal_buffers->at(metalIndexOffset));
        }
    }

    const std::vector<uint16_t> * economic_buffer_indices;
    const std::vector<uint32_t *> * economic_buffers;
    buffer_holder.get_economic_buffers(economic_buffer_indices, economic_buffers);
    for (auto economicIndexOffset : economic_indices) {
        if (economic_buffer_indices->at(economicIndexOffset) > 0) {
            economics[economicIndexOffset]->addManyGuarded(economic_buffer_indices->at(economicIndexOffset), economic_buffers->at(economicIndexOffset));
        }
    }

    const std::vector<uint16_t> * mineral_buffer_indices;
    const std::vector<uint32_t *> * mineral_buffers;
    buffer_holder.get_mineral_buffers(mineral_buffer_indices, mineral_buffers);
    for (auto mineralIndexOffset : mineral_indices) {
        if (mineral_buffer_indices->at(mineralIndexOffset) > 0) {
            minerals[mineralIndexOffset]->addManyGuarded(mineral_buffer_indices->at(mineralIndexOffset), mineral_buffers->at(mineralIndexOffset));
        }
    }

    uint16_t elevationIndex(0);
    const uint8_t *elevation_buffer;
    uint32_t initial_offset;
    buffer_holder.get_mapped_elevation_buffer(elevationIndex, elevation_buffer, initial_offset);
    if (elevationIndex > 0) {
        std::copy(elevation_buffer, elevation_buffer + elevationIndex, &mapped_elevations[initial_offset]);
        //memcpy(&mapped_elevations.at(initial_offset), elevation_buffer, elevationIndex);
    }

    // add all buffers that contain data that is also handled by the incursions to the indices
    this->add(buffer_holder);
    this->check_for_find_single_world_tile_matches(x, y, rtd, "normal add");

    //const auto adding_end = std::chrono::steady_clock::now();
    //index_adding_seconds += adding_end - adding_start;

    // now we may optimize, since all data is collected for this world tile - but do so only after every feature shell (= 16*16 world tiles)
    const uint32_t last_key = world_offset + NUMBER_OF_EMBARK_TILES - 1;
    if ((last_key - feature_set_counter) % (NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL * (feature_set_counter + 1)) == 0) {
        if (!was_optimized_in_current_feature_shell) {
            this->optimize(false);
            feature_set_counter++;
            // out.print("optimizing after key %d\n ", last_key);
            was_optimized_in_current_feature_shell = true;
        }
    }
    else if (was_optimized_in_current_feature_shell) {
        was_optimized_in_current_feature_shell = false;
    }

    //this->check_for_find_single_world_tile_matches(x, y, rtd, "normal add");

    const auto innerEnd = std::chrono::steady_clock::now();
    innerElapsed_seconds += innerEnd - innerStartTime;
}

void embark_assist::index::Index::add(const embark_assist::key_buffer_holder::key_buffer_holder_basic_interface &buffer_holder) {
    // FIXME: time/profile this method, especially the parts that loop over arrays

    uint16_t unflatBufferIndex(0);
    const uint32_t *unflatBuffer;
    buffer_holder.get_unflat_buffer(unflatBufferIndex, unflatBuffer);
    if (unflatBufferIndex > 0) {
        is_unflat_by_incursion.addManyGuarded(unflatBufferIndex, unflatBuffer);
    }

    uint16_t aquiferBufferIndex(0);
    const uint32_t *aquiferBuffer;
    buffer_holder.get_aquifer_buffer(aquiferBufferIndex, aquiferBuffer, true);
    if (aquiferBufferIndex > 0) {
        hasAquifer.addManyGuarded(aquiferBufferIndex, aquiferBuffer);
    }

    uint16_t noAquiferBufferIndex(0);
    const uint32_t *noAquiferBuffer;
    buffer_holder.get_aquifer_buffer(noAquiferBufferIndex, noAquiferBuffer, false);
    if (noAquiferBufferIndex > 0) {
        hasNoAquifer.addManyGuarded(noAquiferBufferIndex, noAquiferBuffer);
    }

    uint16_t clayBufferIndex(0);
    const uint32_t *clayBuffer;
    buffer_holder.get_clay_buffer(clayBufferIndex, clayBuffer);
    if (clayBufferIndex > 0) {
        hasClay.addManyGuarded(clayBufferIndex, clayBuffer);
    }

    uint16_t sandBufferIndex(0);
    const uint32_t *sandBuffer;
    buffer_holder.get_sand_buffer(sandBufferIndex, sandBuffer);
    if (sandBufferIndex > 0) {
        hasSand.addManyGuarded(sandBufferIndex, sandBuffer);
    }

    const std::array<uint16_t, embark_assist::defs::SOIL_DEPTH_LEVELS> *indices;
    const std::array<uint32_t *, embark_assist::defs::SOIL_DEPTH_LEVELS> *buffers;
    buffer_holder.get_soil_depth_buffers(indices, buffers);
    for (int i = 0; i < embark_assist::defs::SOIL_DEPTH_LEVELS; i++) {
        if (indices->at(i) > 0) {
            soil[i].addManyGuarded(indices->at(i), buffers->at(i));
        }
    }

    const std::array<uint16_t, embark_assist::key_buffer_holder::FREEZING_ARRAY_LENGTH> * freezing_indices;
    const std::array<uint32_t *, embark_assist::key_buffer_holder::FREEZING_ARRAY_LENGTH> * freezing_buffers;
    buffer_holder.get_temperatur_freezing(freezing_indices, freezing_buffers);
    for (int i = 0; i < embark_assist::key_buffer_holder::FREEZING_ARRAY_LENGTH; i++) {
        if (freezing_indices->at(i) > 0) {
            freezing[i].addManyGuarded(freezing_indices->at(i), freezing_buffers->at(i));
        }
    }

    uint16_t blood_rain_buffer_index(0);
    const uint32_t *blood_rain_buffer;
    buffer_holder.get_blood_rain_buffer(blood_rain_buffer_index, blood_rain_buffer);
    if (blood_rain_buffer_index > 0) {
        has_blood_rain.addManyGuarded(blood_rain_buffer_index, blood_rain_buffer);
    }

    const std::array<uint16_t, 3> *savagery_indices;
    const std::array<uint32_t *, 3> *savagery_buffers;
    buffer_holder.get_savagery_level_buffers(savagery_indices, savagery_buffers);
    for (int i = 0; i < savagery_level.size(); i++) {
        if (savagery_indices->at(i) > 0) {
            savagery_level[i].addManyGuarded(savagery_indices->at(i), savagery_buffers->at(i));
        }
    }

    const std::array<uint16_t, 3> *evilness_indices;
    const std::array<uint32_t *, 3> *evilness_buffers;
    buffer_holder.get_evilness_level_buffers(evilness_indices, evilness_buffers);
    for (int i = 0; i < evilness_level.size(); i++) {
        if (evilness_indices->at(i) > 0) {
            evilness_level[i].addManyGuarded(evilness_indices->at(i), evilness_buffers->at(i));
        }
    }

    const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> *biome_indices;
    const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> *biome_buffers;
    buffer_holder.get_biome_buffers(biome_indices, biome_buffers);
    for (int i = 0; i < biome.size(); i++) {
        if (biome_indices->at(i) > 0) {
            biome[i].addManyGuarded(biome_indices->at(i), biome_buffers->at(i));
        }
    }

    const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> *region_type_indices;
    const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> *region_type_buffers;
    buffer_holder.get_region_type_buffers(region_type_indices, region_type_buffers);
    for (int i = 0; i < region_type.size(); i++) {
        if (region_type_indices->at(i) > 0) {
            region_type[i].addManyGuarded(region_type_indices->at(i), region_type_buffers->at(i));
        }
    }
}

const bool embark_assist::index::Index::containsEntries() const {
    // FIXME: this does not work anymore...
    //return entryCounter >= maxKeyValue && uniqueKeys.contains(maxKeyValue);
    return entryCounter >= world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y;
}

inline void embark_assist::index::Index::optimize(Roaring *index, bool shrinkToSize) {
    index->runOptimize();
    if (/*true ||*/ shrinkToSize) {
        index->shrinkToFit();
    }
}

void embark_assist::index::Index::optimize(bool debugOutput) {
    if (debugOutput) {
        this->outputSizes("before optimize");
    }

    for (auto it = static_indices.begin(); it != static_indices.end(); it++) {
        //optimize(*it, debugOutput);
        (*it)->runOptimizeAndShrinkToFitGuarded(debugOutput);
    }

    for (auto it = metals.begin(); it != metals.end(); it++) {
        if (*it != nullptr) {
            //optimize(*it, debugOutput);
            (*it)->runOptimizeAndShrinkToFitGuarded(debugOutput);
        }
    }

    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            //optimize(*it, debugOutput);
            (*it)->runOptimizeAndShrinkToFitGuarded(debugOutput);
        }
    }

    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            //optimize(*it, debugOutput);
            (*it)->runOptimizeAndShrinkToFitGuarded(debugOutput);
        }
    }

    if (debugOutput) {
        this->outputSizes("after optimize");
        this->outputContents();

        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("### embark_assist::index::Index::add inorganics processing took %f seconds ###\n", inorganics_processing_seconds.count());
        out.print("### embark_assist::index::Index::add only index adding took %f seconds ###\n", index_adding_seconds.count());
        out.print("### embark_assist::index::Index::add buffer and index adding took total %f seconds ###\n", innerElapsed_seconds.count());
        out.print("### embark_assist::index::Index::optimize sync processed %u world tiles ###\n", processed_world_tiles_with_sync.load());
        out.print("### embark_assist::index::Index::optimize all processed %u world tiles ###\n", processed_world_all.load());
        // std::cout << "### actual adding took total " << innerElapsed_seconds.count() << " seconds ###" << std::endl;
        innerElapsed_seconds = std::chrono::seconds(0);
        index_adding_seconds = std::chrono::seconds(0);
    }
}

void embark_assist::index::Index::shutdown() {
    embark_assist::inorganics::inorganics_information::reset();
    world = nullptr;
    if (keyMapper != nullptr) {
        delete keyMapper;
    }
    if (iterative_query_plan != nullptr) {
        delete iterative_query_plan;
    }
    if (completely_surveyed_positions != nullptr) {
        delete completely_surveyed_positions;
    }

    feature_set_counter = 0;
    entryCounter = 0;
    max_inorganic = 0;
    maxKeyValue = 0;

    for (auto it = static_indices.begin(); it != static_indices.end(); it++) {
        if (*it != nullptr) {
            //roaring_bitmap_clear(&(*it)->roaring);
            (*it)->clear();
            // BEWARE: the indices in static_indices are not created with new, so we really mustn't delete them!
            //delete *it;
        }
    }

    for (auto it = metals.begin(); it != metals.end(); it++) {
        if (*it != nullptr) {
            //roaring_bitmap_clear(&(*it)->roaring);
            (*it)->clear();
            delete *it;
        }
    }
    metals.clear();
    metals.resize(0);
    metals.reserve(0);

    for (auto it = economics.begin(); it != economics.end(); it++) {
        if (*it != nullptr) {
            //roaring_bitmap_clear(&(*it)->roaring);
            (*it)->clear();
            delete *it;
        }
    }
    economics.clear();
    economics.resize(0);
    economics.reserve(0);

    for (auto it = minerals.begin(); it != minerals.end(); it++) {
        if (*it != nullptr) {
            //roaring_bitmap_clear(&(*it)->roaring);
            (*it)->clear();
            delete *it;
        }
    }
    minerals.clear();
    minerals.resize(0);
    minerals.reserve(0);

    //inorganics.clear();
    //inorganics.resize(0);
    //inorganics.reserve(0);

    for (auto it = waterfall_drops.begin(); it != waterfall_drops.end(); it++) {
        for (auto it2 = (*it).begin(); it2 != (*it).end(); it2++) {
            delete (*it2).second;
        }
        (*it).clear();
        (*it).reserve(0);
    }

    keys_in_order.clear();
    positions.clear();
}

const void embark_assist::index::Index::outputContents() const {
    //auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out | std::ios::app);
    FILE* outfile = fopen(index_folder_log_file_name, "a");
    uint32_t index_prefix = 0;

    fprintf(outfile, "number of times add was called: %d\n", entryCounter);
    fprintf(outfile, "number of unique entries: %I64d\n", uniqueKeys.cardinality());
    fprintf(outfile, "number of hasAquifer entries: %I64d\n", hasAquifer.cardinality());
    this->writeCoordsToDisk(hasAquifer, std::to_string(index_prefix) + "_hasAquifier");
    this->writeIndexToDisk(hasAquifer, std::to_string(index_prefix) + "_hasAquifier");
    fprintf(outfile, "number of hasNoAquifer entries: %I64d\n", hasNoAquifer.cardinality());
    this->writeCoordsToDisk(hasNoAquifer, std::to_string(index_prefix)  + "_1_hasNoAquifer");
    this->writeIndexToDisk(hasNoAquifer, std::to_string(index_prefix++) + "_1_hasNoAquifer");
    fprintf(outfile, "number of hasRiver entries: %I64d\n", hasRiver.cardinality());
    this->writeIndexToDisk(hasRiver, std::to_string(index_prefix++) + "_hasRiver");
    fprintf(outfile, "number of hasClay entries: %I64d\n", hasClay.cardinality());
    this->writeIndexToDisk(hasClay, std::to_string(index_prefix++) + "_hasClay");
    fprintf(outfile, "Number of hasCoal entries: %I64d\n", hasCoal.cardinality());
    this->writeIndexToDisk(hasCoal, std::to_string(index_prefix++) + "_hasCoal");
    fprintf(outfile, "number of hasFlux entries: %I64d\n", hasFlux.cardinality());
    this->writeIndexToDisk(hasFlux, std::to_string(index_prefix++) + "_hasFlux");
    fprintf(outfile, "number of hasSand entries: %I64d\n", hasSand.cardinality());
    this->writeIndexToDisk(hasSand, std::to_string(index_prefix++) + "_hasSand");

    uint32_t level_post_fix = 0;
    for (auto& index : soil) {
        fprintf(outfile, "number of soil_level#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_soil_level_" + std::to_string(level_post_fix++));
    }

    level_post_fix = 0;
    for (auto& index : river_size) {
        fprintf(outfile, "number of river_size#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeCoordsToDisk(index, std::to_string(index_prefix) + "_river_size_" + std::to_string(level_post_fix));
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_river_size_" + std::to_string(level_post_fix++));
    }

    level_post_fix = 0;
    for (auto& index : magma_level) {
        fprintf(outfile, "number of magma_level#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_magma_level_" + std::to_string(level_post_fix++));
    }

    level_post_fix = 0;
    for (auto& index : adamantine_level) {
        fprintf(outfile, "number of adamantine_level#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_adamantine_level_" + std::to_string(level_post_fix++));
    }

    level_post_fix = 0;
    for (auto& index : savagery_level) {
        fprintf(outfile, "number of savagery_level#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeCoordsToDisk(index, std::to_string(index_prefix) + "_savagery_level_" + std::to_string(level_post_fix));
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_savagery_level_" + std::to_string(level_post_fix++));
    }

    level_post_fix = 0;
    for (auto& index : evilness_level) {
        fprintf(outfile, "number of evilness_level#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_evilness_level_" + std::to_string(level_post_fix++));
    }

    fprintf(outfile, "number of metal entries:\n");
    for (auto it = metals.cbegin(); it != metals.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN METAL ###";
            const uint16_t index = std::distance(metals.cbegin(), it);
            name = getInorganicName(index, metal_names, name);
            fprintf(outfile, "#%d - %s: %I64d\n", index, name.c_str(), (*it)->cardinality());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fprintf(outfile, "\nnumber of economics entries:\n");
    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN ECONOMIC ###";
            const uint16_t index = std::distance(economics.cbegin(), it);
            name = getInorganicName(index, economic_names, name);
            fprintf(outfile, "#%d - %s: %I64d\n", index, name.c_str(), (*it)->cardinality());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fprintf(outfile, "\nnumber of minerals entries:\n");
    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN MINERAL ###";
            const uint16_t index = std::distance(minerals.cbegin(), it);
            name = getInorganicName(index, mineral_names, name);
            fprintf(outfile, "#%d - %s: %I64d\n", index, name.c_str(), (*it)->cardinality());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    // TODO: move the following blocks (before fclose) before the inorganics
    uint64_t total_number_of_entries = 0;
    level_post_fix = 0;
    for (auto& index : biome) {
        total_number_of_entries += index.cardinality();
        fprintf(outfile, "number of biome #%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_biome_" + std::to_string(level_post_fix++));
    }
    fprintf(outfile, "total number of biome entries: %I64d\n", total_number_of_entries);

    level_post_fix = 0;
    total_number_of_entries = 0;
    for (auto& index : region_type) {
        total_number_of_entries += index.cardinality();
        fprintf(outfile, "number of region_type #%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_region_type_" + std::to_string(level_post_fix++));
    }
    fprintf(outfile, "total number of region_type entries: %I64d\n", total_number_of_entries);

    level_post_fix = 0;
    total_number_of_entries = 0;
    for (auto& hash_map : waterfall_drops) {
        if (0 == level_post_fix) {
            level_post_fix++;
            continue;
        }
        total_number_of_entries += number_of_waterfall_drops[level_post_fix];
        fprintf(outfile, "number of waterfall_drops #%d entries: %I64d\n", level_post_fix, number_of_waterfall_drops[level_post_fix]);
        std::ofstream outputFile(index_folder_name + std::to_string(index_prefix++) + "_waterfall_drops_" + std::to_string(level_post_fix++) + ".txt");
        outputFile << "start_id" << '\t' << "first_target_id" << '\t' << "second_target_id" << '\n';
        std::vector<std::pair<uint32_t, waterfall_drop_bucket*>> elems(hash_map.begin(), hash_map.end());
        std::sort(elems.begin(), elems.end());
        for (auto& iterator : elems) {
            outputFile << iterator.first << '\t' << iterator.second->keys[0] << '\t';
            if (iterator.second->counter == 2) {
                outputFile << iterator.second->keys[1];
            }
            outputFile << '\n';
        }
    }
    fprintf(outfile, "total number of waterfall_drops entries: %I64d\n", total_number_of_entries);

    fprintf(outfile, "number of no_waterfall entries: %I64d\n", no_waterfall.cardinality());
    this->writeIndexToDisk(no_waterfall, std::to_string(index_prefix++) + "_no_waterfall");

    fprintf(outfile, "number of is_unflat_by_incursion entries: %I64d\n", is_unflat_by_incursion.cardinality());
    this->writeIndexToDisk(is_unflat_by_incursion, std::to_string(index_prefix++) + "_is_unflat_by_incursion");

    fprintf(outfile, "total number of mapped_elevations entries: %I64d\n", mapped_elevations.size());
    // FIXME: write mapped_elevations vector to disk

    fprintf(outfile, "number of has_blood_rain entries: %I64d\n", has_blood_rain.cardinality());
    this->writeCoordsToDisk(has_blood_rain, std::to_string(index_prefix) + "_has_blood_rain");
    this->writeIndexToDisk(has_blood_rain, std::to_string(index_prefix++) + "_has_blood_rain");

    level_post_fix = 0;
    for (auto& index : freezing) {
        fprintf(outfile, "number of freezing#%d entries: %I64d\n", level_post_fix, index.cardinality());
        this->writeCoordsToDisk(index, std::to_string(index_prefix) + "_freezing_" + std::to_string(level_post_fix));
        this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_freezing_" + std::to_string(level_post_fix++));
    }

    fclose(outfile);

    const std::string prefix = "keys_in_order";
    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);

    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t i = 0;
    uint16_t k = 0;

    uint32_t key_index = 0;
    for (const auto key : keys_in_order) {
        // get_position(key, x, y, i, k);
        keyMapper->get_position(key, x, y, i, k);
        myfile << key << "/" << keys_in_order[key_index] << " - x:" << x << ", y:" << y << ", i:" << i << ", k:" << k << "\n";
        x = 0;
        y = 0;
        i = 0;
        k = 0;
        key_index++;
    }
    myfile.close();

    const std::string prefix2 = "positions_in_order";
    uint32_t previous_key = 0;
    auto positionsFile = std::ofstream(index_folder_name + prefix2, std::ios::out);
    positionsFile << "x;y;i;k;key;\n";
    for (const auto position : positions) {
        const uint32_t key = keyMapper->key_of(position.x, position.y, 0, 0);
        if (previous_key != 0 && previous_key != key - 1) {
            positionsFile << "### discontinuity ###" << "\n";
        }
        const uint32_t key2 = keyMapper->key_of(position.x, position.y, 15, 15);
        previous_key = key2;
        positionsFile << position.x << ";" << position.y << ";" << 0 << ";" << 0 << ";" << key << "\n";
        positionsFile << position.x << ";" << position.y << ";" << 15 << ";" << 15 << ";" << key2 << "\n";
    }
    positionsFile.close();
}

uint16_t embark_assist::index::Index::calculate_embark_variants(const uint32_t position_id, const uint16_t embark_size_x, const uint16_t embark_size_y, std::vector<Roaring> &embarks,
    uint32_t embark_tile_key_buffer[], embark_tile_tracker &start_tile_tracker) const {
    color_ostream_proxy out(Core::getInstance().getConsole());

    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t i = 0;
    uint16_t k = 0;
    keyMapper->get_position(position_id, x, y, i, k);

    // if the previously inspected world tile is not the current one, we reset the tracker
    if (start_tile_tracker.x != x || start_tile_tracker.y != y) {
        start_tile_tracker.x = x;
        start_tile_tracker.y = y;
        for (uint8_t j = 0; j < 16; j++) {
            //std::fill(&(start_tile_tracker.was_already_used_as_start_position[i][0]), &(start_tile_tracker.was_already_used_as_start_position[i][15]), false);
            for (uint8_t l = 0; l < 16; l++) {
                start_tile_tracker.was_already_used_as_start_position[j][l] = false;
            }
        }
    }

    const int16_t MIN_COORD = 0;
    const int16_t MAX_COORD = 15;

    const int16_t negative_offsetted_i = i - (embark_size_x - 1);
    const int16_t start_i_offset = std::max(MIN_COORD, negative_offsetted_i);
    const int16_t negative_offsetted_k = k - (embark_size_y - 1);
    const int16_t start_k_offset = std::max(MIN_COORD, negative_offsetted_k);

    const int16_t positive_offsetted_i = i + (embark_size_x - 1);
    const int16_t end_i_offset = std::min(MAX_COORD, positive_offsetted_i);
    const int16_t positive_offsetted_k = k + (embark_size_y - 1);
    const int16_t end_k_offset = std::min(MAX_COORD, positive_offsetted_k);

    // uint32_t position_id_region_offset = createKey(x, y, 0, 0);
    uint32_t position_id_region_offset = keyMapper->key_of(x, y, 0, 0);
    // uint32_t start_position_id = position_id - ((i - start_i_offset) + (k - start_k_offset) * 16);

    uint16_t embark_counter = 0;
    uint16_t buffer_position = 0;
    for (uint16_t current_k_offset = start_k_offset; current_k_offset + embark_size_y - 1 <= end_k_offset; current_k_offset++) {
        for (uint16_t current_i_offset = start_i_offset; current_i_offset + embark_size_x - 1 <= end_i_offset; current_i_offset++) {
            // if the current tile was already used as start tile for an embark candidate/variant we skip this embark candidate as it is a duplicate
            if (start_tile_tracker.was_already_used_as_start_position[current_i_offset][current_k_offset]) {
                continue;
            }
            // start_position_id = position_id - ((i - current_i_offset) + (k - current_k_offset) * 16);
            buffer_position = 0;
            for (uint16_t current_k = current_k_offset; current_k < current_k_offset + embark_size_y; current_k++) {
                const uint32_t current_position_id_region_and_k_offset = position_id_region_offset + current_k * 16;

                // following line could/should replace the inner loop completly => FIXME verify/test this and its performance
                //std::iota(&embark_tile_key_buffer[buffer_position], &embark_tile_key_buffer[buffer_position + embark_size_x], current_position_id_region_and_k_offset + current_i_offset);
                //buffer_position += embark_size_x;

                for (uint16_t current_i = current_i_offset; current_i < current_i_offset + embark_size_x; current_i++) {
                    //const uint32_t new_position_id = current_position_id_region_and_k_offset + current_i;
                    // out.print("embark_assist::index::Index::calculate_embark_variants: key:%d / position x:%d y:%d i:%d k:%d\n", new_position_id, x, y, current_i, current_k);
                    //embark_tile_key_buffer[buffer_position++] = new_position_id;
                    embark_tile_key_buffer[buffer_position++] = current_position_id_region_and_k_offset + current_i;
                }
            }
            start_tile_tracker.was_already_used_as_start_position[current_i_offset][current_k_offset] = true;
            // removing any keys that are in the "buffer"/preallocated Roaring index currently from previous iterations
            ra_reset(&(embarks[embark_counter].roaring.high_low_container));
            //roaring_bitmap_clear(&(embarks[embark_counter].roaring));
            embarks[embark_counter++].addMany(buffer_position, embark_tile_key_buffer);
        }
    }

    return embark_counter;
}

/**
TODO: optimize this memory-wise and return a (smart) iterator - which should be interesting especially in the sister method where more than one index is being processed....
**/
//const std::vector<uint32_t>* embark_assist::index::Index::get_keys(const GuardedRoaring &index) const {
//    const uint64_t cardinality = index.cardinalityGuarded();
//    uint32_t* most_significant_ids = new uint32_t[cardinality];
//    index.toUint32ArrayGuarded(most_significant_ids);
//    const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + cardinality);
//
//    delete most_significant_ids;
//    return dest;
//}

void calculate_elapsed(const chrono::time_point<std::chrono::steady_clock> start, std::chrono::duration<double> &total) {
    total += std::chrono::steady_clock::now() - start;
}

// FIXME: move this into own class => query_plan_factory
const embark_assist::index::query_plan_interface* embark_assist::index::Index::create_query_plan(const embark_assist::defs::finders &finder, const bool init_most_significant_ids) const {
    const Index &scope = *this;

    embark_assist::index::query_plan *result = new embark_assist::index::query_plan();

    // savagery queries
    const uint8_t lowSavageryEvilness = static_cast<uint8_t>(embark_assist::defs::evil_savagery_ranges::Low);
    create_savagery_evilness_queries(finder.savagery, savagery_level, lowSavageryEvilness, result);

    const uint8_t mediumEvilness = static_cast<uint8_t>(embark_assist::defs::evil_savagery_ranges::Medium);
    create_savagery_evilness_queries(finder.savagery, savagery_level, mediumEvilness, result);

    const uint8_t highEvilness = static_cast<uint8_t>(embark_assist::defs::evil_savagery_ranges::High);
    create_savagery_evilness_queries(finder.savagery, savagery_level, highEvilness, result);

    // evilness queries
    create_savagery_evilness_queries(finder.evilness, evilness_level, lowSavageryEvilness, result);
    create_savagery_evilness_queries(finder.evilness, evilness_level, mediumEvilness, result);
    create_savagery_evilness_queries(finder.evilness, evilness_level, highEvilness, result);

    // FIXME: be aware, that there are special (corner) cases if a criteria makes assumptions about more than one tile e.g. "All", "Absent", "Partial"
    // - if this query is the most significant one we need another query as helper to verify that all the other tiles also (not) have a aquifer...
    // This is true for all criteria with exclusive/absolute (all/none, absent, ...) meaning
    // actually it is fine to keep the query in the case of "all", "absent" and "partial" by
    // q->flag_for_keeping();
    // which will allow the query plan to use the query again which will make sure all embark candiates have or haven't an aquifer...
    if (finder.aquifer == embark_assist::defs::aquifer_ranges::All) {
        std::vector<const GuardedRoaring*> exclusion_indices = { &hasNoAquifer };
        create_and_add_all_query(hasAquifer, exclusion_indices, result);
    } else if (finder.aquifer == embark_assist::defs::aquifer_ranges::Present) {
        create_and_add_present_query(hasAquifer, result);
    } else if (finder.aquifer == embark_assist::defs::aquifer_ranges::Partial) {
        create_and_add_partial_query(hasAquifer, result);
    } else if (finder.aquifer == embark_assist::defs::aquifer_ranges::Not_All) {
        create_and_add_not_all_query(hasAquifer, result);
    } else if (finder.aquifer == embark_assist::defs::aquifer_ranges::Absent) {
        std::vector<const GuardedRoaring*> exclusion_indices = { &hasAquifer };
        create_and_add_all_query(hasNoAquifer, exclusion_indices, result);
    }

    if (finder.min_river != embark_assist::defs::river_ranges::NA || finder.max_river != embark_assist::defs::river_ranges::NA) {
        std::vector<GuardedRoaring>::const_iterator min = river_size.cbegin();
        if (finder.min_river != embark_assist::defs::river_ranges::NA){
            const int8_t min_river = static_cast<uint64_t>(finder.min_river);
            std::advance(min, min_river);
        }

        std::vector<GuardedRoaring>::const_iterator max = river_size.cend();
        if (finder.max_river != embark_assist::defs::river_ranges::NA) {
            const int8_t max_river = static_cast<uint64_t>(finder.max_river);
            // we start at the beginning to get the proper max value
            max = river_size.cbegin();
            // + 1 as we include the actually selected river size level
            std::advance(max, max_river + 1);
        }

        if (max > min) {
            const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_min_max_present_in_range_query(
                embark_assist::query::multiple_indices_query_context(river_size, min, max));
            result->queries.push_back(q);
        }
    }

    if (finder.min_waterfall > 0) {
        const int8_t waterfall_depth = finder.min_waterfall;
        const std::array<unordered_map<uint32_t, waterfall_drop_bucket*>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> &waterfall_drops = this->waterfall_drops;
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&waterfall_drops, waterfall_depth](const Roaring &embark_candidate) -> bool {
            const uint32_t cardinality = embark_candidate.cardinality();
            // max size (16*16) for a embark_candidate roaring index
            uint32_t keys[256];
            embark_candidate.toUint32Array(keys);
            for (uint8_t drop = waterfall_depth; drop < 10; drop++) {
                for (uint16_t key_index = 0; key_index < cardinality; key_index++) {
                    const uint32_t key = keys[key_index];
                    if (waterfall_drops[drop].count(key)) {
                        const waterfall_drop_bucket &bucket = *waterfall_drops[drop].at(key);
                        const uint32_t secondary_key = bucket.keys[0];
                        if (std::binary_search(keys, keys + cardinality, secondary_key)) {
                            return true;
                        }
                        if (bucket.counter > 1) {
                            const uint32_t secondary_key = bucket.keys[1];
                            if (std::binary_search(keys, keys + cardinality, secondary_key)) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }, [&waterfall_drops, waterfall_depth]() -> uint32_t {
            uint32_t get_number_of_entries = 0;
            for (uint8_t drop = waterfall_depth; drop < 10; drop++) {
                get_number_of_entries += waterfall_drops[drop].size();
            }
            return get_number_of_entries;
        }, [&waterfall_drops, waterfall_depth]() -> const std::vector<uint32_t>* {
            std::vector<uint32_t>* dest = new std::vector<uint32_t>();
            for (uint8_t drop = waterfall_depth; drop < 10; drop++) {
                for (auto it = waterfall_drops[drop].cbegin(); it != waterfall_drops[drop].cend(); ++it) {
                    dest->push_back(it->first);
                }
            }
            std::sort(dest->begin(), dest->end());
            auto last = std::unique(dest->begin(), dest->end());
            dest->erase(last, dest->end());
            return dest;
        }, [&waterfall_drops, waterfall_depth](const uint32_t world_offset, std::vector<uint32_t>& keys) -> void {
            const uint32_t world_offset_end = world_offset + 255;
            std::vector<uint32_t>* dest = &keys;
            for (uint8_t drop = waterfall_depth; drop < 10; drop++) {
                for (uint32_t key = world_offset; key <= world_offset_end; ++key) {
                    auto search = waterfall_drops[drop].find(key);
                    if (search != waterfall_drops[drop].end()) {
                        dest->push_back(key);
                    }
                }
            }

            std::sort(dest->begin(), dest->end());
            auto last = std::unique(dest->begin(), dest->end());
            dest->erase(last, dest->end());
        });
        q->flag_for_keeping();
        result->queries.push_back(q);
    }

    // vector based variant of is flat query
    if (finder.flat == embark_assist::defs::yes_no_ranges::Yes) {
        const GuardedRoaring &is_unflat_by_incursion = this->is_unflat_by_incursion;
        const std::vector<uint8_t> &mapped_elevations = this->mapped_elevations;
        std::chrono::duration<double> &vector_elevation_query_seconds = this->vector_elevation_query_seconds;
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&is_unflat_by_incursion, &mapped_elevations, &vector_elevation_query_seconds](const Roaring &embark_candidate) -> bool {
            const auto innerStart = std::chrono::steady_clock::now();
            if (is_unflat_by_incursion.intersectGuarded(embark_candidate)) {
                calculate_elapsed(innerStart, vector_elevation_query_seconds);
                return false;
            }
            const uint32_t cardinality = embark_candidate.cardinality();
            if (cardinality == 1) {
                calculate_elapsed(innerStart, vector_elevation_query_seconds);
                return true;
            }
            // max size (16*16) for a embark_candidate roaring index
            uint32_t keys[256];
            embark_candidate.toUint32Array(keys);

            const uint32_t first_key = keys[0];
            const uint8_t first_mapped_elevation = mapped_elevations[first_key];

            for (uint16_t key_index = 1; key_index < cardinality; key_index++) {
                const uint32_t current_key = keys[key_index];
                if (first_mapped_elevation != mapped_elevations[current_key]) {
                    calculate_elapsed(innerStart, vector_elevation_query_seconds);
                    return false;
                }
            }
            calculate_elapsed(innerStart, vector_elevation_query_seconds);
            return true;
        }, [&is_unflat_by_incursion]() -> uint32_t {
            return embark_assist::query::single_index_query::get_inverted_cardinality(is_unflat_by_incursion);
        }, [&is_unflat_by_incursion]() -> const std::vector<uint32_t>* {
            return embark_assist::query::single_index_query::get_inverted_keys(is_unflat_by_incursion);
        }, [&is_unflat_by_incursion](const uint32_t world_offset, std::vector<uint32_t>& keys) -> void {
            embark_assist::query::single_index_present_query::get_inverted_world_tile_keys(is_unflat_by_incursion, world_offset, keys);
        });
        q->flag_for_keeping();
        result->queries.push_back(q);
    }
    else if (finder.flat == embark_assist::defs::yes_no_ranges::No) {
        const GuardedRoaring &is_unflat_by_incursion = this->is_unflat_by_incursion;
        const std::vector<uint8_t> mapped_elevations = this->mapped_elevations;
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&is_unflat_by_incursion, &mapped_elevations](const Roaring &embark_candidate) -> bool {
            if (is_unflat_by_incursion.intersectGuarded(embark_candidate)) {
                return true;
            }
            const uint32_t cardinality = embark_candidate.cardinality();
            if (cardinality == 1) {
                return false;
            }

            // max size (16*16) for a embark_candidate roaring index
            uint32_t keys[256];
            embark_candidate.toUint32Array(keys);

            const uint32_t first_key = keys[0];
            const uint8_t first_mapped_elevation = mapped_elevations[first_key];

            for (uint16_t key_index = 1; key_index < cardinality; key_index++) {
                const uint32_t current_key = keys[key_index];
                if (first_mapped_elevation != mapped_elevations[current_key]) {
                    return true;
                }
            }
            return false;
        }, []() -> uint32_t {
            return embark_assist::query::abstract_query::get_world_size();
        }, []() -> const std::vector<uint32_t>* {
            // FIXME: this is very bad, 64MB RAM bad for a 257x257 region => change return type of method to iterator and implement different iterators
            return embark_assist::query::abstract_query::get_world_keys();
        }, [](const uint32_t world_offset, std::vector<uint32_t> &keys) -> void {
            embark_assist::query::abstract_query::get_all_world_tile_keys(world_offset, keys);
        });
        q->flag_for_keeping();
        result->queries.push_back(q);
    }

    if (finder.clay == embark_assist::defs::present_absent_ranges::Present) {
        create_and_add_present_query(hasClay, result);
    }
    else if (finder.clay == embark_assist::defs::present_absent_ranges::Absent) {
        create_and_add_absent_query(hasClay, result);
    }
    // FIXME: replace with this!
    // create_and_add_present_or_absent_query(finder.clay, hasClay, result);

    if (finder.sand == embark_assist::defs::present_absent_ranges::Present) {
        create_and_add_present_query(hasSand, result);
    }
    else if (finder.sand == embark_assist::defs::present_absent_ranges::Absent) {
        create_and_add_absent_query(hasSand, result);
    }
    // FIXME: replace with this!
    // create_and_add_present_or_absent_query(finder.sand, hasSand, result);

    // FIXME: test those!
    create_and_add_present_or_absent_query(finder.flux, hasFlux, result);
    create_and_add_present_or_absent_query(finder.coal, hasCoal, result);

    // FIXME: implement min/max soil + min_soil everywhere => method call
    if (finder.soil_min != embark_assist::defs::soil_ranges::NA || finder.soil_max != embark_assist::defs::soil_ranges::NA) {
        std::vector<GuardedRoaring>::const_iterator min = soil.cbegin();
        if (finder.soil_min != embark_assist::defs::soil_ranges::NA) {
            const int8_t soil_min = static_cast<uint64_t>(finder.soil_min);
            std::advance(min, soil_min);
        }

        std::vector<GuardedRoaring>::const_iterator max = soil.cend();
        if (finder.soil_max != embark_assist::defs::soil_ranges::NA) {
            const int8_t soil_max = static_cast<uint64_t>(finder.soil_max);
            // we start at the beginning to get the proper max value
            max = soil.cbegin();
            // + 1 as we include the actually selected soil level
            std::advance(max, soil_max + 1);
        }

        if (max > min) {
            // only if soil_min_everywhere is specified AND there is also a min value specified
            if (finder.soil_min_everywhere == embark_assist::defs::all_present_ranges::All && min != soil.cbegin()) {
                const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_min_max_all_in_range_query(
                    embark_assist::query::multiple_indices_query_context(soil, min, max));
                result->queries.push_back(q);
            } else{
                const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_min_max_present_in_range_query(
                    embark_assist::query::multiple_indices_query_context(soil, min, max));
                result->queries.push_back(q);
            }
        }
    }

    if (finder.freezing != embark_assist::defs::freezing_ranges::NA) {
        // it is important to know, that for min values the smaller temperature always "wins" and for max values the higher temperature is dominant
        // => if a tile has an entry for both MAX_ZERO_OR_BELOW and MAX_ABOVE_ZERO for most cases only the MAX_ABOVE_ZERO is relevant as it trumps over the lower temperature
        // the same is true for MIN_ZERO_OR_BELOW and MIN_ABOVE_ZERO, just the other way around: if looking for more freezing MIN_ZERO_OR_BELOW trumps MIN_ABOVE_ZERO
        // only for Permanent and Never this is inverted, so we filter/exclude the unwanted entries
        if (finder.freezing == embark_assist::defs::freezing_ranges::Permanent) {
            std::vector<const GuardedRoaring*> exclusion_indices;
            exclusion_indices.push_back(&freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MAX_ABOVE_ZERO]);
            create_and_add_all_query(freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MAX_ZERO_OR_BELOW], exclusion_indices, result);
        }
        else if (finder.freezing == embark_assist::defs::freezing_ranges::At_Least_Partial) {
            // shares all results from Partial and also all from Permanent
            create_and_add_present_query(freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MIN_ZERO_OR_BELOW], result);
        }
        else if (finder.freezing == embark_assist::defs::freezing_ranges::Partial) {
            // => partial means that the minimum is below zero and the max above zero
            // also there currently is no query class/run strategy that implements this logic (requiring 2 intersects from two different indices), so we reuse code for now
            // TODO: implement this as one query
            create_and_add_present_query(freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MIN_ZERO_OR_BELOW], result);
            create_and_add_present_query(freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MAX_ABOVE_ZERO], result);
        }
        else if (finder.freezing == embark_assist::defs::freezing_ranges::At_Most_Partial) {
            // shares all results from Partial and also all from Never
            create_and_add_present_query(freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MAX_ABOVE_ZERO], result);
        }
        else if (finder.freezing == embark_assist::defs::freezing_ranges::Never) {
            std::vector<const GuardedRoaring*> exclusion_indices;
            exclusion_indices.push_back(&freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MIN_ZERO_OR_BELOW]);
            create_and_add_all_query(freezing[(uint8_t)embark_assist::key_buffer_holder::temperatur::MIN_ABOVE_ZERO], exclusion_indices, result);
        }
    }

    create_and_add_present_or_absent_query(finder.blood_rain, has_blood_rain, result);

    // FIXME: implement syndrome rain

    // FIXME: implement reanimation

    if (finder.spire_count_min != -1 || finder.spire_count_max != -1) {
        const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_cardinality_query(
            embark_assist::query::multiple_indices_query_context(adamantine_level, adamantine_level.cbegin(), adamantine_level.cend(), finder.spire_count_min, finder.spire_count_max));
        result->queries.push_back(q);
    }

    if (finder.magma_min != embark_assist::defs::magma_ranges::NA || finder.magma_max != embark_assist::defs::magma_ranges::NA) {
        std::vector<GuardedRoaring>::const_iterator min = magma_level.cbegin();
        if (finder.magma_min != embark_assist::defs::magma_ranges::NA) {
            const int8_t magma_min = static_cast<uint64_t>(finder.magma_min);
            std::advance(min, magma_min);
        }

        std::vector<GuardedRoaring>::const_iterator max = magma_level.end();
        if (finder.magma_max != embark_assist::defs::magma_ranges::NA) {
            const int8_t magma_max = static_cast<uint64_t>(finder.magma_max);
            // we start at the beginning to get the proper max value
            max = magma_level.cbegin();
            // + 1 as we include the actually selected magma level
            std::advance(max, magma_max + 1);
        }

        if (max > min) {
            const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_min_max_present_in_range_query(
                embark_assist::query::multiple_indices_query_context(magma_level, min, max));
            result->queries.push_back(q);
        }
    }

    if (finder.biome_count_min != -1 || finder.biome_count_max != -1) {
        const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_distinct_intersects_query(
            embark_assist::query::multiple_indices_query_context(biome, biome.cbegin(), biome.cend(), finder.biome_count_min, finder.biome_count_max));
        result->queries.push_back(q);
    }

    if (finder.region_type_1 != -1) {
        create_and_add_present_query(this->region_type[finder.region_type_1], result);
    }
    // FIXME: replace with this
    //create_and_add_region_type_query(finder.region_type_1, region_type, result);
    //create_and_add_region_type_query(finder.region_type_2, region_type, result);
    //create_and_add_region_type_query(finder.region_type_3, region_type, result);

    if (finder.region_type_2 != -1) {
        create_and_add_present_query(this->region_type[finder.region_type_2], result);
    }

    if (finder.region_type_3 != -1) {
        create_and_add_present_query(this->region_type[finder.region_type_3], result);
    }

    if (finder.biome_1 != -1) {
        create_and_add_present_query(this->biome[finder.biome_1], result);
    }

    if (finder.biome_2 != -1) {
        create_and_add_present_query(this->biome[finder.biome_2], result);
    }

    if (finder.biome_3 != -1) {
        create_and_add_present_query(this->biome[finder.biome_3], result);
    }
    // FIXME: replace with this
    //create_and_add_biome_type_query(finder.biome_1, biome, result);
    //create_and_add_biome_type_query(finder.biome_2, biome, result);
    //create_and_add_biome_type_query(finder.biome_3, biome, result);

    if (finder.metal_1 != -1) {
        create_and_add_present_query(*metals[finder.metal_1], result);
    }
    // FIXME: replace all creation of inorganics queries with this
    //create_and_add_inorganics_query(finder.metal_1, metals, result);

    if (finder.metal_2 != -1) {
        create_and_add_present_query(*metals[finder.metal_2], result);
    }

    if (finder.metal_3 != -1) {
        create_and_add_present_query(*metals[finder.metal_3], result);
    }

    if (finder.economic_1 != -1) {
        create_and_add_present_query(*economics[finder.economic_1], result);
    }

    if (finder.economic_2 != -1) {
        create_and_add_present_query(*economics[finder.economic_2], result);
    }

    if (finder.economic_3 != -1) {
        create_and_add_present_query(*economics[finder.economic_3], result);
    }

    if (finder.mineral_1 != -1) {
        create_and_add_present_query(*minerals[finder.mineral_1], result);
    }

    if (finder.mineral_2 != -1) {
        create_and_add_present_query(*minerals[finder.mineral_2], result);
    }

    if (finder.mineral_3 != -1) {
        create_and_add_present_query(*minerals[finder.mineral_3], result);
    }

    result->sort_queries();

    if (init_most_significant_ids && !result->queries.empty()) {
        const embark_assist::query::query_interface* first_query = result->queries[0];
        result->set_most_significant_ids(first_query->get_keys());

        // FIXME: keep the query in any case, but move it to an extra "holding place" to be able to move it back if during survey iteration phase queries another query becomes more significant.
        if (first_query->is_to_be_deleted_after_key_extraction()) {
            // remove the first query - as it is being used directly for its keys
            result->queries.erase(result->queries.begin());
            delete first_query;
        }
    }
    else {
        result->set_most_significant_ids(new std::vector<uint32_t>(0));
    }

    return result;
}

void embark_assist::index::Index::output_embark_tiles(std::ofstream &myfile, const uint16_t x, const uint16_t y, uint16_t start_i, uint16_t start_k) const {
    myfile << "[";
    for (uint16_t k = start_k; k < start_k + finder.y_dim; k++) {
        for (uint16_t i = start_i; i < start_i + finder.x_dim; i++) {
            const uint32_t key = keyMapper->key_of(x, y, i, k);
            myfile << " "<< key;
        }
    }
    myfile << " ]" << "\n";
}

void embark_assist::index::Index::output_embark_matches(std::ofstream &myfile, const uint16_t x, const uint16_t y, const embark_assist::defs::matches &match) const {
    for (uint16_t i = 0; i < 16; i++) {
        for (uint16_t k = 0; k < 16; k++) {
            if (match.mlt_match[i][k]) {
                const uint32_t key = keyMapper->key_of(x, y, i, k);
                myfile << " i:" << i << "/k:" << k << "\t(" << key << ")";
                output_embark_tiles(myfile, x, y, i, k);
            }
        }
    }
}

// TODO: remove, just here for debugging
void embark_assist::index::Index::compare_matches(const std::string prefix, df::world *world, const embark_assist::defs::match_results &match_results_matcher, const embark_assist::defs::match_results &match_results_index) const {
    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);

    for (uint16_t x = 0; x < world->worldgen.worldgen_parms.dim_x; x++) {
        for (uint16_t y = 0; y < world->worldgen.worldgen_parms.dim_y; y++) {
            if (match_results_matcher[x][y].contains_match != match_results_index[x][y].contains_match) {
                myfile << "\nx:" << x << "/y:" << y;
                if (match_results_matcher[x][y].contains_match) {
                    myfile << " unique world tile matcher result\n";
                    output_embark_matches(myfile, x, y, match_results_matcher[x][y]);
                }
                else {
                    myfile << " unique world tile index result\n";
                    output_embark_matches(myfile, x, y, match_results_index[x][y]);
                }
            }
            else {
                bool is_first = true;
                for (uint16_t i = 0; i < 16; i++) {
                    for (uint16_t k = 0; k < 16; k++) {
                        if (match_results_matcher[x][y].mlt_match[i][k] != match_results_index[x][y].mlt_match[i][k]) {
                            if (is_first) {
                                myfile << "\nx:" << x << "/y:" << y << " common world tile result\n";
                                is_first = false;
                            }
                            const uint32_t key = keyMapper->key_of(x, y, i, k);
                            myfile << " i:" << i << "/k:" << k << " (" << key << ")";
                            if (match_results_matcher[x][y].mlt_match[i][k]) {
                                myfile << " unique embark tile matcher result\n";
                            }
                            else {
                                myfile << " unique embark tile index result\n";
                            }
                        }
                    }
                }
            }
        }
    }

    myfile.close();
}

// TODO: remove, just here for debugging
void embark_assist::index::Index::write_matches_to_file(const std::string prefix, const embark_assist::defs::match_results &matches, df::world *world) const {
    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);

    for (uint16_t x = 0; x < world->worldgen.worldgen_parms.dim_x; x++) {
        for (uint16_t y = 0; y < world->worldgen.worldgen_parms.dim_y; y++) {
            if (matches[x][y].contains_match) {
                myfile << "\nx:" << x << "/y:" << y << "\n";
                output_embark_matches(myfile, x, y, matches[x][y]);
            }
        }
    }

    myfile.close();
}

void embark_assist::index::Index::init_for_iterative_find() {
    if (!containsEntries()) {
        if (iterative_query_plan != nullptr) {
            delete iterative_query_plan;
            iterative_query_plan = nullptr;
        }

        embark_assist::survey::clear_results(&this->match_results);
        embark_assist::survey::clear_results(&iterative_match_results_comparison);

        iterative_query_plan = create_query_plan(finder, false);
        find_context = new embark_assist::index::find_context(finder);

        embark_assist::query::abstract_query::set_embark_size(find_context->embark_size);
        // FIXME: create find context that contains all values that can be calculated once ot need to be kept for all of the iterative find calls: 

        //const uint16_t embark_size_x = finder.x_dim;
        //const uint16_t embark_size_y = finder.y_dim;

        //const uint16_t embark_size = embark_size_x * embark_size_y;
        //embark_assist::query::abstract_query::set_embark_size(embark_size);

        //const size_t size_factor_x = 16 - embark_size_x + 1;
        //const size_t size_factor_y = 16 - embark_size_y + 1;

        //size_t max_number_of_embark_variants = size_factor_x * size_factor_y;
        //// from a certain embark size (x,y = 9) the region size (16x16) is the limiting factor for how many different embark variants there can be
        //// the smaller number is the correct one in any case
        //max_number_of_embark_variants = std::min(max_number_of_embark_variants, (size_t)embark_size);

        //std::vector<Roaring> embarks(max_number_of_embark_variants);
        //uint32_t embark_tile_key_buffer[256];
        //uint32_t number_of_iterations = 0;
        //uint32_t number_of_matches = 0;
        //uint32_t number_of_matched_worldtiles = 0;

        //int16_t previous_x = -1;
        //int16_t previous_y = -1;

        //embark_tile_tracker start_tile_tracker;
    }
}

void embark_assist::index::Index::check_for_find_single_world_tile_matches(const int16_t x, const int16_t y, embark_assist::defs::region_tile_datum &rtd, const string &prefix) {
    processed_single_world_tiles_file << x << "," << y << "-" << prefix << '\n';

    //if (x == 0 && y == 4) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    auto t = std::chrono::high_resolution_clock::now();
    //    out.print("embark_assist::index::Index::check_for_find_single_world_tile_matches: x == 0 && y == 4 at %lld - %s\n", static_cast<long long int>(t.time_since_epoch().count()), prefix);
    //}

    const uint8_t counter = rtd.process_counter.get()->fetch_add(1, memory_order::memory_order_relaxed);
    // fetch_add always returns the previous value, so 3 is "enough" even if 4 would be easier to understand...
    if (counter >= 3) {
        find_single_world_tiles_file << x << "," << y << "-" << prefix << '\n';
        //processed_world_tiles_with_sync.fetch_add(1);
        //find_single_world_tile_matches(x, y);
        completely_surveyed_positions->emplace(x, y);

    }
    processed_world_all.fetch_add(1);
    //process_counter.reset();
}

void embark_assist::index::Index::find_matches_in_surveyed_world_tiles() const {
    find_result = std::async(std::launch::async, [=]() {
        std::vector<position> surveyed_positions;
        completely_surveyed_positions->get_positions(surveyed_positions);
        for (const position position : surveyed_positions) {
            find_single_world_tile_matches(position.x, position.y);
        }
    });
}

void embark_assist::index::Index::find_single_world_tile_matches(const int16_t x, const int16_t y) const {

    const auto start = std::chrono::steady_clock::now();

    const std::lock_guard<std::mutex> find_single_world_tile_matches_mutex_guard(find_single_mutex);

    entryCounter++;

    const uint32_t world_offset = keyMapper->key_of(x, y, 0, 0);
    //// if (x == 226 && y == 186) {
    //if (x == 0 && y == 4) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    auto t = std::chrono::high_resolution_clock::now();
    //    //out.print("embark_assist::index::Index::find_single_world_tile_matches: x == 226 && y == 186 at %lld\n", static_cast<long long int>(t.time_since_epoch().count()));
    //    out.print("embark_assist::index::Index::find_single_world_tile_matches: x == 0 && y == 4 (%u) at %lld\n", world_offset, static_cast<long long int>(t.time_since_epoch().count()));
    //}
    std::vector<uint32_t> keys{};
    keys.reserve(1024);
    iterative_query_plan->sort_queries();
    iterative_query_plan->get_most_significant_ids(world_offset, keys);

    find_context->number_of_significant_ids += keys.size();

    embark_assist::defs::match_results &match_results = this->iterative_match_results_comparison;

    match_results[x][y].preliminary_match = false;
    iterate_most_significant_keys(*find_context, match_results, *iterative_query_plan, keys);

    const auto innerEnd = std::chrono::steady_clock::now();
    find_context->totalElapsed += innerEnd - start;

    if (containsEntries()) {
        const std::time_t end_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("embark_assist::index::Index::find_single_world_tile_matches: finished index query search at %s with elapsed time: %f seconds with %d iterations and %d matches in %d world tiles - with %d significant ids in total\n", std::ctime(&end_time), find_context->totalElapsed.count(), find_context->number_of_iterations, find_context->number_of_matches, find_context->number_of_matched_worldtiles, find_context->number_of_significant_ids);
        out.print("embark_assist::index::Index::find_single_world_tile_matches: took %f seconds to create query plan, took %f seconds total to calculate embark variants, took %f seconds total to execute query plan \n", find_context->queryPlanCreatedElapsed.count(), find_context->embarkVariantsCreated.count(), find_context->queryElapsed.count());
        if (iterative_query_plan != nullptr) {
            delete iterative_query_plan;
            iterative_query_plan = nullptr;
            write_matches_to_file("iterative_search_matches", match_results, world);
        }

        if (find_context != nullptr) {
            delete find_context;
        }
    }
}

void embark_assist::index::Index::find_all_matches(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results_matcher) const {
    color_ostream_proxy out(Core::getInstance().getConsole());

    // FIXME: remove the if-return, as the case won't exist any more once "find_single_world_tile_matches" is properly implemented
    // don't do a complete find run if there just was an iterative find run 
    if (iterative_query_plan != nullptr) {
        out.print("embark_assist::index::Index::find_all_matches: early exit as the iterative search is being run\n");
        // FIXME: reactivate this again
        //return;
    }

    // TODO: remove, just here for debugging
    embark_assist::survey::clear_results(&this->match_results_comparison);

    embark_assist::defs::match_results &match_results = this->match_results_comparison;
    //embark_assist::defs::match_results &match_results = match_results_matcher;

    const auto innerStartTime = std::chrono::steady_clock::now();
    const std::time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    out.print("embark_assist::index::Index::find_all_matches: started index query search at %s\n", std::ctime(&start_time));

    const uint16_t embark_size_x = finder.x_dim;
    const uint16_t embark_size_y = finder.y_dim;

    const uint16_t embark_size = embark_size_x * embark_size_y;
    embark_assist::query::abstract_query::set_embark_size(embark_size);

    const size_t size_factor_x = 16 - embark_size_x + 1;
    const size_t size_factor_y = 16 - embark_size_y + 1;

    size_t max_number_of_embark_variants = size_factor_x * size_factor_y;
    // from a certain embark size (x,y = 9) the region size (16x16) is the limiting factor for how many different embark variants there can be
    // the smaller number is the correct one in any case
    max_number_of_embark_variants = std::min(max_number_of_embark_variants, (size_t)embark_size);

    std::vector<Roaring> embarks(max_number_of_embark_variants);
    uint32_t embark_tile_key_buffer[256];
    uint32_t number_of_iterations = 0;
    uint32_t number_of_matches = 0;
    uint32_t number_of_matched_worldtiles = 0;

    int16_t previous_x = -1;
    int16_t previous_y = -1;

    embark_tile_tracker start_tile_tracker;

    std::chrono::duration<double> queryPlanCreatedElapsed = std::chrono::seconds(0);
    std::chrono::duration<double> queryElapsed = std::chrono::seconds(0);
    std::chrono::duration<double> embarkVariantsCreated = std::chrono::seconds(0);

    const auto createQueryPlanStart = std::chrono::steady_clock::now();
    // create query plan
    const query_plan_interface *query_plan = create_query_plan(finder, true);
    queryPlanCreatedElapsed = std::chrono::steady_clock::now() - createQueryPlanStart;
    out.print("number of significant_ids: %d\n", query_plan->get_most_significant_ids().size());
    // loop over all keys that are most significant
    for (const uint32_t position_id : query_plan->get_most_significant_ids()) {
        //std::vector<Roaring> embarks(max_number_of_embark_variants);
        //const auto calculate_embark_variants_Start = std::chrono::steady_clock::now();
        const int number_of_embark_variants = calculate_embark_variants(position_id, embark_size_x, embark_size_y, embarks, embark_tile_key_buffer, start_tile_tracker);
        //embarkVariantsCreated += std::chrono::steady_clock::now() - calculate_embark_variants_Start;
        for (int embark_variant_index = 0; embark_variant_index < number_of_embark_variants; embark_variant_index++) {
            //const auto queryStart = std::chrono::steady_clock::now();
            bool matches = query_plan->execute(embarks[embark_variant_index]);
            //const auto innerStartTime = std::chrono::steady_clock::now();
            //queryElapsed += (innerStartTime - queryStart);
            number_of_iterations++;
            if (matches) {
                uint16_t x = 0;
                uint16_t y = 0;
                uint16_t i = 0;
                uint16_t k = 0;

                // getting the key/id of the start tile
                const uint32_t smallest_key = embarks[embark_variant_index].minimum();
                // extracting the x,y + i,k to set the match
                //get_position(smallest_key, x, y, i, k);
                keyMapper->get_position(smallest_key, x, y, i, k);

                if (previous_x != x || previous_y != y) {
                    number_of_matched_worldtiles++;
                    previous_x = x;
                    previous_y = y;
                }

                //match_results_matcher[x][y].contains_match = true;
                //match_results_matcher[x][y].preliminary_match = false;
                // TODO: just for debugging, reactivate above lines
                match_results[x][y].contains_match = true;
                match_results[x][y].preliminary_match = false;

                //if (match_results_matcher[x][y].mlt_match[i][k]) {
                //    out.print("embark_assist::index::Index::find_all_matches: found same match as matcher:");
                //}
                //else {
                    //out.print("embark_assist::index::Index::find_all_matches: found match matcher has not found:");
                //    //out.print("key: %d / position x:%d y:%d i:%d k:%d\n", smallest_key, x, y, i, k);
                //}

                //match_results_matcher[x][y].mlt_match[i][k] = true;
                // TODO: just for debugging, reactivate above line
                match_results[x][y].mlt_match[i][k] = true;

                //out.print("key: %d / position x:%d y:%d i:%d k:%d\n", smallest_key, x,y,i,k);
                number_of_matches++;
            }
        }
    }
    const uint32_t number_of_significant_ids = query_plan->get_most_significant_ids().size();

    delete query_plan;

    const auto innerEnd = std::chrono::steady_clock::now();
    const std::time_t end_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::chrono::duration<double> elapsed_seconds = innerEnd - innerStartTime;
    out.print("embark_assist::index::Index::find_all_matches: finished index query search at %s with elapsed time: %f seconds with %d iterations and %d matches in %d world tiles - with %d significant ids in total\n", std::ctime(&end_time), elapsed_seconds.count(), number_of_iterations, number_of_matches, number_of_matched_worldtiles, number_of_significant_ids);
    out.print("embark_assist::index::Index::find_all_matches: took %f seconds to create query plan, took %f seconds total to calculate embark variants, took %f seconds total to execute query plan \n", queryPlanCreatedElapsed.count(), embarkVariantsCreated.count(), queryElapsed.count());

    out.print("### embark_assist::index::Index::vector elevation query total took %f seconds ###\n", vector_elevation_query_seconds.count());

    vector_elevation_query_seconds = std::chrono::seconds(0);

    // TODO: remove, just here for debugging    
    compare_matches("match_result_delta", world, match_results_matcher, match_results_comparison);
    compare_matches("iterative_match_result_delta", world, iterative_match_results_comparison, match_results_comparison);

    //write_matches_to_file("iterative_search_matches", iterative_match_results_comparison, world);
    write_matches_to_file("full_search_matches", match_results, world);
    write_matches_to_file("matcher_search_matches", match_results_matcher, world);
}

// FIXME refactor methods iterate_most_significant_keys and find_all_matches to use the same code...
void embark_assist::index::Index::iterate_most_significant_keys(embark_assist::index::find_context &find_context, embark_assist::defs::match_results &match_results_matcher, const query_plan_interface &query_plan, const std::vector<uint32_t> &keys) const {

    embark_assist::defs::match_results &local_match_results = match_results_matcher;
    color_ostream_proxy out(Core::getInstance().getConsole());
    //const auto innerStartTime = std::chrono::steady_clock::now();
    //const std::time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    //out.print("embark_assist::index::Index::iterate_most_significant_keys: started index query search at %s\n", std::ctime(&start_time));

    for (const uint32_t position_id : keys) {
        //std::vector<Roaring> embarks(max_number_of_embark_variants);
        //const auto calculate_embark_variants_Start = std::chrono::steady_clock::now();
        const int number_of_embark_variants = calculate_embark_variants(position_id, find_context.embark_size_x, find_context.embark_size_y, find_context.embarks, find_context.embark_tile_key_buffer, find_context.start_tile_tracker);
        //embarkVariantsCreated += std::chrono::steady_clock::now() - calculate_embark_variants_Start;
        for (int embark_variant_index = 0; embark_variant_index < number_of_embark_variants; embark_variant_index++) {
            //const auto queryStart = std::chrono::steady_clock::now();
            bool matches = query_plan.execute(find_context.embarks[embark_variant_index]);
            //const auto innerStartTime = std::chrono::steady_clock::now();
            //queryElapsed += (innerStartTime - queryStart);
            find_context.number_of_iterations++;
            if (matches) {
                uint16_t x = 0;
                uint16_t y = 0;
                uint16_t i = 0;
                uint16_t k = 0;

                // getting the key/id of the start tile
                const uint32_t smallest_key = find_context.embarks[embark_variant_index].minimum();
                // extracting the x,y + i,k to set the match
                //get_position(smallest_key, x, y, i, k);
                keyMapper->get_position(smallest_key, x, y, i, k);

                if (find_context.previous_x != x || find_context.previous_y != y) {
                    find_context.number_of_matched_worldtiles++;
                    find_context.previous_x = x;
                    find_context.previous_y = y;
                }

                //match_results_matcher[x][y].contains_match = true;
                //match_results_matcher[x][y].preliminary_match = false;
                // TODO: just for debugging, reactivate above lines
                local_match_results[x][y].contains_match = true;
                local_match_results[x][y].preliminary_match = false;

                //if (match_results_matcher[x][y].mlt_match[i][k]) {
                //    out.print("embark_assist::index::Index::find_all_matches: found same match as matcher:");
                //}
                //else {
                    //out.print("embark_assist::index::Index::find_all_matches: found match matcher has not found:");
                //    //out.print("key: %d / position x:%d y:%d i:%d k:%d\n", smallest_key, x, y, i, k);
                //}

                //match_results_matcher[x][y].mlt_match[i][k] = true;
                // TODO: just for debugging, reactivate above line
                local_match_results[x][y].mlt_match[i][k] = true;

                //out.print("key: %d / position x:%d y:%d i:%d k:%d\n", smallest_key, x,y,i,k);
                find_context.number_of_matches++;
            }
        }
    }
}

const void embark_assist::index::Index::outputSizes(const string &prefix) {
    color_ostream_proxy out(Core::getInstance().getConsole());
    FILE* outfile = fopen(index_folder_log_file_name, "a");
    fprintf(outfile, "### %s ###\n", prefix.c_str());

    uint32_t byteSize = 0;

    byteSize = uniqueKeys.getSizeInBytes();
    fprintf(outfile, "unique entries bytesize: %zd\n", uniqueKeys.getSizeInBytes());
    byteSize += hasAquifer.getSizeInBytes();
    fprintf(outfile, "hasAquifer bytesize: %zd\n", hasAquifer.getSizeInBytes());
    //fprintf(outfile, "hasAquifer.roaring.high_low_container.size: %d\n", hasAquifer.roaring.high_low_container.size);
    //fprintf(outfile, "hasAquifer.roaring.high_low_container.allocation_size: %d\n", hasAquifer.roaring.high_low_container.allocation_size);
    byteSize += hasNoAquifer.getSizeInBytes();
    fprintf(outfile, "hasNoAquifer bytesize: %zd\n", hasNoAquifer.getSizeInBytes());
    byteSize += hasRiver.getSizeInBytes();
    fprintf(outfile, "hasRiver bytesize: %zd\n", hasRiver.getSizeInBytes());

    byteSize += hasClay.getSizeInBytes();
    fprintf(outfile, "hasClay bytesize: %zd\n", hasClay.getSizeInBytes());
    byteSize += hasCoal.getSizeInBytes();
    fprintf(outfile, "hasCoal bytesize: %zd\n", hasCoal.getSizeInBytes());
    byteSize += hasFlux.getSizeInBytes();
    fprintf(outfile, "hasFlux bytesize: %zd\n", hasFlux.getSizeInBytes());
    byteSize += hasSand.getSizeInBytes();
    fprintf(outfile, "hasSand bytesize: %zd\n", hasSand.getSizeInBytes());
    byteSize += has_blood_rain.getSizeInBytes();
    fprintf(outfile, "has_blood_rain bytesize: %zd\n", has_blood_rain.getSizeInBytes());
    byteSize += no_waterfall.getSizeInBytes();
    fprintf(outfile, "no_waterfall bytesize: %zd\n", no_waterfall.getSizeInBytes());
    byteSize += is_unflat_by_incursion.getSizeInBytes();
    fprintf(outfile, "is_unflat_by_incursion bytesize: %zd\n", is_unflat_by_incursion.getSizeInBytes());

    uint32_t level_post_fix = 0;
    for (auto& index : soil) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "soil_level#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 0;
    for (auto& index : freezing) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "freezing#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 0;
    for (auto& index : river_size) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "river_size#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 1;
    for (auto& index : magma_level) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "magma_level#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 1;
    for (auto& index : adamantine_level) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "adamantine_level#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 0;
    for (auto& index : savagery_level) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "savagery_level#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 0;
    for (auto& index : evilness_level) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "evilness_level#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 0;
    for (auto& index : biome) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "biome#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    level_post_fix = 0;
    for (auto& index : region_type) {
        byteSize += index.getSizeInBytes();
        fprintf(outfile, "region_type#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    }

    fprintf(outfile, "mapped_elevations bytesize: %zd\n", mapped_elevations.size() * sizeof(uint8_t));

    //level_post_fix = 0;
    //for (auto& index : waterfall_drops) {
    //    byteSize += index.getSizeInBytes();
    //    fprintf(outfile, "waterfall_drops#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    //}

    fprintf(outfile, "metal index bytesizes:\n");
    for (auto it = metals.cbegin(); it != metals.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN METAL ###";
            const uint16_t index = std::distance(metals.cbegin(), it);
            name = getInorganicName(index, metal_names, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
            // fprintf(outfile, "%d (index #%d); ", it->second->getSizeInBytes(), it->first);
        }
    }
    fprintf(outfile, "\neconomic index bytesizes:\n");
    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN ECONOMIC ###";
            const uint16_t index = std::distance(economics.cbegin(), it);
            name = getInorganicName(index, economic_names, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
        }
    }
    fprintf(outfile, "\nminerals index bytesizes:\n");
    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN MINERAL ###";
            const uint16_t index = std::distance(minerals.cbegin(), it);
            name = getInorganicName(index, mineral_names, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
        }
    }

    fprintf(outfile, "\n total index bytesize: %d\n", byteSize);
    fclose(outfile);
}

void embark_assist::index::Index::init_inorganic_indices() {
    metals.resize(max_inorganic, nullptr);
    for (uint16_t k = 0; k < max_inorganic; k++) {
        for (uint16_t l = 0; l < world->raws.inorganics[k]->metal_ore.mat_index.size(); l++) {
            const int metalIndex = world->raws.inorganics[k]->metal_ore.mat_index[l];
            if (metals[metalIndex] == nullptr) {
                roaring_bitmap_t* rr = roaring_bitmap_create_with_capacity(capacity);
                GuardedRoaring* inorganicIndex = new GuardedRoaring(rr);
                metals[metalIndex] = inorganicIndex;
            }
        }
    }
    metals.shrink_to_fit();

    economics.resize(max_inorganic, nullptr);
    for (int16_t k = 0; k < max_inorganic; k++) {
        if (world->raws.inorganics[k]->economic_uses.size() != 0 && !world->raws.inorganics[k]->material.flags.is_set(df::material_flags::IS_METAL)) {
            if (economics[k] == nullptr) {
                roaring_bitmap_t* rr = roaring_bitmap_create_with_capacity(capacity);
                GuardedRoaring* inorganicIndex = new GuardedRoaring(rr);
                economics[k] = inorganicIndex;
            }
        }
    }
    economics.shrink_to_fit();

    minerals.resize(max_inorganic, nullptr);
    for (int16_t k = 0; k < max_inorganic; k++) {
        const df::inorganic_raw* raw = world->raws.inorganics[k];
        if (
            // true ||
            raw->environment.location.size() != 0 ||
            raw->environment_spec.mat_index.size() != 0 ||
            raw->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
            raw->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
            raw->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
            raw->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
            raw->flags.is_set(df::inorganic_flags::SOIL)) {
            if (minerals[k] == nullptr) {
                roaring_bitmap_t* rr = roaring_bitmap_create_with_capacity(capacity);
                GuardedRoaring* inorganicIndex = new GuardedRoaring(rr);
                minerals[k] = inorganicIndex;
            }
        }
    }
    minerals.shrink_to_fit();
}
std::string embark_assist::index::Index::getInorganicName(uint16_t index, const std::unordered_map<uint16_t, std::string> &ingorganicNames, std::string name) const {
    const auto iter = ingorganicNames.find(index);
    if (iter != ingorganicNames.end()) {
        name = iter->second;
    }
    return name;
}
void embark_assist::index::Index::writeIndexToDisk(const GuardedRoaring& roaring, const std::string prefix) const {
    std::size_t expected_size_in_bytes = roaring.getSizeInBytes();
    std::vector<char> buffer(expected_size_in_bytes);
    std::size_t size_in_bytes = roaring.write(buffer.data());

    //std::ofstream stream("yourFile", std::ios::binary);
    //stream.write(reinterpret_cast<const char*>(&buffer), size_in_bytes);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto myfile = std::fstream(index_folder_name + prefix + ".index", std::ios::out | std::ios::binary);
    myfile.write((char*)&buffer[0], size_in_bytes);
    myfile.close();
    auto endTime = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

void embark_assist::index::Index::writeCoordsToDisk(const GuardedRoaring& roaring, const std::string prefix) const {
    color_ostream_proxy out(Core::getInstance().getConsole());
    out.printerr("embark_assist::index::Index::writeCoordsToDisk is currently deactivated!\n");
    return;

    const uint64_t cardinality = roaring.cardinalityGuarded();
    uint32_t* most_significant_ids = new uint32_t[cardinality];
    roaring.toUint32ArrayGuarded(most_significant_ids);
    const std::vector<uint32_t>* keys = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + cardinality);

    auto myfile = std::ofstream(index_folder_name + prefix + "_coords.txt", std::ios::out);

    int16_t prev_x = -1;
    int16_t prev_y = -1;

    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t i = 0;
    uint16_t k = 0;

    for (const uint32_t key : *keys) {
        keyMapper->get_position(key, x, y, i, k);
        if (x != prev_x || y != prev_y) {
            myfile << "\nx:" << x << "/y:" << y << "\n";
            prev_x = x;
            prev_y = y;
        }
        myfile << " i:" << i << "/k:" << k << " (" << key << ")";
    }

    myfile.close();
    delete keys;
    delete most_significant_ids;
}
