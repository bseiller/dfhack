#pragma once

#include "roaring.hh"

namespace embark_assist {
    namespace roaring {
        // added locking mechanism to allow for concurrent access without race conditions
        class GuardedRoaring : public Roaring {
            using Roaring::Roaring;
        private:
            mutable std::mutex lock;

        public:
            using Roaring::getSizeInBytes;
            using Roaring::write;

            GuardedRoaring() : Roaring() {
            }

            GuardedRoaring(const GuardedRoaring &r) : Roaring(r) {
            }

            void addManyGuarded(size_t n_args, const uint32_t *vals) {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                Roaring::addMany(n_args, vals);
            }

            uint64_t and_cardinalityGuarded(const Roaring &embark_candidate) const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::and_cardinality(embark_candidate);
            }

            uint64_t cardinality() const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::cardinality();
            }

            uint64_t cardinalityGuarded() const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::cardinality();
            }

            void toUint32ArrayGuarded(uint32_t *ans) const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::toUint32Array(ans);
            }

            void rangeUint32ArrayGuarded(uint32_t *ans, size_t offset, size_t limit) const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::rangeUint32Array(ans, offset, limit);
            }

            uint64_t rankGuarded(uint32_t x) const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::rank(x);
            }

            void getWorldTileKeysGuarded(const uint32_t world_offset, std::vector<uint32_t> &keys) const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);

                uint64_t first_key_position = Roaring::rank(world_offset);
                // if the world_offset itself is in the index we need the position before the returned one because of the way rank works...
                if (first_key_position > 0) {
                    --first_key_position;
                }

                const uint32_t world_offset_end = world_offset + 255;
                const uint64_t last_key_position = Roaring::rank(world_offset_end);

                const uint16_t limit = last_key_position - first_key_position;
                if (limit > 0) {
                    const uint16_t inner_limit = limit;
                    uint32_t key_buffer[256];
                    Roaring::rangeUint32Array(key_buffer, first_key_position, inner_limit);
                    uint32_t* keybuffer_begin_index = key_buffer;
                    if (key_buffer[0] < world_offset) {
                        keybuffer_begin_index = std::upper_bound(key_buffer, key_buffer + inner_limit, world_offset);
                    }
                    keys.insert(keys.begin(), keybuffer_begin_index, key_buffer + inner_limit);
                    int i = 0;
                }

                // debugging
                /*if (world_offset == 73216) {
                    uint32_t key_buffer[256];
                    Roaring::rangeUint32Array(key_buffer, first_key_position - 1, 256);
                    int i = 0;
                }*/
            }

            bool intersectGuarded(const Roaring &embark_candidate) const {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                return Roaring::intersect(embark_candidate);
            }

            bool runOptimizeAndShrinkToFitGuarded(const bool shrinkToFit) {
                const std::lock_guard<std::mutex> add_many_mutex_guard(lock);
                Roaring::runOptimize();
                if (shrinkToFit) {
                    Roaring::shrinkToFit();
                }
                return true;
            }

            void clear() {
                roaring_bitmap_clear(&this->roaring);
            }
        };
    }
}