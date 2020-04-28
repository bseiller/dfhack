#pragma once

#include "defs.h"

namespace embark_assist {
    namespace key_buffer_holder {

        template<int N>
        class basic_key_buffer_holder : public virtual embark_assist::defs::key_buffer_holder_basic_interface {

            static const uint8_t ARRAY_SIZE_FOR_BIOMES = embark_assist::defs::ARRAY_SIZE_FOR_BIOMES;
            static const uint8_t ARRAY_SIZE_FOR_REGIONS = embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES;

            uint32_t aquiferBuffer[N];
            uint16_t aquifierBufferIndex = 0;

            uint32_t clayBuffer[N];
            uint16_t clayBufferIndex = 0;

            uint32_t sandBuffer[N];
            uint16_t sandBufferIndex = 0;

            std::array<uint32_t[N], 3> savagery_buffers;
            std::array<uint32_t*, 3> savagery_buffer_helper;
            std::array<uint16_t, 3> savagery_buffer_indices;

            std::array<uint32_t[N], 3> evilness_buffers;
            std::array<uint32_t*, 3> evilness_buffer_helper;
            std::array<uint16_t, 3> evilness_buffer_indices;

            std::array<uint32_t[N], embark_assist::defs::SOIL_DEPTH_LEVELS> soil_level_buffers;
            std::array<uint32_t*, embark_assist::defs::SOIL_DEPTH_LEVELS> soil_buffer_helper;
            std::array<uint16_t, embark_assist::defs::SOIL_DEPTH_LEVELS> soil_buffer_indices;

            std::array<uint32_t[N], ARRAY_SIZE_FOR_BIOMES> biome_buffers;
            std::array<uint32_t*, ARRAY_SIZE_FOR_BIOMES> biomes_buffer_helper;
            std::array<int16_t, ARRAY_SIZE_FOR_BIOMES> biomes_buffer_indices;

            std::array<uint32_t[N], ARRAY_SIZE_FOR_REGIONS> region_type_buffers;
            std::array<uint32_t*, ARRAY_SIZE_FOR_REGIONS> region_type_buffer_helper;
            std::array<int16_t, ARRAY_SIZE_FOR_REGIONS> region_type_buffer_indices;

        public:
            basic_key_buffer_holder() {
                for (int index = 0; index < savagery_buffers.size(); index++) {
                    savagery_buffer_helper[index] = savagery_buffers[index];
                }

                for (int index = 0; index < evilness_buffers.size(); index++) {
                    evilness_buffer_helper[index] = evilness_buffers[index];
                }

                for (int index = 0; index < soil_level_buffers.size(); index++) {
                    soil_buffer_helper[index] = soil_level_buffers[index];
                }

                for (int index = 0; index < biome_buffers.size(); index++) {
                    biomes_buffer_helper[index] = biome_buffers[index];
                }

                for (int index = 0; index < region_type_buffers.size(); index++) {
                    region_type_buffer_helper[index] = region_type_buffers[index];
                }
            }

            void add_aquifer(const uint32_t key) {
                aquiferBuffer[aquifierBufferIndex++] = key;
                if (aquifierBufferIndex > 256) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("aquiferBuffer buffer overflow %d\n ", aquifierBufferIndex);
                }
            }

            void get_aquifer_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = aquifierBufferIndex;
                buffer = aquiferBuffer;
            }

            void add_clay(const uint32_t key) {
                clayBuffer[clayBufferIndex++] = key;
            }

            void get_clay_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = clayBufferIndex;
                buffer = clayBuffer;
            }

            void add_sand(const uint32_t key) {
                sandBuffer[sandBufferIndex++] = key;
            }

            void get_sand_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = sandBufferIndex;
                buffer = sandBuffer;
            }

            void add_soil_depth(const uint32_t key, const int8_t soil_depth) {
                soil_level_buffers[soil_depth][soil_buffer_indices[soil_depth]++] = key;
            }

            void get_soil_depth_buffers(const std::array<uint16_t, embark_assist::defs::SOIL_DEPTH_LEVELS> *&indices, const std::array<uint32_t *, embark_assist::defs::SOIL_DEPTH_LEVELS> *&buffers) const override {
                indices = &soil_buffer_indices;
                buffers = &soil_buffer_helper;
            }

            void add_savagery_level(const uint32_t key, const uint8_t savagery_level) {
                savagery_buffers[savagery_level][savagery_buffer_indices[savagery_level]++] = key;
            }

            void get_savagery_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const override {
                indices = &savagery_buffer_indices;
                buffers = &savagery_buffer_helper;
            }

            void add_evilness_level(const uint32_t key, const uint8_t evilness_level) {
                evilness_buffers[evilness_level][evilness_buffer_indices[evilness_level]++] = key;
            }

            void get_evilness_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const override {
                indices = &evilness_buffer_indices;
                buffers = &evilness_buffer_helper;
            }

            void add_biome(const uint32_t key, const int16_t biome) {
                biome_buffers[biome][biomes_buffer_indices[biome]++] = key;
            }

            void get_biome_buffers(const std::array<int16_t, ARRAY_SIZE_FOR_BIOMES> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_BIOMES> *&buffers) const override {
                indices = &biomes_buffer_indices;
                buffers = &biomes_buffer_helper;
            }

            void add_region_type(const uint32_t key, const int8_t region_type) {
                region_type_buffers[region_type][region_type_buffer_indices[region_type]++] = key;
            }

            void get_region_type_buffers(const std::array<int16_t, ARRAY_SIZE_FOR_REGIONS> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_REGIONS> *&buffers) const {
                indices = &region_type_buffer_indices;
                buffers = &region_type_buffer_helper;
            }

            void reset() {
                aquifierBufferIndex = 0;
                clayBufferIndex = 0;
                sandBufferIndex = 0;
                savagery_buffer_indices.assign(0);
                evilness_buffer_indices.assign(0);
                soil_buffer_indices.assign(0);
                biomes_buffer_indices.assign(0);
                region_type_buffer_indices.assign(0);
            }
        };
    }
}