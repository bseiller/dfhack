#pragma once

#include <numeric>

namespace embark_assist {
    namespace key_buffer_holder {

        enum class temperatur : uint8_t {
            MIN_ZERO_OR_BELOW,
            MIN_ABOVE_ZERO,
            MAX_ZERO_OR_BELOW,
            MAX_ABOVE_ZERO
        };

        const uint8_t FREEZING_ARRAY_LENGTH = ((const uint8_t)temperatur::MAX_ABOVE_ZERO + 1);

        enum class syndrome_rain_index : uint8_t {
            // both positive and both negative indices are consecutive to allow easier processing with multi index queries and strategies
            PERMANENT_SYNDROME,
            TEMPORARY_SYNDROME,
            NO_PERMANENT_SYNDROME,
            NO_TEMPORARY_SYNDROME
        };

        const uint8_t SYNDROME_RAIN_ARRAY_LENGTH = ((const uint8_t)syndrome_rain_index::NO_TEMPORARY_SYNDROME + 1);

        enum class reanimation_thralling_index : uint8_t {
            // both positive and both negative indices are consecutive to allow easier processing with multi index queries and strategies
            REANIMATION,
            THRALLING,
            NO_REANIMATION,
            NO_THRALLING
        };

        const uint8_t REANIMATION_THRALLING_ARRAY_LENGTH = ((const uint8_t)reanimation_thralling_index::NO_THRALLING + 1);

        class key_buffer_holder_basic_interface {
        public:
            virtual void add_unflat(const uint32_t key) = 0;
            virtual void get_unflat_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_aquifer(const uint32_t key, const bool has_aquifer) = 0;
            virtual void get_aquifer_buffer(uint16_t &index, const uint32_t *&buffer, const bool has_aquifer) const = 0;
            virtual void add_clay(const uint32_t key) = 0;
            virtual void get_clay_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_sand(const uint32_t key) = 0;
            virtual void get_sand_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_blood_rain(const uint32_t key) = 0;
            virtual void get_blood_rain_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void add_soil_depth(const uint32_t key, const int8_t soil_depth) = 0;
            virtual void get_soil_depth_buffers(const std::array<uint16_t, embark_assist::defs::SOIL_DEPTH_LEVELS> *&indices, const std::array<uint32_t *, embark_assist::defs::SOIL_DEPTH_LEVELS> *&buffers) const = 0;
            virtual void add_temperatur_freezing(const uint32_t key, const temperatur temperatur) = 0;
            virtual void get_temperatur_freezing(const std::array<uint16_t, FREEZING_ARRAY_LENGTH> *&indices, const std::array<uint32_t *, FREEZING_ARRAY_LENGTH> *&buffers) const = 0;
            virtual void add_syndrome_rain(const uint32_t key, const syndrome_rain_index syndrome_rain) = 0;
            virtual void get_syndrome_rain(const std::array<uint16_t, SYNDROME_RAIN_ARRAY_LENGTH> *&indices, const std::array<uint32_t *, SYNDROME_RAIN_ARRAY_LENGTH> *&buffers) const = 0;
            virtual void add_reanimation_thralling(const uint32_t key, const reanimation_thralling_index reanimation_thralling) = 0;
            virtual void get_reanimation_thralling(const std::array<uint16_t, REANIMATION_THRALLING_ARRAY_LENGTH> *&indices, const std::array<uint32_t *, REANIMATION_THRALLING_ARRAY_LENGTH> *&buffers) const = 0;
            virtual void add_savagery_level(const uint32_t key, const uint8_t savagery_level) = 0;
            virtual void get_savagery_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const = 0;
            virtual void add_evilness_level(const uint32_t key, const uint8_t evilness_level) = 0;
            virtual void get_evilness_level_buffers(const std::array<uint16_t, 3> *&indices, const std::array<uint32_t *, 3> *&buffers) const = 0;
            virtual void add_biome(const uint32_t key, const int16_t biome) = 0;
            virtual void get_biome_buffers(const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> *&indices, const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_BIOMES> *&buffers) const = 0;
            virtual void add_region_type(const uint32_t key, const int8_t region_type) = 0;
            virtual void get_region_type_buffers(const std::array<int16_t, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> *&indices, const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_REGION_TYPES> *&buffers) const = 0;
            virtual void reset() = 0;
            virtual ~key_buffer_holder_basic_interface() {}
        };

        class key_buffer_holder_interface : public virtual key_buffer_holder_basic_interface {
        public:
            virtual void get_coal_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void get_flux_buffer(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void get_adamantine_level_buffers(const std::array<uint16_t, 4> *&indices, const std::array<uint32_t *, 4> *&buffers) const = 0;
            virtual void get_magma_level_buffers(const std::array<uint16_t, 4> *&indices, const std::array<uint32_t *, 4> *&buffers) const = 0;
            virtual void get_metal_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t*> *&buffers) const = 0;
            virtual void get_economic_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t*> *&buffers) const = 0;
            virtual void get_mineral_buffers(const std::vector<uint16_t> *&indices, const std::vector<uint32_t*> *&buffers) const = 0;
            virtual void get_river_size_buffers(const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> *&indices, const std::array<uint32_t *, embark_assist::defs::ARRAY_SIZE_FOR_RIVER_SIZES> *&buffers) const = 0;
            virtual void get_waterfall_drop_buffers(const std::array<uint16_t, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> *&indices, const std::array<std::array<std::pair<uint32_t, uint32_t>, 480>, embark_assist::defs::ARRAY_SIZE_FOR_WATERFALL_DROPS> *&buffers) const = 0;
            virtual void get_no_waterfall_buffers(uint16_t &index, const uint32_t *&buffer) const = 0;
            virtual void get_mapped_elevation_buffer(uint16_t &index, const uint8_t *&buffer, uint32_t &initial_offset) const = 0;
        };
    }
}