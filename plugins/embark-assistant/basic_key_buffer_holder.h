#pragma once

#include "defs.h"
#include "key_buffer_holder_interface.h"

namespace embark_assist {
    namespace key_buffer_holder {

        template<int N>
        class basic_key_buffer_holder : public virtual embark_assist::key_buffer_holder::key_buffer_holder_basic_interface {

            const uint16_t BUFFER_SIZE = N;

            static const uint8_t ARRAY_SIZE_FOR_BIOMES = embark_assist::defs::ARRAY_SIZE_FOR_BIOMES;
            static const uint8_t ARRAY_SIZE_FOR_REGIONS = embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES;

            uint32_t unflatBuffer[N];
            uint16_t unflatBufferIndex = 0;

            uint32_t aquiferBuffer[N];
            uint16_t aquifierBufferIndex = 0;

            // TODO: make this one member each as with savagery, evilness, soil ...
            // as there will be the need for another aquifer buffer in the new version => none, light, heavy 
            uint32_t noAquiferBuffer[N];
            uint16_t noAquifierBufferIndex = 0;

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

            // FIXME: only for debugging - remove for release
            static int max_index;

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

                init();
            }

            ~basic_key_buffer_holder() {
                get_max();
            }

            void add_unflat(const uint32_t key) {
                unflatBuffer[unflatBufferIndex++] = key;
                if (unflatBufferIndex > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("unflatBuffer buffer overflow %d\n", unflatBufferIndex);
                }
            }

            void get_unflat_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = unflatBufferIndex;
                buffer = unflatBuffer;
            }

            void add_aquifer(const uint32_t key, const bool has_aquifer) {
                if (has_aquifer) {
                    aquiferBuffer[aquifierBufferIndex++] = key;
                    if (aquifierBufferIndex > N) {
                        color_ostream_proxy out(Core::getInstance().getConsole());
                        out.print("aquiferBuffer buffer overflow %d\n", aquifierBufferIndex);
                    }
                }
                else {
                    noAquiferBuffer[noAquifierBufferIndex++] = key;
                    if (noAquifierBufferIndex > N) {
                        color_ostream_proxy out(Core::getInstance().getConsole());
                        out.print("noAquiferBuffer buffer overflow %d\n", noAquifierBufferIndex);
                    }
                }
            }

            void get_aquifer_buffer(uint16_t &index, const uint32_t *&buffer, const bool has_aquifer) const override {
                if (has_aquifer) {
                    index = aquifierBufferIndex;
                    buffer = aquiferBuffer;
                }
                else {
                    index = noAquifierBufferIndex;
                    buffer = noAquiferBuffer;
                }
            }

            void add_clay(const uint32_t key) {
                clayBuffer[clayBufferIndex++] = key;
                if (clayBufferIndex > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("clayBuffer buffer overflow %d\n", clayBufferIndex);
                }
            }

            void get_clay_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = clayBufferIndex;
                buffer = clayBuffer;
            }

            void add_sand(const uint32_t key) {
                sandBuffer[sandBufferIndex++] = key;
                if (sandBufferIndex > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("sandBuffer buffer overflow %d\n", sandBufferIndex);
                }
            }

            void get_sand_buffer(uint16_t &index, const uint32_t *&buffer) const override {
                index = sandBufferIndex;
                buffer = sandBuffer;
            }

            void add_soil_depth(const uint32_t key, const int8_t soil_depth) {
                soil_level_buffers[soil_depth][soil_buffer_indices[soil_depth]++] = key;
                if (soil_buffer_indices[soil_depth] > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("soil_level_buffers buffer overflow %d\n", soil_buffer_indices[soil_depth]);
                }
            }

            void get_soil_depth_buffers(const std::array<uint16_t, embark_assist::defs::SOIL_DEPTH_LEVELS> *&indices, const std::array<uint32_t *, embark_assist::defs::SOIL_DEPTH_LEVELS> *&buffers) const override {
                indices = &soil_buffer_indices;
                buffers = &soil_buffer_helper;
            }

            void add_savagery_level(const uint32_t key, const uint8_t savagery_level) {
                savagery_buffers[savagery_level][savagery_buffer_indices[savagery_level]++] = key;
                if (savagery_buffer_indices[savagery_level] > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("savagery_buffer_indices buffer overflow %d\n", savagery_buffer_indices[savagery_level]);
                }
            }

            void get_savagery_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const override {
                indices = &savagery_buffer_indices;
                buffers = &savagery_buffer_helper;
            }

            void add_evilness_level(const uint32_t key, const uint8_t evilness_level) {
                if (evilness_level < 0 || evilness_level > 2) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("invalid evilness_level %d\n", evilness_level);
                }
                evilness_buffers[evilness_level][evilness_buffer_indices[evilness_level]++] = key;
                if (evilness_buffer_indices[evilness_level] > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("evilness_buffer_indices buffer overflow %d\n", evilness_buffer_indices[evilness_level]);
                }
            }

            void get_evilness_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const override {
                indices = &evilness_buffer_indices;
                buffers = &evilness_buffer_helper;
            }

            void add_biome(const uint32_t key, const int16_t biome) {
                biome_buffers[biome][biomes_buffer_indices[biome]++] = key;
                if (biomes_buffer_indices[biome] > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("biomes_buffer_indices buffer overflow %d\n", biomes_buffer_indices[biome]);
                }
            }

            void get_biome_buffers(const std::array<int16_t, ARRAY_SIZE_FOR_BIOMES> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_BIOMES> *&buffers) const override {
                indices = &biomes_buffer_indices;
                buffers = &biomes_buffer_helper;
            }

            void add_region_type(const uint32_t key, const int8_t region_type) {
                region_type_buffers[region_type][region_type_buffer_indices[region_type]++] = key;
                if (region_type_buffer_indices[region_type] > N) {
                    color_ostream_proxy out(Core::getInstance().getConsole());
                    out.print("region_type_buffers buffer overflow %d, region_type %d\n", region_type_buffer_indices[region_type], region_type);
                }
            }

            void get_region_type_buffers(const std::array<int16_t, ARRAY_SIZE_FOR_REGIONS> *&indices, const std::array<uint32_t *, ARRAY_SIZE_FOR_REGIONS> *&buffers) const {
                indices = &region_type_buffer_indices;
                buffers = &region_type_buffer_helper;
            }

            void init() {
                unflatBufferIndex = 0;
                aquifierBufferIndex = 0;
                noAquifierBufferIndex = 0;
                clayBufferIndex = 0;
                sandBufferIndex = 0;
                std::fill(savagery_buffer_indices.begin(), savagery_buffer_indices.end(), 0);
                std::fill(evilness_buffer_indices.begin(), evilness_buffer_indices.end(), 0);
                std::fill(soil_buffer_indices.begin(), soil_buffer_indices.end(), 0);
                std::fill(biomes_buffer_indices.begin(), biomes_buffer_indices.end(), 0);
                std::fill(region_type_buffer_indices.begin(), region_type_buffer_indices.end(), 0);
            }

            void reset() {
                // FIXME: only for debugging - remove for release
                get_max();

                init();
            }

            // FIXME: only for debugging - remove for release
            void get_max() {
                // only for debugging
                uint32_t max_buffer = 0;
                max_buffer = std::max<int>(aquifierBufferIndex, max_buffer);
                max_buffer = std::max<int>(noAquifierBufferIndex, max_buffer);
                max_buffer = std::max<int>(clayBufferIndex, max_buffer);
                max_buffer = std::max<int>(sandBufferIndex, max_buffer);

                for (int index = 0; index < savagery_buffers.size(); index++) {
                    max_buffer = std::max<int>(savagery_buffer_indices[index], max_buffer);
                }

                for (int index = 0; index < evilness_buffers.size(); index++) {
                    max_buffer = std::max<int>(evilness_buffer_indices[index], max_buffer);
                }

                for (int index = 0; index < soil_level_buffers.size(); index++) {
                    max_buffer = std::max<int>(soil_buffer_indices[index], max_buffer);
                }

                for (int index = 0; index < biome_buffers.size(); index++) {
                    max_buffer = std::max<int>(biomes_buffer_indices[index], max_buffer);
                }

                for (int index = 0; index < region_type_buffers.size(); index++) {
                    max_buffer = std::max<int>(region_type_buffer_indices[index], max_buffer);
                }

                max_index = std::max<int>(max_index, max_buffer);
            }
        };
    }
}