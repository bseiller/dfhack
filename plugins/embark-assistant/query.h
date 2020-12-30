#pragma once

#include <cstdio>
#include <utility>
#include <numeric>

namespace embark_assist {
    namespace query {
        using embark_assist::roaring::GuardedRoaring;

        class query_interface {
        public:
            virtual bool run(const Roaring &embark_candidate) const = 0;
            virtual uint32_t get_number_of_entries() const = 0;
            // FIXME: return a smart iterator instead of a complete vector to allow for partial handling / delta steps
            virtual const std::vector<uint32_t>* get_keys() const = 0;
            virtual const void get_world_tile_keys(const uint32_t world_offset, std::vector<uint32_t>& keys) const = 0;
            virtual bool is_to_be_deleted_after_key_extraction() const = 0;
            virtual void flag_for_keeping() = 0;
        };

        template<class Lambda, class Lambda2, class Lambda3, class Lambda4>
        class query : public query_interface {
            Lambda _query_lambda;
            Lambda2 _cardinality_lambda;
            Lambda3 _keys_lambda;
            Lambda4 _world_tile_keys_lambda;
            bool _is_to_be_deleted_after_key_extraction = true;
        public:
            query(Lambda &&query_lambda, Lambda2 &&cardinality_lambda, Lambda3 &&keys_lambda, Lambda4 &&world_tile_keys_lambda) :
                _query_lambda(std::forward<Lambda>(query_lambda)),
                _cardinality_lambda(std::forward<Lambda2>(cardinality_lambda)),
                _keys_lambda(std::forward<Lambda3>(keys_lambda)),
                _world_tile_keys_lambda(std::forward<Lambda4>(world_tile_keys_lambda)) {
            }

            bool run(const Roaring &embark_candidate) const {
                return _query_lambda(embark_candidate);
            }

            uint32_t get_number_of_entries() const {
                return _cardinality_lambda();
            }

            // FIXME: return a smart iterator instead of a complete vector to allow for partial handling / delta steps
            const std::vector<uint32_t>* get_keys() const {
                return _keys_lambda();
            }

            const void get_world_tile_keys(const uint32_t world_offset, std::vector<uint32_t>& keys) const {
                _world_tile_keys_lambda(world_offset, keys);
            }

            bool is_to_be_deleted_after_key_extraction() const final {
                return _is_to_be_deleted_after_key_extraction;
            }

            void flag_for_keeping() final {
                _is_to_be_deleted_after_key_extraction = false;
            }
        };

        template<class Lambda, class Lambda2, class Lambda3, class Lambda4>
        query<Lambda, Lambda2, Lambda3, Lambda4>* make_myclass(Lambda &&query_lambda, Lambda2 &&cardinality_lambda, Lambda3 &&keys_lambda, Lambda4 &&world_tile_keys_lambda) {
            return new query<Lambda, Lambda2, Lambda3, Lambda4>{ std::forward<Lambda>(query_lambda), std::forward<Lambda2>(cardinality_lambda), std::forward<Lambda3>(keys_lambda), std::forward<Lambda4>(world_tile_keys_lambda) };
        }

        class abstract_query : public query_interface {
        private:
            bool _is_to_be_deleted_after_key_extraction = true;
        protected:
            static uint32_t size_of_world;
            static uint16_t embark_size;
        public:
            bool is_to_be_deleted_after_key_extraction() const final {
                return _is_to_be_deleted_after_key_extraction;
            }

            void flag_for_keeping() final {
                _is_to_be_deleted_after_key_extraction = false;
            }

            static void set_world_size(const uint32_t size_of_world) {
                abstract_query::size_of_world = size_of_world;
            }

            static void set_embark_size(const uint16_t embark_size) {
                abstract_query::embark_size = embark_size;
            }

            static uint32_t get_world_size() {
                return abstract_query::size_of_world;
            }

            static const std::vector<uint32_t>* get_world_keys() {
                // FIXME: this is very bad, 64MB RAM bad for a 257x257 region => change return type of method to iterator and implement different iterators
                std::vector<uint32_t>* dest = new std::vector<uint32_t>(abstract_query::size_of_world);
                // fill with number starting 0 till size of world...
                std::iota(dest->begin(), dest->end(), 0);
                return dest;
            }

            static void get_all_world_tile_keys(const uint32_t world_offset, std::vector<uint32_t> &keys) {
                keys.resize(256);
                std::iota(keys.begin(), keys.end(), world_offset);
            }

            static uint16_t get_embark_size() {
                return abstract_query::embark_size;
            }
        };
        uint32_t abstract_query::size_of_world = 0;
        uint16_t abstract_query::embark_size = 0;

        class single_index_run_strategy {
        public:
            virtual bool run(const GuardedRoaring &index, const Roaring &embark_candidate) const = 0;
        };

        class single_index_count_entries_strategy {
        public:
            virtual uint32_t get_number_of_entries(const GuardedRoaring &index) const = 0;
        };

        class single_index_get_keys_strategy {
        public:
            virtual const std::vector<uint32_t>* get_keys(const GuardedRoaring &index) const = 0;
            virtual void get_world_tile_keys(const GuardedRoaring &index, const uint32_t world_offset, std::vector<uint32_t> &keys) const = 0;
        };

        class single_index_query : public abstract_query {
        private:
            const GuardedRoaring &index;
            const single_index_run_strategy &run_strategy;
            const single_index_count_entries_strategy &count_strategy;
            const single_index_get_keys_strategy &keys_strategy;
        public:
            single_index_query(const GuardedRoaring &index,
                const single_index_run_strategy &run_strategy,
                const single_index_count_entries_strategy &count_strategy,
                const single_index_get_keys_strategy &keys_strategy) : index(index), run_strategy(run_strategy), count_strategy(count_strategy), keys_strategy(keys_strategy) {
            }

            static const std::vector<uint32_t>* get_keys(const GuardedRoaring &index) {
                const uint64_t cardinality = index.cardinalityGuarded();
                uint32_t* most_significant_ids = new uint32_t[cardinality];
                index.toUint32ArrayGuarded(most_significant_ids);
                const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + cardinality);

                delete most_significant_ids;
                return dest;
            }

            static void get_world_tile_keys(const GuardedRoaring &index, const uint32_t world_offset, std::vector<uint32_t> &keys) {
                index.getWorldTileKeysGuarded(world_offset, keys);
                /*const uint64_t first_key_position = index.rankGuarded(world_offset) - 1;

                const uint32_t world_offset_end = world_offset + 255;
                const uint64_t last_key_position = index.rankGuarded(world_offset_end);

                const uint16_t limit = last_key_position - first_key_position;
                if (limit > 0) {
                    uint32_t key_buffer[256];
                    index.rangeUint32ArrayGuarded(key_buffer, first_key_position, limit);
                    uint32_t* keybuffer_begin_index = key_buffer;
                    if (key_buffer[0] < world_offset) {
                        keybuffer_begin_index = std::upper_bound(key_buffer, key_buffer + limit, world_offset);
                    }
                    keys.insert(keys.begin(), keybuffer_begin_index, key_buffer + limit);
                    // keys.insert(keys.begin(), key_buffer, key_buffer + limit);
                    int i = 0;
                }*/

                //// debugging
                //if (world_offset == 73216) {
                //    uint32_t key_buffer[256];
                //    index.rangeUint32ArrayGuarded(key_buffer, first_key_position - 1, 256);
                //    int i = 0;
                //}
            }

            // FIXME: this is likely to be very costly in terms of memory and performance => implement custom iterator that knows about the world size/range and returns only
            // values that are not in the index
            static const std::vector<uint32_t>* get_inverted_keys(const GuardedRoaring &index) {
                // FIXME: move this into GuardedRoaring and add a mutex! at least the roaring_bitmap_flip part
                const roaring_bitmap_t* rr = roaring_bitmap_flip(&index.roaring, 0, abstract_query::size_of_world);
                const uint64_t cardinality = roaring_bitmap_get_cardinality(rr);
                uint32_t* most_significant_ids = new uint32_t[cardinality];
                roaring_bitmap_to_uint32_array(rr, most_significant_ids);
                const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + cardinality);
                roaring_bitmap_free(rr);
                delete most_significant_ids;
                return dest;
            }

            static void get_inverted_world_tile_keys(const GuardedRoaring &index, const uint32_t world_offset, std::vector<uint32_t> &keys) {
                // FIXME: move this into GuardedRoaring and add a mutex! at least the roaring_bitmap_flip part
                const uint32_t world_offset_end = world_offset + 255;
                const roaring_bitmap_t* rr = roaring_bitmap_flip(&index.roaring, world_offset, world_offset_end);

                const uint64_t first_key_position = roaring_bitmap_rank(rr, world_offset);
                const uint64_t last_key_position = roaring_bitmap_rank(rr, world_offset_end);

                const uint8_t limit = last_key_position - first_key_position;
                if (limit > 0) {
                    uint32_t key_buffer[256];
                    roaring_bitmap_range_uint32_array(rr, first_key_position, limit + 1, key_buffer);
                    keys.insert(keys.begin(), key_buffer, key_buffer + limit);
                }
                roaring_bitmap_free(rr);
            }

            static const uint32_t get_inverted_cardinality(const GuardedRoaring &index) {
                return abstract_query::size_of_world - index.cardinalityGuarded();
            }

            virtual bool run(const Roaring &embark_candidate) const {
                return run_strategy.run(index, embark_candidate);
            };

            virtual uint32_t get_number_of_entries() const {
                return count_strategy.get_number_of_entries(index);
            };
            // FIXME: return a smart iterator instead of a complete vector to allow for partial handling / delta steps
            virtual const std::vector<uint32_t>* get_keys() const {
                return keys_strategy.get_keys(index);
            };

            virtual const void get_world_tile_keys(const uint32_t world_offset, std::vector<uint32_t>& keys) const {
                return keys_strategy.get_world_tile_keys(index, world_offset, keys);
            };
        };

        class single_index_run_intersect : public single_index_run_strategy {
            bool run(const GuardedRoaring &index, const Roaring &embark_candidate) const {
                return index.intersectGuarded(embark_candidate);
            }
        };

        class single_index_run_all : public single_index_run_strategy {
            bool run(const GuardedRoaring &index, const Roaring &embark_candidate) const {
                if (index.intersectGuarded(embark_candidate)) {
                    return index.and_cardinalityGuarded(embark_candidate) == abstract_query::get_embark_size();
                }
                return false;
            }
        };

        class single_index_run_partial : public single_index_run_strategy {
            bool run(const GuardedRoaring &index, const Roaring &embark_candidate) const {
                // intersection testing is faster than cardinality checking - so we optimize for the negative case
                if (index.intersectGuarded(embark_candidate)) {
                    // there is at least one match for sure
                    // but we don't want all tiles to have this attribute => <
                    // const uint64_t and_cardinality = index.and_cardinalityGuarded(embark_candidate);
                    if (index.and_cardinalityGuarded(embark_candidate) < abstract_query::get_embark_size()) {
                        return true;
                    }
                    //else {
                    //    //color_ostream_proxy out(Core::getInstance().getConsole());
                    //    //out.print("### single_index_run_partial embark_candidate all matching %s ###\n", embark_candidate.toString());
                    //    const auto test = embark_candidate.toString();
                    //    int i = 0;
                    //}
                    // else "fallthrough" to final return
                }
                //else {
                //    //color_ostream_proxy out(Core::getInstance().getConsole());
                //    //out.print("### single_index_run_partial embark_candidate none matching %s ###\n", embark_candidate.toString());
                //    const auto test = embark_candidate.toString();
                //    int i = 0;
                //}
                // there are no matches which is not ok
                return false;
            }
        };

        class single_index_run_not_all : public single_index_run_strategy {
            bool run(const GuardedRoaring &index, const Roaring &embark_candidate) const {
                // intersection testing is faster than cardinality checking - so we optimize for the negative case
                if (index.intersectGuarded(embark_candidate)) {
                    // there is at least one match for sure
                    // but we don't want all tiles to have this attribute => returning false if they do
                    if (index.and_cardinalityGuarded(embark_candidate) == abstract_query::get_embark_size()) {
                        return false;
                    }
                    // else "fallthrough" to final return
                }
                // there are no matches which is just as fine
                return true;
            }
        };

        class single_index_run_not_intersect : public single_index_run_strategy {
            bool run(const GuardedRoaring &index, const Roaring &embark_candidate) const {
                return !index.intersectGuarded(embark_candidate);
            }
        };

        class single_index_count_cardinality : public single_index_count_entries_strategy {
            uint32_t get_number_of_entries(const GuardedRoaring &index) const {
                return index.cardinalityGuarded();
            }
        };

        class single_index_count_inverted_cardinality : public single_index_count_entries_strategy {
            uint32_t get_number_of_entries(const GuardedRoaring &index) const {
                return single_index_query::get_inverted_cardinality(index);
            }
        };

        class single_index_world_cardinality : public single_index_count_entries_strategy {
            uint32_t get_number_of_entries(const GuardedRoaring &index) const {
                return embark_assist::query::abstract_query::get_world_size();
            }
        };

        class single_index_array_keys : public single_index_get_keys_strategy {
            const std::vector<uint32_t>* get_keys(const GuardedRoaring &index) const {
                return single_index_query::get_keys(index);
            }

            void get_world_tile_keys(const GuardedRoaring &index, const uint32_t world_offset, std::vector<uint32_t> &keys) const {
                return single_index_query::get_world_tile_keys(index, world_offset, keys);
            }
        };

        class single_index_array_inverted_keys : public single_index_get_keys_strategy {
            const std::vector<uint32_t>* get_keys(const GuardedRoaring &index) const {
                return single_index_query::get_inverted_keys(index);
            }

            void get_world_tile_keys(const GuardedRoaring &index, const uint32_t world_offset, std::vector<uint32_t> &keys) const {
                return single_index_query::get_inverted_world_tile_keys(index, world_offset, keys);
            }
        };

        class single_index_array_all_world_keys : public single_index_get_keys_strategy {
            const std::vector<uint32_t>* get_keys(const GuardedRoaring &index) const {
                return embark_assist::query::abstract_query::get_world_keys();
            }

            void get_world_tile_keys(const GuardedRoaring &index, const uint32_t world_offset, std::vector<uint32_t> &keys) const {
                embark_assist::query::abstract_query::get_all_world_tile_keys(world_offset, keys);
            }
        };

        class query_strategies {
        public:
            static const single_index_run_intersect SINGLE_INDEX_RUN_INTERSECT;
            static const single_index_run_all SINGLE_INDEX_RUN_ALL;
            static const single_index_run_partial SINGLE_INDEX_RUN_PARTIAL;
            static const single_index_run_not_all SINGLE_INDEX_RUN_NOT_ALL;
            static const single_index_run_not_intersect SINGLE_INDEX_RUN_NOT_INTERSECT;
            static const single_index_count_cardinality SINGLE_INDEX_COUNT_CARDINALITY;
            static const single_index_count_inverted_cardinality SINGLE_INDEX_INVERTED_CARDINALITY;
            static const single_index_world_cardinality SINGLE_INDEX_WORLD_CARDINALITY;
            static const single_index_array_keys SINGLE_INDEX_ARRAY_KEYS;
            static const single_index_array_inverted_keys SINGLE_INDEX_INVERTED_ARRAY_KEYS;
            static const single_index_array_all_world_keys SINGLE_INDEX_ALL_WORLD_ARRAY_KEYS;
        };

        const single_index_run_intersect query_strategies::SINGLE_INDEX_RUN_INTERSECT = single_index_run_intersect();
        const single_index_run_all query_strategies::SINGLE_INDEX_RUN_ALL = single_index_run_all();
        const single_index_run_partial query_strategies::SINGLE_INDEX_RUN_PARTIAL = single_index_run_partial();
        const single_index_run_not_all query_strategies::SINGLE_INDEX_RUN_NOT_ALL = single_index_run_not_all();
        const single_index_run_not_intersect query_strategies::SINGLE_INDEX_RUN_NOT_INTERSECT = single_index_run_not_intersect();
        const single_index_count_cardinality query_strategies::SINGLE_INDEX_COUNT_CARDINALITY = single_index_count_cardinality();
        const single_index_count_inverted_cardinality query_strategies::SINGLE_INDEX_INVERTED_CARDINALITY = single_index_count_inverted_cardinality();
        const single_index_world_cardinality query_strategies::SINGLE_INDEX_WORLD_CARDINALITY = single_index_world_cardinality();
        const single_index_array_keys query_strategies::SINGLE_INDEX_ARRAY_KEYS = single_index_array_keys();
        const single_index_array_inverted_keys query_strategies::SINGLE_INDEX_INVERTED_ARRAY_KEYS = single_index_array_inverted_keys();
        const single_index_array_all_world_keys query_strategies::SINGLE_INDEX_ALL_WORLD_ARRAY_KEYS = single_index_array_all_world_keys();

        class single_index_present_query : public single_index_query {
        public:
            single_index_present_query(const GuardedRoaring &index)
                : single_index_query(index,
                    query_strategies::SINGLE_INDEX_RUN_INTERSECT,
                    query_strategies::SINGLE_INDEX_COUNT_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ARRAY_KEYS) {
            }
        };

        class single_index_absent_query : public single_index_query {
        public:
            single_index_absent_query(const GuardedRoaring &index)
                : single_index_query(index,
                    query_strategies::SINGLE_INDEX_RUN_NOT_INTERSECT,
                    query_strategies::SINGLE_INDEX_INVERTED_CARDINALITY,
                    query_strategies::SINGLE_INDEX_INVERTED_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };

        class single_index_all_query : public single_index_query {
        public:
            single_index_all_query(const GuardedRoaring &index)
                : single_index_query(index,
                    query_strategies::SINGLE_INDEX_RUN_ALL,
                    query_strategies::SINGLE_INDEX_COUNT_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };

        class single_index_partial_query : public single_index_query {
        public:
            single_index_partial_query(const GuardedRoaring &index)
                : single_index_query(index,
                    query_strategies::SINGLE_INDEX_RUN_PARTIAL,
                    query_strategies::SINGLE_INDEX_COUNT_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };

        class single_index_not_all_query : public single_index_query {
        public:
            single_index_not_all_query(const GuardedRoaring &index)
                : single_index_query(index,
                    query_strategies::SINGLE_INDEX_RUN_NOT_ALL,
                    query_strategies::SINGLE_INDEX_WORLD_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ALL_WORLD_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };

        class single_index_multiple_exclusions_run_strategy {
        public:
            virtual bool run(const GuardedRoaring &index, const std::vector<const GuardedRoaring*> &exclusion_indices, const Roaring &embark_candidate) const = 0;
        };

        class single_index_multiple_exclusions_run_all : public single_index_multiple_exclusions_run_strategy {
            bool run(const GuardedRoaring &index, const std::vector<const GuardedRoaring*> &exclusion_indices, const Roaring &embark_candidate) const {
                if (index.intersectGuarded(embark_candidate)) {
                    for (auto exclusion_index : exclusion_indices) {
                        if (exclusion_index->intersectGuarded(embark_candidate)) {
                            return false;
                        }
                    }
                    return index.and_cardinalityGuarded(embark_candidate) == abstract_query::get_embark_size();
                }
                return false;
            }
        };

        class single_index_multiple_exclusions_query_strategies {
        public:
            static const single_index_multiple_exclusions_run_all SINGLE_INDEX_MULTIPLE_EXCLUSIONS_RUN_ALL;
        };

        const single_index_multiple_exclusions_run_all single_index_multiple_exclusions_query_strategies::SINGLE_INDEX_MULTIPLE_EXCLUSIONS_RUN_ALL = single_index_multiple_exclusions_run_all();

        class single_index_multiple_exclusions_query : public abstract_query {
        private:
            const GuardedRoaring &index;
            const std::vector<const GuardedRoaring*> exclusion_indices;
            const single_index_multiple_exclusions_run_strategy &run_strategy;
            const single_index_count_entries_strategy &count_strategy;
            const single_index_get_keys_strategy &keys_strategy;
        public:
            single_index_multiple_exclusions_query(const GuardedRoaring &index,
                const std::vector<const GuardedRoaring*> &exclusion_indices,
                const single_index_multiple_exclusions_run_strategy &run_strategy,
                const single_index_count_entries_strategy &count_strategy,
                const single_index_get_keys_strategy &keys_strategy) : index(index), exclusion_indices(exclusion_indices), run_strategy(run_strategy), count_strategy(count_strategy), keys_strategy(keys_strategy) {
            }

            virtual bool run(const Roaring &embark_candidate) const {
                return run_strategy.run(index, exclusion_indices, embark_candidate);
            };

            virtual uint32_t get_number_of_entries() const {
                return count_strategy.get_number_of_entries(index);
            };
            // FIXME: return a smart iterator instead of a complete vector to allow for partial handling / delta steps
            virtual const std::vector<uint32_t>* get_keys() const {
                return keys_strategy.get_keys(index);
            };

            virtual const void get_world_tile_keys(const uint32_t world_offset, std::vector<uint32_t>& keys) const {
                return keys_strategy.get_world_tile_keys(index, world_offset, keys);
            };
        };

        class single_index_multiple_exclusions_all_query : public single_index_multiple_exclusions_query {
        public:
            single_index_multiple_exclusions_all_query(const GuardedRoaring &index, const std::vector<const GuardedRoaring*> &exclusion_indices)
                : single_index_multiple_exclusions_query(index, exclusion_indices,
                    single_index_multiple_exclusions_query_strategies::SINGLE_INDEX_MULTIPLE_EXCLUSIONS_RUN_ALL,
                    query_strategies::SINGLE_INDEX_COUNT_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };


        class multiple_indices_query_context {
        public:
            const std::vector<GuardedRoaring> &indices;
            const std::vector<GuardedRoaring>::const_iterator min;
            const std::vector<GuardedRoaring>::const_iterator max;

            const int8_t min_results;
            const int8_t max_results;

            multiple_indices_query_context(
                const std::vector<GuardedRoaring> &indices,
                const std::vector<GuardedRoaring>::const_iterator min,
                const std::vector<GuardedRoaring>::const_iterator max) : indices(indices), min(min), max(max), min_results(0), max_results(0) {
            }

            multiple_indices_query_context(
                const std::vector<GuardedRoaring> &indices,
                const std::vector<GuardedRoaring>::const_iterator min,
                const std::vector<GuardedRoaring>::const_iterator max,
                const int8_t min_results,
                const int8_t max_results) : indices(indices), min(min), max(max), min_results(min_results), max_results(max_results) {
            }
        };

        class multiple_indices_run_strategy {
        public:
            virtual bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const = 0;
        };

        class multiple_indices_count_entries_strategy {
        public:
            virtual uint32_t get_number_of_entries(const multiple_indices_query_context context) const = 0;
        };

        class multiple_indices_get_keys_strategy {
        public:
            virtual const std::vector<uint32_t>* get_keys(const multiple_indices_query_context context) const = 0;
            virtual void get_world_tile_keys(const multiple_indices_query_context context, const uint32_t world_offset, std::vector<uint32_t>& keys) const = 0;
        };

        class multiple_indices_query : public abstract_query {
            const multiple_indices_query_context context;
            const multiple_indices_run_strategy &run_strategy;
            const multiple_indices_count_entries_strategy &count_strategy;
            const multiple_indices_get_keys_strategy &keys_strategy;
        public:
            multiple_indices_query(const multiple_indices_query_context context,
                const multiple_indices_run_strategy &run_strategy,
                const multiple_indices_count_entries_strategy &count_strategy,
                const multiple_indices_get_keys_strategy &keys_strategy) : context(context), run_strategy(run_strategy), count_strategy(count_strategy), keys_strategy(keys_strategy) {
            }

            // yes this might count embark tile matches more than once, if they are in multiple indices but thats fine as this is just a rough estimate that helps improve performance
            static const uint32_t get_cardinality(const multiple_indices_query_context context) {
                uint32_t cardinality = 0;

                for (auto iter = context.min; iter != context.max; ++iter) {
                    cardinality += (*iter).cardinalityGuarded();
                }
                return cardinality;
            }

            static const std::vector<uint32_t>* get_keys(const multiple_indices_query_context context) {
                uint32_t cardinality = get_cardinality(context);

                uint32_t* most_significant_ids = new uint32_t[cardinality];
                uint32_t* most_significant_ids_iter = most_significant_ids;

                for (auto iter = context.min; iter != context.max; ++iter) {
                    const GuardedRoaring &index = (*iter);
                    const uint32_t current_cardinality = index.cardinalityGuarded();
                    index.toUint32ArrayGuarded(most_significant_ids_iter);
                    most_significant_ids_iter += current_cardinality;
                }

                //color_ostream_proxy out(Core::getInstance().getConsole());
                //out.print("unflatBuffer buffer overflow %d\n", 44);

                //color_ostream_proxy out(Core::getInstance().getConsole());
                //out.print("cardinality %d\n", cardinality);
                //out.print("iterator distance %d\n", most_significant_ids_iter - most_significant_ids);

                std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + cardinality);
                // making sure that even if a tile was contained in multiple processed indices, we only keep its key once => unique
                std::sort(dest->begin(), dest->end());
                auto last = std::unique(dest->begin(), dest->end());
                dest->erase(last, dest->end());

                delete most_significant_ids;
                return dest;
            }

            static void get_world_tile_keys(const multiple_indices_query_context context, const uint32_t world_offset, std::vector<uint32_t> &keys) {
                uint32_t key_buffer[256];
                for (auto iter = context.min; iter != context.max; ++iter) {
                    const GuardedRoaring &index = (*iter);

                    const uint64_t first_key_position = index.rankGuarded(world_offset);

                    const uint32_t world_offset_end = world_offset + 255;
                    const uint64_t last_key_position = index.rankGuarded(world_offset_end);

                    const uint8_t limit = last_key_position - first_key_position;
                    if (limit > 0) {
                        index.rangeUint32ArrayGuarded(key_buffer, first_key_position, limit + 1);
                        keys.insert(keys.end(), key_buffer, key_buffer + limit);
                    }
                }
            }

            virtual bool run(const Roaring &embark_candidate) const {
                return run_strategy.run(context, embark_candidate);
            };

            virtual uint32_t get_number_of_entries() const {
                return count_strategy.get_number_of_entries(context);
            };

            // FIXME: return a smart iterator instead of a complete vector to allow for partial handling / delta steps
            virtual const std::vector<uint32_t>* get_keys() const {
                return keys_strategy.get_keys(context);
            };

            virtual const void get_world_tile_keys(const uint32_t world_offset, std::vector<uint32_t>& keys) const {
                keys_strategy.get_world_tile_keys(context, world_offset, keys);
            };
        };

        class multiple_indices_run_intersect : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                bool intersects = false;

                for (auto iter = context.min; iter != context.max && !intersects; ++iter) {
                    intersects |= (*iter).intersectGuarded(embark_candidate);
                }

                if (intersects && context.max != context.indices.cend()) {
                    for (auto iter = context.max; iter != context.indices.cend() && intersects; ++iter) {
                        // mustn't intersect, otherwise there is a hit beyond max
                        intersects = intersects && !(*iter).intersectGuarded(embark_candidate);
                    }
                }
                return intersects;
            }
        };

        // doing it differently here than in single_index_run_all
        // instead of using cardinality and compare to embark_size we just make sure that there are no matches outside of the allowed range...
        class multiple_indices_run_all : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                bool intersects = false;

                // making sure there are no matches below the specified min level
                // TODO: find a more performant way to do this, that "knows" if there are exclusions?
                for (std::vector<GuardedRoaring>::const_iterator possible_exclusion_index = context.indices.cbegin(); possible_exclusion_index < context.min; std::advance(possible_exclusion_index, 1)) {
                    if (possible_exclusion_index->intersectGuarded(embark_candidate)) {
                        return false;
                    }
                }

                for (auto iter = context.min; iter != context.max && !intersects; ++iter) {
                    intersects |= (*iter).intersectGuarded(embark_candidate);
                }

                if (intersects && context.max != context.indices.cend()) {
                    for (auto iter = context.max; iter != context.indices.cend() && intersects; ++iter) {
                        // mustn't intersect, otherwise there is a hit beyond max
                        intersects = intersects && !(*iter).intersectGuarded(embark_candidate);
                    }
                }
                return intersects;
            }
        };

        class multiple_indices_run_min_cardinality : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                uint16_t total_cardinality = 0;
                for (auto iter = context.min; iter != context.max; ++iter) {
                    total_cardinality += (*iter).and_cardinalityGuarded(embark_candidate);
                    if (total_cardinality >= context.min_results) {
                        return true;
                    }
                }
                return false;
            }
        };

        class multiple_indices_run_max_cardinality : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                uint16_t total_cardinality = 0;
                // FIXFIX context.min/context.max
                for (auto iter = context.min; iter != context.max; ++iter) {
                    total_cardinality += (*iter).and_cardinalityGuarded(embark_candidate);
                    if (total_cardinality > context.max_results) {
                        return false;
                    }
                }
                return true;
            }
        };

        class multiple_indices_run_cardinality_in_range : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                uint16_t total_cardinality = 0;
                // FIXFIX context.min/context.max
                for (auto iter = context.min; iter != context.max; ++iter) {
                    total_cardinality += (*iter).and_cardinalityGuarded(embark_candidate);
                    if (total_cardinality > context.max_results) {
                        return false;
                    }
                }
                //  no early exit possible in this case
                if (total_cardinality >= context.min_results) {
                    return true;
                }

                return false;
            }
        };

        class multiple_indices_run_min_distinct_intersects : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                uint16_t total_cardinality = 0;
                // FIXFIX context.min/context.max
                for (auto iter = context.min; iter != context.max; ++iter) {
                    if ((*iter).intersectGuarded(embark_candidate)) {
                        ++total_cardinality;
                        if (total_cardinality >= context.min_results) {
                            return true;
                        }
                    }
                }
                return false;
            }
        };

        class multiple_indices_run_max_distinct_intersects : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                uint16_t total_cardinality = 0;
                // FIXFIX context.min/context.max
                for (auto iter = context.min; iter != context.max; ++iter) {
                    if ((*iter).intersectGuarded(embark_candidate)) {
                        ++total_cardinality;
                        if (total_cardinality > context.max_results) {
                            return false;
                        }
                    }
                }
                return true;
            }
        };

        class multiple_indices_run_distinct_intersects_in_range : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                uint16_t total_cardinality = 0;
                // FIXFIX context.min/context.max
                for (auto iter = context.min; iter != context.max; ++iter) {
                    if ((*iter).intersectGuarded(embark_candidate)) {
                        ++total_cardinality;
                        if (total_cardinality > context.max_results) {
                            return false;
                        }
                    }
                }
                //  no early exit possible in this case
                if (total_cardinality >= context.min_results) {
                    return true;
                }

                return false;
            }
        };

        class multiple_indices_count_cardinality : public multiple_indices_count_entries_strategy {
            uint32_t get_number_of_entries(const multiple_indices_query_context context) const {
                return multiple_indices_query::get_cardinality(context);
            }
        };

        class multiple_indices_array_keys : public multiple_indices_get_keys_strategy {
            const std::vector<uint32_t>* get_keys(const multiple_indices_query_context context) const {
                return multiple_indices_query::get_keys(context);
            }

            void get_world_tile_keys(const multiple_indices_query_context context, const uint32_t world_offset, std::vector<uint32_t> &keys) const {
                multiple_indices_query::get_world_tile_keys(context, world_offset, keys);
            }
        };

        class multi_indices_query_strategies {
        public:
            static const multiple_indices_run_intersect MULTI_INDICES_RUN_INTERSECT;
            static const multiple_indices_run_all MULTI_INDICES_RUN_ALL;
            static const multiple_indices_run_min_cardinality MULTI_INDICES_RUN_MIN_CARDINALITY;
            static const multiple_indices_run_max_cardinality MULTI_INDICES_RUN_MAX_CARDINALITY;
            static const multiple_indices_run_cardinality_in_range MULTI_INDICES_RUN_CARDINALITY_IN_RANGE;
            static const multiple_indices_run_min_distinct_intersects MULTI_INDICES_RUN_MIN_DISTINCT_INTERSECTS;
            static const multiple_indices_run_max_distinct_intersects MULTI_INDICES_RUN_MAX_DISTINCT_INTERSECTS;
            static const multiple_indices_run_distinct_intersects_in_range MULTI_INDICES_RUN_DISTINCT_INTERSECTS_IN_RANGE;
            static const multiple_indices_count_cardinality MULTI_INDICES_COUNT_CARDINALITY;
            static const multiple_indices_array_keys MULTI_INDICES_ARRAY_KEYS;
        };

        const multiple_indices_run_intersect multi_indices_query_strategies::MULTI_INDICES_RUN_INTERSECT = multiple_indices_run_intersect();
        const multiple_indices_run_all multi_indices_query_strategies::MULTI_INDICES_RUN_ALL = multiple_indices_run_all();
        const multiple_indices_run_min_cardinality multi_indices_query_strategies::MULTI_INDICES_RUN_MIN_CARDINALITY = multiple_indices_run_min_cardinality();
        const multiple_indices_run_max_cardinality multi_indices_query_strategies::MULTI_INDICES_RUN_MAX_CARDINALITY = multiple_indices_run_max_cardinality();
        const multiple_indices_run_cardinality_in_range multi_indices_query_strategies::MULTI_INDICES_RUN_CARDINALITY_IN_RANGE = multiple_indices_run_cardinality_in_range();
        const multiple_indices_run_min_distinct_intersects multi_indices_query_strategies::MULTI_INDICES_RUN_MIN_DISTINCT_INTERSECTS = multiple_indices_run_min_distinct_intersects();
        const multiple_indices_run_max_distinct_intersects multi_indices_query_strategies::MULTI_INDICES_RUN_MAX_DISTINCT_INTERSECTS = multiple_indices_run_max_distinct_intersects();
        const multiple_indices_run_distinct_intersects_in_range multi_indices_query_strategies::MULTI_INDICES_RUN_DISTINCT_INTERSECTS_IN_RANGE = multiple_indices_run_distinct_intersects_in_range();
        const multiple_indices_count_cardinality multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY = multiple_indices_count_cardinality();
        const multiple_indices_array_keys multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS = multiple_indices_array_keys();

        class multiple_index_min_max_present_in_range_query : public multiple_indices_query {
        public:
            multiple_index_min_max_present_in_range_query(const multiple_indices_query_context context)
                : multiple_indices_query(context,
                    multi_indices_query_strategies::MULTI_INDICES_RUN_INTERSECT,
                    multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY,
                    multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS) {
                // if there is a max value we need to keep the query to verify that all matches are within the min-max-bound
                if (context.max != context.indices.cend()) {
                    flag_for_keeping();
                }
            }
        };

        class multiple_index_min_max_all_in_range_query : public multiple_indices_query {
        public:
            multiple_index_min_max_all_in_range_query(const multiple_indices_query_context context)
                : multiple_indices_query(context,
                    multi_indices_query_strategies::MULTI_INDICES_RUN_ALL,
                    multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY,
                    multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS) {
                // MULTI_INDICES_RUN_ALL makes it necessary to check every embark candidate
                flag_for_keeping();
            }
        };

        const multiple_indices_run_strategy& get_multi_indices_run_cardinality_strategy_for_context(const multiple_indices_query_context context) {
            if (context.min_results != -1) {
                if (context.max_results != -1) {
                    return multi_indices_query_strategies::MULTI_INDICES_RUN_CARDINALITY_IN_RANGE;
                }
                else {
                    return multi_indices_query_strategies::MULTI_INDICES_RUN_MIN_CARDINALITY;
                }
            }
            else if (context.max_results != -1) {
                return multi_indices_query_strategies::MULTI_INDICES_RUN_MAX_CARDINALITY;
            }
            throw std::runtime_error("multiple_indices_query_context contains neither a valid value for min_results nor for max_results");
        }

        class multiple_index_cardinality_query : public multiple_indices_query {
        public:
            multiple_index_cardinality_query(const multiple_indices_query_context context)
                : multiple_indices_query(context,
                    get_multi_indices_run_cardinality_strategy_for_context(context),
                    multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY,
                    multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS) {
                // all 3 MULTI_INDICES_RUN_*CARDINALITY_* make it necessary to check every embark candidate
                flag_for_keeping();
            }
        };

        const multiple_indices_run_strategy& get_multi_indices_run_distinct_intersects_strategy_for_context(const multiple_indices_query_context context) {
            if (context.min_results != -1) {
                if (context.max_results != -1) {
                    return multi_indices_query_strategies::MULTI_INDICES_RUN_DISTINCT_INTERSECTS_IN_RANGE;
                }
                else {
                    return multi_indices_query_strategies::MULTI_INDICES_RUN_MIN_DISTINCT_INTERSECTS;
                }
            }
            else if (context.max_results != -1) {
                return multi_indices_query_strategies::MULTI_INDICES_RUN_MAX_DISTINCT_INTERSECTS;
            }
            throw std::runtime_error("multiple_indices_query_context contains neither a valid value for min_results nor for max_results");
        }

        class multiple_index_distinct_intersects_query : public multiple_indices_query {
        public:
            multiple_index_distinct_intersects_query(const multiple_indices_query_context context)
                : multiple_indices_query(context,
                    get_multi_indices_run_distinct_intersects_strategy_for_context(context),
                    multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY,
                    multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS) {
                // all 3 MULTI_INDICES_RUN_*CARDINALITY_* make it necessary to check every embark candidate
                flag_for_keeping();
            }
        };
    }
}