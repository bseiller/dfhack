
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

        std::mutex add_many_aquifer_m;
        std::mutex add_many_clay_m;
        std::mutex add_many_sand_m;

        std::array<std::mutex, embark_assist::defs::SOIL_DEPTH_LEVELS> add_many_soil_ms;
        std::array<std::mutex, 3> add_many_savagery_ms;
        std::array<std::mutex, 3> add_many_evilness_ms;
        std::array<std::mutex, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> add_many_biomes_ms;
        std::array<std::mutex, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> add_many_regions_ms;

        class query_plan : public query_plan_interface {
        public:
            std::vector<const embark_assist::query::query_interface*> queries;

            // TODO: make this a smart iterator instead of a vector - which would save some memory...
            const std::vector<uint32_t>& get_most_significant_ids() const final {
                return *most_significant_ids;
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

            // FIXME: add sort and init method that can be called after every step during the iterative search/find phase

        private: 
            // FIXME: make this an iterator
            const std::vector<uint32_t>* most_significant_ids;
        };

        //void add_to_buffers(const uint32_t key, const embark_assist::defs::mid_level_tile_basic &mlt, const embark_assist::defs::region_tile_datum &rtd, embark_assist::key_buffer_holder::basic_key_buffer_holder<256> &buffer_holder) {
        //    if (mlt.aquifer) {
        //        buffer_holder.add_aquifer(key);
        //        // buffer_holder.aquiferBuffer[buffer_holder.aquifierBufferIndex++] = key;
        //    }

        //    if (mlt.clay) {
        //        buffer_holder.add_clay(key);
        //        //buffer_holder.clayBuffer[buffer_holder.clayBufferIndex++] = key;
        //    }

        //    if (mlt.sand) {
        //        buffer_holder.add_sand(key);
        //        // buffer_holder.sandBuffer[buffer_holder.sandBufferIndex++] = key;
        //    }

        //    buffer_holder.add_soil_depth(key, mlt.soil_depth);
        //    //buffer_holder.soil_buffer[mlt.soil_depth][buffer_holder.soil_buffer_index[mlt.soil_depth]++] = key;
        //    buffer_holder.add_savagery_level(key, mlt.savagery_level);
        //    // buffer_holder.savagery_buffer[mlt.savagery_level][buffer_holder.savagery_buffer_index[mlt.savagery_level]++] = key;
        //    buffer_holder.add_evilness_level(key, mlt.evilness_level);
        //    // buffer_holder.evilness_buffer[mlt.evilness_level][buffer_holder.evilness_buffer_index[mlt.evilness_level]++] = key;

        //    // needs additional processing!
        //    const int16_t biome_id = rtd.biome[mlt.biome_offset];
        //    buffer_holder.add_biome(key, biome_id);

        //    // TODO: how to do this for regions here?
        //    //buffer_holder.add_region_type(key);
        //}

        void set_capacity_and_add_to_static_indices(Roaring& index, const uint32_t capacity, std::vector<Roaring*> static_indices) {
            // sadly the copy constructor of Roaring does not copy the value for allocation_size, so we have to do this manually...
            ra_init_with_capacity(&index.roaring.high_low_container, capacity);
            static_indices.push_back(&index);
        }

        void set_capacity_and_add_to_static_indices2(LockedRoaring& index, const uint32_t capacity, std::vector<LockedRoaring*> static_indices) {
            // sadly the copy constructor of Roaring does not copy the value for allocation_size, so we have to do this manually...
            ra_init_with_capacity(&index.roaring.high_low_container, capacity);
            static_indices.push_back(&index);
        }

        void create_and_add_present_query(const Roaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_present_query(index);
            result->queries.push_back(q);
        }

        void create_and_add_absent_query(const Roaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_absent_query(index);
            result->queries.push_back(q);
        }

        void create_and_add_all_query(const Roaring &index, embark_assist::index::query_plan *result) {
            const embark_assist::query::query_interface *q = new embark_assist::query::single_index_all_query(index);
            result->queries.push_back(q);
        }

        void create_savagery_evilness_queries(
            const embark_assist::defs::evil_savagery_values evil_savagery_values[4], const std::array<LockedRoaring, 3> &index_array, 
            const uint8_t savageryEvilnessLevel, embark_assist::index::query_plan *result) {

            const embark_assist::defs::evil_savagery_values finderValue = evil_savagery_values[savageryEvilnessLevel];
            if (finderValue != embark_assist::defs::evil_savagery_values::NA) {
                const LockedRoaring &index = index_array[savageryEvilnessLevel];
                if (finderValue == embark_assist::defs::evil_savagery_values::Present) {
                    create_and_add_present_query(index, result);
                }
                else if (finderValue == embark_assist::defs::evil_savagery_values::Absent) {
                    create_and_add_absent_query(index, result);
                }
                else if (finderValue == embark_assist::defs::evil_savagery_values::All) {
                    create_and_add_all_query(index, result);
                }
            }
        }
    }
}

embark_assist::index::Index::Index(df::world *world)
    : capacity(std::ceil(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * NUMBER_OF_EMBARK_TILES / NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL)), 
    hasAquifer(roaring_bitmap_create_with_capacity(capacity)),
    hasClay(roaring_bitmap_create_with_capacity(capacity)),
    hasCoal(roaring_bitmap_create_with_capacity(capacity)),
    hasFlux(roaring_bitmap_create_with_capacity(capacity)),
    hasRiver(roaring_bitmap_create_with_capacity(capacity)),
    hasSand(roaring_bitmap_create_with_capacity(capacity)),
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
    magma_level(4){

    this->world = world;

    maxKeyValue = world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * NUMBER_OF_EMBARK_TILES;
    embark_assist::query::abstract_query::set_world_size(maxKeyValue);

    keys_in_order.reserve(maxKeyValue);
    mapped_elevations.reserve(maxKeyValue);
    mapped_elevations.resize(maxKeyValue);

    positions.reserve(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y);

    keyMapper = new embark_assist::index::key_position_mapper::KeyPositionMapper(world->world_data->world_width, world->world_data->world_height);

    number_of_waterfall_drops.assign(0);

    static_indices.push_back(&uniqueKeys);
    static_indices.push_back(&hasCoal);
    static_indices.push_back(&hasFlux);
    static_indices.push_back(&hasRiver);
    static_indices.push_back(&no_waterfall);

    static_indices2.push_back(&hasAquifer);
    static_indices2.push_back(&hasClay);
    static_indices2.push_back(&hasSand);
    static_indices2.push_back(&is_unflat_by_incursion);

    for (auto& index : soil) {
        set_capacity_and_add_to_static_indices2(index, capacity, static_indices2);
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
        set_capacity_and_add_to_static_indices2(index, capacity, static_indices2);
    }

    for (auto& index : evilness_level) {
        set_capacity_and_add_to_static_indices2(index, capacity, static_indices2);
    }

    for (auto& index : biome) {
        set_capacity_and_add_to_static_indices2(index, capacity, static_indices2);
    }

    for (auto& index : region_type) {
        set_capacity_and_add_to_static_indices2(index, capacity, static_indices2);
    }

    //for (auto& index : mapped_elevation_index) {
    //    set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    //}

    //for (auto& index : waterfall_drops) {
    //    set_capacity_and_add_to_static_indices(index, capacity, static_indices);
    //}

    // TODO: remove, just here for debugging
    match_results.resize(world->worldgen.worldgen_parms.dim_x);
    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        match_results[i].resize(world->worldgen.worldgen_parms.dim_y);
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

void embark_assist::index::Index::add(const int16_t x, const int16_t y, const embark_assist::defs::region_tile_datum &rtd, const embark_assist::defs::mid_level_tiles *mlts, 
                                      const embark_assist::defs::key_buffer_holder_interface &buffer_holder2) {
    color_ostream_proxy out(Core::getInstance().getConsole());
    const auto innerStartTime = std::chrono::steady_clock::now();

    const uint32_t world_offset = keyMapper->key_of(x, y, 0, 0);
    if (previous_key == world_offset) {
        return;
    }
    previous_key = world_offset;
    entryCounter++;

    //positions.push_back({ x,y });

    // uint32_t mlt_offset = 0;

    /*
    // beware: we process "first" in k(y) than in the i(x) dimension, otherwise the key can't be calculated just be using a rolling offset
    // the inner loop processes one row, then the next row with a higher k/y value
    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            entryCounter++;

            const embark_assist::defs::mid_level_tile &mlt = mlts->at(i).at(k);
            const uint32_t key = world_offset + mlt_offset++;
            
            // keys_in_order.push_back(key);
            // uniqueKeys.add(key);

            if (mlt.coal) {
                buffer_holder.coalBuffer[buffer_holder.coalBufferIndex++] = key;
            }

            if (mlt.flux) {
                buffer_holder.fluxBuffer[buffer_holder.fluxBufferIndex++] = key;
            }

            if (mlt.river_present) {
                buffer_holder.riverBuffer[buffer_holder.riverBufferIndex++] = key;
            }

            if (mlt.magma_level > -1) {
                buffer_holder.magma_buffer[mlt.magma_level][buffer_holder.magma_buffer_index[mlt.magma_level]++] = key;
            }

            if (mlt.adamantine_level > -1) {
                buffer_holder.adamantine_buffer[mlt.adamantine_level][buffer_holder.adamantine_buffer_index[mlt.adamantine_level]++] = key;
            }

            //  Region Type
            // result.region_types[world->world_data->regions[rtd.biome_index[mlt.biome_offset]]->type] = true;
            // world->world_data->regions[rtd.biome_index[mlt.biome_offset]]->type;
            // regions[world->world_data->regions[rtd.biome_index[mlt.biome_offset]]->type].add(key);

            //const auto inorganicsStartTime = std::chrono::steady_clock::now();

            // create indices for metals, economics and minerals
            //const auto begin_metals = mlt.metals.cbegin();
            uint16_t index = 0;
            for (auto it = mlt.metals.cbegin(); it != mlt.metals.cend(); it++) {
                if (*it) {
                    metalBuffer[index][metalBufferIndex[index]++] = key;
                }
                index++;
            }
            

            // const auto begin_economics = mlt.economics.cbegin();
            index = 0;
            for (auto it = mlt.economics.cbegin(); it != mlt.economics.cend(); it++) {
                if (*it) {
                    economicBuffer[index][economicBufferIndex[index]++] = key;
                }
                index++;
            }

            // const auto begin_minerals = mlt.minerals.cbegin();
            index = 0;
            for (auto it = mlt.minerals.cbegin(); it != mlt.minerals.cend(); it++) {
                if (*it) {
                    const df::inorganic_raw* raw = world->raws.inorganics[index];
                    if (
                        // true || 
                        raw->environment.location.size() != 0 ||
                        raw->environment_spec.mat_index.size() != 0 ||
                        raw->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
                        raw->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
                        raw->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
                        raw->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
                        raw->flags.is_set(df::inorganic_flags::SOIL)) {

                        mineralBuffer[index][mineralBufferIndex[index]++] = key;
                    }
                }
                index++;
            }

            //const auto inorganics_processing_end = std::chrono::steady_clock::now();
            //inorganics_processing_seconds += inorganics_processing_end - inorganicsStartTime;

            // handling data that is also processed by incursions
            add_to_buffers(key, mlt, rtd, buffer_holder);
        }
    }
    */

    //const auto adding_start = std::chrono::steady_clock::now();

    uint16_t coalBufferIndex(0);
    const uint32_t *coalBuffer;
    buffer_holder2.get_coal_buffer(coalBufferIndex, coalBuffer);
    if (coalBufferIndex > 0) {
        hasCoal.addMany(coalBufferIndex, coalBuffer);
    }

    uint16_t fluxBufferIndex(0);
    const uint32_t *fluxBuffer;
    buffer_holder2.get_flux_buffer(fluxBufferIndex, fluxBuffer);
    if (fluxBufferIndex > 0) {
        hasFlux.addMany(fluxBufferIndex, fluxBuffer);
    }

    const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> * river_indices;
    const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> * river_buffers;
    buffer_holder2.get_river_size_buffers(river_indices, river_buffers);
    for (int i = 0; i < river_indices->size(); i++) {
        if (river_indices->at(i) > 0) {
            river_size[i].addMany(river_indices->at(i), river_buffers->at(i));
        }
    }

    const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> * waterfall_indices;
    const std::array<std::array<std::pair<uint32_t, uint32_t>, 480>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS > * waterfall_buffers;
    buffer_holder2.get_waterfall_drop_buffers(waterfall_indices, waterfall_buffers);
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
    buffer_holder2.get_no_waterfall_buffers(no_waterfall_index, no_waterfall_buffer);
    if (no_waterfall_index > 0) {
        no_waterfall.addMany(no_waterfall_index, no_waterfall_buffer);
    }

    const std::array<uint16_t, 4> * magma_indices;
    const std::array<uint32_t *, 4> * magma_buffers;
    buffer_holder2.get_magma_level_buffers(magma_indices, magma_buffers);
    for (int i = 0; i < magma_indices->size(); i++) {
        if (magma_indices->at(i) > 0) {
            magma_level[i].addMany(magma_indices->at(i), magma_buffers->at(i));
        }
    }

    const std::array<uint16_t, 4> * adamantine_indices;
    const std::array<uint32_t *, 4> * adamantine_buffers;
    buffer_holder2.get_adamantine_level_buffers(adamantine_indices, adamantine_buffers);
    for (int i = 0; i < adamantine_indices->size(); i++) {
        if (adamantine_indices->at(i) > 0) {
            adamantine_level[i].addMany(adamantine_indices->at(i), adamantine_buffers->at(i));
        }
    }

    const std::vector<uint16_t> * metal_buffer_indices;
    const std::vector<uint32_t *> * metal_buffers;
    buffer_holder2.get_metal_buffers(metal_buffer_indices, metal_buffers);
    for (auto metalIndexOffset : this->metal_indices) {
        if (metal_buffer_indices->at(metalIndexOffset) > 0) {
            metals[metalIndexOffset]->addMany(metal_buffer_indices->at(metalIndexOffset), metal_buffers->at(metalIndexOffset));
        }
    }

    const std::vector<uint16_t> * economic_buffer_indices;
    const std::vector<uint32_t *> * economic_buffers;
    buffer_holder2.get_economic_buffers(economic_buffer_indices, economic_buffers);
    for (auto economicIndexOffset : economic_indices) {
        if (economic_buffer_indices->at(economicIndexOffset) > 0) {
            economics[economicIndexOffset]->addMany(economic_buffer_indices->at(economicIndexOffset), economic_buffers->at(economicIndexOffset));
        }
    }

    const std::vector<uint16_t> * mineral_buffer_indices;
    const std::vector<uint32_t *> * mineral_buffers;
    buffer_holder2.get_mineral_buffers(mineral_buffer_indices, mineral_buffers);
    for (auto mineralIndexOffset : mineral_indices) {
        if (mineral_buffer_indices->at(mineralIndexOffset) > 0) {
            minerals[mineralIndexOffset]->addMany(mineral_buffer_indices->at(mineralIndexOffset), mineral_buffers->at(mineralIndexOffset));
        }
    }

    //const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_ELEVATION_INDICES> *mapped_elevation_indices;
    //const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_ELEVATION_INDICES> *mapped_elevation_buffers;
    //buffer_holder2.get_mapped_elevation_buffers(mapped_elevation_indices, mapped_elevation_buffers);
    //for (int i = 0; i < embark_assist::defs::ARRAY_SIZE_FOR_ELEVATION_INDICES; i++) {
    //    if (mapped_elevation_indices->at(i) > 0) {
    //        mapped_elevation_index[i].addMany(mapped_elevation_indices->at(i), mapped_elevation_buffers->at(i));
    //    }
    //}

    uint16_t elevationIndex(0);
    const uint8_t *elevation_buffer;
    uint32_t initial_offset;
    buffer_holder2.get_mapped_elevation_buffer(elevationIndex, elevation_buffer, initial_offset);
    if (elevationIndex > 0) {
        std::copy(elevation_buffer, elevation_buffer + elevationIndex, &mapped_elevations[initial_offset]);
       //memcpy(&mapped_elevations.at(initial_offset), elevation_buffer, elevationIndex);
    }

    // add all buffers that contain data that is also handled by the incursions to the indices
    this->add(buffer_holder2);
    
    //const auto adding_end = std::chrono::steady_clock::now();
    //index_adding_seconds += adding_end - adding_start;

    // sync with incursion task, wait block till it finishes
    //incursion_processing_result.get();

    // now we may optimize, since all data is collected for this world tile - but do so only after every feature shell (= 16*16 world tiles)
    // ATTENTION: if the line incursion_processing_result.get(); is moved after the optimize call locking must be implemented otherwise race conditions will happen!
    const uint32_t last_key = world_offset + NUMBER_OF_EMBARK_TILES - 1;
    if ((last_key - feature_set_counter) % (NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL * (feature_set_counter + 1)) == 0) {
        if (!was_optimized_in_current_feature_shell) {
            this->optimize(false);
            feature_set_counter ++;
            // out.print("optimizing after key %d\n ", last_key);
            was_optimized_in_current_feature_shell = true;
        }
    }
    else if (was_optimized_in_current_feature_shell) {
        was_optimized_in_current_feature_shell = false;
    }

    const auto innerEnd = std::chrono::steady_clock::now();
    innerElapsed_seconds += innerEnd - innerStartTime;
}

void embark_assist::index::Index::add(const embark_assist::defs::key_buffer_holder_basic_interface &buffer_holder) {
    // FIXME: time/profile this method, especially the parts that loop over arrays

    uint16_t unflatBufferIndex(0);
    const uint32_t *unflatBuffer;
    buffer_holder.get_unflat_buffer(unflatBufferIndex, unflatBuffer);
    if (unflatBufferIndex > 0) {
        is_unflat_by_incursion.addManyGuarded(unflatBufferIndex, unflatBuffer);
    }

    uint16_t aquiferBufferIndex(0);
    const uint32_t *aquiferBuffer;
    buffer_holder.get_aquifer_buffer(aquiferBufferIndex, aquiferBuffer);
    if (aquiferBufferIndex > 0) {
        //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_aquifer_m);
        hasAquifer.addManyGuarded(aquiferBufferIndex, aquiferBuffer);
    }

    uint16_t clayBufferIndex(0);
    const uint32_t *clayBuffer;
    buffer_holder.get_clay_buffer(clayBufferIndex, clayBuffer);
    if (clayBufferIndex > 0) {
        //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_clay_m);
        hasClay.addManyGuarded(clayBufferIndex, clayBuffer);
    }

    uint16_t sandBufferIndex(0);
    const uint32_t *sandBuffer;
    buffer_holder.get_sand_buffer(sandBufferIndex, sandBuffer);
    if (sandBufferIndex > 0) {
        //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_sand_m);
        hasSand.addManyGuarded(sandBufferIndex, sandBuffer);
    }

    const std::array<uint16_t, embark_assist::defs::SOIL_DEPTH_LEVELS> *indices;
    const std::array<uint32_t *, embark_assist::defs::SOIL_DEPTH_LEVELS> *buffers;
    buffer_holder.get_soil_depth_buffers(indices, buffers);
    for (int i = 0; i < embark_assist::defs::SOIL_DEPTH_LEVELS; i++) {
        if (indices->at(i) > 0) {
            //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_soil_ms[i]);
            soil[i].addManyGuarded(indices->at(i), buffers->at(i));
        }
    }

    const std::array<uint16_t, 3> *savagery_indices;
    const std::array<uint32_t *, 3> *savagery_buffers;
    buffer_holder.get_savagery_level_buffers(savagery_indices, savagery_buffers);
    for (int i = 0; i < savagery_level.size(); i++) {
        if (savagery_indices->at(i) > 0) {
            //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_savagery_ms[i]);
            savagery_level[i].addManyGuarded(savagery_indices->at(i), savagery_buffers->at(i));
        }
    }

    const std::array<uint16_t, 3> *evilness_indices;
    const std::array<uint32_t *, 3> *evilness_buffers;
    buffer_holder.get_evilness_level_buffers(evilness_indices, evilness_buffers);
    for (int i = 0; i < evilness_level.size(); i++) {
        if (evilness_indices->at(i) > 0) {
            //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_evilness_ms[i]);
            evilness_level[i].addManyGuarded(evilness_indices->at(i), evilness_buffers->at(i));
        }
    }

    const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> *biome_indices;
    const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> *biome_buffers;
    buffer_holder.get_biome_buffers(biome_indices, biome_buffers);
    for (int i = 0; i < biome.size(); i++) {
        if (biome_indices->at(i) > 0) {
            //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_biomes_ms[i]);
            biome[i].addManyGuarded(biome_indices->at(i), biome_buffers->at(i));
        }
    }

    const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> *region_type_indices;
    const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> *region_type_buffers;
    buffer_holder.get_region_type_buffers(region_type_indices, region_type_buffers);
    for (int i = 0; i < region_type.size(); i++) {
        if (region_type_indices->at(i) > 0) {
            //const std::lock_guard<std::mutex> add_many_mutex_guard(add_many_regions_ms[i]);
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
        optimize(*it, debugOutput);
    }

    for (auto it = static_indices2.begin(); it != static_indices2.end(); it++) {
        (*it)->runOptimizeAndShrinkToFitGuarded(debugOutput);
    }

    for (auto it = metals.begin(); it != metals.end(); it++) {
        if (*it != nullptr) {
            optimize(*it, debugOutput);
        }
    }

    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            optimize(*it, debugOutput);
        }
    }

    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            optimize(*it, debugOutput);
        }
    }

    if (debugOutput) {
        this->outputSizes("after optimize");
        this->outputContents();

        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("### embark_assist::index::Index::add inorganics processing took %f seconds ###\n", inorganics_processing_seconds.count());
        out.print("### embark_assist::index::Index::add only index adding took %f seconds ###\n", index_adding_seconds.count());
        out.print("### embark_assist::index::Index::add buffer and index adding took total %f seconds ###\n", innerElapsed_seconds.count());
        // std::cout << "### actual adding took total " << innerElapsed_seconds.count() << " seconds ###" << std::endl;
        innerElapsed_seconds = std::chrono::seconds(0);
        index_adding_seconds = std::chrono::seconds(0);
    }
}

void embark_assist::index::Index::shutdown() {
    embark_assist::inorganics::inorganics_information::reset();
    world = nullptr;
    feature_set_counter = 0;
    entryCounter = 0;
    max_inorganic = 0;
    maxKeyValue = 0;

    for (auto it = static_indices.begin(); it != static_indices.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            //delete *it;
        }
    }

    for (auto it = static_indices2.begin(); it != static_indices2.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            //delete *it;
        }
    }

    for (auto it = metals.begin(); it != metals.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    metals.clear();
    metals.resize(0);
    metals.reserve(0);
    
    for (auto it = economics.begin(); it != economics.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    economics.clear();
    economics.resize(0);
    economics.reserve(0);
    
    for (auto it = minerals.begin(); it != minerals.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    minerals.clear();
    minerals.resize(0);
    minerals.reserve(0);

    inorganics.clear();
    inorganics.resize(0);
    inorganics.reserve(0);

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
    this->writeIndexToDisk(hasAquifer, std::to_string(index_prefix++) + "_hasAquifier");
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

    //level_post_fix = 0;
    //total_number_of_entries = 0;
    //for (auto& index : mapped_elevation_index) {
    //    total_number_of_entries += index.cardinality();
    //    fprintf(outfile, "number of mapped_elevation #%d entries: %I64d\n", level_post_fix, index.cardinality());
    //    this->writeIndexToDisk(index, std::to_string(index_prefix++) + "_mapped_elevation_" + std::to_string(level_post_fix++));
    //}
    fprintf(outfile, "total number of mapped_elevations entries: %I64d\n", mapped_elevations.size());

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
    // get_position(position_id, x, y, i, k);
    keyMapper->get_position(position_id, x, y, i, k);

    // if the previously inspected world tile is not the current one, we reset the tracker
    if (start_tile_tracker.x != x || start_tile_tracker.y != y) {
        start_tile_tracker.x = x;
        start_tile_tracker.y = y;
        for (uint8_t i = 0; i < 16; i++) {
            //std::fill(&(start_tile_tracker.was_already_used_as_start_position[i][0]), &(start_tile_tracker.was_already_used_as_start_position[i][15]), false);
            for (uint8_t k = 0; k < 16; k++) {
                start_tile_tracker.was_already_used_as_start_position[i][k] = false;
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
    //uint32_t start_position_id = position_id - ((i - start_i_offset) + (k - start_k_offset) * 16);

    uint16_t embark_counter = 0;
    uint16_t buffer_position = 0;
    for (uint16_t current_k_offset = start_k_offset; current_k_offset + embark_size_y - 1 <= end_k_offset; current_k_offset++) {
        for (uint16_t current_i_offset = start_i_offset; current_i_offset + embark_size_x - 1 <= end_i_offset; current_i_offset++) {
            // if the current tile was already used as start tile for an embark candidate/variant we skip it
            if (start_tile_tracker.was_already_used_as_start_position[current_i_offset][current_k_offset]) {
                continue;
            }
            //start_position_id = position_id - ((i - current_i_offset) + (k - current_k_offset) * 16);
            buffer_position = 0;
            for (uint16_t current_k = current_k_offset; current_k < current_k_offset + embark_size_y; current_k++) {
                const uint32_t current_position_id_region_and_k_offset = position_id_region_offset + current_k * 16;

                // following line should replace the inner loop completly => FIXME verify/test this
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
const std::vector<uint32_t>* embark_assist::index::Index::get_keys(const Roaring &index) const {
    uint32_t* most_significant_ids = new uint32_t[index.cardinality()];
    index.toUint32Array(most_significant_ids);
    const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + index.cardinality());

    delete most_significant_ids;
    return dest;
}

void calculate_elapsed(const chrono::time_point<std::chrono::steady_clock> start, std::chrono::duration<double> &total) {
    total += std::chrono::steady_clock::now() - start;
}

// FIXME: move this into own class => query_plan_factory
const embark_assist::index::query_plan_interface* embark_assist::index::Index::create_query_plan(const embark_assist::defs::finders &finder) const {
    const Index &scope = *this;
    
    embark_assist::index::query_plan *result = new embark_assist::index::query_plan();

    if (finder.aquifer == embark_assist::defs::aquifer_ranges::Present) {
        // TODO: be aware, that there are special (corner) cases if a criteria makes assumptions about more than one tile e.g. "All", "Absent", "Partial"
        // - if this query is the most significant one we need another query as helper to verify that all the other tiles also (not) have a aquifer... 
        // This is true for all criteria with exclusive/absolute (all/none, absent, ...) meaning
        // actually it is fine to keep the query in the case of "all" and "absent" by 
        // q->flag_for_keeping();
        // which will allow the query plan to use the query again which will make sure all embark candiates have or haven't an aquifer...
        const Roaring &hasAquifer = this->hasAquifer;
        const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&hasAquifer](const Roaring &embark_candidate) -> bool {
            // std::cout << "hasAquifer.and_cardinality" << std::endl;
            return hasAquifer.and_cardinality(embark_candidate) > 0;
        }, [&hasAquifer]() -> uint32_t {
            // std::cout << "hasAquifer.cardinality" << std::endl;
            return hasAquifer.cardinality();
        }, [&hasAquifer, &scope]() -> const std::vector<uint32_t>* {
            return scope.get_keys(hasAquifer);
        });
        result->queries.push_back(q);
    }

    //if (finder.sand == embark_assist::defs::present_absent_ranges::Present) {
    //    const Roaring &hasSand = this->hasSand;
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&hasSand](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "hasSand.and_cardinality" << std::endl;
    //        return hasSand.and_cardinality(embark_candidate) > 0;
    //    }, [&hasSand]() -> uint32_t {
    //        // std::cout << "hasSand.cardinality" << std::endl;
    //        return hasSand.cardinality();
    //    }, [&hasSand, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(hasSand);
    //    });
    //    result->queries.push_back(q);
    //}

    //if (finder.clay == embark_assist::defs::present_absent_ranges::Present) {
    //    const Roaring &hasClay = this->hasClay;
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&hasClay](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "hasClay.and_cardinality" << std::endl;
    //        return hasClay.and_cardinality(embark_candidate) > 0;
    //    }, [&hasClay]() -> uint32_t {
    //        // std::cout << "hasClay.cardinality" << std::endl;
    //        return hasClay.cardinality();
    //    }, [&hasClay, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(hasClay);
    //    });
    //    result->queries.push_back(q);
    //}

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

    if (finder.aquifer == embark_assist::defs::aquifer_ranges::Present) {
        create_and_add_present_query(hasAquifer, result);
    } 
    else if (finder.aquifer == embark_assist::defs::aquifer_ranges::Absent) {
        create_and_add_absent_query(hasAquifer, result);
    } 
    else if (finder.aquifer == embark_assist::defs::aquifer_ranges::All) {
        create_and_add_all_query(hasAquifer, result);
    }

    if (finder.clay == embark_assist::defs::present_absent_ranges::Present) {
        create_and_add_present_query(hasClay, result);
    }
    else if (finder.clay == embark_assist::defs::present_absent_ranges::Absent) {
        create_and_add_absent_query(hasClay, result);
    }

    if (finder.sand == embark_assist::defs::present_absent_ranges::Present) {
        create_and_add_present_query(hasSand, result);
    }
    else if (finder.sand == embark_assist::defs::present_absent_ranges::Absent) {
        create_and_add_absent_query(hasSand, result);
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

    if (finder.region_type_1 != -1) {
        create_and_add_present_query(this->region_type[finder.region_type_1], result);
    }

    if (finder.region_type_2 != -1) {
        create_and_add_present_query(this->region_type[finder.region_type_2], result);
    }

    if (finder.region_type_3 != -1) {
        create_and_add_present_query(this->region_type[finder.region_type_3], result);
    }

    if (finder.metal_1 != -1) {
        create_and_add_present_query(*metals[finder.metal_1], result);
    }

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

    //if (finder.biome_1 != -1) {
    //    const Roaring &biome = this->biome[finder.biome_1];
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&biome](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "biome.and_cardinality" << std::endl;
    //        return biome.and_cardinality(embark_candidate) > 0;
    //    }, [&biome]() -> uint32_t {
    //        // std::cout << "biome.cardinality" << std::endl;
    //        return biome.cardinality();
    //    }, [&biome, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(biome);
    //    });
    //    result->queries.push_back(q);
    //}

    //if (finder.region_type_1 != -1) {
    //    const Roaring &region_type = this->region_type[finder.region_type_1];
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&region_type](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "region_type.and_cardinality" << std::endl;
    //        return region_type.and_cardinality(embark_candidate) > 0;
    //    }, [&region_type]() -> uint32_t {
    //        // std::cout << "region_type.cardinality" << std::endl;
    //        return region_type.cardinality();
    //    }, [&region_type, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(region_type);
    //    });
    //    result->queries.push_back(q);
    //}

    //if (finder.magma_min == embark_assist::defs::magma_ranges::Volcano) {
    //    const uint64_t i = static_cast<uint64_t>(embark_assist::defs::magma_ranges::Volcano);
    //    const Roaring &magma_level = this->magma_level[i];
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&magma_level](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "magma_level.and_cardinality" << std::endl;
    //        return magma_level.and_cardinality(embark_candidate) > 0;
    //    }, [&magma_level]() -> uint32_t {
    //        // std::cout << "magma_level.cardinality" << std::endl;
    //        return magma_level.cardinality();
    //    }, [&magma_level, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(magma_level);
    //    });
    //    result->queries.push_back(q);
    //}

    if (finder.magma_min != embark_assist::defs::magma_ranges::NA || finder.magma_max != embark_assist::defs::magma_ranges::NA) {
        std::vector<Roaring>::const_iterator min = magma_level.cbegin();
        if (finder.magma_min != embark_assist::defs::magma_ranges::NA) {
            const int8_t magma_min = static_cast<uint64_t>(finder.magma_min);
            std::advance(min, magma_min);
        }

        std::vector<Roaring>::const_iterator max = magma_level.end();
        if (finder.magma_max != embark_assist::defs::magma_ranges::NA) {
            const int8_t magma_max = static_cast<uint64_t>(finder.magma_max);
            max = magma_level.cbegin();
            std::advance(max, magma_max + 1);
        }

        if (max > min) {
            const embark_assist::query::query_interface *q = new embark_assist::query::multiple_index_min_max_in_range_query(
                embark_assist::query::multiple_indices_query_context(magma_level, min, max));
            result->queries.push_back(q);
        }
    }

    //if (finder.evilness[static_cast<uint8_t>(embark_assist::defs::evil_savagery_ranges::Medium)] != embark_assist::defs::evil_savagery_values::Present) {
    //    const uint64_t i = static_cast<uint64_t>(embark_assist::defs::evil_savagery_ranges::Medium);
    //    const Roaring &evilness_level = this->evilness_level[i];
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&evilness_level](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "evilness_level.and_cardinality" << std::endl;
    //        return evilness_level.and_cardinality(embark_candidate) > 0;
    //    }, [&evilness_level]() -> uint32_t {
    //        // std::cout << "evilness_level.cardinality" << std::endl;
    //        return evilness_level.cardinality();
    //    }, [&evilness_level, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(evilness_level);
    //    });
    //    result->queries.push_back(q);
    //}

    //if (finder.metal_1 != -1) {
    //    const Roaring &metal = *this->metals[finder.metal_1];
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&metal](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "metal.and_cardinality" << std::endl;
    //        return metal.and_cardinality(embark_candidate) > 0;
    //    }, [&metal]() -> uint32_t {
    //        // std::cout << "metal.cardinality" << std::endl;
    //        return metal.cardinality();
    //    }, [&metal, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(metal);
    //    });
    //    result->queries.push_back(q);
    //}

    //if (finder.metal_2 != -1) {
    //    const Roaring &metal = *this->metals[finder.metal_2];
    //    const embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&metal](const Roaring &embark_candidate) -> bool {
    //        // std::cout << "metal.and_cardinality" << std::endl;
    //        return metal.and_cardinality(embark_candidate) > 0;
    //    }, [&metal]() -> uint32_t {
    //        // std::cout << "metal.cardinality" << std::endl;
    //        return metal.cardinality();
    //    }, [&metal, &scope]() -> const std::vector<uint32_t>* {
    //        return scope.get_keys(metal);
    //    });
    //    result->queries.push_back(q);
    //}

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
        });
        q->flag_for_keeping();
        result->queries.push_back(q);
    }

    // indices based variant of is flat query
    //if (this->run_all_vector_query && finder.flat == embark_assist::defs::yes_no_ranges::Yes) {
    //    color_ostream_proxy out(Core::getInstance().getConsole());
    //    out.print("running indices based elevation/flat query");

    //    const LockedRoaring &is_unflat_by_incursion = this->is_unflat_by_incursion;
    //    const std::array<Roaring, embark_assist::defs::ARRAY_SIZE_FOR_ELEVATION_INDICES> &mapped_elevation_index = this->mapped_elevation_index;
    //    std::chrono::duration<double> &all_indices_elevation_query_seconds = (const_cast<Index*>(this))->all_indices_elevation_query_seconds;
    //    embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&is_unflat_by_incursion, &mapped_elevation_index, &all_indices_elevation_query_seconds](const Roaring &embark_candidate) -> bool {
    //        const auto innerStart = std::chrono::steady_clock::now();
    //        if (is_unflat_by_incursion.intersect(embark_candidate)) {
    //            calculate_elapsed(innerStart, all_indices_elevation_query_seconds);
    //            return false;
    //        }
    //        const uint32_t cardinality = embark_candidate.cardinality();
    //        if (cardinality == 1) {
    //            calculate_elapsed(innerStart, all_indices_elevation_query_seconds);
    //            return true;
    //        }

    //        for (auto index : mapped_elevation_index) {
    //            const uint32_t and_cardinality = index.and_cardinality(embark_candidate);
    //            if (and_cardinality > 0) {
    //                if (and_cardinality == cardinality) {
    //                    calculate_elapsed(innerStart, all_indices_elevation_query_seconds);
    //                    return true;
    //                }
    //                else {
    //                    calculate_elapsed(innerStart, all_indices_elevation_query_seconds);
    //                    return false;
    //                }
    //            }
    //        }
    //        // this should be unreachable - unless there is no elevation for the embark_cadidate at all
    //        return true;
    //    }, [&is_unflat_by_incursion]() -> uint32_t {
    //        return embark_assist::query::single_index_query::get_inverted_cardinality(is_unflat_by_incursion);
    //    }, [&is_unflat_by_incursion]() -> const std::vector<uint32_t>* {
    //        return embark_assist::query::single_index_query::get_inverted_keys(is_unflat_by_incursion);
    //    });
    //    q->flag_for_keeping();
    //    result->queries.push_back(q);
    //}
    //else if (finder.flat == embark_assist::defs::yes_no_ranges::No) {
    //    const LockedRoaring &is_unflat_by_incursion = this->is_unflat_by_incursion;
    //    const std::array<Roaring, embark_assist::defs::ARRAY_SIZE_FOR_ELEVATION_INDICES> &mapped_elevation_index = this->mapped_elevation_index;
    //    embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&is_unflat_by_incursion, &mapped_elevation_index](const Roaring &embark_candidate) -> bool {
    //        if (is_unflat_by_incursion.intersect(embark_candidate)) {
    //            return true;
    //        }
    //        const uint32_t cardinality = embark_candidate.cardinality();
    //        if (cardinality == 1) {
    //            return false;
    //        }

    //        for (auto index : mapped_elevation_index) {
    //            const uint32_t and_cardinality = index.and_cardinality(embark_candidate);
    //            if (and_cardinality > 0) {
    //                if (and_cardinality != cardinality) {
    //                    return true;
    //                }
    //                else {
    //                    return false;
    //                }
    //            }
    //        }
    //        // this should be unreachable - unless there is no elevation for the embark_cadidate at all
    //        return true;
    //    }, []() -> uint32_t {
    //        return embark_assist::query::abstract_query::get_world_size();
    //    }, []() -> const std::vector<uint32_t>* {
    //        // FIXME: this is very bad, 64MB RAM bad for a 257x257 region => change return type of method to iterator and implement differnt iterators
    //        return embark_assist::query::abstract_query::get_world_keys();
    //    });
    //    q->flag_for_keeping();
    //    result->queries.push_back(q);
    //}

    // vector based variant of is flat query
    if (finder.flat == embark_assist::defs::yes_no_ranges::Yes) {
        color_ostream_proxy out(Core::getInstance().getConsole());
        out.print("running vector based elevation/flat query");

        const LockedRoaring &is_unflat_by_incursion = this->is_unflat_by_incursion;
        const std::vector<uint8_t> &mapped_elevations = this->mapped_elevations;
        std::chrono::duration<double> &vector_elevation_query_seconds = (const_cast<Index*>(this))->vector_elevation_query_seconds;
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&is_unflat_by_incursion, &mapped_elevations, &vector_elevation_query_seconds](const Roaring &embark_candidate) -> bool {
            const auto innerStart = std::chrono::steady_clock::now();
            if (is_unflat_by_incursion.intersect(embark_candidate)) {
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
        });
        q->flag_for_keeping();
        result->queries.push_back(q);
    }
    else if (finder.flat == embark_assist::defs::yes_no_ranges::No) {
        const LockedRoaring &is_unflat_by_incursion = this->is_unflat_by_incursion;
        const std::vector<uint8_t> mapped_elevations = this->mapped_elevations;
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&is_unflat_by_incursion, &mapped_elevations](const Roaring &embark_candidate) -> bool {
            if (is_unflat_by_incursion.intersect(embark_candidate)) {
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
            // FIXME: this is very bad, 64MB RAM bad for a 257x257 region => change return type of method to iterator and implement differnt iterators
            return embark_assist::query::abstract_query::get_world_keys();
        });
        q->flag_for_keeping();
        result->queries.push_back(q);
    }

    // move this into the class query_plan to allow for resort during survey iteration phase
    std::sort(result->queries.begin(), result->queries.end(), [](const embark_assist::query::query_interface* a, const embark_assist::query::query_interface* b) -> bool {
        return a->get_number_of_entries() < b->get_number_of_entries();
    });

    if (!result->queries.empty()) {
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

void output_embark_matches(std::ofstream &myfile, const embark_assist::defs::matches &match) {
    for (uint16_t i = 0; i < 16; i++) {
        for (uint16_t k = 0; k < 16; k++) {
            if (match.mlt_match[i][k]) {
                myfile << " i:" << i << "/k:" << k;
            }
        }
    }
}

// TODO: remove, just here for debugging
void compare_matches(df::world *world, const embark_assist::defs::match_results &match_results_matcher, const embark_assist::defs::match_results &match_results_index) {
    const std::string prefix = "match_result_delta";
    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);
    
    for (uint16_t x = 0; x < world->worldgen.worldgen_parms.dim_x; x++) {
        for (uint16_t y = 0; y < world->worldgen.worldgen_parms.dim_y; y++) {
            if(match_results_matcher[x][y].contains_match != match_results_index[x][y].contains_match) {
                myfile << "\nx:" << x << "/y:" << y;
                if (match_results_matcher[x][y].contains_match) {
                    myfile << " unique world tile matcher result\n";
                    output_embark_matches(myfile, match_results_matcher[x][y]);
                }
                else {
                    myfile << " unique world tile index result\n";
                    output_embark_matches(myfile, match_results_index[x][y]);
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
                            myfile << " i:" << i << "/k:" << k;
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

void embark_assist::index::Index::find(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results_matcher) const {

    // TODO: remove, just here for debugging
    embark_assist::survey::clear_results(&const_cast<Index*>(this)->match_results);

    //embark_assist::defs::match_results &match_results = const_cast<Index*>(this)->match_results;
    embark_assist::defs::match_results &match_results = match_results_matcher;

    color_ostream_proxy out(Core::getInstance().getConsole());
    const auto innerStartTime = std::chrono::steady_clock::now();
    const std::time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    out.print("embark_assist::index::Index::find: started index query search at %s\n", std::ctime(&start_time));

    const uint16_t embark_size_x = finder.x_dim;
    const uint16_t embark_size_y = finder.y_dim;

    const uint16_t embark_size = embark_size_x * embark_size_y;
    embark_assist::query::abstract_query::set_embark_size(embark_size);

    const size_t size_factor_x = 16 - embark_size_x + 1;
    const size_t size_factor_y = 16 - embark_size_y + 1;

    size_t max_number_of_embark_variants = size_factor_x * size_factor_y;
    // from a certain embark size (x,y = 9) the region size (16x16) is the limiting factor for how many different embark variants there can be
    // the smaller number is the correct one in any case
    max_number_of_embark_variants = std::min(max_number_of_embark_variants, (size_t) embark_size);

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
    const query_plan_interface *query_plan = create_query_plan(finder);
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
                //    out.print("embark_assist::index::Index::find: found same match as matcher:");
                //}
                //else {
                    //out.print("embark_assist::index::Index::find: found match matcher has not found:");
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
    delete query_plan;
    
    const auto innerEnd = std::chrono::steady_clock::now();
    const std::time_t end_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::chrono::duration<double> elapsed_seconds = innerEnd - innerStartTime;
    out.print("embark_assist::index::Index::find: finished index query search at %s with elapsed time: %f seconds with %d iterations and %d matches in %d world tiles\n", std::ctime(&end_time), elapsed_seconds.count(), number_of_iterations, number_of_matches, number_of_matched_worldtiles);
    out.print("embark_assist::index::Index::find: took %f seconds to create query plan, took %f seconds total to calculate embark variants, took %f seconds total to execute query plan \n", queryPlanCreatedElapsed.count(), embarkVariantsCreated.count(), queryElapsed.count());
    
    //out.print("### embark_assist::index::Index::all indices elevation query total took %f seconds ###\n", all_indices_elevation_query_seconds.count());
    out.print("### embark_assist::index::Index::vector elevation query total took %f seconds ###\n", vector_elevation_query_seconds.count());

    //(const_cast<Index*>(this))->all_indices_elevation_query_seconds = std::chrono::seconds(0);
    (const_cast<Index*>(this))->vector_elevation_query_seconds = std::chrono::seconds(0);
    //(const_cast<Index*>(this))->run_all_vector_query = !this->run_all_vector_query;

    // TODO: remove, just here for debugging
    compare_matches(world, match_results_matcher, match_results);
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
    fprintf(outfile, "hasAquifer.roaring.high_low_container.size: %d\n", hasAquifer.roaring.high_low_container.size);
    fprintf(outfile, "hasAquifer.roaring.high_low_container.allocation_size: %d\n", hasAquifer.roaring.high_low_container.allocation_size);
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

    //level_post_fix = 0;
    //for (auto& index : mapped_elevation_index) {
    //    byteSize += index.getSizeInBytes();
    //    fprintf(outfile, "mapped_elevation_index#%d bytesize: %zd\n", level_post_fix++, index.getSizeInBytes());
    //}

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
                Roaring* inorganicIndex = new Roaring(rr);
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
                Roaring* inorganicIndex = new Roaring(rr);
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
                Roaring* inorganicIndex = new Roaring(rr);
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
void embark_assist::index::Index::writeIndexToDisk(const Roaring& roaring, const std::string prefix) const {
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
