#pragma once

#include <cstdio>
#include <utility>

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
            // FIXME: call this is_to_be_deactivated_after_key_extraction
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
    }
}