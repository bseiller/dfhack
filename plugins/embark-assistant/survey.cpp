
#include <math.h>
#include <vector>
//#include <unordered_map>
#include <unordered_set>
#include <future>
#include <map>

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>

#include <modules/Gui.h>
#include "modules/Materials.h"
#include "modules/Maps.h"

#include "DataDefs.h"
#include "df/builtin_mats.h"
#include "df/coord2d.h"
#include "df/creature_interaction_effect.h"
#include "df/creature_interaction_effect_display_symbolst.h"
#include "df/creature_interaction_effect_type.h"
#include "df/feature_init.h"
#include "df/feature_init_deep_special_tubest.h"
#include "df/feature_init_magma_poolst.h"
#include "df/feature_init_volcanost.h"
#include "df/feature_type.h"
#include "df/inorganic_flags.h"
#include "df/inorganic_raw.h"
#include "df/interaction.h"
#include "df/interaction_effect.h"
#include "df/interaction_effect_type.h"
#include "df/interaction_effect_animatest.h"
#include "df/interaction_instance.h"
#include "df/interaction_source.h"
#include "df/interaction_source_regionst.h"
#include "df/interaction_source_type.h"
#include "df/interaction_target.h"
#include "df/interaction_target_corpsest.h"
#include "df/interaction_target_materialst.h"
#include "df/material_common.h"
#include "df/reaction.h"
#include "df/reaction_product.h"
#include "df/reaction_product_itemst.h"
#include "df/reaction_product_type.h"
#include "df/region_map_entry.h"
#include "df/syndrome.h"
#include "df/viewscreen.h"
#include "df/viewscreen_choose_start_sitest.h"
#include "df/world.h"
#include "df/world_data.h"
#include "df/world_geo_biome.h"
#include "df/world_geo_layer.h"
#include "df/world_raws.h"
#include "df/world_region.h"
#include "df/world_region_details.h"
#include "df/world_region_feature.h"
#include "df/world_region_type.h"
#include "df/world_river.h"
#include "df/world_site.h"
#include "df/world_site_type.h"
#include "df/world_underground_region.h"

#include "defs.h"
#include "incursion_processor.h"
#include "survey.h"
#include "key_buffer_holder.h"

#define edge_root_folder "region4_edges/"
#define edge_root_folder_path index_folder_name edge_root_folder

using namespace DFHack;
using namespace df::enums;
using namespace Gui;

using df::global::world;

namespace embark_assist {
    namespace survey {

        std::chrono::duration<double> elapsed_init_seconds = std::chrono::seconds(0);
        std::chrono::duration<double> elapsed_big_loop_seconds = std::chrono::seconds(0);
        std::chrono::duration<double> elapsed_workaround_seconds = std::chrono::seconds(0);
        std::chrono::duration<double> elapsed_tile_summary_seconds = std::chrono::seconds(0);
        std::chrono::duration<double> elapsed_waterfall_and_biomes_seconds = std::chrono::seconds(0);
        std::chrono::duration<double> elapsed_survey_seconds = std::chrono::seconds(0);
        std::chrono::duration<double> elapsed_survey_total_seconds = std::chrono::seconds(0);

        uint32_t number_of_waterfalls = 0;

        uint32_t number_of_layer_cache_misses = 0;
        uint32_t number_of_layer_cache_hits = 0;

        /*struct layer_content_cache_entry {
            std::unordered_set<uint16_t> economics;
            std::unordered_set<uint16_t> minerals;
            std::unordered_set<int16_t> metals;

            bool is_aquifer_layer = false;
            bool coal = false;
            bool clay = false;
            bool flux = false;
            bool sand = false;
        };*/

        //typedef std::unordered_map<const df::world_geo_layer*, layer_content_cache_entry*> layer_content_cache;

        struct states {
            uint16_t clay_reaction = -1;
            uint16_t flux_reaction = -1;
            std::vector<uint16_t> coals;
            uint16_t x;
            uint16_t y;
            uint8_t local_min_x;
            uint8_t local_min_y;
            uint8_t local_max_x;
            uint8_t local_max_y;
            uint16_t max_inorganic;
            embark_assist::key_buffer_holder::key_buffer_holder buffer_holder;
            FILE* rivers_file;
            FILE* river_anomalies_file;
            FILE* invalid_river_file;
            /*std::vector<layer_content_cache_entry> layer_content_cache_entry_pool;
            uint16_t layer_content_cache_entry_pool_index = 0;
            layer_content_cache layer_content_cache;*/
            embark_assist::incursion::incursion_processor incursion_processor;
            std::future<void> regular_data_processing_result;
            std::future<void> internal_incursion_processing_result;
            std::future<void> external_incursion_processing_result;
        };

        static states *state;

        //=======================================================================================

        /*bool process_layer_from_cache(const df::world_geo_layer *layer, embark_assist::defs::mid_level_tile &mid_level_tile,
            embark_assist::key_buffer_holder::key_buffer_holder &buffer_holder, const uint32_t key, const int16_t bottom_z, const int16_t elevation) {
            if (state->layer_content_cache.count(layer) == 0) {
                ++number_of_layer_cache_misses;
                return false;
            }
            if (state->layer_content_cache.count(layer) > 1) {
                color_ostream_proxy out(Core::getInstance().getConsole());
                out.print("multiple entries in bucket for layer with material %d \n", layer->mat_index);
            }
            ++number_of_layer_cache_hits;
            layer_content_cache_entry *cache_entry = state->layer_content_cache[layer];

            if (cache_entry->is_aquifer_layer && bottom_z <= elevation - 3) {
                if (!mid_level_tile.aquifer) {
                    mid_level_tile.aquifer = true;
                    buffer_holder.add_aquifer(key);
                }
            }

            if (cache_entry->clay && !mid_level_tile.clay) {
                mid_level_tile.clay = true;
                buffer_holder.add_clay(key);
            }

            if (cache_entry->coal && !mid_level_tile.coal) {
                mid_level_tile.coal = true;
                buffer_holder.add_coal(key);
            }

            if (cache_entry->flux && !mid_level_tile.flux) {
                mid_level_tile.flux = true;
                buffer_holder.add_flux(key);
            }

            if (cache_entry->sand && !mid_level_tile.sand) {
                mid_level_tile.sand = true;
                buffer_holder.add_sand(key);
            }

            for (auto economic : cache_entry->economics) {
                if (!mid_level_tile.economics[economic]) {
                    mid_level_tile.economics[economic] = true;
                    buffer_holder.add_economic(key, economic);
                }
            }

            for (auto metal : cache_entry->metals) {
                if (!mid_level_tile.metals[metal]) {
                    mid_level_tile.metals[metal] = true;
                    buffer_holder.add_metal(key, metal);
                }
            }

            for (auto mineral : cache_entry->minerals) {
                if (!mid_level_tile.minerals[mineral]) {
                    mid_level_tile.minerals[mineral] = true;
                    buffer_holder.add_mineral(key, mineral);
                }
            }

            return true;
        }*/

        bool geo_survey(embark_assist::defs::geo_data *geo_summary) {
            color_ostream_proxy out(Core::getInstance().getConsole());
            df::world_data *world_data = world->world_data;
            auto reactions = df::reaction::get_vector();
            bool non_soil_found;
            uint16_t size;

            for (uint16_t i = 0; i < reactions.size(); i++) {
                if (reactions[i]->code == "MAKE_CLAY_BRICKS") {
                    state->clay_reaction = i;
                }

                if (reactions[i]->code == "PIG_IRON_MAKING") {
                    state->flux_reaction = i;
                }
            }

            if (state->clay_reaction == -1) {
                out.printerr("The reaction 'MAKE_CLAY_BRICKS' was not found, so clay can't be identified.\n");
            }

            if (state->flux_reaction == -1) {
                out.printerr("The reaction 'PIG_IRON_MAKING' was not found, so flux can't be identified.\n");
            }

            for (uint16_t i = 0; i < world->raws.inorganics.size(); i++) {
                for (uint16_t k = 0; k < world->raws.inorganics[i]->economic_uses.size(); k++) {
                    for (uint16_t l = 0; l < world->raws.reactions.reactions[world->raws.inorganics[i]->economic_uses[k]]->products.size(); l++) {
                        df::reaction_product_itemst *product = static_cast<df::reaction_product_itemst*>(world->raws.reactions.reactions[world->raws.inorganics[i]->economic_uses[k]]->products[l]);

                        if (product->mat_type == df::builtin_mats::COAL) {
                            state->coals.push_back(i);
                            break;
                        }
                    }
                }
            }

            for (uint16_t i = 0; i < world_data->geo_biomes.size(); i++) {
                embark_assist::defs::geo_datum &geo_datum = geo_summary->at(i);
                geo_datum.possible_metals.resize(state->max_inorganic);
                geo_datum.possible_economics.resize(state->max_inorganic);
                geo_datum.possible_minerals.resize(state->max_inorganic);

                non_soil_found = true;
                df::world_geo_biome *geo = world_data->geo_biomes[i];

                for (uint16_t k = 0; k < geo->layers.size() && k < 16; k++) {
                    df::world_geo_layer *layer = geo->layers[k];

                    if (layer->type == df::geo_layer_type::SOIL ||
                        layer->type == df::geo_layer_type::SOIL_SAND) {
                        geo_datum.soil_size += layer->top_height - layer->bottom_height + 1;

                        if (world->raws.inorganics[layer->mat_index]->flags.is_set(df::inorganic_flags::SOIL_SAND)) {
                            geo_datum.sand_absent = false;
                        }

                        if (non_soil_found) {
                            geo_datum.top_soil_only = false;
                        }
                    }
                    else {
                        non_soil_found = true;
                    }

                    geo_datum.possible_minerals[layer->mat_index] = true;

                    size = (uint16_t)world->raws.inorganics[layer->mat_index]->metal_ore.mat_index.size();

                    for (uint16_t l = 0; l < size; l++) {
                        geo_datum.possible_metals.at(world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[l]) = true;
                    }

                    size = (uint16_t)world->raws.inorganics[layer->mat_index]->economic_uses.size();
                    if (size != 0) {
                        geo_datum.possible_economics[layer->mat_index] = true;

                        for (uint16_t l = 0; l < size; l++) {
                            if (world->raws.inorganics[layer->mat_index]->economic_uses[l] == state->clay_reaction) {
                                geo_datum.clay_absent = false;
                            }

                            if (world->raws.inorganics[layer->mat_index]->economic_uses[l] == state->flux_reaction) {
                                geo_datum.flux_absent = false;
                            }
                        }
                    }

                    for (uint16_t l = 0; l < state->coals.size(); l++) {
                        if (layer->mat_index == state->coals[l]) {
                            geo_datum.coal_absent = false;
                            break;
                        }
                    }

                    size = (uint16_t)layer->vein_mat.size();

                    for (uint16_t l = 0; l < size; l++) {
                        auto vein = layer->vein_mat[l];
                        geo_datum.possible_minerals[vein] = true;

                        for (uint16_t m = 0; m < world->raws.inorganics[vein]->metal_ore.mat_index.size(); m++) {
                            geo_datum.possible_metals.at(world->raws.inorganics[vein]->metal_ore.mat_index[m]) = true;
                        }

                        if (world->raws.inorganics[vein]->economic_uses.size() != 0) {
                            geo_datum.possible_economics[vein] = true;

                            for (uint16_t m = 0; m < world->raws.inorganics[vein]->economic_uses.size(); m++) {
                                if (world->raws.inorganics[vein]->economic_uses[m] == state->clay_reaction) {
                                    geo_datum.clay_absent = false;
                                }

                                if (world->raws.inorganics[vein]->economic_uses[m] == state->flux_reaction) {
                                    geo_datum.flux_absent = false;
                                }
                            }

                            for (uint16_t m = 0; m < state->coals.size(); m++) {
                                if (vein == state->coals[m]) {
                                    geo_datum.coal_absent = false;
                                    break;
                                }
                            }

                        }
                    }

                    if (layer->bottom_height <= -3 &&
                        world->raws.inorganics[layer->mat_index]->flags.is_set(df::inorganic_flags::AQUIFER)) {
                        geo_datum.aquifer_absent = false;
                    }

                    if (non_soil_found == true) {
                        geo_datum.top_soil_aquifer_only = false;
                    }
                }
            }
            return true;
        }


        //=================================================================================

        void survey_rivers(embark_assist::defs::world_tile_data *survey_results) {
            color_ostream_proxy out(Core::getInstance().getConsole());
            df::world_data *world_data = world->world_data;
            int16_t x;
            int16_t y;

            int32_t invalid_regular_river_regions = 0;
            int32_t invalid_river_endpoint_regions = 0;

            for (uint16_t i = 0; i < world_data->rivers.size(); i++) {
                for (uint16_t k = 0; k < world_data->rivers[i]->path.x.size(); k++) {
                    x = world_data->rivers[i]->path.x[k];
                    y = world_data->rivers[i]->path.y[k];

                    embark_assist::defs::region_tile_datum &region_tile_datum = survey_results->at(x).at(y);

                    embark_assist::defs::river_sizes river_size = embark_assist::defs::river_sizes::None;

                    if (world_data->rivers[i]->flow[k] < 5000) {
                        if (world_data->region_map[x][y].flags.is_set(df::region_map_entry_flags::is_brook)) {
                            river_size = embark_assist::defs::river_sizes::Brook;
                        }
                        else {
                            river_size = embark_assist::defs::river_sizes::Stream;
                        }
                    }
                    else if (world_data->rivers[i]->flow[k] < 10000) {
                        river_size = embark_assist::defs::river_sizes::Minor;
                    }
                    else if (world_data->rivers[i]->flow[k] < 20000) {
                        river_size = embark_assist::defs::river_sizes::Medium;
                    }
                    else {
                        river_size = embark_assist::defs::river_sizes::Major;
                    }

                    if (region_tile_datum.river_size != embark_assist::defs::river_sizes::None) {
                        //color_ostream_proxy out(Core::getInstance().getConsole());
                        if (region_tile_datum.river_size > river_size) {
                            out.printerr("Overwritting existing river_size with smaller river_size at %02d,%02d - %d with %d\n", x, y, region_tile_datum.river_size, river_size);
                        }
                        else {
                            out.printerr("Overwritting existing river_size with same or larger river_size at %02d,%02d - %d with %d\n", x, y, region_tile_datum.river_size, river_size);
                        }
                    }
                    region_tile_datum.river_size = river_size;
                    if (!world_data->region_map[x][y].flags.is_set(df::region_map_entry_flags::has_river)) {
                        ++invalid_regular_river_regions;
                    }
                }

                x = world_data->rivers[i]->end_pos.x;
                y = world_data->rivers[i]->end_pos.y;

                //  Make the guess the river size for the end is the same as the tile next to the end. Note that DF
                //  doesn't actually recognize this tile as part of the river in the pre embark river name display.
                //  We also assume the is_river/is_brook flags are actually set properly for the end tile.
                //
                if (x >= 0 && y >= 0 && x < world->worldgen.worldgen_parms.dim_x && y < world->worldgen.worldgen_parms.dim_y) {
                    
                    if (!world_data->region_map[x][y].flags.is_set(df::region_map_entry_flags::has_river)) {
                        ++invalid_river_endpoint_regions;
                    }

                    embark_assist::defs::region_tile_datum &region_tile_datum_river_end = survey_results->at(x).at(y);

                    embark_assist::defs::river_sizes river_size = embark_assist::defs::river_sizes::None;
                    
                    if (region_tile_datum_river_end.river_size == embark_assist::defs::river_sizes::None) {
                        if (world_data->rivers[i]->path.x.size() &&
                            world_data->rivers[i]->flow[world_data->rivers[i]->path.x.size() - 1] < 5000) {
                            if (world_data->region_map[x][y].flags.is_set(df::region_map_entry_flags::is_brook)) {
                                region_tile_datum_river_end.river_size = embark_assist::defs::river_sizes::Brook;
                            }
                            else {
                                region_tile_datum_river_end.river_size = embark_assist::defs::river_sizes::Stream;
                            }
                        }
                        else if (world_data->rivers[i]->flow[world_data->rivers[i]->path.x.size() - 1] < 10000) {
                            region_tile_datum_river_end.river_size = embark_assist::defs::river_sizes::Minor;
                        }
                        else if (world_data->rivers[i]->flow[world_data->rivers[i]->path.x.size() - 1] < 20000) {
                            region_tile_datum_river_end.river_size = embark_assist::defs::river_sizes::Medium;
                        }
                        else {
                            region_tile_datum_river_end.river_size = embark_assist::defs::river_sizes::Major;
                        }
                    }
                }
            }

            out.printerr("Invalid river regions counters - regular: %d, endpoints %d\n", invalid_regular_river_regions, invalid_river_endpoint_regions);
        }

        //=================================================================================

        void survey_evil_weather(embark_assist::defs::world_tile_data *survey_results) {
            df::world_data *world_data = world->world_data;

            for (uint16_t i = 0; i < world->interaction_instances.all.size(); i++) {
                auto interaction = world->raws.interactions[world->interaction_instances.all[i]->interaction_id];
                uint16_t region_index = world->interaction_instances.all[i]->region_index;
                bool blood_rain = false;
                bool permanent_syndrome_rain = false;
                bool temporary_syndrome_rain = false;
                bool thralling = false;
                bool reanimating = false;

                if (interaction->sources.size() &&
                    interaction->sources[0]->getType() == df::interaction_source_type::REGION) {
                    for (uint16_t k = 0; k < interaction->targets.size(); k++) {
                        if (interaction->targets[k]->getType() == df::interaction_target_type::CORPSE) {
                            for (uint16_t l = 0; l < interaction->effects.size(); l++) {
                                if (interaction->effects[l]->getType() == df::interaction_effect_type::ANIMATE) {
                                    reanimating = true;
                                    break;
                                }
                            }
                        }
                        else if (interaction->targets[k]->getType() == df::interaction_target_type::MATERIAL) {
                            df::interaction_target_materialst* material = virtual_cast<df::interaction_target_materialst>(interaction->targets[k]);
                            if (material && DFHack::MaterialInfo(material->mat_type, material->mat_index).isInorganic()) {
                                for (const auto &syndrome : world->raws.inorganics[material->mat_index]->material.syndrome) {
                                    for (const auto &ce : syndrome->ce) {
                                        df::creature_interaction_effect_type ce_type = ce->getType();
                                        if (ce_type == df::creature_interaction_effect_type::FLASH_TILE) {
                                            //  Using this as a proxy. There seems to be a group of 4 effects for thralls:
                                            //  display symbol, flash symbol, phys att change and one more.
                                            thralling = true;
                                        }
                                        else if (ce_type == df::creature_interaction_effect_type::PAIN ||
                                            ce_type == df::creature_interaction_effect_type::SWELLING ||
                                            ce_type == df::creature_interaction_effect_type::OOZING ||
                                            ce_type == df::creature_interaction_effect_type::BRUISING ||
                                            ce_type == df::creature_interaction_effect_type::BLISTERS ||
                                            ce_type == df::creature_interaction_effect_type::NUMBNESS ||
                                            ce_type == df::creature_interaction_effect_type::PARALYSIS ||
                                            ce_type == df::creature_interaction_effect_type::FEVER ||
                                            ce_type == df::creature_interaction_effect_type::BLEEDING ||
                                            ce_type == df::creature_interaction_effect_type::COUGH_BLOOD ||
                                            ce_type == df::creature_interaction_effect_type::VOMIT_BLOOD ||
                                            ce_type == df::creature_interaction_effect_type::NAUSEA ||
                                            ce_type == df::creature_interaction_effect_type::UNCONSCIOUSNESS ||
                                            ce_type == df::creature_interaction_effect_type::NECROSIS ||
                                            ce_type == df::creature_interaction_effect_type::IMPAIR_FUNCTION ||
                                            ce_type == df::creature_interaction_effect_type::DROWSINESS ||
                                            ce_type == df::creature_interaction_effect_type::DIZZINESS ||
                                            ce_type == df::creature_interaction_effect_type::ERRATIC_BEHAVIOR) {  // Doubtful if possible for region.
                                            if (ce->end == -1) {
                                                permanent_syndrome_rain = true;
                                            }
                                            else {
                                                temporary_syndrome_rain = true;
                                            }
                                        }
                                    }
                                }
                            }
                            else {  // If not inorganic it's always blood, as far as known.
                                blood_rain = true;
                            }
                        }
                    }
                }

                for (uint16_t k = 0; k < world_data->regions[region_index]->region_coords.size(); k++) {
                    auto &results = survey_results->at(world_data->regions[region_index]->region_coords[k].x).at(world_data->regions[region_index]->region_coords[k].y);
                    results.blood_rain[5] = blood_rain;
                    results.permanent_syndrome_rain[5] = permanent_syndrome_rain;
                    results.temporary_syndrome_rain[5] = temporary_syndrome_rain;
                    results.reanimating[5] = reanimating;
                    results.thralling[5] = thralling;
                }
            }

            for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
                for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
                    auto &results = survey_results->at(i).at(k);
                    results.blood_rain_possible = false;
                    results.permanent_syndrome_rain_possible = false;
                    results.temporary_syndrome_rain_possible = false;
                    results.reanimating_possible = false;
                    results.thralling_possible = false;
                    results.blood_rain_full = true;
                    results.permanent_syndrome_rain_full = true;
                    results.temporary_syndrome_rain_full = true;
                    results.reanimating_full = true;
                    results.thralling_full = true;

                    for (uint8_t l = 1; l < 10; l++) {
                        if (results.biome_index[l] != -1) {
                            df::coord2d adjusted = apply_offset(i, k, l);
                            results.blood_rain[l] = survey_results->at(adjusted.x).at(adjusted.y).blood_rain[5];
                            results.permanent_syndrome_rain[l] = survey_results->at(adjusted.x).at(adjusted.y).permanent_syndrome_rain[5];
                            results.temporary_syndrome_rain[l] = survey_results->at(adjusted.x).at(adjusted.y).temporary_syndrome_rain[5];
                            results.reanimating[l] = survey_results->at(adjusted.x).at(adjusted.y).reanimating[5];
                            results.thralling[l] = survey_results->at(adjusted.x).at(adjusted.y).thralling[5];

                            if (results.blood_rain[l]) {
                                results.blood_rain_possible = true;
                            }
                            else {
                                results.blood_rain_full = false;
                            }

                            if (results.permanent_syndrome_rain[l]) {
                                results.permanent_syndrome_rain_possible = true;
                            }
                            else {
                                results.permanent_syndrome_rain_full = false;
                            }

                            if (results.temporary_syndrome_rain[l]) {
                                results.temporary_syndrome_rain_possible = true;
                            }
                            else {
                                results.temporary_syndrome_rain_full = false;
                            }

                            if (results.reanimating[l]) {
                                results.reanimating_possible = true;
                            }
                            else {
                                results.reanimating_full = false;
                            }

                            if (results.thralling[l]) {
                                results.thralling_possible = true;
                            }
                            else {
                                results.thralling_full = false;
                            }
                        }
                    }
                }
            }
        }

        //=================================================================================

        int16_t min_temperature(int16_t max_temperature, uint16_t latitude) {
            uint16_t divisor;
            uint16_t steps;
            uint16_t lat;

            if (world->world_data->flip_latitude == df::world_data::T_flip_latitude::None) {
                return max_temperature;
            }

            else if (world->world_data->flip_latitude == df::world_data::T_flip_latitude::North ||
                world->world_data->flip_latitude == df::world_data::T_flip_latitude::South) {
                steps = world->world_data->world_height / 2;

                if (latitude > steps) {
                    lat = world->world_data->world_height - 1 - latitude;
                }
                else
                {
                    lat = latitude;
                }
            }
            else {  // Both
                steps = world->world_data->world_height / 4;

                if (latitude < steps) {
                    lat = latitude;
                }
                else if (latitude <= steps * 2) {
                    lat = steps * 2 - latitude;
                }
                else if (latitude <= steps * 3) {
                    lat = latitude - steps * 2;
                }
                else {
                    lat = world->world_data->world_height - latitude;
                }

            }

            if (world->world_data->world_height == 17) {
                divisor = (57 / steps * lat + 0.4);
            }
            else if (world->world_data->world_height == 33) {
                divisor = (61 / steps * lat + 0.1);
            }
            else if (world->world_data->world_height == 65) {
                divisor = (63 / steps * lat);
            }
            else if (world->world_data->world_height == 129 ||
                world->world_data->world_height == 257) {
                divisor = (64 / steps * lat);
                //divisor = ((lat * 64) / steps);
            }
            else {
                return max_temperature; // Not any standard world height. No formula available
            }

            return max_temperature - ceil(divisor * 3 / 4);
        }

        //=================================================================================

        void process_embark_incursion(embark_assist::defs::site_infos *site_info,
            embark_assist::defs::world_tile_data *survey_results,
            const embark_assist::defs::mid_level_tile_basic *mlt,  // Note this is a single tile, as opposed to most usages of this variable name.
            int16_t elevation,
            uint16_t x,
            uint16_t y) {

            if (mlt->aquifer) {
                site_info->aquifer = true;
            }
            else {
                site_info->aquifer_full = false;
            }

            if (mlt->soil_depth < site_info->min_soil) {
                site_info->min_soil = mlt->soil_depth;
            }

            if (mlt->soil_depth > site_info->max_soil) {
                site_info->max_soil = mlt->soil_depth;
            }

            if (elevation != mlt->elevation) {
                site_info->flat = false;
            }

            if (mlt->clay) {
                site_info->clay = true;
            }

            if (mlt->sand) {
                site_info->sand = true;
            }

            if (survey_results->at(x).at(y).blood_rain [mlt->biome_offset]) {
                site_info->blood_rain = true;
            }

            if (survey_results->at(x).at(y).permanent_syndrome_rain[mlt->biome_offset]) {
                site_info->permanent_syndrome_rain = true;
            }

            if (survey_results->at(x).at(y).temporary_syndrome_rain[mlt->biome_offset]) {
                site_info->temporary_syndrome_rain = true;
            }

            if (survey_results->at(x).at(y).reanimating[mlt->biome_offset]) {
                site_info->reanimating = true;
            }

            if (survey_results->at(x).at(y).thralling[mlt->biome_offset]) {
                site_info->thralling = true;
            }
        }

        //=================================================================================

        void process_embark_incursion_mid_level_tile(uint8_t from_direction,
            embark_assist::defs::site_infos *site_info,
            embark_assist::defs::world_tile_data *survey_results,
            embark_assist::defs::mid_level_tiles *mlt,
            const uint8_t i,
            const uint8_t k) {

            int8_t fetch_i = i;
            int8_t fetch_k = k;
            int16_t fetch_x = state->x;
            int16_t fetch_y = state->y;
            df::world_data *world_data = world->world_data;
            embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);

            //  Logic can be implemented with modulo and division, but that's harder to read.
            switch (from_direction) {
            case 0:
                fetch_i = i - 1;
                fetch_k = k - 1;
                break;

            case 1:
                fetch_k = k - 1;
                break;

            case 2:
                fetch_i = i + 1;
                fetch_k = k - 1;
                break;

            case 3:
                fetch_i = i - 1;
                break;

            case 4:
                return;   //  Own tile provides the data, so there's no incursion.
                break;

            case 5:
                fetch_i = i + 1;
                break;

            case 6:
                fetch_i = i - 1;
                fetch_k = k + 1;
                break;

            case 7:
                fetch_k = k + 1;
                break;

            case 8:
                fetch_i = i + 1;
                fetch_k = k + 1;
                break;
            }

            if (fetch_i < 0) {
                fetch_x = state->x - 1;
            }
            else if (fetch_i > 15) {
                fetch_x = state->x + 1;
            }

            if (fetch_k < 0) {
                fetch_y = state->y - 1;
            }
            else if (fetch_k > 15) {
                fetch_y = state->y + 1;
            }

            if (fetch_x < 0 ||
                fetch_x == world_data->world_width ||
                fetch_y < 0 ||
                fetch_y == world_data->world_height) {
                return;  //  We're at the world edge, so no incursions from the outside.
            }

            const embark_assist::defs::region_tile_datum &region_tile_datum = survey_results->at(fetch_x).at(fetch_y);

            if (fetch_k < 0) {
                if (fetch_i < 0) {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.south_row[15],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
                else if (fetch_i > 15) {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.south_row[0],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
                else {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.south_row[fetch_i],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
            }
            else if (fetch_k > 15) {
                if (fetch_i < 0) {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.north_row[15],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
                else if (fetch_i > 15) {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.north_row[0],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
                else {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.north_row[fetch_i],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
            }
            else {
                if (fetch_i < 0) {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.east_column[fetch_k],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
                else if (fetch_i > 15) {
                    process_embark_incursion(site_info,
                        survey_results,
                        &region_tile_datum.west_column[fetch_k],
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
                else {
                    process_embark_incursion(site_info,
                        survey_results,
                        &mlt->at(fetch_i).at(fetch_k),
                        mid_level_tile.elevation,
                        fetch_x,
                        fetch_y);
                }
            }
        }
    }
}

//=================================================================================
//  Exported operations
//=================================================================================

void embark_assist::survey::setup(uint16_t max_inorganic) {
    embark_assist::survey::state = new(embark_assist::survey::states);
    embark_assist::survey::state->max_inorganic = max_inorganic;
    embark_assist::survey::state->rivers_file = fopen(rivers_file_name, "a");
    embark_assist::survey::state->river_anomalies_file = fopen(river_anomalies_file_name, "a");
    embark_assist::survey::state->invalid_river_file = fopen(invalid_rivers_file_name, "a");

    const auto empty = []() {};
    state->regular_data_processing_result = std::async(std::launch::deferred, empty);
    state->internal_incursion_processing_result = std::async(std::launch::deferred, empty);
    state->external_incursion_processing_result = std::async(std::launch::deferred, empty);

    color_ostream_proxy out(Core::getInstance().getConsole());
    uint16_t pool_size = 0;
    for (auto biome : world->world_data->geo_biomes) {
        pool_size += biome->layers.size();
    }
    
    /*embark_assist::survey::state->layer_content_cache_entry_pool.reserve(pool_size);
    embark_assist::survey::state->layer_content_cache_entry_pool.resize(pool_size);
    embark_assist::survey::state->layer_content_cache.reserve(world->world_data->geo_biomes.size() * 16);*/

    out.print("embark_assist::survey::setup: pool_size: %d, world->world_data->geo_biomes.size(): %d\n", pool_size, world->world_data->geo_biomes.size());
}

//=================================================================================

df::coord2d embark_assist::survey::get_last_pos() {
    return{ embark_assist::survey::state->x, embark_assist::survey::state->y };
}

//=================================================================================

void embark_assist::survey::initiate(embark_assist::defs::mid_level_tiles *mlt) {
    for (uint8_t i = 0; i < 16; i++) {
        for (uint8_t k = 0; k < 16; k++) {
            mlt->at(i).at(k).metals.resize(state->max_inorganic);
            mlt->at(i).at(k).economics.resize(state->max_inorganic);
            mlt->at(i).at(k).minerals.resize(state->max_inorganic);
        }
    }
}

//=================================================================================

void embark_assist::survey::clear_results(embark_assist::defs::match_results *match_results) {
    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
            match_results->at(i).at(k).preliminary_match = false;
            match_results->at(i).at(k).contains_match = false;

            for (uint16_t l = 0; l < 16; l++) {
                for (uint16_t m = 0; m < 16; m++) {
                    match_results->at(i).at(k).mlt_match[l][m] = false;
                }
            }
        }
    }
}

//=================================================================================

void embark_assist::survey::high_level_world_survey(embark_assist::defs::geo_data *geo_summary,
    embark_assist::defs::world_tile_data *survey_results) {
//    color_ostream_proxy out(Core::getInstance().getConsole());

    int16_t temperature;
    bool negative;

    embark_assist::survey::geo_survey(geo_summary);
    for (uint16_t i = 0; i < world->worldgen.worldgen_parms.dim_x; i++) {
        for (uint16_t k = 0; k < world->worldgen.worldgen_parms.dim_y; k++) {
            df::coord2d adjusted;
            df::world_data *world_data = world->world_data;
            uint16_t geo_index;
            uint16_t sav_ev;
            uint8_t offset_count = 0;
            auto &results = survey_results->at(i).at(k);
            results.surveyed = false;
            results.aquifer_count = 0;
            results.clay_count = 0;
            results.sand_count = 0;
            results.flux_count = 0;
            results.coal_count = 0;
            results.min_region_soil = 10;
            results.max_region_soil = 0;
            results.max_waterfall = 0;
            results.savagery_count[0] = 0;
            results.savagery_count[1] = 0;
            results.savagery_count[2] = 0;
            results.evilness_count[0] = 0;
            results.evilness_count[1] = 0;
            results.evilness_count[2] = 0;
            results.metals.resize(state->max_inorganic);
            results.economics.resize(state->max_inorganic);
            results.minerals.resize(state->max_inorganic);
            //  Evil weather and rivers are handled in later operations. Should probably be merged into one.

            for (uint8_t l = 1; l < 10; l++)
            {
                adjusted = apply_offset(i, k, l);
                if (adjusted.x != i || adjusted.y != k || l == 5) {
                    offset_count++;

                    const df::region_map_entry &region_map_entry = world_data->region_map[adjusted.x][adjusted.y];
                    results.biome_index[l] = region_map_entry.region_id;
                    results.biome[l] = DFHack::Maps::GetBiomeTypeWithRef(adjusted.x, adjusted.y, k);
                    temperature = region_map_entry.temperature;
                    negative = temperature < 0;

                    if (negative) {
                        temperature = -temperature;
                    }

                    results.max_temperature[l] = (temperature / 4) * 3;
                    if (temperature % 4 > 1) {
                        results.max_temperature[l] = results.max_temperature[l] + temperature % 4 - 1;
                    }

                    if (negative) {
                        results.max_temperature[l] = -results.max_temperature[l];
                    }

                    results.min_temperature[l] = min_temperature(results.max_temperature[l], adjusted.y);
                    geo_index = region_map_entry.geo_index;

                    const embark_assist::defs::geo_datum &geo_datum = geo_summary->at(geo_index);
                    if (!geo_datum.aquifer_absent) results.aquifer_count++;
                    if (!geo_datum.clay_absent) results.clay_count++;
                    if (!geo_datum.sand_absent) results.sand_count++;
                    if (!geo_datum.flux_absent) results.flux_count++;
                    if (!geo_datum.coal_absent) results.coal_count++;

                    if (geo_datum.soil_size < results.min_region_soil)
                        results.min_region_soil = geo_datum.soil_size;

                    if (geo_datum.soil_size > results.max_region_soil)
                        results.max_region_soil = geo_datum.soil_size;

                    sav_ev = region_map_entry.savagery / 33;
                    if (sav_ev == 3) sav_ev = 2;
                    results.savagery_count[sav_ev]++;

                    sav_ev = region_map_entry.evilness / 33;
                    if (sav_ev == 3) sav_ev = 2;
                    results.evilness_count[sav_ev]++;

                    for (uint16_t m = 0; m < state->max_inorganic; m++) {
                        if (geo_datum.possible_metals[m]) results.metals[m] = true;
                        if (geo_datum.possible_economics[m]) results.economics[m] = true;
                        if (geo_datum.possible_minerals[m]) results.minerals[m] = true;
                    }
                }
                else {
                    results.biome_index[l] = -1;
                    results.biome[l] = -1;
                    results.max_temperature[l] = -30000;
                    results.min_temperature[l] = -30000;
                }
            }

            results.biome_count = 0;
            for (uint8_t l = 1; l < 10; l++) {
                if (results.biome[l] != -1) results.biome_count++;
            }

            if (results.aquifer_count == offset_count) results.aquifer_count = 256;
            if (results.clay_count == offset_count) results.clay_count = 256;
            if (results.sand_count == offset_count) results.sand_count = 256;
            if (results.flux_count == offset_count) results.flux_count = 256;
            if (results.coal_count == offset_count) results.coal_count = 256;

            for (uint8_t l = 0; l < 3; l++) {
                if (results.savagery_count[l] == offset_count) results.savagery_count[l] = 256;
                if (results.evilness_count[l] == offset_count) results.evilness_count[l] = 256;
            }
        }
    }

    embark_assist::survey::survey_rivers(survey_results);
    embark_assist::survey::survey_evil_weather(survey_results);
}

//=================================================================================

embark_assist::defs::river_sizes map_river_width_to_size(const int16_t river_width, const embark_assist::defs::river_sizes region_river_size, const bool is_brook) {
    if (region_river_size == embark_assist::defs::river_sizes::Brook || is_brook) {
        if (river_width != 3) {
            // TODO output brook with river_width larger than 3
        }
        return embark_assist::defs::river_sizes::Brook;
    }
    else if (river_width <= 6) {
        return embark_assist::defs::river_sizes::Stream;
    }
    else if (river_width > 6 && river_width <= 11) {
        return embark_assist::defs::river_sizes::Minor;
    }
    else if (river_width > 11 && river_width < 23) {
        return embark_assist::defs::river_sizes::Medium;
    }
    return embark_assist::defs::river_sizes::Major;
}

//void survey_layers(embark_assist::defs::geo_data *geo_summary, df::world_data *world_data, df::region_map_entry *world_tile, embark_assist::defs::mid_level_tiles *mlt,
//    const uint16_t state_clay_reaction, const uint16_t state_flux_reaction, const std::vector<uint16_t> state_coals,
//    embark_assist::key_buffer_holder::key_buffer_holder &buffer_holder, embark_assist::defs::index_interface &index) {
//    const auto start = std::chrono::steady_clock::now();
//    auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
//    int16_t x = screen->location.region_pos.x;
//    int16_t y = screen->location.region_pos.y;
//    int8_t max_soil_depth;
//    int8_t offset;
//    int16_t elevation;
//    int16_t last_bottom;
//    int16_t top_z;
//    int16_t min_z = 0;  //  Initialized to silence warning about potential usage of uninitialized data.
//    int16_t bottom_z;
//    int16_t base_z;
//    df::coord2d adjusted;
//    df::world_region_details *details = world_data->region_details[0];
//    uint8_t soil_erosion;
//    uint16_t end_check_l;
//    uint16_t end_check_m;
//    uint16_t end_check_n;
//    std::vector <df::world_region_feature *> features;
//
//    color_ostream_proxy out(Core::getInstance().getConsole());
//
//    const uint32_t world_offset = index.get_key(x, y);
//    uint32_t mlt_offset = 0;
//
//    // beware: we process "first" in k(y) than in the i(x) dimension, otherwise the key can't be calculated just be using a rolling offset
//    // the inner loop processes one row, then the next row with a higher k/y value
//    for (uint8_t k = 0; k < 16; k++) {
//        for (uint8_t i = 0; i < 16; i++) {
//            const uint32_t key = world_offset + mlt_offset++;
//            embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);
//            max_soil_depth = -1;
//
//            offset = details->biome[i][k];
//            adjusted = embark_assist::survey::apply_offset(x, y, offset);
//
//            const df::region_map_entry &region_map_entry = world_data->region_map[adjusted.x][adjusted.y];
//            elevation = details->elevation[i][k];
//
//            // Special biome adjustments
//            if (!region_map_entry.flags.is_set(region_map_entry_flags::is_lake)) {
//                if (region_map_entry.elevation >= 150) {  //  Mountain
//                    max_soil_depth = 0;
//
//                }
//                else if (region_map_entry.elevation < 100) {  // Ocean
//                    if (elevation == 99) {
//                        elevation = 98;
//                    }
//
//                    if ((world_data->geo_biomes[world_data->region_map[x][y].geo_index]->unk1 == 4 ||
//                        world_data->geo_biomes[world_data->region_map[x][y].geo_index]->unk1 == 5) &&
//                        details->unk12e8 < 500) {
//                        max_soil_depth = 0;
//                    }
//                }
//            }
//
//            // base_z = elevation - 1;
//            features = details->features[i][k];
//            //std::map<int, int> layer_bottom, layer_top;
//            //mid_level_tile.adamantine_level = -1;
//            //mid_level_tile.magma_level = -1;
//
//            end_check_l = static_cast<uint16_t>(features.size());
//            for (size_t l = 0; l < end_check_l; l++) {
//                auto feature = features[l];
//
//                if (feature->layer != -1 &&
//                    feature->min_z != -30000) {
//                    auto layer = world_data->underground_regions[feature->layer];
//
//                    //layer_bottom[layer->layer_depth] = feature->min_z;
//                    //layer_top[layer->layer_depth] = feature->max_z;
//                    //base_z = std::min((int)base_z, (int)feature->min_z);
//
//                    if (layer->type == df::world_underground_region::MagmaSea) {
//                        min_z = feature->min_z;  //  The features are individual per region tile
//                    }
//                }
//            }
//
//            //  Compute shifts for layers in the stack.
//
//            if (max_soil_depth == -1) {  //  Not set to zero by the biome
//                max_soil_depth = std::max((154 - elevation) / 5, 1);
//            }
//
//            soil_erosion = geo_summary->at(region_map_entry.geo_index).soil_size -
//                std::min((int)geo_summary->at(region_map_entry.geo_index).soil_size, (int)max_soil_depth);
//            int16_t layer_shift[16];
//            int16_t cur_shift = elevation + soil_erosion - 1;
//
//            mid_level_tile.aquifer = false;
//            mid_level_tile.clay = false;
//            mid_level_tile.sand = false;
//            mid_level_tile.flux = false;
//            mid_level_tile.coal = false;
//
//            if (max_soil_depth == 0) {
//                mid_level_tile.soil_depth = 0;
//            }
//            else {
//                mid_level_tile.soil_depth = geo_summary->at(region_map_entry.geo_index).soil_size - soil_erosion;
//            }
//
//            mid_level_tile.offset = offset;
//            mid_level_tile.elevation = details->elevation[i][k];
//
//            end_check_l = static_cast<uint16_t>(world_data->geo_biomes[region_map_entry.geo_index]->layers.size());
//            if (end_check_l > 16) end_check_l = 16;
//
//            for (uint16_t l = 0; l < end_check_l; l++) {
//                auto layer = world_data->geo_biomes[region_map_entry.geo_index]->layers[l];
//                layer_shift[l] = cur_shift;
//
//                if (layer->type == df::geo_layer_type::SOIL ||
//                    layer->type == df::geo_layer_type::SOIL_SAND) {
//                    int16_t size = layer->top_height - layer->bottom_height - 1;
//                    //  Comment copied from prospector.cpp (like the logic)...
//                    //  This is to replicate the behavior of a probable bug in the
//                    //  map generation code : if a layer is partially eroded, the
//                    //  removed levels are in fact transferred to the layer below,
//                    //  because unlike the case of removing the whole layer, the code
//                    //  does not execute a loop to shift the lower part of the stack up.
//                    if (size > soil_erosion) {
//                        cur_shift = cur_shift - soil_erosion;
//                    }
//
//                    soil_erosion -= std::min((int)soil_erosion, (int)size);
//                }
//            }
//
//            last_bottom = elevation;
//            //  Don't have to set up the end_check as we can reuse the one above.
//
//            for (uint16_t l = 0; l < end_check_l; l++) {
//                auto layer = world_data->geo_biomes[region_map_entry.geo_index]->layers[l];
//                top_z = last_bottom - 1;
//                bottom_z = std::max((int)layer->bottom_height + layer_shift[l], (int)min_z);
//
//                if (l == 15) {
//                    bottom_z = min_z;  // stretch layer if needed
//                }
//
//                if (top_z >= bottom_z) {
//                    last_bottom = bottom_z;
//
//                    if (embark_assist::survey::process_layer_from_cache(layer, mid_level_tile, buffer_holder, key, bottom_z, elevation)) {
//                        continue;
//                    }
//                    embark_assist::survey::layer_content_cache_entry &cache_entry = embark_assist::survey::state->layer_content_cache_entry_pool[embark_assist::survey::state->layer_content_cache_entry_pool_index++];
//                    if (embark_assist::survey::state->layer_content_cache.count(layer) > 0) {
//                        out.print("should not happen!\n");
//                    }
//                    else {
//                        embark_assist::survey::state->layer_content_cache[layer] = &cache_entry;
//                    }
//
//                    if (!mid_level_tile.minerals[layer->mat_index]) {
//                        buffer_holder.add_mineral(key, layer->mat_index);
//                    }
//                    cache_entry.minerals.insert(layer->mat_index);
//                    mid_level_tile.minerals[layer->mat_index] = true;
//
//                    end_check_m = static_cast<uint16_t>(world->raws.inorganics[layer->mat_index]->metal_ore.mat_index.size());
//
//                    for (uint16_t m = 0; m < end_check_m; m++) {
//                        if (!mid_level_tile.metals[world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]]) {
//                            buffer_holder.add_metal(key, world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]);
//                        }
//                        cache_entry.metals.insert(world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]);
//                        mid_level_tile.metals[world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]] = true;
//                    }
//
//                    if (layer->type == df::geo_layer_type::SOIL ||
//                        layer->type == df::geo_layer_type::SOIL_SAND) {
//                        if (world->raws.inorganics[layer->mat_index]->flags.is_set(df::inorganic_flags::SOIL_SAND)) {
//                            if (!mid_level_tile.sand) {
//                                buffer_holder.add_sand(key);
//                            }
//                            cache_entry.sand = true;
//                            mid_level_tile.sand = true;
//                        }
//                    }
//
//                    if (world->raws.inorganics[layer->mat_index]->economic_uses.size() > 0) {
//                        if (!mid_level_tile.economics[layer->mat_index]) {
//                            buffer_holder.add_economic(key, layer->mat_index);
//                        }
//                        cache_entry.economics.insert(layer->mat_index);
//                        mid_level_tile.economics[layer->mat_index] = true;
//
//                        end_check_m = static_cast<uint16_t>(world->raws.inorganics[layer->mat_index]->economic_uses.size());
//                        for (uint16_t m = 0; m < end_check_m; m++) {
//                            if (world->raws.inorganics[layer->mat_index]->economic_uses[m] == state_clay_reaction) {
//                                if (!mid_level_tile.clay) {
//                                    buffer_holder.add_clay(key);
//                                }
//                                cache_entry.clay = true;
//                                mid_level_tile.clay = true;
//                            }
//
//                            else if (world->raws.inorganics[layer->mat_index]->economic_uses[m] == state_flux_reaction) {
//                                if (!mid_level_tile.flux) {
//                                    buffer_holder.add_flux(key);
//                                }
//                                cache_entry.flux = true;
//                                mid_level_tile.flux = true;
//                            }
//                        }
//
//                        for (uint16_t m = 0; m < state_coals.size(); m++) {
//                            if (layer->mat_index == state_coals[m]) {
//                                if (!mid_level_tile.coal) {
//                                    buffer_holder.add_coal(key);
//                                }
//                                cache_entry.coal = true;
//                                mid_level_tile.coal = true;
//                                break;
//                            }
//                        }
//                    }
//
//                    end_check_m = static_cast<uint16_t>(layer->vein_mat.size());
//
//                    for (uint16_t m = 0; m < end_check_m; m++) {
//                        if (!mid_level_tile.minerals[layer->vein_mat[m]]) {
//                            buffer_holder.add_mineral(key, layer->vein_mat[m]);
//                        }
//                        cache_entry.minerals.insert(layer->vein_mat[m]);
//                        mid_level_tile.minerals[layer->vein_mat[m]] = true;
//
//                        end_check_n = static_cast<uint16_t>(world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index.size());
//
//                        for (uint16_t n = 0; n < end_check_n; n++) {
//                            if (!mid_level_tile.metals[world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]]) {
//                                buffer_holder.add_metal(key, world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]);
//                            }
//                            cache_entry.metals.insert(world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]);
//                            mid_level_tile.metals[world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]] = true;
//                        }
//
//                        if (world->raws.inorganics[layer->vein_mat[m]]->economic_uses.size() > 0) {
//                            if (!mid_level_tile.economics[layer->vein_mat[m]]) {
//                                buffer_holder.add_economic(key, layer->vein_mat[m]);
//                            }
//                            cache_entry.economics.insert(layer->vein_mat[m]);
//                            mid_level_tile.economics[layer->vein_mat[m]] = true;
//
//                            end_check_n = static_cast<uint16_t>(world->raws.inorganics[layer->vein_mat[m]]->economic_uses.size());
//                            for (uint16_t n = 0; n < end_check_n; n++) {
//                                if (world->raws.inorganics[layer->vein_mat[m]]->economic_uses[n] == state_clay_reaction) {
//                                    if (!mid_level_tile.clay) {
//                                        buffer_holder.add_clay(key);
//                                    }
//                                    cache_entry.clay = true;
//                                    mid_level_tile.clay = true;
//                                }
//
//                                else if (world->raws.inorganics[layer->vein_mat[m]]->economic_uses[n] == state_flux_reaction) {
//                                    if (!mid_level_tile.flux) {
//                                        buffer_holder.add_flux(key);
//                                    }
//                                    cache_entry.flux = true;
//                                    mid_level_tile.flux = true;
//                                }
//                            }
//
//                            for (uint16_t n = 0; n < state_coals.size(); n++) {
//                                if (layer->vein_mat[m] == state_coals[n]) {
//                                    if (!mid_level_tile.coal) {
//                                        buffer_holder.add_coal(key);
//                                    }
//                                    cache_entry.coal = true;
//                                    mid_level_tile.coal = true;
//                                    break;
//                                }
//                            }
//                        }
//                    }
//
//                    if (world->raws.inorganics[layer->mat_index]->flags.is_set(df::inorganic_flags::AQUIFER)) {
//                        cache_entry.is_aquifer_layer = true;
//                        if (bottom_z <= elevation - 3) {
//                            if (!mid_level_tile.aquifer) {
//                                buffer_holder.add_aquifer(key);
//                            }
//                            mid_level_tile.aquifer = true;
//                        }
//                    }
//                }
//            }
//        }
//    }
//}


// FIXME for debugging - remove for release
void log_incursion_data(std::ofstream &file, 
    const int x, const int y, const int i, const int k, const int key, 
    const int biome_id, const embark_assist::defs::mid_level_tile_basic &data) {

    file
        << x << ";"
        << y << ";"
        << (int)i << ";"
        << (int)k << ";"
        << (int)key << ";"
        << data.aquifer << ";"
        << data.clay << ";"
        << data.sand << ";"
        << data.elevation << ";"
        << (int)data.biome_offset << ";"
        << (int)biome_id << ";"
        << (int)data.soil_depth << ";"
        << (int)data.evilness_level << ";"
        << (int)data.savagery_level << ";"
        << "\n";
}

void write_array(std::ofstream &file, int8_t values[16][16]) {
    for (int8_t y = 0; y < 16; y++) {
        for (int8_t x = 0; x < 16; x++) {
            file << std::to_string(values[x][y]) << ";";
        }
        file << "\n";
    }
}

void write_biomes_edges_to_file(int16_t x, int16_t y,  df::world_region_details::T_edges edges) {
    //std::copy(iterInStart, iterInEnd, iterOutStart);
    //embark_assist::defs::region_tile_datum tile;
    //std::copy(&edges.biome_x[0][0], &edges.biome_x[0][0] + 16 * 16, &tile.biome_x[0][0]);
    
    const std::string x_string = std::to_string(x);
    const std::string y_string = std::to_string(y);

    std::ofstream biome_corner = std::ofstream(edge_root_folder_path + x_string + "/" + y_string + "_biome_corner.csv", std::ios::out);
    write_array(biome_corner, edges.biome_corner);
    biome_corner.close();

    std::ofstream biome_x = std::ofstream(edge_root_folder_path + x_string + "/" + y_string + "_biome_x.csv", std::ios::out);
    write_array(biome_x, edges.biome_x);
    biome_x.close();

    std::ofstream biome_y = std::ofstream(edge_root_folder_path + x_string + "/" + y_string + "_biome_y.csv", std::ios::out);
    write_array(biome_y, edges.biome_y);
    biome_y.close();
}

void write_savagery_2_to_file(const uint32_t target_key) {
    const std::string x_string = "";

    std::ofstream file = std::ofstream(index_folder_name + x_string + "_savagery_2_native.txt", std::ios::out | std::ios::app);
    file << target_key << "\n";
    file.close();
}

int32_t calculate_flow(const int16_t width) {
    return (width - 1) * 40000 / 46;
}

void embark_assist::survey::survey_mid_level_tile(embark_assist::defs::geo_data *geo_summary,
    embark_assist::defs::world_tile_data *survey_results,
    embark_assist::defs::mid_level_tiles *mlt,
    embark_assist::defs::index_interface &index,
    const output_coordinates output_coordinates) {
//                color_ostream_proxy out(Core::getInstance().getConsole());
    const auto start = std::chrono::steady_clock::now();
    auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
    int16_t x = screen->location.region_pos.x;
    int16_t y = screen->location.region_pos.y;
    embark_assist::defs::region_tile_datum &tile = survey_results->at(x).at(y);
    int8_t max_soil_depth;
    int8_t offset;
    int16_t elevation;
    int16_t last_bottom;
    int16_t top_z;
    int16_t base_z;
    int16_t min_z = 0;  //  Initialized to silence warning about potential usage of uninitialized data.
    int16_t bottom_z;
    df::coord2d adjusted;
    df::world_data *world_data = world->world_data;
    df::world_region_details *details = world_data->region_details[0];
    df::region_map_entry *world_tile = &world_data->region_map[x][y];
    std::vector <df::world_region_feature *> features;
    uint8_t soil_erosion;
    uint16_t end_check_l;
    uint16_t end_check_m;
    uint16_t end_check_n;

    int16_t max_river_width = 0;
    bool has_actual_river = false;
    const bool has_river = world_data->region_map[x][y].flags.is_set(df::region_map_entry_flags::has_river);
    const bool is_brook = world_data->region_map[x][y].flags.is_set(df::region_map_entry_flags::is_brook);
    color_ostream_proxy out(Core::getInstance().getConsole());
    //out.print("x,y: %02d,%02d, has_river: %d, is_brook: %d, tile.river_size: %d - ", x, y, has_river, is_brook, tile.river_size);
    if (output_coordinates == output_coordinates::SHOW_COORDINATES) {
        out.print("x,y: %02d,%02d\n", x, y);
    }

    const uint32_t world_offset = index.get_key(x, y);

    // write_biomes_edges_to_file(x, y, details->edges);
    
    // making sure that the previous async/thread is finished before resetting buffer_holder, as this is possibly still being used by the thread
    if (state->regular_data_processing_result.valid()) {
        state->regular_data_processing_result.get();
    }
    //state->regular_data_processing_result.wait();

    embark_assist::key_buffer_holder::key_buffer_holder &buffer_holder = state->buffer_holder;
    buffer_holder.reset();

    for (uint16_t i = 0; i < state->max_inorganic; i++) {
        tile.metals[i] = false;
        tile.economics[i] = false;
        tile.minerals[i] = false;
    }

    for (uint8_t i = 1; i < 10; i++) tile.biome_index[i] = -1;

    // needed for asynchronous/concurrent incursion processing
    std::copy(&details->edges.biome_x[0][0], &details->edges.biome_x[0][0] + 16 * 16, &tile.biome_x[0][0]);
    std::copy(&details->edges.biome_y[0][0], &details->edges.biome_y[0][0] + 16 * 16, &tile.biome_y[0][0]);
    std::copy(&details->edges.biome_corner[0][0], &details->edges.biome_corner[0][0] + 16 * 16, &tile.biome_corner[0][0]);

    if (mlt->at(0).at(0).metals.size() == state->max_inorganic) {
        for (uint8_t i = 0; i < 16; i++) {
            for (uint8_t k = 0; k < 16; k++) {
                embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);
                //mid_level_tile.metals.assign(state->max_inorganic, false);
                // FIXME: profile performance difference between assign and fill
                std::fill(mid_level_tile.metals.begin(), mid_level_tile.metals.end(), false);
                //mid_level_tile.economics.assign(state->max_inorganic, false);
                std::fill(mid_level_tile.economics.begin(), mid_level_tile.economics.end(), false);
                //mid_level_tile.minerals.assign(state->max_inorganic, false);
                std::fill(mid_level_tile.minerals.begin(), mid_level_tile.minerals.end(), false);
            }
        }
    } else {
        embark_assist::survey::initiate(mlt);
    }

    // making sure that the previous async/thread is finished before resetting mlt, as this is possibly still being used by the thread
    if (state->internal_incursion_processing_result.valid()) {
        state->internal_incursion_processing_result.get();
    }
    //state->internal_incursion_processing_result.wait();

    if (state->external_incursion_processing_result.valid()) {
        state->external_incursion_processing_result.get();
    }
    //state->external_incursion_processing_result.wait();

    const auto _1_start_big_loop = std::chrono::steady_clock::now();
    uint32_t mlt_offset = 0;

    /*auto survey_layers_result = std::async(std::launch::async, [&]() { survey_layers(geo_summary, world_data, world_tile, mlt, 
        state->clay_reaction, state->flux_reaction, state->coals, buffer_holder, index); });*/

    //survey_layers(geo_summary, world_data, world_tile, mlt, state->clay_reaction, state->flux_reaction, state->coals, buffer_holder, index);

    // beware: we process "first" in k(y) than in the i(x) dimension, otherwise the key can't be calculated just be using a rolling offset
    // the inner loop processes one row, then the next row with a higher k/y value
    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            const uint32_t key = world_offset + mlt_offset++;
            embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);
            max_soil_depth = -1;

            offset = details->biome[i][k];
            adjusted = apply_offset(x, y, offset);

            if (adjusted.x != x || adjusted.y != y)
            {
                mid_level_tile.biome_offset = offset;
            }
            else
            {
                mid_level_tile.biome_offset = 5;
            }
            const df::region_map_entry &region_map_entry = world_data->region_map[adjusted.x][adjusted.y];

            tile.biome_index[mid_level_tile.biome_offset] = region_map_entry.region_id;
            const int16_t biome_id = tile.biome[mid_level_tile.biome_offset];
            if (biome_id < 0 || biome_id > 50) {
                color_ostream_proxy out(Core::getInstance().getConsole());
                out.print("invalid biome_id %d\n", biome_id);
            }
            buffer_holder.add_biome(key, biome_id);
            const df::world_region_type region_type = world_data->regions[tile.biome_index[mid_level_tile.biome_offset]]->type;
            const int16_t region_type_id = region_type;
            // moved from below
            tile.region_type[i][k] = region_type;
            if (region_type_id < 0 || region_type_id > 10) {
                color_ostream_proxy out(Core::getInstance().getConsole());
                out.print("invalid region_type_id %d\n", region_type_id);
            }
            buffer_holder.add_region_type(key, region_type_id);

            if (tile.blood_rain[mid_level_tile.biome_offset]) {
                buffer_holder.add_blood_rain(key);
            }

            if (tile.min_temperature[mid_level_tile.biome_offset] <= 0) {
                buffer_holder.add_temperatur_freezing(key, embark_assist::key_buffer_holder::temperatur::MIN_ZERO_OR_BELOW);
            }
            else {
                buffer_holder.add_temperatur_freezing(key, embark_assist::key_buffer_holder::temperatur::MIN_ABOVE_ZERO);
            }

            if (tile.max_temperature[mid_level_tile.biome_offset] <= 0) {
                buffer_holder.add_temperatur_freezing(key, embark_assist::key_buffer_holder::temperatur::MAX_ZERO_OR_BELOW);
            }
            else {
                buffer_holder.add_temperatur_freezing(key, embark_assist::key_buffer_holder::temperatur::MAX_ABOVE_ZERO);
            }

            if (tile.permanent_syndrome_rain[mid_level_tile.biome_offset]) {
                buffer_holder.add_syndrome_rain(key, embark_assist::key_buffer_holder::syndrome_rain_index::PERMANENT_SYNDROME);
            }
            else {
                buffer_holder.add_syndrome_rain(key, embark_assist::key_buffer_holder::syndrome_rain_index::NO_PERMANENT_SYNDROME);
            }

            if (tile.temporary_syndrome_rain[mid_level_tile.biome_offset]) {
                buffer_holder.add_syndrome_rain(key, embark_assist::key_buffer_holder::syndrome_rain_index::TEMPORARY_SYNDROME);
            }
            else {
                buffer_holder.add_syndrome_rain(key, embark_assist::key_buffer_holder::syndrome_rain_index::NO_TEMPORARY_SYNDROME);
            }

            mid_level_tile.savagery_level = region_map_entry.savagery / 33;
            if (mid_level_tile.savagery_level == 3) {
                mid_level_tile.savagery_level = 2;
            }
            buffer_holder.add_savagery_level(key, mid_level_tile.savagery_level);
            /*if (mid_level_tile.savagery_level == 2 && key >= 71936 && key <= 72191) {
                write_savagery_2_to_file(key);
            }*/
            mid_level_tile.evilness_level = region_map_entry.evilness / 33;
            if (mid_level_tile.evilness_level == 3) {
                mid_level_tile.evilness_level = 2;
            }
            buffer_holder.add_evilness_level(key, mid_level_tile.evilness_level);

            elevation = details->elevation[i][k];

            // Special biome adjustments
            if (!region_map_entry.flags.is_set(region_map_entry_flags::is_lake)) {
                if (region_map_entry.elevation >= 150) {  //  Mountain
                    max_soil_depth = 0;
                }
                else if (region_map_entry.elevation < 100) {  // Ocean
                    if (elevation == 99) {
                        elevation = 98;
                    }

                    if ((world_data->geo_biomes[world_data->region_map[x][y].geo_index]->unk1 == 4 ||
                        world_data->geo_biomes[world_data->region_map[x][y].geo_index]->unk1 == 5) &&
                        details->unk12e8 < 500) {
                        max_soil_depth = 0;
                    }
                }
            }

            base_z = elevation - 1;
            features = details->features[i][k];
            //std::map<int, int> layer_bottom, layer_top;
            mid_level_tile.adamantine_level = -1;
            mid_level_tile.magma_level = -1;

            end_check_l = static_cast<uint16_t>(features.size());
            for (size_t l = 0; l < end_check_l; l++) {
                auto feature = features[l];

                if (feature->feature_idx != -1) {
                    switch (world_data->feature_map[x / 16][y / 16].features->feature_init[x % 16][y % 16][feature->feature_idx]->getType())
                    {
                    case df::feature_type::deep_special_tube:
                        mid_level_tile.adamantine_level = world_data->feature_map[x / 16][y / 16].features->feature_init[x % 16][y % 16][feature->feature_idx]->start_depth;
                        break;

                    case df::feature_type::magma_pool:
                        mid_level_tile.magma_level = 2 - world_data->feature_map[x / 16][y / 16].features->feature_init[x % 16][y % 16][feature->feature_idx]->start_depth;
                        break;

                    case df::feature_type::volcano:
                        mid_level_tile.magma_level = 3;
                        break;

                    default:
                        break;
                    }
                }
                else if (feature->layer != -1 &&
                    feature->min_z != -30000) {
                    auto layer = world_data->underground_regions[feature->layer];

                    //layer_bottom[layer->layer_depth] = feature->min_z;
                    //layer_top[layer->layer_depth] = feature->max_z;
                    //base_z = std::min((int)base_z, (int)feature->min_z);

                    if (layer->type == df::world_underground_region::MagmaSea) {
                        min_z = feature->min_z;  //  The features are individual per region tile
                    }
                }
            }
            buffer_holder.add_adamantine_level(key, mid_level_tile.adamantine_level);
            buffer_holder.add_magma_level(key, mid_level_tile.magma_level);

            //  Compute shifts for layers in the stack.

            if (max_soil_depth == -1) {  //  Not set to zero by the biome
                max_soil_depth = std::max((154 - elevation) / 5, 1);
            }

            soil_erosion = geo_summary->at(region_map_entry.geo_index).soil_size -
                std::min((int)geo_summary->at(region_map_entry.geo_index).soil_size, (int)max_soil_depth);
            int16_t layer_shift[16];
            int16_t cur_shift = elevation + soil_erosion - 1;

            mid_level_tile.aquifer = false;
            mid_level_tile.clay = false;
            mid_level_tile.sand = false;
            mid_level_tile.flux = false;
            mid_level_tile.coal = false;

            if (max_soil_depth == 0) {
                mid_level_tile.soil_depth = 0;
            }
            else {
                mid_level_tile.soil_depth = geo_summary->at(region_map_entry.geo_index).soil_size - soil_erosion;
            }
            buffer_holder.add_soil_depth(key, mid_level_tile.soil_depth);

            mid_level_tile.offset = offset;
            mid_level_tile.elevation = details->elevation[i][k];
            mid_level_tile.river_present = false;
            mid_level_tile.river_elevation = 100;

            int16_t inner_river_width = 0;
            bool has_river_inner = false;
            if (details->rivers_vertical.active[i][k] != 0) {
                mid_level_tile.river_present = true;
                mid_level_tile.river_elevation = details->rivers_vertical.elevation[i][k];

                has_river_inner = true;
                has_actual_river = true;
                inner_river_width = details->rivers_vertical.x_max[i][k] - details->rivers_vertical.x_min[i][k];
                if (details->rivers_vertical.x_min[i][k] != 0) {
                    ++inner_river_width;
                }
                max_river_width = std::max(max_river_width, inner_river_width);
            }
            else if (details->rivers_horizontal.active[i][k] != 0) {
                mid_level_tile.river_present = true;
                mid_level_tile.river_elevation = details->rivers_horizontal.elevation[i][k];

                has_river_inner = true;
                has_actual_river = true;
                inner_river_width = details->rivers_horizontal.y_max[i][k] - details->rivers_horizontal.y_min[i][k];
                if (details->rivers_horizontal.y_min[i][k] != 0) {
                    ++inner_river_width;
                }
                max_river_width = std::max(max_river_width, inner_river_width);
            }

            // TODO: do we want to add river for freezing biome or if river-mlt is in ocean or swamp "water"?
            if (has_river_inner) {
                const embark_assist::defs::river_sizes river_size = map_river_width_to_size(inner_river_width, tile.river_size, is_brook);

                buffer_holder.add_river_size(key, river_size);
                //fprintf(state->rivers_file, "%02d;%02d;%02d;%02d;%d;%d\n", x, y, i, k, inner_river_width, river_size);
                //
                //if (tile.river_size != river_size) {
                //    const int32_t flow = calculate_flow(inner_river_width);
                //    fprintf(state->river_anomalies_file, "%02d;%02d;%02d;%02d;%d;%d;%d;%d\n", x, y, i, k, inner_river_width, river_size, tile.river_size, flow);
                //}
            }

            if (tile.min_region_soil > mid_level_tile.soil_depth) {
                tile.min_region_soil = mid_level_tile.soil_depth;
            }

            if (tile.max_region_soil < mid_level_tile.soil_depth) {
                tile.max_region_soil = mid_level_tile.soil_depth;
            }

            /* start here with comment to use survey_layers */
            end_check_l = static_cast<uint16_t>(world_data->geo_biomes[region_map_entry.geo_index]->layers.size());
            if (end_check_l > 16) end_check_l = 16;

            for (uint16_t l = 0; l < end_check_l; l++) {
                auto layer = world_data->geo_biomes[region_map_entry.geo_index]->layers[l];
                layer_shift[l] = cur_shift;

                if (layer->type == df::geo_layer_type::SOIL ||
                    layer->type == df::geo_layer_type::SOIL_SAND) {
                    int16_t size = layer->top_height - layer->bottom_height - 1;
                    //  Comment copied from prospector.cpp (like the logic)...
                    //  This is to replicate the behavior of a probable bug in the
                    //  map generation code : if a layer is partially eroded, the
                    //  removed levels are in fact transferred to the layer below,
                    //  because unlike the case of removing the whole layer, the code
                    //  does not execute a loop to shift the lower part of the stack up.
                    if (size > soil_erosion) {
                        cur_shift = cur_shift - soil_erosion;
                    }

                    soil_erosion -= std::min((int)soil_erosion, (int)size);
                }
            }

            last_bottom = elevation;
            //  Don't have to set up the end_check as we can reuse the one above.

            for (uint16_t l = 0; l < end_check_l; l++) {
                auto layer = world_data->geo_biomes[region_map_entry.geo_index]->layers[l];
                top_z = last_bottom - 1;
                bottom_z = std::max((int)layer->bottom_height + layer_shift[l], (int)min_z);

                if (l == 15) {
                    bottom_z = min_z;  // stretch layer if needed
                }

                if (top_z >= bottom_z) {
                    last_bottom = bottom_z;

                    /*if (process_layer_from_cache(layer, mid_level_tile, buffer_holder, key, bottom_z, elevation)) {
                        continue;
                    }
                    layer_content_cache_entry &cache_entry = state->layer_content_cache_entry_pool[state->layer_content_cache_entry_pool_index++];
                    if (state->layer_content_cache.count(layer) > 0) {
                        out.print("test\n");
                    }
                    else {
                        state->layer_content_cache[layer] = &cache_entry;
                    }*/

                    if (!mid_level_tile.minerals[layer->mat_index]) {
                        buffer_holder.add_mineral(key, layer->mat_index);
                    }
                    //cache_entry.minerals.insert(layer->mat_index);
                    mid_level_tile.minerals[layer->mat_index] = true;

                    end_check_m = static_cast<uint16_t>(world->raws.inorganics[layer->mat_index]->metal_ore.mat_index.size());

                    for (uint16_t m = 0; m < end_check_m; m++) {
                        if (!mid_level_tile.metals[world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]]) {
                            buffer_holder.add_metal(key, world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]);
                        }
                        //cache_entry.metals.insert(world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]);
                        mid_level_tile.metals[world->raws.inorganics[layer->mat_index]->metal_ore.mat_index[m]] = true;
                    }

                    if (layer->type == df::geo_layer_type::SOIL ||
                        layer->type == df::geo_layer_type::SOIL_SAND) {
                        if (world->raws.inorganics[layer->mat_index]->flags.is_set(df::inorganic_flags::SOIL_SAND)) {
                            if (!mid_level_tile.sand) {
                                buffer_holder.add_sand(key);
                            }
                            //cache_entry.sand = true;
                            mid_level_tile.sand = true;
                        }
                    }

                    if (world->raws.inorganics[layer->mat_index]->economic_uses.size() > 0) {
                        if (!mid_level_tile.economics[layer->mat_index]) {
                            buffer_holder.add_economic(key, layer->mat_index);
                        }
                        //cache_entry.economics.insert(layer->mat_index);
                        mid_level_tile.economics[layer->mat_index] = true;

                        end_check_m = static_cast<uint16_t>(world->raws.inorganics[layer->mat_index]->economic_uses.size());
                        for (uint16_t m = 0; m < end_check_m; m++) {
                            if (world->raws.inorganics[layer->mat_index]->economic_uses[m] == state->clay_reaction) {
                                if (!mid_level_tile.clay) {
                                    buffer_holder.add_clay(key);
                                }
                                //cache_entry.clay = true;
                                mid_level_tile.clay = true;
                            }

                            else if (world->raws.inorganics[layer->mat_index]->economic_uses[m] == state->flux_reaction) {
                                if (!mid_level_tile.flux) {
                                    buffer_holder.add_flux(key);
                                }
                                //cache_entry.flux = true;
                                mid_level_tile.flux = true;
                            }
                        }

                        for (uint16_t m = 0; m < state->coals.size(); m++) {
                            if (layer->mat_index == state->coals [m]) {
                                if (!mid_level_tile.coal) {
                                    buffer_holder.add_coal(key);
                                }
                                //cache_entry.coal = true;
                                mid_level_tile.coal = true;
                                break;
                            }
                        }
                    }

                    end_check_m = static_cast<uint16_t>(layer->vein_mat.size());

                    for (uint16_t m = 0; m < end_check_m; m++) {
                        if (!mid_level_tile.minerals[layer->vein_mat[m]]) {
                            buffer_holder.add_mineral(key, layer->vein_mat[m]);
                        }
                        //cache_entry.minerals.insert(layer->vein_mat[m]);
                        mid_level_tile.minerals[layer->vein_mat[m]] = true;

                        end_check_n = static_cast<uint16_t>(world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index.size());

                        for (uint16_t n = 0; n < end_check_n; n++) {
                            if (!mid_level_tile.metals[world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]]) {
                                buffer_holder.add_metal(key, world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]);
                            }
                            //cache_entry.metals.insert(world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]);
                            mid_level_tile.metals[world->raws.inorganics[layer->vein_mat[m]]->metal_ore.mat_index[n]] = true;
                        }

                        if (world->raws.inorganics[layer->vein_mat[m]]->economic_uses.size() > 0) {
                            if (!mid_level_tile.economics[layer->vein_mat[m]]) {
                                buffer_holder.add_economic(key, layer->vein_mat[m]);
                            }
                            //cache_entry.economics.insert(layer->vein_mat[m]);
                            mid_level_tile.economics[layer->vein_mat[m]] = true;

                            end_check_n = static_cast<uint16_t>(world->raws.inorganics[layer->vein_mat[m]]->economic_uses.size());
                            for (uint16_t n = 0; n < end_check_n; n++) {
                                if (world->raws.inorganics[layer->vein_mat[m]]->economic_uses[n] == state->clay_reaction) {
                                    if (!mid_level_tile.clay) {
                                        buffer_holder.add_clay(key);
                                    }
                                    //cache_entry.clay = true;
                                    mid_level_tile.clay = true;
                                }

                                else if (world->raws.inorganics[layer->vein_mat[m]]->economic_uses[n] == state->flux_reaction) {
                                    if (!mid_level_tile.flux) {
                                        buffer_holder.add_flux(key);
                                    }
                                    //cache_entry.flux = true;
                                    mid_level_tile.flux = true;
                                }
                            }

                            for (uint16_t n = 0; n < state->coals.size(); n++) {
                                if (layer->vein_mat [m] == state->coals[n]) {
                                    if (!mid_level_tile.coal) {
                                        buffer_holder.add_coal(key);
                                    }
                                    //cache_entry.coal = true;
                                    mid_level_tile.coal = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (world->raws.inorganics[layer->mat_index]->flags.is_set(df::inorganic_flags::AQUIFER)) {
                        //cache_entry.is_aquifer_layer = true;
                        if (bottom_z <= elevation - 3) {
                            // moved to the end of the inner loop as we need a final value for the attribut aquifer
                            /*if (!mid_level_tile.aquifer) {
                                buffer_holder.add_aquifer(key, true);
                            }*/
                            mid_level_tile.aquifer = true;
                        }
                    }
                }
            }
            buffer_holder.add_aquifer(key, mid_level_tile.aquifer);
            /* end here with comment to use survey_layers cache */
        }
    }

    if (!tile.surveyed) {
        //out.print("%llu - before start of async \n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];
        state->incursion_processor.init_incursion_context(x, y, rtd);

        embark_assist::defs::world_tile_data &survey_results_ref = *survey_results;
        embark_assist::defs::mid_level_tiles &mlt_ref = *mlt;
        state->internal_incursion_processing_result = std::async(std::launch::async, [&, world_offset, x, y]() { state->incursion_processor.process_internal_incursions(world_offset, x, y, survey_results_ref, mlt_ref, index); });
        //state->internal_incursion_processing_result = std::async(std::launch::async, [&, world_offset, x, y]() { state->incursion_processor.process_internal_incursions(world_offset, x, y, survey_results, mlt, index); });
        //state->incursion_processor.process_internal_incursions(world_offset, x, y, survey_results_ref, mlt_ref, index);
    }
    const auto _2_start_river_workaround = std::chrono::steady_clock::now();
    //out.print("%llu - after start of async \n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    //out.print("max river_width: %d\n", max_river_width);
    //if (!has_river && has_actual_river) {
    //    fprintf(state->invalid_river_file, "%02d;%02d;%d;%d;%d\n", x, y, max_river_width, tile.river_size, is_brook);
    //}

    //  This is messy. DF has some weird logic to leave out river bends with a South and an East connection, as well
    //  as river sources (and presumably sinks) that are to the North or the West of the connecting river.
    //  Experiments indicate these implicit river bends inherit their River Elevation from the lower of the two
    //  "parents", and it's assumed river sources and sinks similarly inherit it from their sole "parent".
    //  Two issues are known:
    //  - Lake and Ocean tiles may be marked as having a river when DF doesn't. However, DF does allow for rivers to
    //    exist in Ocean/Lake tiles, as well as sources/sinks.
    //  - DF generates rivers on/under glaciers, but does not display them (as they're frozen), nor are their names
    //    displayed.
    //
    int16_t inner_river_width = 0;
    for (uint8_t i = 1; i < 16; i++) {
        for (uint8_t k = 0; k < 15; k++) {
            if (details->rivers_horizontal.active[i][k] != 0 &&
                details->rivers_vertical.active[i - 1][k + 1] != 0 &&
                !mlt->at(i - 1).at(k).river_present) {  //  Probably never true
                mlt->at(i - 1).at(k).river_present = true;
                mlt->at(i - 1).at(k).river_elevation = mlt->at(i).at(k).river_elevation;

                inner_river_width = details->rivers_horizontal.y_max[i][k] - details->rivers_horizontal.y_min[i][k];
                if (details->rivers_horizontal.y_min[i][k] != 0) {
                    ++inner_river_width;
                }

                if (mlt->at(i - 1).at(k).river_elevation > mlt->at(i - 1).at(k + 1).river_elevation) {
                    mlt->at(i - 1).at(k).river_elevation = mlt->at(i - 1).at(k + 1).river_elevation;
                    inner_river_width = details->rivers_horizontal.y_max[i - 1][k + 1] - details->rivers_horizontal.y_min[i - 1][k + 1];
                    if (details->rivers_horizontal.y_min[i - 1][k + 1] != 0) {
                        ++inner_river_width;
                    }
                }

                const embark_assist::defs::river_sizes river_size = map_river_width_to_size(inner_river_width, tile.river_size, is_brook);
                const uint32_t key = world_offset + k * 16 + (i - 1);
                buffer_holder.add_river_size(key, river_size);
                //fprintf(state->rivers_file, "%02d;%02d;%02d;%02d;%d;%d\n", x, y, i, k, inner_river_width, river_size);

                //if (tile.river_size != river_size) {
                //    const int32_t flow = calculate_flow(inner_river_width);
                //    fprintf(state->river_anomalies_file, "%02d;%02d;%02d;%02d;%d;%d;%d;%d\n", x, y, i, k, inner_river_width, river_size, tile.river_size, flow);
                //}
            }
        }
    }

    for (uint8_t i = 0; i < 16; i++) {
        for (uint8_t k = 1; k < 16; k++) {
            if (details->rivers_vertical.active[i][k] != 0 &&
                !mlt->at(i).at(k - 1).river_present) {
                mlt->at(i).at(k - 1).river_present = true;
                mlt->at(i).at(k - 1).river_elevation = mlt->at(i).at(k).river_elevation;

                inner_river_width = details->rivers_horizontal.y_max[i][k] - details->rivers_horizontal.y_min[i][k];
                if (details->rivers_horizontal.y_min[i][k] != 0) {
                    ++inner_river_width;
                }
                const embark_assist::defs::river_sizes river_size = map_river_width_to_size(inner_river_width, tile.river_size, is_brook);
                const uint32_t key = world_offset + (k - 1) * 16 + i;
                buffer_holder.add_river_size(key, river_size);
                //fprintf(state->rivers_file, "%02d;%02d;%02d;%02d;%d;%d\n", x, y, i, k, inner_river_width, river_size);

                //if (tile.river_size != river_size) {
                //    const int32_t flow = calculate_flow(inner_river_width);
                //    fprintf(state->river_anomalies_file, "%02d;%02d;%02d;%02d;%d;%d;%d;%d\n", x, y, i, k, inner_river_width, river_size, tile.river_size, flow);
                //}
            }
        }
    }

    for (uint8_t i = 1; i < 16; i++) {
        for (uint8_t k = 0; k < 16; k++) {
            if (details->rivers_horizontal.active[i][k] != 0 &&
                !mlt->at(i - 1).at(k).river_present) {
                mlt->at(i - 1).at(k).river_present = true;
                mlt->at(i - 1).at(k).river_elevation = mlt->at(i).at(k).river_elevation;

                inner_river_width = details->rivers_horizontal.y_max[i][k] - details->rivers_horizontal.y_min[i][k];
                if (details->rivers_horizontal.y_min[i][k] != 0) {
                    ++inner_river_width;
                }
                const embark_assist::defs::river_sizes river_size = map_river_width_to_size(inner_river_width, tile.river_size, is_brook);
                const uint32_t key = world_offset + k * 16 + (i - 1);
                buffer_holder.add_river_size(key, river_size);
                //if (tile.river_size != river_size) {
                //    const int32_t flow = calculate_flow(inner_river_width);
                //    fprintf(state->river_anomalies_file, "%02d;%02d;%02d;%02d;%d;%d;%d;%d\n", x, y, i, k, inner_river_width, river_size, tile.river_size, flow);
                //}
            }
        }
    }

    mlt_offset = 0;
    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            if (!mlt->at(i).at(k).river_present) {
                const uint32_t key = world_offset + mlt_offset;
                buffer_holder.add_river_size(key, embark_assist::defs::river_sizes::None);
            }
            ++mlt_offset;
        }
    }

    const auto _3_start_tile_summary = std::chrono::steady_clock::now();
    //survey_layers_result.get();

    tile.aquifer_count = 0;
    tile.clay_count = 0;
    tile.sand_count = 0;
    tile.flux_count = 0;
    tile.coal_count = 0;
    tile.min_region_soil = 10;
    tile.max_region_soil = 0;
    tile.savagery_count[0] = 0;
    tile.savagery_count[1] = 0;
    tile.savagery_count[2] = 0;
    tile.evilness_count[0] = 0;
    tile.evilness_count[1] = 0;
    tile.evilness_count[2] = 0;

    mlt_offset = 0;
    std::array<std::array<bool, 16>, 16> has_waterfall;

    // beware: we process "first" in k(y) than in the i(x) dimension, otherwise the key can't be calculated just be using a rolling offset
    // the inner loop processes one row, then the next row with a higher k/y value
    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            has_waterfall[i][k] = false;
            const embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);
            if (mid_level_tile.aquifer) { tile.aquifer_count++; }
            if (mid_level_tile.clay) { tile.clay_count++; }
            if (mid_level_tile.sand) { tile.sand_count++; }
            if (mid_level_tile.flux) { tile.flux_count++; }
            if (mid_level_tile.coal) { tile.coal_count++; }

            if (mid_level_tile.soil_depth < tile.min_region_soil) {
                tile.min_region_soil = mid_level_tile.soil_depth;
            }

            if (mid_level_tile.soil_depth > tile.max_region_soil) {
                tile.max_region_soil = mid_level_tile.soil_depth;
            }

            if (mid_level_tile.river_present) {
                const uint32_t key = world_offset + mlt_offset;
                if (i < 15 &&
                    mlt->at(i + 1).at(k).river_present &&
                    abs (mid_level_tile.river_elevation - mlt->at(i + 1).at(k).river_elevation) >
                    tile.max_waterfall) {
                    tile.max_waterfall = abs(mid_level_tile.river_elevation - mlt->at(i + 1).at(k).river_elevation);
                }

                if (k < 15 &&
                    mlt->at(i).at(k + 1).river_present &&
                    abs(mid_level_tile.river_elevation - mlt->at(i).at(k + 1).river_elevation) >
                    tile.max_waterfall) {
                    tile.max_waterfall = abs(mid_level_tile.river_elevation - mlt->at(i).at(k + 1).river_elevation);
                }

                if (i < 15 && mlt->at(i + 1).at(k).river_present) {
                    const uint16_t waterfall_drop = abs(mid_level_tile.river_elevation - mlt->at(i + 1).at(k).river_elevation);
                    if (waterfall_drop > 0) {
                        has_waterfall[i][k] = true;
                        has_waterfall[i + 1][k] = true;
                        ++embark_assist::survey::number_of_waterfalls;
                        buffer_holder.add_waterfall_drop(key, key + 1, waterfall_drop);
                    }
                }

                if (k < 15 && mlt->at(i).at(k + 1).river_present) {
                    const uint16_t waterfall_drop = abs(mid_level_tile.river_elevation - mlt->at(i).at(k + 1).river_elevation);
                    if (waterfall_drop > 0) {
                        has_waterfall[i][k] = true;
                        has_waterfall[i][k + 1] = true;
                        ++embark_assist::survey::number_of_waterfalls;
                        buffer_holder.add_waterfall_drop(key, key + 16, waterfall_drop);
                    }
                }
            }

            // River size surveyed separately
            // biome_index handled above
            // biome handled below
            // evil weather handled separately
            // reanimating handled separately
            // thralling handled separately

            tile.savagery_count[mid_level_tile.savagery_level]++;
            tile.evilness_count[mid_level_tile.evilness_level]++;

            for (uint16_t l = 0; l < state->max_inorganic; l++) {
                // TODO: deactivate this and test performance
                // use inorganics_information to only loop over the relevant indices/positions of metal/economic/mineral
                if (mid_level_tile.metals[l]) { tile.metals[l] = true; }
                if (mid_level_tile.economics[l]) { tile.economics[l] = true; }
                if (mid_level_tile.minerals[l]) { tile.minerals[l] = true; }
            }
            ++mlt_offset;
        }
    }

    const auto _4_start_waterfall_and_biomes = std::chrono::steady_clock::now();

    // handle elevation and mlts with no waterfalls
    buffer_holder.set_current_initial_offset(world_offset);
    mlt_offset = 0;
    // beware: we process "first" in k(y) than in the i(x) dimension, otherwise the key can't be calculated just be using a rolling offset
    // the inner loop processes one row, then the next row with a higher k/y value
    
    // here we map the actual elevation of a tile to a "random" number between 0 and 255
    // as there are only 256 embark tiles in a world tile there can't be more mapped elevations than 256
    // => we can store the "relative" elevation as a uint8_t instead of a int16_t, saving 1 byte
    uint8_t next_mapped_index = 0;
    unordered_map<int16_t, uint8_t> elevation_to_index_mapping;
    for (uint8_t k = 0; k < 16; k++) {
        for (uint8_t i = 0; i < 16; i++) {
            const embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);
            uint8_t mapped_index = next_mapped_index;
            const auto iter = elevation_to_index_mapping.find(mid_level_tile.elevation);
            if (iter == elevation_to_index_mapping.end()) {
                mapped_index = next_mapped_index++;
                elevation_to_index_mapping[mid_level_tile.elevation] = mapped_index;
            } else {
                mapped_index = iter->first;
            }

            const uint32_t key = world_offset + mlt_offset;
            buffer_holder.add_mapped_elevation(key, mapped_index);

            if (!has_waterfall[i][k]) {
                buffer_holder.add_no_waterfall(key);
            }
            ++mlt_offset;
        }
    }

    for (uint8_t i = 1; i < 10; i++) {
        if (tile.biome_index[i] == -1) {
            tile.biome[i] = -1;
        }
    }

    bool biomes[ENUM_LAST_ITEM(biome_type) + 1];
    for (uint8_t i = 0; i <= ENUM_LAST_ITEM(biome_type); i++) {
        biomes[i] = false;
    }

    for (uint8_t i = 1; i < 10; i++)
    {
        if (tile.biome[i] != -1) {
            biomes[tile.biome[i]] = true;
        }
    }
    int count = 0;
    for (uint8_t i = 0; i <= ENUM_LAST_ITEM(biome_type); i++) {
        if (biomes[i]) count++;
    }

    tile.biome_count = count;

    const auto incursion_start = std::chrono::steady_clock::now();

    for (uint8_t i = 0; i < 16; i++) {
        embark_assist::defs::mid_level_tile_basic &north_row = tile.north_row[i];
        embark_assist::defs::mid_level_tile_basic &south_row = tile.south_row[i];
        embark_assist::defs::mid_level_tile_basic &west_column = tile.west_column[i];
        embark_assist::defs::mid_level_tile_basic &east_column = tile.east_column[i];

        embark_assist::defs::mid_level_tile_basic &north_tile = mlt->at(i).at(0);
        embark_assist::defs::mid_level_tile_basic &south_tile = mlt->at(i).at(15);
        embark_assist::defs::mid_level_tile_basic &west_tile = mlt->at(0).at(i);
        embark_assist::defs::mid_level_tile_basic &east_tile = mlt->at(15).at(i);

        north_row.aquifer = north_tile.aquifer;
        south_row.aquifer = south_tile.aquifer;
        west_column.aquifer = west_tile.aquifer;
        east_column.aquifer = east_tile.aquifer;

        north_row.clay= north_tile.clay;
        south_row.clay = south_tile.clay;
        west_column.clay = west_tile.clay;
        east_column.clay = east_tile.clay;

        north_row.sand = north_tile.sand;
        south_row.sand = south_tile.sand;
        west_column.sand = west_tile.sand;
        east_column.sand = east_tile.sand;

        //north_row.flux = north_tile.flux;  //  Not used
        //south_row.flux = south_tile.flux;
        //west_column.flux = west_tile.flux;
        //east_column.flux = east_tile.flux;

        //north_row.coal = north_tile.coal; //  Not used
        //south_row.coal = south_tile.coal;
        //west_column.coal = west_tile.coal;
        //east_column.coal = east_tile.coal;

        north_row.soil_depth = north_tile.soil_depth;
        south_row.soil_depth = south_tile.soil_depth;
        west_column.soil_depth = west_tile.soil_depth;
        east_column.soil_depth = east_tile.soil_depth;

        //north_row.offset = north_tile.offset; //  Not used
        //south_row.offset = south_tile.offset;
        //west_column.offset = west_tile.offset;
        //east_column.offset = east_tile.offset;

        north_row.elevation = north_tile.elevation;
        south_row.elevation = south_tile.elevation;
        west_column.elevation = west_tile.elevation;
        east_column.elevation = east_tile.elevation;

        //north_row.river_present = north_tile.river_present; //  Not used
        //south_row.river_present = south_tile.river_present;
        //west_column.river_present = west_tile.river_present;
        //east_column.river_present = east_tile.river_present;

        //north_row.river_elevation = north_tile.river_elevation; //  Not used
        //south_row.river_elevation = south_tile.river_elevation;
        //west_column.river_elevation = west_tile.river_elevation;
        //east_column.river_elevation = east_tile.river_elevation;

        //north_row.adamantine_level = north_tile.adamantine_level; //  Not used
        //south_row.adamantine_level = south_tile.adamantine_level;
        //west_column.adamantine_level = west_tile.adamantine_level;
        //east_column.adamantine_level = east_tile.adamantine_level;

        //north_row.magma_level = north_tile.magma_level; //  Not used
        //south_row.magma_level = south_tile.magma_level;
        //west_column.magma_level = west_tile.magma_level;
        //east_column.magma_level = east_tile.magma_level;

        north_row.biome_offset = north_tile.biome_offset;
        south_row.biome_offset = south_tile.biome_offset;
        west_column.biome_offset = west_tile.biome_offset;
        east_column.biome_offset = east_tile.biome_offset;

        north_row.savagery_level = north_tile.savagery_level;
        south_row.savagery_level = south_tile.savagery_level;
        west_column.savagery_level = west_tile.savagery_level;
        east_column.savagery_level = east_tile.savagery_level;

        north_row.evilness_level = north_tile.evilness_level;
        south_row.evilness_level = south_tile.evilness_level;
        west_column.evilness_level = west_tile.evilness_level;
        east_column.evilness_level = east_tile.evilness_level;

        //north_row.metals.resize(0);  //  Not used
        //south_row.metals.resize(0);
        //west_column.metals.resize(0);
        //east_column.metals.resize(0);

        //north_row.economics.resize(0);  //  Not used
        //south_row.economics.resize(0);
        //west_column.economics.resize(0);
        //east_column.economics.resize(0);

        //north_row.minerals.resize(0);  //  Not used
        //south_row.minerals.resize(0);
        //west_column.minerals.resize(0);
        //east_column.minerals.resize(0);

        //tile.north_corner_selection[i] = world_data->region_details[0]->edges.biome_corner[i][0];
        //tile.northern_row_biome_x[i] = world_data->region_details[0]->edges.biome_x[i][0];
        //tile.west_corner_selection[i] = world_data->region_details[0]->edges.biome_corner[0][i];
        //tile.western_column_biome_y[i] = world_data->region_details[0]->edges.biome_y[0][i];
    }

    // TODO: remove this, if there is no output because of inequal region_types
    //for (uint8_t i = 0; i < 16; i++) {
    //    for (uint8_t k = 0; k < 16; k++) {
    //        if (tile.region_type[i][k] != world_data->regions[tile.biome_index[mlt->at(i).at(k).biome_offset]]->type) {
    //            out.print("region_type does not match => x:%d, y:%d, i:%d, k:%d, old_region:%d, new_region:%d \n", x, y, i, k, tile.region_type[i][k], world_data->regions[tile.biome_index[mlt->at(i).at(k).biome_offset]]->type);
    //        }
    //        tile.region_type[i][k] = world_data->regions[tile.biome_index[mlt->at(i).at(k).biome_offset]]->type;
    //    }
    //}

    const auto end = std::chrono::steady_clock::now();
    elapsed_survey_seconds += end - start;

    if (!tile.surveyed) {
        // "casting" this to a reference - as the pointer will become invalid when passed on into a new thread...
        const embark_assist::key_buffer_holder::key_buffer_holder &buffer_holder_ref = buffer_holder;
        embark_assist::defs::region_tile_datum &rtd = survey_results->at(x)[y];
        state->regular_data_processing_result = std::async(std::launch::async, [&, x, y]() { index.add(x, y, rtd, buffer_holder_ref); });

        // making sure the async job is done before we process the "regular" survey data - to be sure it does operate on valid data, 
        // which might not be the case if the cursor iterates on to another world tile will the internal incursions are still being processed
        //state->internal_incursion_processing_result.get();
        //out.print("%llu - after get of async \n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        
        // making sure that the previous async/thread is finished before we start a new one...
        //if (state->external_incursion_processing_result.valid()) {
        //    state->external_incursion_processing_result.get();
        //}

        // "casting" this to a reference - as the pointer will become invalid when passed on into a new thread...
        embark_assist::defs::world_tile_data &survey_results_ref = *survey_results;

        // as now all the required data has been collected in the world tile (north_corner_selection, ...)
        // starting to process the external incursions of the neighbours and potentially also the current tile
        // not operating on any transient data that gets changed during iteration we can let this run async/concurrent, even while the cursor iterates onto another world tile
        // as the search is iterative and triggered by below method directly we don't have to queue anything
        state->external_incursion_processing_result
            = std::async(std::launch::async, [&, x, y]() { state->incursion_processor.update_and_check_survey_counters_of_neighbouring_world_tiles(x, y, survey_results_ref, index); });
        //out.print("%llu - after start of external incursion async \n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    // write all incursion relevant data into a csv file for every embark tile that has either (x == world_data->world_width - 1 AND i == 15) OR (y == world_data->world_height - 1 AND k == 15)
    //if (!tile.surveyed && (x == 0 || x == world_data->world_width - 1 || y == 0 || y == world_data->world_height - 1)) {
    //    const std::string prefix = "edge_incursion_data.csv";
    //    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out | std::ofstream::app);

    //    if (x == 0) {
    //        // output eastern col
    //        //const int i = 15;
    //        for (int8_t i = 0; i < 2; i++) {
    //            for (int8_t k = 0; k < 16; k++) {
    //                const embark_assist::defs::mid_level_tile_basic &data = mlt->at(i)[k];
    //                const uint32_t key = index.get_key(x, y, i, k);

    //                log_incursion_data(myfile, x, y, i, k, key, tile.biome[data.biome_offset], data);
    //            }
    //        }
    //    }

    //    if (x == world_data->world_width - 1) {
    //        // output eastern col
    //        //const int i = 15;
    //        for (int8_t i = 14; i < 16; i++) {
    //            for (int8_t k = 0; k < 16; k++) {
    //                const embark_assist::defs::mid_level_tile_basic &data = mlt->at(i)[k];
    //                const uint32_t key = index.get_key(x, y, i, k);

    //                log_incursion_data(myfile, x, y, i, k, key, tile.biome[data.biome_offset], data);
    //            }
    //        }
    //    }

    //    if (y == 0) {
    //        // output southern row
    //        //const int k = 15;
    //        for (int8_t k = 0; k < 2; k++) {
    //            for (int8_t i = 0; i < 16; i++) {
    //                const embark_assist::defs::mid_level_tile_basic &data = mlt->at(i)[k];
    //                const uint32_t key = index.get_key(x, y, i, k);

    //                log_incursion_data(myfile, x, y, i, k, key, tile.biome[data.biome_offset], data);
    //            }
    //        }
    //    }

    //    if (y == world_data->world_height - 1) {
    //        // output southern row
    //        //const int k = 15;
    //        for (int8_t k = 14; k < 16; k++) {
    //            for (int8_t i = 0; i < 16; i++) {
    //                const embark_assist::defs::mid_level_tile_basic &data = mlt->at(i)[k];
    //                const uint32_t key = index.get_key(x, y, i, k);

    //                log_incursion_data(myfile, x, y, i, k, key, tile.biome[data.biome_offset], data);
    //            }
    //        }
    //    }
    //}

    //if (state->internal_incursion_processing_result.valid()) {
    //    state->internal_incursion_processing_result.get();
    //}
    // state->internal_incursion_processing_result.wait();

    tile.surveyed = true;

    const auto total_end = std::chrono::steady_clock::now();
    elapsed_survey_total_seconds += total_end - start;

    elapsed_init_seconds += _1_start_big_loop - start;
    elapsed_big_loop_seconds += _2_start_river_workaround - _1_start_big_loop;
    elapsed_workaround_seconds +=  _3_start_tile_summary -_2_start_river_workaround;
    elapsed_tile_summary_seconds += _4_start_waterfall_and_biomes - _3_start_tile_summary;
    elapsed_waterfall_and_biomes_seconds += end - _4_start_waterfall_and_biomes;
}

//=================================================================================

df::coord2d embark_assist::survey::apply_offset(uint16_t x, uint16_t y, int8_t offset) {
    df::coord2d result;
    result.x = x;
    result.y = y;

    switch (offset) {
    case 1:
        result.x--;
        result.y++;
        break;

    case 2:
        result.y++;
        break;

    case 3:
        result.x++;
        result.y++;
        break;

    case 4:
        result.x--;
        break;

    case 5:
        break;  // Center. No change

    case 6:
        result.x++;
        break;

    case 7:
        result.x--;
        result.y--;
        break;

    case 8:
        result.y--;
        break;

    case 9:
        result.x++;
        result.y--;
        break;

    default:
        //  Bug. Just act as if it's the center...
        break;
    }

    if (result.x < 0) {
        result.x = 0;
    }
    else if (result.x >= world->worldgen.worldgen_parms.dim_x) {
        result.x = world->worldgen.worldgen_parms.dim_x - 1;
    }

    if (result.y < 0) {
        result.y = 0;
    }
    else if (result.y >= world->worldgen.worldgen_parms.dim_y) {
        result.y = world->worldgen.worldgen_parms.dim_y - 1;
    }

    return result;
}

//=================================================================================

void adjust_coordinates(int16_t *x, int16_t *y, int8_t *i, int8_t *k) {
    if (*i < 0) {
        *x = *x - 1;
        *i = *i + 16;
    }
    else if (*i > 15) {
        *x = *x + 1;
        *i = *i - 16;
    }

    if (*k < 0) {
        *y = *y - 1;
        *k = *k + 16;
    }
    else if (*k > 15) {
        *y = *y + 1;
        *k = *k - 16;
    }
}

//=================================================================================

df::world_region_type embark_assist::survey::region_type_of(const embark_assist::defs::world_tile_data *survey_results,
    int16_t x,
    int16_t y,
    int8_t i,
    int8_t k) {

    df::world_data *world_data = world->world_data;
    int16_t effective_x = x;
    int16_t effective_y = y;
    int8_t effective_i = i;
    int8_t effective_k = k;
    adjust_coordinates(&effective_x, &effective_y, &effective_i, &effective_k);

    if (effective_x < 0 ||
        effective_x >= world_data->world_width ||
        effective_y < 0 ||
        effective_y >= world_data->world_height) {
        return df::world_region_type::Lake;  //  Essentially dummy value, yielding to everything. It will
                                             //  be handled properly later anyway.
    }

    return survey_results->at(effective_x).at(effective_y).region_type[effective_i][effective_k];
}

//=================================================================================

uint8_t embark_assist::survey::translate_corner(const embark_assist::defs::world_tile_data *survey_results,
    uint8_t corner_location,
    uint16_t x,
    uint16_t y,
    uint8_t i,
    uint8_t k) {

    df::world_data *world_data = world->world_data;
    df::world_region_type nw_region_type;
    df::world_region_type n_region_type;
    df::world_region_type w_region_type;
    df::world_region_type home_region_type;

    int16_t effective_x = x;
    int16_t effective_y = y;
    int8_t effective_i = i;
    int8_t effective_k = k;

    uint8_t effective_corner;
    bool nw_region_type_active;
    bool n_region_type_active;
    bool w_region_type_active;
    bool home_region_type_active;
    int8_t nw_region_type_level;
    int8_t n_region_type_level;
    int8_t w_region_type_level;
    int8_t home_region_type_level;

    if (corner_location == 4) {  //  We're the reference. No change.
    }
    else if (corner_location == 5) {  //  Tile to the east is the reference
        effective_i = i + 1;
    }
    else if (corner_location == 7) {  //  Tile to the south is the reference
        effective_k = k + 1;
    }
    else {  //  8, tile to the southeast is the reference.
        effective_i = i + 1;
        effective_k = k + 1;
    }

    adjust_coordinates(&effective_x, &effective_y, &effective_i, &effective_k);

    if (effective_x == world_data->world_width) {
        if (effective_y == world_data->world_height) {  //  Only the SE corner of the SE most tile of the world can reference this.
            return 4;
        }
        else {  //  East side corners of the east edge of the world
            if (corner_location == 5) {
                return 1;
            }
            else {  //  Can only be corner_location == 8
                return 4;
            }
        }
    }
    else if (effective_y == world_data->world_height) {
        if (corner_location == 7) {
            return 4;
        }
        else {  //  Can only be corner_location == 8
            // FIXME: compare region_type of current and eastern neighbour tile to determine return value
            return 5;
        }
    }

    if (effective_x == x && effective_y == y) {
        //effective_corner = world_data->region_details[0]->edges.biome_corner[effective_i][effective_k];
        effective_corner = survey_results->at(effective_x).at(effective_y).biome_corner[effective_i][effective_k];
    }
    else if (effective_y != y) {
        // effective_corner = survey_results->at(effective_x).at(effective_y).north_corner_selection[effective_i];
        effective_corner = survey_results->at(effective_x).at(effective_y).biome_corner[effective_i][0];
    }
    else {
        // effective_corner = survey_results->at(effective_x).at(effective_y).west_corner_selection[effective_k];
        effective_corner = survey_results->at(effective_x).at(effective_y).biome_corner[0][effective_k];
    }

    nw_region_type = embark_assist::survey::region_type_of(survey_results, effective_x, effective_y, effective_i - 1, effective_k - 1);
    n_region_type = embark_assist::survey::region_type_of(survey_results, effective_x, effective_y, effective_i, effective_k - 1);
    w_region_type = embark_assist::survey::region_type_of(survey_results, effective_x, effective_y, effective_i - 1, effective_k);
    home_region_type = embark_assist::survey::region_type_of(survey_results, effective_x, effective_y, effective_i, effective_k);

    if (nw_region_type == df::world_region_type::Lake ||
        nw_region_type == df::world_region_type::Ocean) {
        nw_region_type_level = 0;
    }
    else if (nw_region_type == df::world_region_type::Mountains) {
        nw_region_type_level = 1;
    }
    else {
        nw_region_type_level = 2;
    }

    if (n_region_type == df::world_region_type::Lake ||
        n_region_type == df::world_region_type::Ocean) {
        n_region_type_level = 0;
    }
    else if (n_region_type == df::world_region_type::Mountains) {
        n_region_type_level = 1;
    }
    else {
        n_region_type_level = 2;
    }

    if (w_region_type == df::world_region_type::Lake ||
        w_region_type == df::world_region_type::Ocean) {
        w_region_type_level = 0;
    }
    else if (w_region_type == df::world_region_type::Mountains) {
        w_region_type_level = 1;
    }
    else {
        w_region_type_level = 2;
    }

    if (home_region_type == df::world_region_type::Lake ||
        home_region_type == df::world_region_type::Ocean) {
        home_region_type_level = 0;
    }
    else if (n_region_type == df::world_region_type::Mountains) {
        home_region_type_level = 1;
    }
    else {
        home_region_type_level = 2;
    }

    if (effective_x == 0 && effective_i == 0) {  //  West edge of the world
        if (effective_y == 0 && effective_k == 0) {
            return 4;  //  Only a single reference to this info, the own tile.
        }
        else {
            nw_region_type_level = -1;  //  Knock out the unreachable corners
            w_region_type_level = -1;
        }
    }

    if (effective_y == 0 && effective_k == 0) {  //  North edge of the world
        nw_region_type_level = -1;  //  Knock out the unreachable corners
        n_region_type_level = -1;

        if (corner_location == 4 && effective_corner == 1) { // The logic below would select the wrong alternative.
            effective_corner = 3;
        }
    }

    nw_region_type_active = nw_region_type_level >= n_region_type_level &&
        nw_region_type_level >= w_region_type_level &&
        nw_region_type_level >= home_region_type_level;

    n_region_type_active = n_region_type_level >= nw_region_type_level &&
        n_region_type_level >= w_region_type_level &&
        n_region_type_level >= home_region_type_level;

    w_region_type_active = w_region_type_level >= nw_region_type_level &&
        w_region_type_level >= n_region_type_level &&
        w_region_type_level >= home_region_type_level;

    home_region_type_active = home_region_type_level >= nw_region_type_level &&
        home_region_type_level >= n_region_type_level &&
        home_region_type_level >= w_region_type_level;

    if ((effective_corner == 0 && !nw_region_type_active) ||
        (effective_corner == 1 && !n_region_type_active) ||
        (effective_corner == 2 && !w_region_type_active) ||
        (effective_corner == 3 && !home_region_type_active)) {
        //  The designated corner is suppressed. The precedence list below seems
        //  to match what DF produces except in the case adjusted above.
        if (nw_region_type_active) {
            effective_corner = 0;
        }
        else if (n_region_type_active) {
            effective_corner = 1;
        }
        else if (w_region_type_active) {
            effective_corner = 2;
        }
        else {
            effective_corner = 3;
        }
    }

    switch (effective_corner) {
    case 0:
        return corner_location - 4;
        break;

    case 1:
        return corner_location - 3;
        break;

    case 2:
        return corner_location - 1;
        break;

    case 3:
        return corner_location;
        break;
    }

    return -1;  //  Should never happen

                /*  The logic that's reduced to the code above.
                switch (corner_location) {
                case 0: //  N/A Not to the north or west
                case 1: //  N/A
                case 2: //  N/A
                case 3: //  N/A
                case 6: //  N/A
                return -1;  //  Should never happen
                break;

                case 4: //  Self
                switch (corner) {
                case 0:
                return 0;  //  Northwest
                break;

                case 1:
                return 1;  //  North
                break;

                case 2:
                return 3;  //  West
                break;

                case 3:
                return 4;  //  Self
                }

                case 5: //  East
                switch (corner) {
                case 0:
                return 1;  //  North
                break;

                case 1:
                return 2;  //  Northeast
                break;

                case 2:
                return 4;  //  Self
                break;

                case 3:
                return 5;  //  East
                }
                case 7: //  South
                switch (corner) {
                case 0:
                return 3;  //  West
                break;

                case 1:
                return 4;  //  Self
                break;

                case 2:
                return 6;  //  Southwest
                break;

                case 3:
                return 7;  //  South
                }

                case 8: //  Southeast
                switch (corner) {
                case 0:
                return 4;  //  Self
                break;

                case 1:
                return 5;  //  East
                break;

                case 2:
                return 7;  //  South
                break;

                case 3:
                return 8;  //  Southeast
                }
                }
                */
}

//=================================================================================

uint8_t embark_assist::survey::translate_ns_edge(embark_assist::defs::world_tile_data *survey_results,
    bool own_edge,
    uint16_t x,
    uint16_t y,
    uint8_t i,
    uint8_t k) {

    df::world_data *world_data = world->world_data;
    uint8_t effective_edge;
    df::world_region_type north_region_type;
    df::world_region_type south_region_type;

    if (own_edge) {
        // the edge belongs to the currently processed tile thus its counterpart is the north tile (k - 1)
        //effective_edge = world_data->region_details[0]->edges.biome_x[i][k];
        effective_edge = survey_results->at(x).at(y).biome_x[i][k];
        // region_type_of actually properly handles the case that we need information from the world tile north (y - 1) and returns df::world_region_type::Lake in case of y < 0, which prevents incursion processing
        south_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i, k);
        north_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i, k - 1);
    }
    else {
        // the edge belongs to the tile south of the currently processed tile thus its counterpart is the south tile (k + 1)
        // here the case that we need information from the next world tile south is being handled properly
        if (k < 15) {
            //effective_edge = world_data->region_details[0]->edges.biome_x[i][k + 1];
            effective_edge = survey_results->at(x).at(y).biome_x[i][k + 1];
        }
        else {
            // TODO: is that right?
            if (y + 1 == world_data->world_height) {
                return 4;
            }
            //effective_edge = survey_results->at(x).at(y + 1).northern_row_biome_x[i];
            effective_edge = survey_results->at(x).at(y + 1).biome_x[i][0];
        }

        north_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i, k);
        south_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i, k + 1);
    }

    //  Apply rules for Ocean && Lake to yield to everything else,
    //  and Mountain to everything but those.
    //
    if ((north_region_type == df::world_region_type::Lake ||
        north_region_type == df::world_region_type::Ocean) &&
        south_region_type != df::world_region_type::Lake &&
        south_region_type != df::world_region_type::Ocean) {
        effective_edge = 1;
    }

    if ((south_region_type == df::world_region_type::Lake ||
        south_region_type == df::world_region_type::Ocean) &&
        north_region_type != df::world_region_type::Lake &&
        north_region_type != df::world_region_type::Ocean) {
        effective_edge = 0;
    }

    if (north_region_type == df::world_region_type::Mountains &&
        south_region_type != df::world_region_type::Lake &&
        south_region_type != df::world_region_type::Ocean &&
        south_region_type != df::world_region_type::Mountains) {
        effective_edge = 1;
    }

    if (south_region_type == df::world_region_type::Mountains &&
        north_region_type != df::world_region_type::Lake &&
        north_region_type != df::world_region_type::Ocean &&
        north_region_type != df::world_region_type::Mountains) {
        effective_edge = 0;
    }

    if (effective_edge == 0) {
        if (own_edge) {
            return 1;
        }
        else {
            return 4;
        }
    }
    else {
        if (own_edge) {
            return 4;
        }
        else {
            return 7;
        }
    }
}

//=================================================================================

uint8_t embark_assist::survey::translate_ew_edge(embark_assist::defs::world_tile_data *survey_results,
    bool own_edge,
    uint16_t x,
    uint16_t y,
    uint8_t i,
    uint8_t k) {

    df::world_data *world_data = world->world_data;
    uint8_t effective_edge;
    df::world_region_type west_region_type;
    df::world_region_type east_region_type;

    if (own_edge) {
        // the edge belongs to the currently processed tile thus its counterpart is the west tile (i - 1)
        //effective_edge = world_data->region_details[0]->edges.biome_y[i][k];
        effective_edge = survey_results->at(x).at(y).biome_y[i][k];
        // region_type_of actually properly handles the case that we need information from the world tile west (x - 1)
        east_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i, k);
        west_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i - 1, k);
    }
    else {
        // the edge belongs to the tile east of the currently processed tile thus its counterpart is the east tile (i + 1)
        // here the case that we need information from the next world tile east (x + 1) is being handled properly
        if (i < 15) {
            //effective_edge = world_data->region_details[0]->edges.biome_y[i + 1][k];
            effective_edge = survey_results->at(x).at(y).biome_y[i + 1][k];
        }
        else {
            // TODO: is that right?
            if (x + 1 == world_data->world_width) {
                return 4;
            }
            //effective_edge = survey_results->at(x + 1).at(y).western_column_biome_y[k];
            effective_edge = survey_results->at(x + 1).at(y).biome_y[0][k];
        }
        west_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i, k);
        east_region_type = embark_assist::survey::region_type_of(survey_results, x, y, i + 1, k);
    }

    //  Apply rules for Ocean && Lake to yield to everything else,
    //  and Mountain to everything but those.
    //
    if ((west_region_type == df::world_region_type::Lake ||
        west_region_type == df::world_region_type::Ocean) &&
        east_region_type != df::world_region_type::Lake &&
        east_region_type != df::world_region_type::Ocean) {
        effective_edge = 1;
    }

    if ((east_region_type == df::world_region_type::Lake ||
        east_region_type == df::world_region_type::Ocean) &&
        west_region_type != df::world_region_type::Lake &&
        west_region_type != df::world_region_type::Ocean) {
        effective_edge = 0;
    }

    if (west_region_type == df::world_region_type::Mountains &&
        west_region_type != df::world_region_type::Lake &&
        east_region_type != df::world_region_type::Ocean &&
        east_region_type != df::world_region_type::Mountains) {
        effective_edge = 1;
    }

    if (east_region_type == df::world_region_type::Mountains &&
        east_region_type != df::world_region_type::Lake &&
        west_region_type != df::world_region_type::Ocean &&
        west_region_type != df::world_region_type::Mountains) {
        effective_edge = 0;
    }
    if (effective_edge == 0) {
        if (own_edge) {
            return 3;
        }
        else {
            return 4;
        }
    }
    else {
        if (own_edge) {
            return 4;
        }
        else {
            return 5;
        }
    }
}

//=================================================================================

void embark_assist::survey::survey_region_sites(embark_assist::defs::site_lists *site_list) {
    //            color_ostream_proxy out(Core::getInstance().getConsole());
    auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
    df::world_data *world_data = world->world_data;
    int8_t index = 0;

    site_list->clear();

    for (uint32_t i = 0; i < world_data->region_map[screen->location.region_pos.x][screen->location.region_pos.y].sites.size(); i++) {
        auto site = world_data->region_map[screen->location.region_pos.x][screen->location.region_pos.y].sites[i];
        switch (site->type) {
        case df::world_site_type::PlayerFortress:
        case df::world_site_type::DarkFortress:
        case df::world_site_type::MountainHalls:
        case df::world_site_type::ForestRetreat:
        case df::world_site_type::Town:
        case df::world_site_type::Fortress:
            break;  //  Already visible

        case df::world_site_type::Cave:
            if (!world->worldgen.worldgen_parms.all_caves_visible) {
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'c' });  //  Cave
            }
            break;

        case df::world_site_type::Monument:
            if (site->subtype_info->lair_type != -1 ||
                site->subtype_info->is_monument == 0) {  //  Not Tomb, which is visible already
            }
            else if (site->subtype_info->lair_type == -1) {
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'V' });  //  Vault
            }
            else {
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'M' });  //  Any other Monument type. Pyramid?
            }
            break;

        case df::world_site_type::ImportantLocation:
            site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'i' });  //  Don't really know what that is...
            break;

        case df::world_site_type::LairShrine:
            if (site->subtype_info->lair_type == 0 ||
                site->subtype_info->lair_type == 1 ||
                site->subtype_info->lair_type == 4) {  // Only Rocs seen. Mountain lair?
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'l' });  //  Lair
            }
            else if (site->subtype_info->lair_type == 2) {
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'L' });  //  Labyrinth
            }
            else if (site->subtype_info->lair_type == 3) {
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'S' });  //  Shrine
            }
            else {
                site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, '?' });  //  Can these exist?
            }
            break;

        case df::world_site_type::Camp:
            site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, 'C' });  //  Camp
            break;

        default:
            site_list->push_back({ (uint8_t)site->rgn_min_x , (uint8_t)site->rgn_min_y, '!' });  //  Not even in the enum...
            break;
        }
    }
}

//=================================================================================

void embark_assist::survey::survey_embark(embark_assist::defs::mid_level_tiles *mlt,
    embark_assist::defs::world_tile_data *survey_results,
    embark_assist::defs::site_infos *site_info,
    bool use_cache) {

    //color_ostream_proxy out(Core::getInstance().getConsole());
    auto screen = Gui::getViewscreenByType<df::viewscreen_choose_start_sitest>(0);
    int16_t elevation = 0;
    uint16_t x = screen->location.region_pos.x;
    uint16_t y = screen->location.region_pos.y;

    std::vector<bool> metals(state->max_inorganic);
    std::vector<bool> economics(state->max_inorganic);
    std::vector<bool> minerals(state->max_inorganic);
    bool incursion_processing_failed = false;
    df::world_data *world_data = world->world_data;

    if (!use_cache) {  //  For some reason DF scrambles these values on world tile movements (at least in Lua...).
        state->local_min_x = screen->location.embark_pos_min.x;
        state->local_min_y = screen->location.embark_pos_min.y;
        state->local_max_x = screen->location.embark_pos_max.x;
        state->local_max_y = screen->location.embark_pos_max.y;
    }

    color_ostream_proxy out(Core::getInstance().getConsole());
    out.print("i,k: %02d,%02d\n", state->local_min_x, state->local_min_y);

    state->x = x;
    state->y = y;

    site_info->incursions_processed = true;
    site_info->aquifer = false;
    site_info->aquifer_full = true;
    site_info->min_soil = 10;
    site_info->max_soil = 0;
    site_info->flat = true;
    site_info->max_waterfall = 0;
    site_info->clay = false;
    site_info->sand = false;
    site_info->flux = false;
    site_info->coal = false;
    site_info->blood_rain = false;
    site_info->permanent_syndrome_rain = false;
    site_info->temporary_syndrome_rain = false;
    site_info->reanimating = false;
    site_info->thralling = false;
    site_info->metals.clear();
    site_info->economics.clear();
    site_info->metals.clear();

    //color_ostream_proxy out(Core::getInstance().getConsole());
    //out.print("i,k: %02d,%02d\n", state->local_min_x, state->local_min_y);

    for (uint8_t i = state->local_min_x; i <= state->local_max_x; i++) {
        for (uint8_t k = state->local_min_y; k <= state->local_max_y; k++) {
            const embark_assist::defs::mid_level_tile &mid_level_tile = mlt->at(i).at(k);
            if (mid_level_tile.aquifer) {
                site_info->aquifer = true;
            }
            else {
                site_info->aquifer_full = false;
            }

            if (mid_level_tile.soil_depth < site_info->min_soil) {
                site_info->min_soil = mid_level_tile.soil_depth;
            }

            if (mid_level_tile.soil_depth > site_info->max_soil) {
                site_info->max_soil = mid_level_tile.soil_depth;
            }

            if (i == state->local_min_x && k == state->local_min_y) {
                elevation = mid_level_tile.elevation;
            }
            else if (elevation != mid_level_tile.elevation) {
                site_info->flat = false;
            }

            if (mid_level_tile.river_present) {
                if (i < 15 &&
                    mlt->at(i + 1).at(k).river_present &&
                    abs(mid_level_tile.river_elevation - mlt->at(i + 1).at(k).river_elevation) >
                    site_info->max_waterfall) {
                    site_info->max_waterfall =
                        abs(mid_level_tile.river_elevation - mlt->at(i + 1).at(k).river_elevation);
                }

                if (k < 15 &&
                    mlt->at(i).at(k + 1).river_present &&
                    abs(mid_level_tile.river_elevation - mlt->at(i).at(k + 1).river_elevation) >
                    site_info->max_waterfall) {
                    site_info->max_waterfall =
                        abs(mid_level_tile.river_elevation - mlt->at(i).at(k + 1).river_elevation);
                }
            }

            if (mid_level_tile.clay) {
                site_info->clay = true;
            }

            if (mid_level_tile.sand) {
                site_info->sand = true;
            }

            if (mid_level_tile.flux) {
                site_info->flux = true;
            }

            if (mid_level_tile.coal) {
                site_info->coal = true;
            }

            if (survey_results->at(x).at(y).blood_rain[mid_level_tile.biome_offset]) {
                site_info->blood_rain = true;
            }

            if (survey_results->at(x).at(y).permanent_syndrome_rain[mid_level_tile.biome_offset]) {
                site_info->permanent_syndrome_rain = true;
            }

            if (survey_results->at(x).at(y).temporary_syndrome_rain[mid_level_tile.biome_offset]) {
                site_info->temporary_syndrome_rain = true;
            }

            if (survey_results->at(x).at(y).reanimating[mid_level_tile.biome_offset]) {
                site_info->reanimating = true;
            }

            if (survey_results->at(x).at(y).thralling[mid_level_tile.biome_offset]) {
                site_info->thralling = true;
            }

            for (uint16_t l = 0; l < state->max_inorganic; l++) {
                metals[l] = metals[l] || mid_level_tile.metals[l];
                economics[l] = economics[l] || mid_level_tile.economics[l];
                minerals[l] = minerals[l] || mid_level_tile.minerals[l];
            }
        }
    }

    for (uint16_t l = 0; l < state->max_inorganic; l++) {
        if (metals[l]) {
            site_info->metals.push_back(l);
        }

        if (economics[l]) {
            site_info->economics.push_back(l);
        }

        if (minerals[l]) {
            site_info->minerals.push_back(l);
        }
    }

    //  Take incursions into account.

    for (int8_t i = state->local_min_x; i <= state->local_max_x; i++) {
        //  NW corner, north row
        if ((i == 0 && state->local_min_y == 0 && x - 1 >= 0 && y - 1 >= 0 && !survey_results->at(x - 1).at (y - 1).surveyed) ||
            (i == 0 && x - 1 >= 0 && !survey_results->at(x - 1).at(y).surveyed) ||
            (state->local_min_y == 0 && y - 1 >= 0 && !survey_results->at(x).at(y - 1).surveyed)) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                4,
                x,
                y,
                i,
                state->local_min_y),
                site_info,
                survey_results,
                mlt,
                i,
                state->local_min_y);
        }

        //  N edge, north row
        if (state->local_min_y == 0 && y - 1 >= 0 && !survey_results->at(x).at(y - 1).surveyed) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_ns_edge(survey_results,
                true,
                x,
                y,
                i,
                state->local_min_y),
                site_info,
                survey_results,
                mlt,
                i,
                state->local_min_y);
        }

        //  NE corner, north row
        if ((i == 15 && state->local_min_y == 0 && x + 1 < world_data->world_width && y - 1 >= 0 && !survey_results->at(x + 1).at(y - 1).surveyed) ||
            (i == 15 && x + 1 < world_data->world_width && !survey_results->at(x + 1).at(y).surveyed) ||
            (state->local_min_y == 0 && y - 1 >= 0 && !survey_results->at(x).at(y - 1).surveyed)) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                5,
                x,
                y,
                i,
                state->local_min_y),
                site_info,
                survey_results,
                mlt,
                i,
                state->local_min_y);
         }

        //  SW corner, south row
        if ((i == 0 && state->local_max_y == 15 && x - 1 >= 0 && y + 1 < world_data->world_height && !survey_results->at(x - 1).at(y + 1).surveyed) ||
            (i == 0 && x - 1 >= 0 && !survey_results->at(x - 1).at(y).surveyed) ||
            (state->local_max_y == 15 && y + 1 < world_data->world_height && !survey_results->at(x).at(y + 1).surveyed)) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                7,
                x,
                y,
                i,
                state->local_max_y),
                site_info,
                survey_results,
                mlt,
                i,
                state->local_max_y);
        }

        //  S edge, south row
        if (state->local_max_y == 15 && y + 1 < world_data->world_height && !survey_results->at(x).at(y + 1).surveyed) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_ns_edge(survey_results,
                false,
                x,
                y,
                i,
                state->local_max_y),
                site_info,
                survey_results,
                mlt,
                i,
                state->local_max_y);
        }

        //  SE corner south row
        if ((i == 15 && state->local_max_y == 15 && x + 1 < world_data->world_width && y + 1 < world_data->world_height && !survey_results->at(x + 1).at(y + 1).surveyed) ||
            (i == 15 && x + 1 < world_data->world_width && !survey_results->at(x + 1).at(y).surveyed) ||
            (state->local_max_y == 15 && y + 1 < world_data->world_height && !survey_results->at(x).at(y + 1).surveyed)) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                8,
                x,
                y,
                i,
                state->local_max_y),
                site_info,
                survey_results,
                mlt,
                i,
                state->local_max_y);
        }
    }

    for (int8_t k = state->local_min_y; k <= state->local_max_y; k++) {
        // NW corner, west side
        if ((state->local_min_x == 0 && x - 1 >= 0 && !survey_results->at(x - 1).at(y).surveyed)) {
            incursion_processing_failed = true;
        }
        else if (k > state->local_min_y) { //  We've already covered the NW corner of the NW, with its complications.
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                4,
                x,
                y,
                state->local_min_x,
                k),
                site_info,
                survey_results,
                mlt,
                state->local_min_x,
                k);
        }

        // W edge, west side
        if (state->local_min_x == 0 && x - 1 >= 0 && !survey_results->at(x - 1).at(y).surveyed) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_ew_edge(survey_results,
                true,
                x,
                y,
                state->local_min_x,
                k),
                site_info,
                survey_results,
                mlt,
                state->local_min_x,
                k);
        }

        // SW corner, west side
        if (state->local_min_x == 0 && x - 1 >= 0 && !survey_results->at(x - 1).at(y).surveyed) {
            incursion_processing_failed = true;
        }
        else if (k < state->local_max_y) { //  We've already covered the SW corner of the SW tile, with its complicatinons.
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                7,
                x,
                y,
                state->local_min_x,
                k),
                site_info,
                survey_results,
                mlt,
                state->local_min_x,
                k);
        }

        // NE corner, east side
        if ((state->local_max_x == 15 && x + 1 < world_data->world_width && !survey_results->at(x + 1).at(y).surveyed)) {
            incursion_processing_failed = true;
        }
        else if (k > state->local_min_y) { //  We've already covered the NE tile's NE corner, with its complications.
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                5,
                x,
                y,
                state->local_max_x,
                k),
                site_info,
                survey_results,
                mlt,
                state->local_max_x,
                k);
        }

        // E edge, east side
        if (state->local_max_x == 15 && x + 1 < world_data->world_width && !survey_results->at(x + 1).at(y).surveyed) {
            incursion_processing_failed = true;
        }
        else {
            process_embark_incursion_mid_level_tile
            (translate_ew_edge(survey_results,
                false,
                x,
                y,
                state->local_max_x,
                k),
                site_info,
                survey_results,
                mlt,
                state->local_max_x,
                k);
        }

        // SE corner, east side
        if (state->local_max_x == 15 && x + 1 < world_data->world_width && !survey_results->at(x + 1).at(y).surveyed) {
            incursion_processing_failed = true;
        }
        else if (k < state->local_max_y) { //  We've already covered the SE tile's SE corner, with its complications.
            process_embark_incursion_mid_level_tile
            (translate_corner(survey_results,
                8,
                x,
                y,
                state->local_max_x,
                k),
                site_info,
                survey_results,
                mlt,
                state->local_max_x,
                k);
        }
    }

    if (incursion_processing_failed) site_info->incursions_processed = false;
}

//=================================================================================

void embark_assist::survey::shutdown() {
    fclose(state->rivers_file);
    fclose(state->river_anomalies_file);
    fclose(state->invalid_river_file);

    #define survey_layer_cache_bucket_info_file_name "./data/init/embark_assistant_indexes/survey_layer_cache_bucket_info"
    FILE* bucket_info_file = fopen(survey_layer_cache_bucket_info_file_name, "a");

    /*for (auto iter = state->layer_content_cache.begin(); iter != state->layer_content_cache.end(); ++iter) {
        const auto bucket_number = state->layer_content_cache.bucket((*iter).first);
    }*/

    color_ostream_proxy out(Core::getInstance().getConsole());
    //out.print("embark_assist::survey::shutdown()\n");
    //out.print("bucket_count: %d\n", state->layer_content_cache.bucket_count());
    //out.print("max_bucket_count: %d\n", state->layer_content_cache.max_bucket_count());
    //out.print("load_factor: %f\n", state->layer_content_cache.load_factor());
    //out.print("max_load_factor: %f\n", state->layer_content_cache.max_load_factor());

    //fprintf(bucket_info_file, "bucket_index;bucket_size\n");
    //for (int bucket_index = 0; bucket_index < state->layer_content_cache.bucket_count(); bucket_index++) {
    //    const int bucket_size = state->layer_content_cache.bucket_size(bucket_index);
    //    fprintf(bucket_info_file, "%d;%d\n", bucket_index, bucket_size);
    //}
    //fclose(bucket_info_file);

    delete state;

    number_of_layer_cache_hits = 0;
    number_of_layer_cache_misses = 0;
    number_of_waterfalls = 0;

}

