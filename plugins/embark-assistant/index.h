#pragma once

#include <unordered_map>

#include "DataDefs.h"

#include "defs.h"
#include "roaring.hh"

namespace embark_assist {
    namespace index {
        class Index final : public embark_assist::defs::index_interface {
        private:
            static const uint32_t numberEmbarkTiles = 16 * 16;
            df::world *world;
            uint16_t world_dims;
            uint32_t entryCounter = 0;
            uint16_t max_inorganic = 0;
            uint32_t maxKeyValue = 0;
            uint32_t previous_key = -1;
            Roaring uniqueKeys;
            Roaring hasAquifier;
            Roaring hasClay;
            Roaring hasCoal;
            Roaring hasFlux;
            Roaring hasRiver;
            Roaring hasSand;
            std::array<Roaring, 5> soil;
            std::array<Roaring, 6> river_size;
            std::array<Roaring, 4> magma_level;
            std::array<Roaring, 4> adamantine_level;
            std::array<Roaring, 3> savagery_level;
            std::array<Roaring, 3> evilness_level;

            std::vector<Roaring*> metals;
            std::vector<Roaring*> economics;
            std::vector<Roaring*> minerals;

            std::vector<Roaring*> inorganics;

            std::unordered_map<uint16_t, std::string> metalNames;
            std::unordered_map<uint16_t, std::string> economicNames;
            std::unordered_map<uint16_t, std::string> mineralNames;

            std::vector<uint32_t> keys_in_order;

            // helper for easier reset, debug and output of over all indexes
            std::vector<Roaring*> static_indexes;

            // Roaring& getInorganicsIndex(std::vector<Roaring*> &indexMap, uint16_t metal_index);
            void init_inorganic_index();
            // void addInorganic(std::vector<uint16_t, Roaring*> &indexMap, uint16_t metal_index);
            void initInorganicNames();
            std::string getInorganicName(const uint16_t index, std::unordered_map<uint16_t, std::string> &ingorganicNames, std::string name);
            void embark_assist::index::Index::writeIndexToDisk(const Roaring &roaring, const std::string prefix);
        public:
            Index();
            ~Index();
            void setup(df::world *world, uint16_t max_inorganic);
            void shutdown();
            virtual const bool containsEntries() final override;
            virtual const uint32_t createKey(int16_t x, int16_t y, uint8_t i, uint8_t k) final override;
            virtual void add(uint32_t key, const embark_assist::defs::mid_level_tile &data) final override;
            virtual void optimize(bool debugOutput) final override;
            virtual const void outputContents() final override;
            virtual const void outputSizes(const string &prefix) final override;
        };
    }
}