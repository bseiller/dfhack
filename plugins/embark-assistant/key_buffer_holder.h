
#pragma once

#include <list>

#include "df/inorganic_raw.h"
#include "df/global_objects.h"
#include "df/world.h"
#include "df/world_data.h"

#include "basic_key_buffer_holder.h"
#include "inorganics_information.h"

using df::global::world;

namespace embark_assist {
    namespace key_buffer_holder {

// we don't care for the warning "C4250: 'embark_assist::survey::key_buffer_holder': inherits 'embark_assist::key_buffer_holder::basic_key_buffer_holder<256>::embark_assist::key_buffer_holder::basic_key_buffer_holder<256>::get_aquifer_buffer' via dominance"
#pragma warning( push )
#pragma warning( disable : 4250 )

        class key_buffer_holder : public virtual embark_assist::defs::key_buffer_holder_interface, public embark_assist::key_buffer_holder::basic_key_buffer_holder<256> {
            uint16_t max_inorganic;
            const std::vector<bool> *is_relevant_mineral;

            uint32_t coalBuffer[256];
            uint16_t coalBufferIndex = 0;

            uint32_t fluxBuffer[256];
            uint16_t fluxBufferIndex = 0;

            uint32_t bloodRainBuffer[256];
            uint16_t bloodRainBufferIndex = 0;

            std::vector<uint32_t*> metal_buffers;
            std::vector<uint16_t> metal_buffer_indices;

            std::vector<uint32_t*> economic_buffers;
            std::vector<uint16_t> economic_buffer_indices;

            std::vector<uint32_t*> mineral_buffers;
            std::vector<uint16_t> mineral_buffer_indices;

            std::array<uint32_t[256], 4> adamantine_level_buffers;
            std::array<uint32_t*, 4> adamantine_buffer_helper;
            std::array<uint16_t, 4> adamantine_buffer_indices;

            std::array<uint32_t[256], 4> magma_level_buffers;
            std::array<uint32_t*, 4> magma_buffer_helper;
            std::array<uint16_t, 4> mamgma_buffer_indices;

            std::array<uint32_t[256], embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> river_size_buffers;
            std::array<uint32_t*, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> river_size_buffer_helper;
            std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> river_size_indices;

            uint32_t current_initial_offset = 0;
            uint8_t mapped_elevation_buffer[256];
            uint16_t mapped_elevation_buffer_index = 0;

            // 480 = 256*2 - (2*16) or 2 * 15 * 16 , as every tile can have 2 outgoing waterfalls, but the last row and the last column can't have 2 "outgoing" waterfalls but just 1
            std::array<std::array<std::pair<uint32_t, uint32_t>, 480>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> waterfall_drop_buffers;
            std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> waterfall_drop_indices;

            uint32_t no_waterfall_buffer[256];
            uint16_t no_waterfall_index = 0;
        public:
            key_buffer_holder() {
                const embark_assist::inorganics::inorganics_information &information = embark_assist::inorganics::inorganics_information::get_instance();

                max_inorganic = information.get_max_inorganics();
                is_relevant_mineral = information.get_relevant_minerals();

                metal_buffers.resize(max_inorganic, nullptr);
                const std::list<uint16_t>& metal_indices = information.get_metal_indices();
                for (auto metal_index : metal_indices) {
                    metal_buffers[metal_index] = new uint32_t[256];
                }
                metal_buffer_indices.resize(max_inorganic, 0);

                economic_buffers.resize(max_inorganic, nullptr);
                const std::list<uint16_t>& economic_indices = information.get_economic_indices();
                for (auto economic_index : economic_indices) {
                    economic_buffers[economic_index] = new uint32_t[256];
                }
                economic_buffer_indices.resize(max_inorganic, 0);

                mineral_buffers.resize(max_inorganic, nullptr);
                const std::list<uint16_t>& mineral_indices = information.get_mineral_indices();
                for (auto mineral_index : mineral_indices) {
                    mineral_buffers[mineral_index] = new uint32_t[256];
                }
                mineral_buffer_indices.resize(max_inorganic, 0);

                for (int index = 0; index < adamantine_level_buffers.size(); index++) {
                    adamantine_buffer_helper[index] = adamantine_level_buffers[index];
                }

                for (int index = 0; index < magma_level_buffers.size(); index++) {
                    magma_buffer_helper[index] = magma_level_buffers[index];
                }

                for (int index = 0; index < river_size_buffers.size(); index++) {
                    river_size_buffer_helper[index] = river_size_buffers[index];
                }
            }

            ~key_buffer_holder() {
                clear();
            }

            void add_coal(const uint32_t key) {
                coalBuffer[coalBufferIndex++] = key;
                if (coalBufferIndex > 256) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("coalBuffer buffer overflow %d\n ", coalBufferIndex);
                }
            }

            void get_coal_buffer(uint16_t & index, const uint32_t *&buffer) const override {
                index = coalBufferIndex;
                buffer = coalBuffer;
            }

            void add_flux(const uint32_t key) {
                fluxBuffer[fluxBufferIndex++] = key;
                if (fluxBufferIndex > 256) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("fluxBuffer buffer overflow %d\n ", fluxBufferIndex);
                }
            }

            void get_flux_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = fluxBufferIndex;
                buffer = fluxBuffer;
            }

            void add_blood_rain(const uint32_t key) {
                bloodRainBuffer[bloodRainBufferIndex++] = key;
                if (bloodRainBufferIndex > 256) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("bloodRainBuffer buffer overflow %d\n ", bloodRainBufferIndex);
                }
            }

            void get_blood_rain_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = bloodRainBufferIndex;
                buffer = bloodRainBuffer;
            }

            void add_metal(const uint32_t key, const int16_t index) {
                if (index < 0) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("negative metal index %d\n ", index);
                }
                metal_buffers[index][metal_buffer_indices[index]++] = key;
                if (metal_buffer_indices[index] > 256 || index >= max_inorganic) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("metal index overflow %d\n ", metal_buffer_indices[index]);
                }
            }

            void get_metal_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t *> *&buffers) const override {
                indices = &metal_buffer_indices;
                buffers = &metal_buffers;
            }

            void add_economic(const uint32_t key, const uint16_t index) {
                economic_buffers[index][economic_buffer_indices[index]++] = key;
                if (economic_buffer_indices[index] > 256 || index >= max_inorganic) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("economic index overflow %d\n ", economic_buffer_indices[index]);
                }
            }

            void get_economic_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t *> *&buffers) const override {
                indices = &economic_buffer_indices;
                buffers = &economic_buffers;
            }

            void add_mineral(const uint32_t key, const uint16_t index) {
                if (is_relevant_mineral->at(index)) {
                    mineral_buffers[index][mineral_buffer_indices[index]++] = key;
                    if (mineral_buffer_indices[index] > 256 || index >= max_inorganic) {
                        color_ostream_proxy out(Core::getInstance().getConsole());
                        out.print("mineral index overflow %d\n ", mineral_buffer_indices[index]);
                    }
                }
            }

            void get_mineral_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t *> *&buffers) const override {
                indices = &mineral_buffer_indices;
                buffers = &mineral_buffers;
            }

            void add_adamantine_level(const uint32_t key, const int8_t adamantine_level) {
                if (adamantine_level > -1) {
                    adamantine_level_buffers[adamantine_level][adamantine_buffer_indices[adamantine_level]++] = key;
                    if (adamantine_buffer_indices[adamantine_level] > 256) {
                        color_ostream_proxy out(Core::getInstance().getConsole());
                        out.print("adamantine_level_buffers overflow value %d, adamantine_level: %d\n ", adamantine_buffer_indices[adamantine_level], adamantine_level);
                    }
                }
            }

            void get_adamantine_level_buffers(const std::array<uint16_t, 4> *&indices, const std::array<uint32_t *, 4> *&buffers) const override {
                indices = &adamantine_buffer_indices;
                buffers = &adamantine_buffer_helper;
            }

            void add_magma_level(const uint32_t key, const int8_t magma_level) {
                if (magma_level > -1) {
                    magma_level_buffers[magma_level][mamgma_buffer_indices[magma_level]++] = key;
                    if (mamgma_buffer_indices[magma_level] > 256) {
                        color_ostream_proxy out(Core::getInstance().getConsole());
                        out.print("mamgma_level_buffers overflow value %d, magma_level: %d\n ", mamgma_buffer_indices[magma_level], magma_level);
                    }
                }
            }

            void get_magma_level_buffers(const std::array<uint16_t, 4> *&indices, const std::array<uint32_t *, 4> *&buffers) const override {
                indices = &mamgma_buffer_indices;
                buffers = &magma_buffer_helper;
            }

            void add_river_size(const uint32_t key, const embark_assist::defs::river_sizes river_size) {
                const auto index = static_cast<uint8_t>(river_size);
                river_size_buffers[index][river_size_indices[index]++] = key;
            }

            void get_river_size_buffers(const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> *&indices, const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> *&buffers) const override {
                indices = &river_size_indices;
                buffers = &river_size_buffer_helper;
            }

            void add_mapped_elevation(const uint32_t key, const uint8_t mapped_elevation) {
                mapped_elevation_buffer[mapped_elevation_buffer_index++] = mapped_elevation;
                if (mapped_elevation_buffer_index > 256) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("mapped_elevation_buffer_index buffer overflow %d, mapped_elevation %d\n", mapped_elevation_buffer_index, mapped_elevation);
                }
            }

            void set_current_initial_offset(uint32_t initial_offset) {
                current_initial_offset = initial_offset;
            }

            void get_mapped_elevation_buffer(uint16_t & index, const uint8_t *&buffer, uint32_t &initial_offset) const override {
                index = mapped_elevation_buffer_index;
                buffer = mapped_elevation_buffer;
                initial_offset = current_initial_offset;
            }

            void add_waterfall_drop(const uint32_t key, const uint32_t key2, const uint8_t raw_waterfall_drop) {
                uint8_t waterfall_drop = raw_waterfall_drop;
                if (raw_waterfall_drop > 9) {
                    waterfall_drop = 9;
                }
                uint16_t &index = waterfall_drop_indices[waterfall_drop];
                std::pair<uint32_t, uint32_t> &keys = waterfall_drop_buffers[waterfall_drop][index];
                keys.first = key;
                keys.second = key2;
                ++index;
            }

            void get_waterfall_drop_buffers(const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> *&indices, 
                                            const std::array<std::array<std::pair<uint32_t, uint32_t>, 480>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> *&buffers) const {
                indices = &waterfall_drop_indices;
                buffers = &waterfall_drop_buffers;
            }

            void add_no_waterfall(const uint32_t key) {
                no_waterfall_buffer[no_waterfall_index++] = key;
            }

            void get_no_waterfall_buffers(uint16_t &index, const uint32_t *&buffer) const {
                index = no_waterfall_index;
                buffer = no_waterfall_buffer;
            }

            void reset() {
                embark_assist::key_buffer_holder::basic_key_buffer_holder<256>::reset();
                coalBufferIndex = 0;
                fluxBufferIndex = 0;
                bloodRainBufferIndex = 0;
                std::fill(metal_buffer_indices.begin(), metal_buffer_indices.end(), 0);
                std::fill(economic_buffer_indices.begin(), economic_buffer_indices.end(), 0);
                std::fill(mineral_buffer_indices.begin(), mineral_buffer_indices.end(), 0);
                std::fill(adamantine_buffer_indices.begin(), adamantine_buffer_indices.end(), 0);
                std::fill(mamgma_buffer_indices.begin(), mamgma_buffer_indices.end(), 0);
                std::fill(river_size_indices.begin(), river_size_indices.end(), 0);
                std::fill(waterfall_drop_indices.begin(), waterfall_drop_indices.end(), 0);
                mapped_elevation_buffer_index = 0;
                no_waterfall_index = 0;
            }

            void clear() {
                reset();
                for (auto &it : metal_buffers) {
                    if (it != nullptr) {
                        delete it;
                    }
                }
                metal_buffers.clear();

                for (auto &it : economic_buffers) {
                    if (it != nullptr) {
                        delete it;
                    }
                }
                economic_buffers.clear();

                for (auto &it : mineral_buffers) {
                    if (it != nullptr) {
                        delete it;
                    }
                }
                mineral_buffers.clear();
            }
        };
#pragma warning( pop )
    }
}