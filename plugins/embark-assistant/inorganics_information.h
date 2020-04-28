#pragma once

#include <unordered_map>

namespace embark_assist {
    namespace inorganics {
        class inorganics_information {
            static inorganics_information *instance;
            inorganics_information(df::world *world);
            ~inorganics_information();

            bool initialized = false;
            uint16_t max_inorganics = 0;

            std::list<uint16_t> metal_indices;
            std::list<uint16_t> economic_indices;
            std::list<uint16_t> mineral_indices;

            std::unordered_map<uint16_t, std::string> metal_names;
            std::unordered_map<uint16_t, std::string> economic_names;
            std::unordered_map<uint16_t, std::string> mineral_names;

            std::vector<bool> is_relevant_mineral;

            void init(df::world *world);
            void clear();
        public:
            inorganics_information(inorganics_information const&) = delete;
            void operator=(inorganics_information const&) = delete;

            static const inorganics_information& get_instance();
            static void reset();

            const uint16_t get_max_inorganics() const;

            const std::list<uint16_t>& get_metal_indices() const;
            const std::list<uint16_t>& get_economic_indices() const;
            const std::list<uint16_t>& get_mineral_indices() const;

            const std::vector<bool>* get_relevant_minerals() const;

            const std::unordered_map<uint16_t, std::string>& get_metal_names() const;
            const std::unordered_map<uint16_t, std::string>& get_economic_names() const;
            const std::unordered_map<uint16_t, std::string>& get_mineral_names() const;
        };
    }
}
