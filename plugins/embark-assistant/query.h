#pragma once

#include <cstdio>
#include <utility>
#include <numeric>

namespace embark_assist {
    namespace query {
        class query_interface {
        public:
            virtual bool run(const Roaring &embark_candidate) const = 0;
            virtual uint32_t get_number_of_entries() const = 0;
            // FIXME: return a smart iterator instead of a complete vector to allow for partial handling / delta steps
            virtual const std::vector<uint32_t>* get_keys() const = 0;
            // FIXME: add method to query for significant keys within the key range of a world tile
            // virtual const std::vector<uint32_t>* get_keys_in_range(uint32_t start, uint32_t end) const = 0;
            // or virtual const bool has_keys_in_range(uint32_t start, uint32_t end) const = 0;
            virtual bool is_to_be_deleted_after_key_extraction() const = 0;
            virtual void flag_for_keeping() = 0;
        };

        template<class Lambda, class Lambda2, class Lambda3>
        class query : public query_interface {
            Lambda _query_lambda;
            Lambda2 _cardinality_lambda;
            Lambda3 _keys_lambda;
            bool _is_to_be_deleted_after_key_extraction = true;
        public:
            query(Lambda &&query_lambda, Lambda2 &&cardinality_lambda, Lambda3 &&keys_lambda) : 
                _query_lambda(std::forward<Lambda>(query_lambda)), 
                //_cardinality_lambda(cardinality_lambda),
                _cardinality_lambda(std::forward<Lambda2>(cardinality_lambda)),
                _keys_lambda(std::forward<Lambda3>(keys_lambda)) {
            }

            /*void operator()() {
                _query_lambda();
            }*/

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

            bool is_to_be_deleted_after_key_extraction() const final{
                return _is_to_be_deleted_after_key_extraction;
            }

            void flag_for_keeping() final {
                _is_to_be_deleted_after_key_extraction = false;
            }
        };

        template<class Lambda, class Lambda2, class Lambda3>
        query<Lambda, Lambda2, Lambda3>* make_myclass(Lambda &&query_lambda, Lambda2 &&cardinality_lambda, Lambda3 &&keys_lambda) {
            return new query<Lambda, Lambda2, Lambda3>{ std::forward<Lambda>(query_lambda), std::forward<Lambda2>(cardinality_lambda), std::forward<Lambda3>(keys_lambda) };
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
                std::vector<uint32_t>* dest = new std::vector<uint32_t>(abstract_query::size_of_world);
                // fill with number starting 0 till size of world...
                std::iota(dest->begin(), dest->end(), 0);
                return dest;
            }

            static uint16_t get_embark_size() {
                return abstract_query::embark_size;
            }
        };
        uint32_t abstract_query::size_of_world = 0;
        uint16_t abstract_query::embark_size = 0;

        class single_index_run_strategy {
        public:
            virtual bool run(const Roaring &index, const Roaring &embark_candidate) const = 0;
        };

        class single_index_count_entries_strategy {
        public:
            virtual uint32_t get_number_of_entries(const Roaring &index) const = 0;
        };

        class single_index_get_keys_strategy {
        public:
            virtual const std::vector<uint32_t>* get_keys(const Roaring &index) const = 0;
        };

        class single_index_query : public abstract_query {
        private:
            const Roaring &index;
            const single_index_run_strategy &run_strategy;
            const single_index_count_entries_strategy &count_strategy;
            const single_index_get_keys_strategy &keys_strategy;
        public:
            single_index_query(const Roaring &index,
                const single_index_run_strategy &run_strategy,
                const single_index_count_entries_strategy &count_strategy,
                const single_index_get_keys_strategy &keys_strategy) : index(index), run_strategy(run_strategy), count_strategy(count_strategy), keys_strategy(keys_strategy) {
            }

            static const std::vector<uint32_t>* get_keys(const Roaring &index) {
                uint32_t* most_significant_ids = new uint32_t[index.cardinality()];
                index.toUint32Array(most_significant_ids);
                const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + index.cardinality());

                delete most_significant_ids;
                return dest;
            }

            // FIXME: this is likely to be very costly in terms of memory and performance => implement custom iterator that knows about the world size/range and returns only
            // values that are not in the index
            static const std::vector<uint32_t>* get_inverted_keys(const Roaring &index) {
                const roaring_bitmap_t* rr = roaring_bitmap_flip(&index.roaring, 0, abstract_query::size_of_world);
                const uint64_t cardinality = roaring_bitmap_get_cardinality(rr);
                uint32_t* most_significant_ids = new uint32_t[cardinality];
                roaring_bitmap_to_uint32_array(rr, most_significant_ids);
                const std::vector<uint32_t>* dest = new std::vector<uint32_t>(most_significant_ids, most_significant_ids + cardinality);
                roaring_bitmap_free(rr);
                delete most_significant_ids;
                return dest;
            }

            static const uint32_t get_inverted_cardinality(const Roaring &index) {
                return abstract_query::size_of_world - index.cardinality();
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

            // FIXME: add method to query for significant keys within the key range of a world tile
            // virtual const std::vector<uint32_t>* get_keys_in_range(uint32_t start, uint32_t end) const = 0;
            // or virtual const bool has_keys_in_range(uint32_t start, uint32_t end) const = 0;
        };

        class single_index_run_intersect : public single_index_run_strategy {
            bool run(const Roaring &index, const Roaring &embark_candidate) const {
                return index.intersect(embark_candidate);
            }
        };

        class single_index_run_all : public single_index_run_strategy {
            bool run(const Roaring &index, const Roaring &embark_candidate) const {
                if (index.intersect(embark_candidate)) {
                    return index.and_cardinality(embark_candidate) == abstract_query::get_embark_size();
                }
                return false;
            }
        };

        class single_index_run_partial : public single_index_run_strategy {
            bool run(const Roaring &index, const Roaring &embark_candidate) const {
                if (index.intersect(embark_candidate)) {
                    // there is at least one match for sure
                    // but we don't want all tiles to have this attribute => <
                    if (index.and_cardinality(embark_candidate) < abstract_query::get_embark_size()) {
                        return true;
                    }
                }
                return false;
            }
        };

        class single_index_run_not_all : public single_index_run_strategy {
            bool run(const Roaring &index, const Roaring &embark_candidate) const {
                if (index.intersect(embark_candidate)) {
                    // there is at least one match for sure
                    // but we don't want all tiles to have this attribute => <
                    if (index.and_cardinality(embark_candidate) < abstract_query::get_embark_size()) {
                        return true;
                    }
                }
                // there are no matches which is just as fine
                return true;
            }
        };

        class single_index_run_not_intersect : public single_index_run_strategy {
            bool run(const Roaring &index, const Roaring &embark_candidate) const {
                return !index.intersect(embark_candidate);
            }
        };

        class single_index_count_cardinality : public single_index_count_entries_strategy {
            uint32_t get_number_of_entries(const Roaring &index) const {
                return index.cardinality();
            }
        };

        class single_index_count_inverted_cardinality : public single_index_count_entries_strategy {
            uint32_t get_number_of_entries(const Roaring &index) const {
                return single_index_query::get_inverted_cardinality(index);
            }
        };

        class single_index_array_keys : public single_index_get_keys_strategy {
            const std::vector<uint32_t>* get_keys(const Roaring &index) const {
                return single_index_query::get_keys(index);
            }
        };

        class single_index_array_inverted_keys : public single_index_get_keys_strategy {
            const std::vector<uint32_t>* get_keys(const Roaring &index) const {
                return single_index_query::get_inverted_keys(index);
            }
        };

        class query_strategies {
        public:
            static const single_index_run_intersect SINGLE_INDEX_RUN_INTERSECT;
            static const single_index_run_all SINGLE_INDEX_RUN_ALL;
            static const single_index_run_not_intersect SINGLE_INDEX_RUN_NOT_INTERSECT;
            static const single_index_count_cardinality SINGLE_INDEX_COUNT_CARDINALITY;
            static const single_index_count_inverted_cardinality SINGLE_INDEX_INVERTED_CARDINALITY;
            static const single_index_array_keys SINGLE_INDEX_ARRAY_KEYS;
            static const single_index_array_inverted_keys SINGLE_INDEX_INVERTED_ARRAY_KEYS;
        };

        const single_index_run_intersect query_strategies::SINGLE_INDEX_RUN_INTERSECT = single_index_run_intersect();
        const single_index_run_all query_strategies::SINGLE_INDEX_RUN_ALL = single_index_run_all();
        const single_index_run_not_intersect query_strategies::SINGLE_INDEX_RUN_NOT_INTERSECT = single_index_run_not_intersect();
        const single_index_count_cardinality query_strategies::SINGLE_INDEX_COUNT_CARDINALITY = single_index_count_cardinality();
        const single_index_count_inverted_cardinality query_strategies::SINGLE_INDEX_INVERTED_CARDINALITY = single_index_count_inverted_cardinality();
        const single_index_array_keys query_strategies::SINGLE_INDEX_ARRAY_KEYS = single_index_array_keys();
        const single_index_array_inverted_keys query_strategies::SINGLE_INDEX_INVERTED_ARRAY_KEYS = single_index_array_inverted_keys();

        class single_index_present_query : public single_index_query {
        public:
            single_index_present_query(const Roaring &index)
                : single_index_query(index, 
                    query_strategies::SINGLE_INDEX_RUN_INTERSECT,
                    query_strategies::SINGLE_INDEX_COUNT_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ARRAY_KEYS) {
            }
        };

        class single_index_absent_query : public single_index_query {
        public:
            single_index_absent_query(const Roaring &index)
                : single_index_query(index, 
                    query_strategies::SINGLE_INDEX_RUN_NOT_INTERSECT,
                    query_strategies::SINGLE_INDEX_INVERTED_CARDINALITY,
                    query_strategies::SINGLE_INDEX_INVERTED_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };

        class single_index_all_query : public single_index_query {
        public:
            single_index_all_query(const Roaring &index)
                : single_index_query(index,
                    query_strategies::SINGLE_INDEX_RUN_ALL,
                    query_strategies::SINGLE_INDEX_COUNT_CARDINALITY,
                    query_strategies::SINGLE_INDEX_ARRAY_KEYS) {
                flag_for_keeping();
            }
        };

        class multiple_indices_query_context {
        public:
            const std::vector<Roaring> &indices;
            const std::vector<Roaring>::const_iterator min;
            const std::vector<Roaring>::const_iterator max;
            multiple_indices_query_context(const std::vector<Roaring> &indices, 
            const std::vector<Roaring>::const_iterator min,
            const std::vector<Roaring>::const_iterator max) : indices(indices), min(min), max(max){

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

            // yes this might count tiles more than once, if they are in multiple indices but thats fine
            static const uint32_t get_cardinality(const multiple_indices_query_context context) {
                uint32_t cardinality = 0;

                for (auto iter = context.min; iter != context.max; ++iter) {
                    cardinality += (*iter).cardinality();
                }
                return cardinality;
            }

            static const std::vector<uint32_t>* get_keys(const multiple_indices_query_context context) {
                uint32_t cardinality = get_cardinality(context);

                uint32_t* most_significant_ids = new uint32_t[cardinality];
                uint32_t* most_significant_ids_iter = most_significant_ids;

                for (auto iter = context.min; iter != context.max; ++iter) {
                    const Roaring &index = (*iter);
                    const uint32_t current_cardinality = index.cardinality();
                    index.toUint32Array(most_significant_ids_iter);
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

            // FIXME: add method to query for significant keys within the key range of a world tile
            // virtual const std::vector<uint32_t>* get_keys_in_range(uint32_t start, uint32_t end) const = 0;
            // or virtual const bool has_keys_in_range(uint32_t start, uint32_t end) const = 0;
        };

        class multiple_indices_run_intersect : public multiple_indices_run_strategy {
            bool run(const multiple_indices_query_context context, const Roaring &embark_candidate) const {
                bool intersects = false;

                for (auto iter = context.min; iter != context.max && !intersects; ++iter) {
                    intersects |= (*iter).intersect(embark_candidate);
                }
                if (intersects && context.max != context.indices.cend()) {
                    for (auto iter = context.max; iter != context.indices.cend() && intersects; ++iter) {
                        // mustn't intersect, otherwise there is a hit beyond max
                        intersects = intersects && !(*iter).intersect(embark_candidate);
                    }
                }
                return intersects;
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
        };
        
        class multi_indices_query_strategies {
        public:
            static const multiple_indices_run_intersect MULTI_INDICES_RUN_INTERSECT;
            static const multiple_indices_count_cardinality MULTI_INDICES_COUNT_CARDINALITY;
            static const multiple_indices_array_keys MULTI_INDICES_ARRAY_KEYS;
        };

        const multiple_indices_run_intersect multi_indices_query_strategies::MULTI_INDICES_RUN_INTERSECT = multiple_indices_run_intersect();
        const multiple_indices_count_cardinality multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY = multiple_indices_count_cardinality();
        const multiple_indices_array_keys multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS = multiple_indices_array_keys();

        class multiple_index_min_max_in_range_query : public multiple_indices_query {
        public:
            multiple_index_min_max_in_range_query(const multiple_indices_query_context context)
                : multiple_indices_query(context,
                    multi_indices_query_strategies::MULTI_INDICES_RUN_INTERSECT,
                    multi_indices_query_strategies::MULTI_INDICES_COUNT_CARDINALITY,
                    multi_indices_query_strategies::MULTI_INDICES_ARRAY_KEYS) {
                if (context.max != context.indices.cend()) {
                    flag_for_keeping();
                }
            }
        };
    }
}