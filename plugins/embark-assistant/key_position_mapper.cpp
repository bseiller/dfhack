
#define NOMINMAX

#include <cmath>
#include "key_position_mapper.h"

embark_assist::index::key_position_mapper::KeyPositionMapper::KeyPositionMapper(const uint16_t world_width, const uint16_t world_height) {
    this->world_width = world_width;
    world_last_x = world_width - 1;
    world_last_y = world_height - 1;

    x_tail = world_width % 16;
    y_tail = world_height % 16;
    total_number_of_mid_level_tiles_per_feature_shell_column_for_world_width = world_width * 256 * 16;
}

void embark_assist::index::key_position_mapper::KeyPositionMapper::get_position(uint32_t position_id, uint16_t &x, uint16_t &y, uint16_t &i, uint16_t &k) const {
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

const uint32_t embark_assist::index::key_position_mapper::KeyPositionMapper::key_of(int16_t x, int16_t y, uint8_t i, uint8_t k) const {
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
    y_base = fs_y_offset * world_width * 256;

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
