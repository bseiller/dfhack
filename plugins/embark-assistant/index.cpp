
#define NOMINMAX

#include <stdio.h>
#include "Core.h"
#include <Console.h>

// #include "ColorText.h"
#include "DataDefs.h"
#include "df/inorganic_raw.h"
#include "df/world.h"
#include "df/world_data.h"
#include "df/world_region.h"
#include "df/world_region_details.h"
#include "df/world_region_feature.h"
#include "df/world_region_type.h"

#include "defs.h"
#include "index.h"
#include "query.h"

using namespace DFHack;

namespace embark_assist {
    namespace index {
        class query_plan : public query_plan_interface {
        public:
            std::vector<embark_assist::query::query_interface*> queries;

            const std::vector<uint32_t>& get_most_significant_ids() const {
                return *most_significant_ids;
            }

            void set_most_significant_ids(const std::vector<uint32_t>* ids) {
                most_significant_ids = ids;
            }

            bool execute(const Roaring &embark_candidates) const {
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

        private: 
            const std::vector<uint32_t>* most_significant_ids;
        };
    }
}


embark_assist::index::Index::Index(void) /*: world(), capacity(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * 256 / NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL), 
    hasAquifer(roaring_bitmap_create_with_capacity(capacity)), hasClay(roaring_bitmap_create_with_capacity(capacity)) */ {
    static_indices.push_back(&uniqueKeys);
    static_indices.push_back(&hasAquifer);
    static_indices.push_back(&hasClay);
    static_indices.push_back(&hasCoal);
    static_indices.push_back(&hasFlux);
    static_indices.push_back(&hasRiver);
    static_indices.push_back(&hasSand);

    for (auto& index : soil) {
        static_indices.push_back(&index);
    }

    for (auto& index : river_size) {
        static_indices.push_back(&index);
    }

    for (auto& index : magma_level) {
        static_indices.push_back(&index);
    }

    for (auto& index : adamantine_level) {
        static_indices.push_back(&index);
    }

    for (auto& index : savagery_level) {
        static_indices.push_back(&index);
    }

    for (auto& index : evilness_level) {
        static_indices.push_back(&index);
    }
}

embark_assist::index::Index::Index(df::world *world): Index() /*capacity(std::ceil(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * 256 / NUMBER_OF_EMBARK_TILES_IN_FEATURE_SHELL))*/ {
    const int x = 0;
}

embark_assist::index::Index::~Index() {
    this->world = nullptr;
}

void embark_assist::index::Index::setup(df::world *world, const uint16_t max_inorganic) {
    this->world = world;
    this->max_inorganic = max_inorganic;
    maxKeyValue = world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y * 16 * 16;
    init_inorganic_index();
    initInorganicNames();
    keys_in_order.reserve(maxKeyValue);
    keys_in_order2.reserve(maxKeyValue);
    positions.reserve(world->worldgen.worldgen_parms.dim_x * world->worldgen.worldgen_parms.dim_y);

    world_last_x = world->world_data->world_width - 1;
    world_last_y = world->world_data->world_height - 1;

    x_tail = world->world_data->world_width % 16;
    y_tail = world->world_data->world_height % 16;
    total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width = world->world_data->world_width * 256 * 16;

    // FIXME: use Roaring* for all static indices, so that we can construct them like this
    // hasAquifer = new Roaring(roaring_bitmap_create_with_capacity(260));
    for (auto *index : static_indices) {
        // index->roaring = *roaring_bitmap_create_with_capacity(260);
    }
}

const uint32_t embark_assist::index::Index::createKey(int16_t x, int16_t y, uint8_t i, uint8_t k) const {
    return y * world->worldgen.worldgen_parms.dim_x * numberEmbarkTiles + (x * numberEmbarkTiles) + (k * 16) + i;
}

void embark_assist::index::Index::add(const int16_t x, const int16_t y, const embark_assist::defs::region_tile_datum &rtd, const embark_assist::defs::mid_level_tiles *mlts) {
    color_ostream_proxy out(Core::getInstance().getConsole());

    const auto innerStartTime = std::chrono::system_clock::now();

    //const uint32_t world_offset = this->createKey(x, y, 0, 0);
    const uint32_t world_offset2 = this->key_of2(x, y, 0, 0);
    uint32_t mlt_offset = 0;
    
    if (previous_key == world_offset2) {
        out.print("skipping key %d as the position was just processed\n ", previous_key);
        return;
    }
    previous_key = world_offset2;

    positions.push_back({ x,y });

    uint16_t aquifierBufferIndex = 0;
    uint16_t clayBufferIndex = 0;
    uint16_t coalBufferIndex = 0;
    uint16_t fluxBufferIndex = 0;
    uint16_t riverBufferIndex = 0;
    uint16_t sandBufferIndex = 0;

    metalBufferIndex.assign(max_inorganic, 0);
    economicBufferIndex.assign(max_inorganic, 0);
    mineralBufferIndex.assign(max_inorganic, 0);

    uint32_t last_key = 0;

    // beware: we process "first" in k(y) than in the i(x) dimension, otherwise the key can't be calculated just be using a rolling offset
    // the inner loop processes one row, then the next row with a higher k/y value
    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            entryCounter++;

            const embark_assist::defs::mid_level_tile &mlt = mlts->at(i).at(k);
            //const uint32_t key = world_offset + mlt_offset;
            // currently using the proper key following the iteration pattern over the world tiles
            // FIXME: also implement the related version of key_of2
            const uint32_t key = world_offset2 + mlt_offset;
            last_key = key;
            keys_in_order.push_back(key);

            const uint32_t key2 = world_offset2 + mlt_offset;
            keys_in_order2.push_back(key2);

            mlt_offset++;

            // uniqueKeys.add(key);

            if (mlt.aquifer) {
                aquiferBuffer[aquifierBufferIndex++] = key;
                //hasAquifer.add(key);
            }

            if (mlt.clay) {
                clayBuffer[clayBufferIndex++] = key;
                // hasClay.add(key);
            }

            if (mlt.coal) {
                coalBuffer[coalBufferIndex++] = key;
                // hasCoal.add(key);
            }

            if (mlt.flux) {
                fluxBuffer[fluxBufferIndex++] = key;
                //hasFlux.add(key);
            }

            if (mlt.river_present) {
                riverBuffer[riverBufferIndex++] = key;
                // hasRiver.add(key);
            }

            if (mlt.sand) {
                sandBuffer[sandBufferIndex++] = key;
                //hasSand.add(key);
            }

            if (mlt.magma_level > -1) {
                magma_level[mlt.magma_level].add(key);
            }

            if (mlt.adamantine_level > -1) {
                adamantine_level[mlt.adamantine_level].add(key);
            }

            soil[mlt.soil_depth].add(key);

            savagery_level[mlt.savagery_level].add(key);
            evilness_level[mlt.evilness_level].add(key);

            //  Biomes
            // result.biomes[rtd.biome[mlt.biome_offset]] = true;
            // rtd.biome[mlt.biome_offset];
            // biomes[rtd.biome[mlt.biome_offset]].add(key);

            //  Region Type
            // result.region_types[world->world_data->regions[rtd.biome_index[mlt.biome_offset]]->type] = true;
            // world->world_data->regions[rtd.biome_index[mlt.biome_offset]]->type;
            // regions[world->world_data->regions[rtd.biome_index[mlt.biome_offset]]->type].add(key);

            // create indexes for metals, economics and minerals
            //const auto begin_metals = mlt.metals.cbegin();
            uint16_t index = 0;
            for (auto it = mlt.metals.cbegin(); it != mlt.metals.cend(); it++) {
                if (*it) {
                    //const int metalIndex = std::distance(begin_metals, it);
                    //metals[index]->add(key);
                    metalBuffer[index][metalBufferIndex[index]++] = key;
                }
                index++;
            }

            // const auto begin_economics = mlt.economics.cbegin();
            index = 0;
            for (auto it = mlt.economics.cbegin(); it != mlt.economics.cend(); it++) {
                if (*it) {
                    //const int economicIndex = std::distance(begin_economics, it);
                    //economics[index]->add(key);
                    economicBuffer[index][economicBufferIndex[index]++] = key;
                }
                index++;
            }

            // const auto begin_minerals = mlt.minerals.cbegin();
            index = 0;
            for (auto it = mlt.minerals.cbegin(); it != mlt.minerals.cend(); it++) {
                if (*it) {
                   //  const int mineralIndex = std::distance(begin_minerals, it);
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
                        //minerals[index]->add(key);

                        mineralBuffer[index][mineralBufferIndex[index]++] = key;
                    }
                }
                index++;
            }
        }
    }

    hasAquifer.addMany(aquifierBufferIndex, aquiferBuffer);
    hasCoal.addMany(coalBufferIndex, coalBuffer);
    hasClay.addMany(clayBufferIndex, clayBuffer);
    hasFlux.addMany(fluxBufferIndex, fluxBuffer);
    hasRiver.addMany(riverBufferIndex, riverBuffer);
    hasSand.addMany(sandBufferIndex, sandBuffer);

    for (auto it = metalIndexes.cbegin(); it != metalIndexes.cend(); it++) {
        metals[*it]->addMany(metalBufferIndex[*it], metalBuffer[*it]);
    }

    for (auto it = economicIndexes.cbegin(); it != economicIndexes.cend(); it++) {
        economics[*it]->addMany(economicBufferIndex[*it], economicBuffer[*it]);
    }

    for (auto it = mineralIndexes.cbegin(); it != mineralIndexes.cend(); it++) {
        minerals[*it]->addMany(mineralBufferIndex[*it], mineralBuffer[*it]);
    }

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

    const auto innerEnd = std::chrono::system_clock::now();
    innerElapsed_seconds += innerEnd - innerStartTime;
}

void embark_assist::index::Index::add(uint32_t key, const embark_assist::defs::mid_level_tile &mlt) {
    // color_ostream_proxy out(Core::getInstance().getConsole());

    //bool schedule_optimize = false;
    //if (previous_key != -1 && !(key - previous_key == 1 || key - previous_key == 256)) {
    //    schedule_optimize = true;
    //}
    //previous_key = key;

    keys_in_order.push_back(key);

    //if (key > maxKeyValue) {
    //    out.print("key %d larger than %d \n ", key, maxKeyValue);
    //}

    entryCounter++;
    //if (uniqueKeys.contains(key)) {
    //    out.print("key %d already processed\n ", key);
    //}
    uniqueKeys.add(key);

    if (mlt.aquifer) {
        hasAquifer.add(key);
    }

    if (mlt.clay) {
        hasClay.add(key);
    }

    if (mlt.coal) {
        hasCoal.add(key);
    }

    if (mlt.flux) {
        hasFlux.add(key);
    }

    if (mlt.river_present) {
        hasRiver.add(key);
    }

    if (mlt.sand) {
        hasSand.add(key);
    }

    if (mlt.magma_level > -1) {
        magma_level[mlt.magma_level].add(key);
    }

    if (mlt.adamantine_level > -1) {
        adamantine_level[mlt.adamantine_level].add(key);
    }

    soil[mlt.soil_depth].add(key);

    savagery_level[mlt.savagery_level].add(key);
    evilness_level[mlt.evilness_level].add(key);

    // create indexes for metals, economics and minerals
    for (auto it = mlt.metals.cbegin(); it != mlt.metals.cend(); it++) {
        if (*it) {
            const int metalIndex = std::distance(mlt.metals.cbegin(), it);
            metals[metalIndex]->add(key);
        }
    }

    for (auto it = mlt.economics.cbegin(); it != mlt.economics.cend(); it++) {
        if (*it) {
            const int economicIndex = std::distance(mlt.economics.cbegin(), it);
            economics[economicIndex]->add(key);
        }
    }

    for (auto it = mlt.minerals.cbegin(); it != mlt.minerals.cend(); it++) {
        if (*it) {            
            const int mineralIndex = std::distance(mlt.minerals.cbegin(), it);
            const df::inorganic_raw* raw = world->raws.inorganics[mineralIndex];
            if (
                // true || 
                raw->environment.location.size() != 0 ||
                raw->environment_spec.mat_index.size() != 0 ||
                raw->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
                raw->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
                raw->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
                raw->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
                raw->flags.is_set(df::inorganic_flags::SOIL)) {
                minerals[mineralIndex]->add(key);
            }
        }
    }

    //if (schedule_optimize) {
    //    out.print("optimizing after key %d\n ", key);
    //    this->optimize(false);
    //}
}

const bool embark_assist::index::Index::containsEntries() const {
    return entryCounter >= maxKeyValue && uniqueKeys.contains(maxKeyValue);
}

inline void embark_assist::index::Index::optimize(Roaring *index, bool shrinkToSize) {
    index->runOptimize();
    if (true || shrinkToSize) {
        index->shrinkToFit();
    }
}

void embark_assist::index::Index::optimize(bool debugOutput) {

    if (debugOutput) {
        if (metalNames.empty()) {
            initInorganicNames();
        }
        this->outputSizes("before optimize");
    }

    for (auto it = static_indices.begin(); it != static_indices.end(); it++) {
        optimize(*it, debugOutput);
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
        out.print("### actual adding took total %f seconds ###\n", innerElapsed_seconds.count());
        // std::cout << "### actual adding took total " << innerElapsed_seconds.count() << " seconds ###" << std::endl;
        innerElapsed_seconds = std::chrono::seconds(0);
    }
}

void embark_assist::index::Index::shutdown() {
    world = nullptr; 
    entryCounter = 0;
    max_inorganic = 0;
    maxKeyValue = 0;

    for (auto it = static_indices.begin(); it != static_indices.end(); it++) {
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
    metalNames.clear();
    metalNames.reserve(0);
       
    for (auto it = metalBuffer.begin(); it != metalBuffer.end(); it++) {
        if (*it != nullptr) {
            delete *it;
        }
    }
    metalBuffer.clear();
    metalBuffer.resize(0);
    metalBuffer.reserve(0);

    metalBufferIndex.clear();
    metalBufferIndex.resize(0);
    metalBufferIndex.reserve(0);

    metalIndexes.clear();
    metalIndexes.resize(0);

    for (auto it = economics.begin(); it != economics.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    economics.clear();
    economics.resize(0);
    economics.reserve(0);
    economicNames.clear();
    economicNames.reserve(0);

    for (auto it = economicBuffer.begin(); it != economicBuffer.end(); it++) {
        if (*it != nullptr) {
            delete *it;
        }
    }
    economicBuffer.clear();
    economicBuffer.resize(0);
    economicBuffer.reserve(0);

    economicBufferIndex.clear();
    economicBufferIndex.resize(0);
    economicBufferIndex.reserve(0);

    economicIndexes.clear();
    economicIndexes.resize(0);

    for (auto it = minerals.begin(); it != minerals.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    minerals.clear();
    minerals.resize(0);
    minerals.reserve(0);
    mineralNames.clear();
    mineralNames.reserve(0);

    for (auto it = mineralBuffer.begin(); it != mineralBuffer.end(); it++) {
        if (*it != nullptr) {
            delete *it;
        }
    }
    mineralBuffer.clear();
    mineralBuffer.resize(0);
    mineralBuffer.reserve(0);

    mineralBufferIndex.clear();
    mineralBufferIndex.resize(0);
    mineralBufferIndex.reserve(0);

    mineralIndexes.clear();
    mineralIndexes.resize(0);

    inorganics.clear();
    inorganics.resize(0);
    inorganics.reserve(0);

    keys_in_order.clear();
    keys_in_order2.clear();
    positions.clear();
}

void embark_assist::index::Index::get_position(const uint32_t position_id, uint16_t &x, uint16_t &y, uint16_t &i, uint16_t &k) const {
    if (position_id >= (numberEmbarkTiles * world->worldgen.worldgen_parms.dim_x)) {
        y = std::floor(position_id / (numberEmbarkTiles * world->worldgen.worldgen_parms.dim_x));
    }
    uint32_t remainder = position_id - (y * numberEmbarkTiles * world->worldgen.worldgen_parms.dim_x);
    if (remainder >= numberEmbarkTiles) {
        x = std::floor(remainder / numberEmbarkTiles);
        remainder -= x * numberEmbarkTiles;
    }
    k = std::floor(remainder / 16);
    if (remainder > k * 16) {
        i = remainder - k * 16;
    }
}

void embark_assist::index::Index::get_position2(const uint32_t position_id, uint16_t &x, uint16_t &y, uint16_t &i, uint16_t &k) const {
    const int32_t fs_y_offset = std::floor(position_id / (total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width)) * 16;
    const int32_t y_base = std::floor(position_id / (total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width)) * total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width;
    const bool fs_left = 0 == ((int)std::floor(position_id / total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width) % 2);

    int32_t fs_x_offset;
    int32_t x_offset = 0;
    int32_t y_offset = 0;
    int32_t x_base;
    int32_t index;

    i = position_id % 16;
    k = std::floor((position_id % 256) / 16);

    if (fs_left) {
        if (world_last_y - fs_y_offset >= 16) {
            fs_x_offset = std::floor((position_id - y_base) / 65536) * 16;
            x_base = fs_x_offset * 256 * 16;
        }
        else {
            fs_x_offset = std::floor((position_id - y_base) / (y_tail * 16 * 256)) * 16;
            x_base = fs_x_offset * 256 * y_tail;
        }
    }
    else {
        if (world_last_y - fs_y_offset >= 16) {
            if (position_id - y_base < x_tail * 16 * 256) {
                fs_x_offset = 0;
                x_base = 0;
            }
            else {
                fs_x_offset = std::floor((position_id - y_base - x_tail * 16 * 256) / 65536) * 16 + x_tail;
                x_base = fs_x_offset * 256 * 16;
            }
        }
        else {
            if (position_id - y_base < x_tail * y_tail * 256) {
                fs_x_offset = 0;
                x_base = 0;
            }
            else {
                fs_x_offset = std::floor((position_id - y_base - x_tail * y_tail * 256) / (16 * 256 * y_tail)) * 16 + x_tail;
                    x_base = fs_x_offset * 256 * y_tail;
            }
        }
    }

    index = position_id - y_base - x_base;

    if (world_last_y - fs_y_offset >= 16) {
        x_offset = std::floor(index / (16 * 256));
        y_offset = std::floor((index - x_offset * 16 * 256) / 256);
    }

    else {
        x_offset = std::floor(index / (y_tail * 256));
        y_offset = std::floor((index - x_offset * y_tail * 256) / 256);
    }

    x = fs_x_offset + x_offset;

    if (x % 2 == 1) {//n  --  Relies on the world having an odd X dimension number and the odd ones always are reversed, regardless of traversing left or right.
        if (world_last_y - fs_y_offset >= 16) {
            y_offset = 15 - y_offset;
        }
        else {
            y_offset = y_tail - 1 - y_offset;
        }
    }
         
    y = fs_y_offset + y_offset;

    if (!fs_left) {
        x = world_last_x - x;
    }
}


const uint32_t embark_assist::index::Index::key_of(int16_t x, int16_t y, uint8_t i, uint8_t k) const {
    const int32_t world_last_x = world->world_data->world_width - 1;
    const int32_t world_last_y = world->world_data->world_height - 1;
    const int16_t fs_y_offset = std::floor(y / 16) * 16;
    int16_t y_offset = 0;
    const bool fs_left = y % 32 < 16;
    int16_t x_result = x;

    if (!fs_left) {
        x_result = world_last_x - x;
    }

    if ((fs_left && x % 2 == 0) || (!fs_left && x % 2 == 1)) {
        // Assumes the X dimension is an uneven number, but the search doesn't work if it isn't...
        y_offset = y % 16;
    }
    else if (world_last_y - std::floor(y / 16) * 16 < 16) {
        y_offset = y - world_last_y;
    }
    else {
        y_offset = 15 - y % 16;
    }

    const uint32_t key = i + k * 16 + (x_result)* world->world_data->world_height * 256 + (fs_y_offset + y_offset) * 256;
    //const uint32_t key = i + k * 16 + (x_result) * (y + 1) * 256 + (fs_y_offset + y_offset) * 256;
    // dfhack.println("key_of:", x, y, i, k, key, x_result, fs_y_offset, y_offset)
    return key;
}

const uint32_t embark_assist::index::Index::key_of2(int16_t x, int16_t y, uint8_t i, uint8_t k) const {
    const int16_t fs_y_offset = std::floor(y / 16) * 16;
    int16_t y_offset = 0;
    const bool fs_left = y % 32 < 16;
    int16_t fs_x_offset = 0;
    int32_t x_offset = 0;
    int32_t y_base;
    int32_t x_base;
    int32_t index;

    if (fs_left) {
        fs_x_offset = std::floor(x / 16) * 16;
        x_offset = x - fs_x_offset;
    }
    else if (world_last_x - x < x_tail) {
        fs_x_offset = 0;
        x_offset = world_last_x - x;
    }
    else {
        fs_x_offset = std::floor((world_last_x - x - x_tail) / 16) * 16 + x_tail;
        x_offset = (world_last_x - x) - fs_x_offset;
    }

    if (x % 2 == 0) {  //  Assumes the X dimension is an uneven number, but the search doesn't work if it isn't...
        y_offset = y % 16;
    }
    else if (world_last_y - std::floor(y / 16) * 16 < 16) {
        y_offset = y - world_last_y;
    }
    else {
        y_offset = 15 - y % 16;
    }

    //  All the tiles in the prior feature shell block rows.
    //
    y_base = fs_y_offset * world->world_data->world_width * 256;

    if (y <= world_last_y - y_tail) {
        x_base = fs_x_offset * 16 * 256;  //All feature shells before this one in this row
    }
    else {
        x_base = fs_x_offset * y_tail * 256;  //All feature shells before this one in this row
    }

    if (y <= world_last_y - y_tail) {
        index = i + k * 16 + x_offset * 16 * 256 + y_offset * 256;
    }
    else {
        index = i + k * 16 + x_offset * y_tail * 256 + y_offset * 256;
    }

    const uint32_t key = y_base + x_base + index;
    //  dfhack.println("key_of:", x, y, i, k, key, y_base, x_base, index, fs_left, fs_x_offset, x_offset, fs_y_offset, y_offset)
    return key;
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
            name = getInorganicName(index, metalNames, name);
            fprintf(outfile, "#%d - %s: %I64d\n", index, name.c_str(), (*it)->cardinality());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fprintf(outfile, "\nnumber of economics entries:\n");
    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN ECONOMIC ###";
            const uint16_t index = std::distance(economics.cbegin(), it);
            name = getInorganicName(index, economicNames, name);
            fprintf(outfile, "#%d - %s: %I64d\n", index, name.c_str(), (*it)->cardinality());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fprintf(outfile, "\nnumber of minerals entries:\n");
    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN MINERAL ###";
            const uint16_t index = std::distance(minerals.cbegin(), it);
            name = getInorganicName(index, mineralNames, name);
            fprintf(outfile, "#%d - %s: %I64d\n", index, name.c_str(), (*it)->cardinality());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
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
        get_position2(key, x, y, i, k);
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
        const uint32_t key = key_of2(position.x, position.y, 0, 0);
        if (previous_key != 0 && previous_key != key - 1) {
            positionsFile << "### discontinuity ###" << "\n";
        }
        const uint32_t key2 = key_of2(position.x, position.y, 15, 15);
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
    get_position2(position_id, x, y, i, k);

    // if the previously inspected world tile is not the current one, we reset the tracker
    if (start_tile_tracker.x != x || start_tile_tracker.y != y) {
        start_tile_tracker.x = x;
        start_tile_tracker.y = y;
        for (uint8_t i = 0; i < 16; i++) {
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
    uint32_t position_id_region_offset = key_of2(x, y, 0, 0);
    uint32_t start_position_id = position_id - ((i - start_i_offset) + (k - start_k_offset) * 16);

    uint16_t embark_counter = 0;
    uint16_t buffer_position = 0;
    for (uint16_t current_k_offset = start_k_offset; current_k_offset + embark_size_y - 1 <= end_k_offset; current_k_offset++) {
        for (uint16_t current_i_offset = start_i_offset; current_i_offset + embark_size_x - 1 <= end_i_offset; current_i_offset++) {
            // if the current tile was already used as start tile for an embark candidate/variant we skip it
            if (start_tile_tracker.was_already_used_as_start_position[current_i_offset][current_k_offset]) {
                continue;
            }
            start_position_id = position_id - ((i - current_i_offset) + (k - current_k_offset) * 16);
            buffer_position = 0;
            for (uint16_t current_k = current_k_offset; current_k < current_k_offset + embark_size_y; current_k++) {
                for (uint16_t current_i = current_i_offset; current_i < current_i_offset + embark_size_x; current_i++) {
                    const uint32_t new_position_id = position_id_region_offset + current_i + current_k * 16;
                    // out.print("embark_assist::index::Index::calculate_embark_variants: key:%d / position x:%d y:%d i:%d k:%d\n", new_position_id, x, y, current_i, current_k);
                    embark_tile_key_buffer[buffer_position++] = new_position_id;
                }
            }
            start_tile_tracker.was_already_used_as_start_position[current_i_offset][current_k_offset] = true;
            // removing any keys that are in the "buffer"/preallocated Roaring index currently from previous iterations
            ra_reset(&(embarks[embark_counter].roaring.high_low_container));
            embarks[embark_counter++].addMany(buffer_position, embark_tile_key_buffer);
        }
    }

    return embark_counter;
}

//void query_index() {
//    //const uint32_t position_id = 0;
//    // const uint32_t position_id = 255;
//    const uint32_t position_id = 254;
//    const uint16_t embark_size_x = 4;
//    const uint16_t embark_size_y = 4;
//    std::vector<Roaring> embarks(16);
//    uint32_t buffer[256];
//    const int number_of_embark_variants = calculate_embark_variants(position_id, embark_size_x, embark_size_y, embarks, buffer);
//}


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

const embark_assist::index::query_plan_interface* embark_assist::index::Index::create_query_plan(const embark_assist::defs::finders &finder) const {
    const Index &scope = *this;
    
    embark_assist::index::query_plan *result = new embark_assist::index::query_plan();

    if (finder.aquifer == embark_assist::defs::aquifer_ranges::Present) {
        // TODO: be aware, that there are special (corner) cases if a criteria makes assumptions about more than one tile e.g. "All", "Absent", "Partial"
        // - if this query is the most significant one we need another query as helper to verify that all the other tiles also (not) have a aquifer... 
        // This is true for all criteria with exclusive/absolute (all/none, absent, ...) meaning
        const Roaring &hasAquifer = this->hasAquifer;
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&hasAquifer](const Roaring &embark_candidate) -> bool {
            // std::cout << "hasAquifer.and_cardinality" << std::endl;
            return hasAquifer.and_cardinality(embark_candidate) > 0;
        }, [&hasAquifer]() -> uint32_t {
            // std::cout << "hasAquifer.cardinality" << std::endl;
            return hasAquifer.cardinality();
        }, [&hasAquifer, &scope]() -> const std::vector<uint32_t>* {
            return scope.get_keys(hasAquifer);
            //uint32_t* most_significant_ids = new uint32_t[hasAquifer.cardinality()];
            //hasAquifer.toUint32Array(most_significant_ids);
            //const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + hasAquifer.cardinality());

            //delete most_significant_ids;
            //return dest;
        });
        result->queries.push_back(q);
    }

    if (finder.magma_min == embark_assist::defs::magma_ranges::Volcano) {
        const uint64_t i = static_cast<uint64_t>(embark_assist::defs::magma_ranges::Volcano);
        const Roaring &magma_level = this->magma_level[i];
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&magma_level](const Roaring &embark_candidate) -> bool {
            // std::cout << "magma_level.and_cardinality" << std::endl;
            return magma_level.and_cardinality(embark_candidate) > 0;
        }, [&magma_level]() -> uint32_t {
            // std::cout << "magma_level.cardinality" << std::endl;
            return magma_level.cardinality();
        }, [&magma_level, &scope]() -> const std::vector<uint32_t>* {
            return scope.get_keys(magma_level);
            //uint32_t* most_significant_ids = new uint32_t[magma_level.cardinality()];
            //magma_level.toUint32Array(most_significant_ids);
            //const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + magma_level.cardinality());

            //delete most_significant_ids;
            //return dest;
        });
        result->queries.push_back(q);
    }

    if (finder.evilness[static_cast<uint8_t>(embark_assist::defs::evil_savagery_ranges::Medium)] != embark_assist::defs::evil_savagery_values::Present) {
        const uint64_t i = static_cast<uint64_t>(embark_assist::defs::evil_savagery_ranges::Medium);
        const Roaring &evilness_level = this->evilness_level[i];
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&evilness_level](const Roaring &embark_candidate) -> bool {
            // std::cout << "evilness_level.and_cardinality" << std::endl;
            return evilness_level.and_cardinality(embark_candidate) > 0;
        }, [&evilness_level]() -> uint32_t {
            // std::cout << "evilness_level.cardinality" << std::endl;
            return evilness_level.cardinality();
        }, [&evilness_level, &scope]() -> const std::vector<uint32_t>* {
            return scope.get_keys(evilness_level);
            //uint32_t* most_significant_ids = new uint32_t[evilness_level.cardinality()];
            //evilness_level.toUint32Array(most_significant_ids);
            //const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + evilness_level.cardinality());

            //delete most_significant_ids;
            //return dest;
        });
        result->queries.push_back(q);
    }

    if (finder.metal_1 != -1) {
        const Roaring &metal = *this->metals[finder.metal_1];
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&metal](const Roaring &embark_candidate) -> bool {
            // std::cout << "metal.and_cardinality" << std::endl;
            return metal.and_cardinality(embark_candidate) > 0;
        }, [&metal]() -> uint32_t {
            // std::cout << "metal.cardinality" << std::endl;
            return metal.cardinality();
        }, [&metal, &scope]() -> const std::vector<uint32_t>* {
            return scope.get_keys(metal);
            //uint32_t* most_significant_ids = new uint32_t[metal.cardinality()];
            //metal.toUint32Array(most_significant_ids);
            //const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + metal.cardinality());

            //delete most_significant_ids;
            //return dest;
        });
        result->queries.push_back(q);
    }

    if (finder.metal_2 != -1) {
        const Roaring &metal = *this->metals[finder.metal_2];
        embark_assist::query::query_interface *q = embark_assist::query::make_myclass([&metal](const Roaring &embark_candidate) -> bool {
            // std::cout << "metal.and_cardinality" << std::endl;
            return metal.and_cardinality(embark_candidate) > 0;
        }, [&metal]() -> uint32_t {
            // std::cout << "metal.cardinality" << std::endl;
            return metal.cardinality();
        }, [&metal, &scope]() -> const std::vector<uint32_t>* {
            return scope.get_keys(metal);
            //uint32_t* most_significant_ids = new uint32_t[metal.cardinality()];
            //metal.toUint32Array(most_significant_ids);
            //const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + metal.cardinality());

            //delete most_significant_ids;
            //return dest;
        });
        result->queries.push_back(q);
    }

    std::sort(result->queries.begin(), result->queries.end(), [](const embark_assist::query::query_interface* a, const embark_assist::query::query_interface* b) -> bool {
        return a->get_number_of_entries() < b->get_number_of_entries();
    });

    //uint32_t* most_significant_ids = new uint32_t[magma_level.cardinality()];
    //magma_level.toUint32Array(most_significant_ids);
    //const std::vector<uint32_t> dest(most_significant_ids, most_significant_ids + magma_level.cardinality());

    const embark_assist::query::query_interface* first_query = result->queries[0];
    result->set_most_significant_ids(first_query->get_keys());

    if (first_query->is_to_be_deleted_after_key_extraction()) {
        // remove the first query - as it is being used directly for its keys
        result->queries.erase(result->queries.begin());
        delete first_query;
    }

    return result;
}

void embark_assist::index::Index::find(const embark_assist::defs::finders &finder, embark_assist::defs::match_results &match_results) const {
    color_ostream_proxy out(Core::getInstance().getConsole());
    const auto innerStartTime = std::chrono::system_clock::now();
    const std::time_t start_time = std::chrono::system_clock::to_time_t(innerStartTime);
    out.print("embark_assist::index::Index::find: started index query search at %s\n", std::ctime(&start_time));
    
    const uint16_t embark_size_x = finder.x_dim;
    const uint16_t embark_size_y = finder.y_dim;

    const size_t size_factor_x = 16 - embark_size_x + 1;
    const size_t size_factor_y = 16 - embark_size_y + 1;

    size_t max_number_of_embark_variants = size_factor_x * size_factor_y;
    // starting with a certain embark size (x,y = 9) the region size (16x16) is the limiting factor for how many different embark variants there can be
    // the smaller number is the correct one in any case
    max_number_of_embark_variants = std::min(max_number_of_embark_variants, (size_t) embark_size_x * embark_size_y);

    std::vector<Roaring> embarks(max_number_of_embark_variants);
    uint32_t embark_tile_key_buffer[256];
    uint32_t number_of_iterations = 0;
    uint32_t number_of_matches = 0;

    embark_tile_tracker start_tile_tracker;

    // create query plan
    const query_plan_interface *query_plan = create_query_plan(finder);
    out.print("number of significant_ids: %d\n", query_plan->get_most_significant_ids().size());
    // loop over all keys that are most significant
    for (const uint32_t position_id : query_plan->get_most_significant_ids()) {
        const int number_of_embark_variants = calculate_embark_variants(position_id, embark_size_x, embark_size_y, embarks, embark_tile_key_buffer, start_tile_tracker);
        for (int embark_variant_index = 0; embark_variant_index < number_of_embark_variants; embark_variant_index++) {
            bool matches = query_plan->execute(embarks[embark_variant_index]);
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
                get_position2(smallest_key, x, y, i, k);

                match_results[x][y].contains_match = true;
                //if (match_results[x][y].mlt_match[i][k]) {
                //    out.print("embark_assist::index::Index::find: found same match as matcher:");
                //}
                //else {
                //    out.print("embark_assist::index::Index::find: found match matcher has not found:");
                //    //out.print("key: %d / position x:%d y:%d i:%d k:%d\n", smallest_key, x, y, i, k);
                //}
                match_results[x][y].mlt_match[i][k] = true;

                //out.print("key: %d / position x:%d y:%d i:%d k:%d\n", smallest_key, x,y,i,k);
                number_of_matches++;
            }
        }
    }
    delete query_plan;
    
    const auto innerEnd = std::chrono::system_clock::now();
    const std::time_t end_time = std::chrono::system_clock::to_time_t(innerEnd);
    const std::chrono::duration<double> elapsed_seconds = innerEnd - innerStartTime;
    out.print("embark_assist::index::Index::find: finished index query search at %s with elapsed time: %f seconds with %d iterations and %d matches\n", std::ctime(&end_time), elapsed_seconds.count(), number_of_iterations, number_of_matches);
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

    fprintf(outfile, "metal index bytesizes:\n");
    for (auto it = metals.cbegin(); it != metals.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN METAL ###";
            const uint16_t index = std::distance(metals.cbegin(), it);
            name = getInorganicName(index, metalNames, name);
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
            name = getInorganicName(index, economicNames, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
        }
    }
    fprintf(outfile, "\nminerals index bytesizes:\n");
    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN MINERAL ###";
            const uint16_t index = std::distance(minerals.cbegin(), it);
            name = getInorganicName(index, mineralNames, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
        }
    }

    fprintf(outfile, "\n total index bytesize: %d\n", byteSize);
    fclose(outfile);
}

//Roaring& embark_assist::index::Index::getInorganicsIndex(std::unordered_map<uint16_t, Roaring*> &indexMap, uint16_t metal_index) {
//    const unordered_map<uint16_t, Roaring*>::iterator it = indexMap.find(metal_index);
//    if (it != indexMap.end()) {
//        return *(it->second);
//    }
//    Roaring* inorganicIndex = new Roaring();
//    indexMap.insert(std::pair<uint16_t, Roaring*>(metal_index, inorganicIndex));
//    // indexMap.at(metal_index) = inorganicIndex;
//    return *inorganicIndex;
//}

void embark_assist::index::Index::init_inorganic_index() {
    const uint32_t starting_capacity = 0;
    metals.resize(max_inorganic, nullptr);
    metalBuffer.resize(max_inorganic, nullptr);
    metalBufferIndex.resize(max_inorganic);
    for (uint16_t k = 0; k < max_inorganic; k++) {
        for (uint16_t l = 0; l < world->raws.inorganics[k]->metal_ore.mat_index.size(); l++) {
            const int metalIndex = world->raws.inorganics[k]->metal_ore.mat_index[l];
            if (metals[metalIndex] == nullptr) {
                //Roaring* inorganicIndex = new Roaring();
                roaring_bitmap_t* rr = roaring_bitmap_create_with_capacity(starting_capacity);
                Roaring* inorganicIndex = new Roaring(rr);
                metals[metalIndex] = inorganicIndex;

                metalBuffer[metalIndex] = new uint32_t[256];
                metalIndexes.push_back(metalIndex);
            }
        }
    }
    //metals.shrink_to_fit();

    economics.resize(max_inorganic, nullptr);
    economicBuffer.resize(max_inorganic, nullptr);
    economicBufferIndex.resize(max_inorganic);
    for (int16_t k = 0; k < max_inorganic; k++) {
        if (world->raws.inorganics[k]->economic_uses.size() != 0 && !world->raws.inorganics[k]->material.flags.is_set(df::material_flags::IS_METAL)) {
            if (economics[k] == nullptr) {
                //Roaring* inorganicIndex = new Roaring();
                roaring_bitmap_t* rr = roaring_bitmap_create_with_capacity(starting_capacity);
                Roaring* inorganicIndex = new Roaring(rr);
                economics[k] = inorganicIndex;

                economicBuffer[k] = new uint32_t[256];
                economicIndexes.push_back(k);
            }
        }
    }
    //economics.shrink_to_fit();

    minerals.resize(max_inorganic, nullptr);
    mineralBuffer.resize(max_inorganic, nullptr);
    mineralBufferIndex.resize(max_inorganic);
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
            //Roaring* inorganicIndex = new Roaring();
            roaring_bitmap_t* rr = roaring_bitmap_create_with_capacity(starting_capacity);
            Roaring* inorganicIndex = new Roaring(rr);
            minerals[k] = inorganicIndex;

            mineralBuffer[k] = new uint32_t[256];
            mineralIndexes.push_back(k);
        }
        }
    }
    //minerals.shrink_to_fit();
}

void embark_assist::index::Index::initInorganicNames() {
    // std::unordered_map<uint16_t, std::string> metalNames
    for (uint16_t k = 0; k < max_inorganic; k++) {
        for (uint16_t l = 0; l < world->raws.inorganics[k]->metal_ore.mat_index.size(); l++) {
            metalNames.insert(std::pair<uint16_t, std::string>(world->raws.inorganics[k]->metal_ore.mat_index[l],
                world->raws.inorganics[world->raws.inorganics[k]->metal_ore.mat_index[l]]->id));
        }
    }

    for (int16_t k = 0; k < max_inorganic; k++) {
        if (world->raws.inorganics[k]->economic_uses.size() != 0 && !world->raws.inorganics[k]->material.flags.is_set(df::material_flags::IS_METAL)) {
            economicNames.insert(std::pair<uint16_t, std::string>(k, world->raws.inorganics[k]->id));
        }
    }

    for (int16_t k = 0; k < max_inorganic; k++) {
        const df::inorganic_raw* raw = world->raws.inorganics[k];
        /*if (
            raw->environment.location.size() != 0 ||
            raw->environment_spec.mat_index.size() != 0 ||
            raw->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
            raw->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
            raw->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
            raw->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
            raw->flags.is_set(df::inorganic_flags::SOIL)) {*/
        mineralNames.insert(std::pair<uint16_t, std::string>(k, world->raws.inorganics[k]->id));
        //}
    }
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