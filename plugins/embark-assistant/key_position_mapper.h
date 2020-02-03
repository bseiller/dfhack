#pragma once
#include <stdint.h>

namespace embark_assist {
    namespace index {
        namespace key_position_mapper {
            class KeyPositionMapper final {
            private:
                uint16_t world_width;
                uint16_t world_last_x;
                uint16_t world_last_y;
                uint16_t x_tail;
                uint16_t y_tail;
                uint32_t total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width;
            public:
                KeyPositionMapper(const uint16_t world_width, const uint16_t world_height);
                void get_position(uint32_t position_id, uint16_t &x, uint16_t &y, uint16_t &i, uint16_t &k) const;
                const uint32_t key_of(int16_t x, int16_t y, uint8_t i, uint8_t k) const;
            };
        }
    }
}
