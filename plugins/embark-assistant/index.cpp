#include <stdio.h>
#include "Core.h"
#include <Console.h>

// #include "ColorText.h"
#include "DataDefs.h"
#include "df/inorganic_raw.h"
#include "df/world.h"
#include "df/world_data.h"

#include "defs.h"
#include "index.h"

using namespace DFHack;

embark_assist::index::Index::Index(void) {

    static_indexes.push_back(&uniqueKeys);
    static_indexes.push_back(&hasAquifier);
    static_indexes.push_back(&hasClay);
    static_indexes.push_back(&hasCoal);
    static_indexes.push_back(&hasFlux);
    static_indexes.push_back(&hasRiver);
    static_indexes.push_back(&hasSand);

    for (auto& index : soil) {
        static_indexes.push_back(&index);
    }

    for (auto& index : river_size) {
        static_indexes.push_back(&index);
    }

    for (auto& index : magma_level) {
        static_indexes.push_back(&index);
    }

    for (auto& index : adamantine_level) {
        static_indexes.push_back(&index);
    }

    for (auto& index : savagery_level) {
        static_indexes.push_back(&index);
    }

    for (auto& index : evilness_level) {
        static_indexes.push_back(&index);
    }
}

embark_assist::index::Index::~Index() {
    this->world = nullptr;
}

void embark_assist::index::Index::setup(df::world *world, const uint16_t max_inorganic) {
    this->world = world;
    this->max_inorganic = max_inorganic;
    this->world_dims = world->worldgen.worldgen_parms.dim_x;
    maxKeyValue = world_dims * world_dims * 16 * 16;
    init_inorganic_index();
    initInorganicNames();
    keys_in_order.reserve(maxKeyValue);

    //hasRiver.roaring = *roaring_bitmap_create_with_capacity(201);
}

const uint32_t embark_assist::index::Index::createKey(int16_t x, int16_t y, uint8_t i, uint8_t k) {
    return y * world_dims * numberEmbarkTiles + (x * numberEmbarkTiles) + (i * 16) + k;
}

void embark_assist::index::Index::add(uint32_t key, const embark_assist::defs::mid_level_tile &mlt) {
    // color_ostream_proxy out(Core::getInstance().getConsole());

    //bool schedule_optimize = false;
    //if (previous_key != -1 && !(key - previous_key == 1 || key - previous_key == 256)) {
    //    schedule_optimize = true;
    //}
    //previous_key = key;

    keys_in_order.push_back(key);

    //if (key > maxKeyValue) {
    //    out.print("key %d larger than %d \n ", key, maxKeyValue);
    //}

    entryCounter++;
    if (uniqueKeys.contains(key)) {
        // out.print("key %d already processed\n ", key);
    }
    uniqueKeys.add(key);

    if (mlt.aquifer) {
        hasAquifier.add(key);
    }

    if (mlt.clay) {
        hasClay.add(key);
    }

    if (mlt.coal) {
        hasCoal.add(key);
    }

    if (mlt.flux) {
        hasFlux.add(key);
    }

    if (mlt.river_present) {
        hasRiver.add(key);
    }

    if (mlt.sand) {
        hasSand.add(key);
    }

    if (mlt.magma_level > 0) {
        magma_level[mlt.magma_level].add(key);
    }

    if (mlt.adamantine_level > 0) {
        adamantine_level[mlt.adamantine_level].add(key);
    }

    savagery_level[mlt.savagery_level].add(key);
    evilness_level[mlt.evilness_level].add(key);

    // create indexes for metals, economics and minerals
    for (auto it = mlt.metals.cbegin(); it != mlt.metals.cend(); it++) {
        if (*it) {
            const int metalIndex = std::distance(mlt.metals.cbegin(), it);
            metals[metalIndex]->add(key);
        }
    }

    for (auto it = mlt.economics.cbegin(); it != mlt.economics.cend(); it++) {
        if (*it) {
            const int economicIndex = std::distance(mlt.economics.cbegin(), it);
            economics[economicIndex]->add(key);
        }
    }

    for (auto it = mlt.minerals.cbegin(); it != mlt.minerals.cend(); it++) {
        if (*it) {            
            const int mineralIndex = std::distance(mlt.minerals.cbegin(), it);
            if (
                // true || 
                world->raws.inorganics[mineralIndex]->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
                world->raws.inorganics[mineralIndex]->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
                world->raws.inorganics[mineralIndex]->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
                world->raws.inorganics[mineralIndex]->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
                world->raws.inorganics[mineralIndex]->flags.is_set(df::inorganic_flags::SOIL)) {
                minerals[mineralIndex]->add(key);
            }
        }
    }

    //if (schedule_optimize) {
    //    out.print("optimizing after key %d\n ", key);
    //    this->optimize(false);
    //}
}

const bool embark_assist::index::Index::containsEntries() {
    return entryCounter > 0;
}

void embark_assist::index::Index::optimize(bool debugOutput) {

    if (debugOutput) {
        if (metalNames.empty()) {
            initInorganicNames();
        }
        this->outputSizes("before optimize");
    }

    for (auto it = static_indexes.begin(); it != static_indexes.end(); it++) {
        (*it)->runOptimize();
        (*it)->shrinkToFit();
    }

    for (auto it = metals.begin(); it != metals.end(); it++) {
        if (*it != nullptr) {
            (*it)->runOptimize();
            (*it)->shrinkToFit();
        }
    }

    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            (*it)->runOptimize();
            (*it)->shrinkToFit();
        }
    }

    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            (*it)->runOptimize();
            (*it)->shrinkToFit();
        }
    }

    if (debugOutput) {
        this->outputSizes("after optimize");
        this->outputContents();
    }
}

void embark_assist::index::Index::shutdown() {
    world = nullptr; 
    entryCounter = 0;
    max_inorganic = 0;
    maxKeyValue = 0;

    for (auto it = static_indexes.begin(); it != static_indexes.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            //delete *it;
        }
    }

    for (auto it = metals.begin(); it != metals.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    metals.clear();
    metals.resize(0);
    metals.reserve(0);
    metalNames.clear();
    metalNames.reserve(0);

    for (auto it = economics.begin(); it != economics.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    economics.clear();
    economics.resize(0);
    economics.reserve(0);
    economicNames.clear();    
    economicNames.reserve(0);

    for (auto it = minerals.begin(); it != minerals.end(); it++) {
        if (*it != nullptr) {
            roaring_bitmap_clear(&(*it)->roaring);
            delete *it;
        }
    }
    minerals.clear();
    minerals.resize(0);
    minerals.reserve(0);
    mineralNames.clear();
    mineralNames.reserve(0);

    inorganics.clear();
    inorganics.resize(0);
    inorganics.reserve(0);

    keys_in_order.clear();
}

const void embark_assist::index::Index::outputContents() {
    //auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out | std::ios::app);
    FILE* outfile = fopen(index_folder_log_file_name, "a");
    uint32_t index_prefix = 0;
    
    fprintf(outfile, "number of times add was called: %d\n", entryCounter);
    fprintf(outfile, "number of unique entries: %I64d\n", uniqueKeys.cardinality());
    fprintf(outfile, "number of hasAquifier entries: %I64d\n", hasAquifier.cardinality());
    this->writeIndexToDisk(hasAquifier, std::to_string(index_prefix++) + "_hasAquifier");
    fprintf(outfile, "number of hasRiver entries: %I64d\n", hasRiver.cardinality());
    this->writeIndexToDisk(hasRiver, std::to_string(index_prefix++) + "_hasRiver");
    fprintf(outfile, "number of hasClay entries: %I64d\n", hasClay.cardinality());
    this->writeIndexToDisk(hasClay, std::to_string(index_prefix++) + "_hasClay");
    fprintf(outfile, "Number of hasCoal entries: %I64d\n", hasCoal.cardinality());
    this->writeIndexToDisk(hasCoal, std::to_string(index_prefix++) + "_hasCoal");
    fprintf(outfile, "number of hasFlux entries: %I64d\n", hasFlux.cardinality());
    this->writeIndexToDisk(hasFlux, std::to_string(index_prefix++) + "_hasFlux");
    fprintf(outfile, "number of hasSand entries: %I64d\n", hasSand.cardinality());
    this->writeIndexToDisk(hasSand, std::to_string(index_prefix++) + "_hasSand");

    fprintf(outfile, "number of metal entries:\n");
    for (auto it = metals.cbegin(); it != metals.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN METAL ###";
            const uint16_t index = std::distance(metals.cbegin(), it);
            name = getInorganicName(index, metalNames, name);            
            fprintf(outfile, "%I64d (#%d - %s); ", (*it)->cardinality(), index, name.c_str());            
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fprintf(outfile, "\nnumber of economics entries:\n");
    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN ECONOMIC ###";
            const uint16_t index = std::distance(economics.cbegin(), it);
            name = getInorganicName(index, economicNames, name);
            fprintf(outfile, "%I64d (#%d - %s); ", (*it)->cardinality(), index, name.c_str());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fprintf(outfile, "\nnumber of minerals entries:\n");
    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            std::string name = "### UNKOWN MINERAL ###";
            const uint16_t index = std::distance(minerals.cbegin(), it);
            name = getInorganicName(index, mineralNames, name);
            fprintf(outfile, "%I64d (#%d - %s); ", (*it)->cardinality(), index, name.c_str());
            this->writeIndexToDisk((**it), std::to_string(index_prefix++) + "_#" + std::to_string(index) + "_" + name);
        }
    }
    fclose(outfile);
    
    const std::string prefix = "keys_in_order";
    auto myfile = std::ofstream(index_folder_name + prefix, std::ios::out);
    
    for (const auto key : keys_in_order) {
        myfile << key << "\n";
    }
    myfile.close();
}

const void embark_assist::index::Index::outputSizes(const string &prefix) {
    color_ostream_proxy out(Core::getInstance().getConsole());
    FILE* outfile = fopen(index_folder_log_file_name, "a");
    fprintf(outfile, "### %s ###\n", prefix.c_str());

    uint32_t byteSize = 0;

    byteSize = uniqueKeys.getSizeInBytes();
    fprintf(outfile, "unique entries bytesize: %zd\n", uniqueKeys.getSizeInBytes());
    byteSize += hasAquifier.getSizeInBytes();
    fprintf(outfile, "hasAquifier bytesize: %zd\n", hasAquifier.getSizeInBytes());
    byteSize += hasRiver.getSizeInBytes();
    fprintf(outfile, "hasRiver bytesize: %zd\n", hasRiver.getSizeInBytes());

    byteSize += hasClay.getSizeInBytes();
    fprintf(outfile, "hasClay bytesize: %zd\n", hasClay.getSizeInBytes());
    byteSize += hasCoal.getSizeInBytes();
    fprintf(outfile, "hasCoal bytesize: %zd\n", hasCoal.getSizeInBytes());
    byteSize += hasFlux.getSizeInBytes();
    fprintf(outfile, "hasFlux bytesize: %zd\n", hasFlux.getSizeInBytes());
    byteSize += hasSand.getSizeInBytes();
    fprintf(outfile, "hasSand bytesize: %zd\n", hasSand.getSizeInBytes());

    fprintf(outfile, "metal index bytesizes:\n");
    for (auto it = metals.cbegin(); it != metals.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN METAL ###";
            const uint16_t index = std::distance(metals.cbegin(), it);
            name = getInorganicName(index, metalNames, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
            // fprintf(outfile, "%d (index #%d); ", it->second->getSizeInBytes(), it->first);
        }
    }
    fprintf(outfile, "\neconomic index bytesizes:\n");
    for (auto it = economics.cbegin(); it != economics.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN ECONOMIC ###";
            const uint16_t index = std::distance(economics.cbegin(), it);
            name = getInorganicName(index, economicNames, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
        }
    }
    fprintf(outfile, "\nminerals index bytesizes:\n");
    for (auto it = minerals.cbegin(); it != minerals.cend(); it++) {
        if (*it != nullptr) {
            byteSize += (*it)->getSizeInBytes();
            std::string name = "### UNKOWN MINERAL ###";
            const uint16_t index = std::distance(minerals.cbegin(), it);
            name = getInorganicName(index, mineralNames, name);
            fprintf(outfile, "%zd (#%d - %s); ", (*it)->getSizeInBytes(), index, name.c_str());
        }
    }

    fprintf(outfile, "\n total index bytesize: %d\n", byteSize);
    fclose(outfile);
}

//Roaring& embark_assist::index::Index::getInorganicsIndex(std::unordered_map<uint16_t, Roaring*> &indexMap, uint16_t metal_index) {
//    const unordered_map<uint16_t, Roaring*>::iterator it = indexMap.find(metal_index);
//    if (it != indexMap.end()) {
//        return *(it->second);
//    }
//    Roaring* inorganicIndex = new Roaring();
//    indexMap.insert(std::pair<uint16_t, Roaring*>(metal_index, inorganicIndex));
//    // indexMap.at(metal_index) = inorganicIndex;
//    return *inorganicIndex;
//}

void embark_assist::index::Index::init_inorganic_index() {
    metals.resize(max_inorganic, nullptr);
    for (uint16_t k = 0; k < max_inorganic; k++) {
        for (uint16_t l = 0; l < world->raws.inorganics[k]->metal_ore.mat_index.size(); l++) {
            const int metalIndex = world->raws.inorganics[k]->metal_ore.mat_index[l];
            if (metals[metalIndex] == nullptr) {
                Roaring* inorganicIndex = new Roaring();
                metals[metalIndex] = inorganicIndex;
            }
        }
    }
    metals.shrink_to_fit();

    economics.resize(max_inorganic, nullptr);
    for (int16_t k = 0; k < max_inorganic; k++) {
        if (world->raws.inorganics[k]->economic_uses.size() != 0 && !world->raws.inorganics[k]->material.flags.is_set(df::material_flags::IS_METAL)) {
            if (economics[k] == nullptr) {
                Roaring* inorganicIndex = new Roaring();
                economics[k] = inorganicIndex;
            }
        }
    }
    economics.shrink_to_fit();

    minerals.resize(max_inorganic, nullptr);
    for (int16_t k = 0; k < max_inorganic; k++) {
        if (
            // true || 
             world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
             world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
             world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
             world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
             world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::SOIL)) {
        if (minerals[k] == nullptr) {
            Roaring* inorganicIndex = new Roaring();
            minerals[k] = inorganicIndex;
        }
        }
    }
    minerals.shrink_to_fit();
}

void embark_assist::index::Index::initInorganicNames() {
    // std::unordered_map<uint16_t, std::string> metalNames
    for (uint16_t k = 0; k < max_inorganic; k++) {
        for (uint16_t l = 0; l < world->raws.inorganics[k]->metal_ore.mat_index.size(); l++) {
            metalNames.insert(std::pair<uint16_t, std::string>(world->raws.inorganics[k]->metal_ore.mat_index[l],
                world->raws.inorganics[world->raws.inorganics[k]->metal_ore.mat_index[l]]->id));
        }
    }

    for (int16_t k = 0; k < max_inorganic; k++) {
        if (world->raws.inorganics[k]->economic_uses.size() != 0 && !world->raws.inorganics[k]->material.flags.is_set(df::material_flags::IS_METAL)) {
            economicNames.insert(std::pair<uint16_t, std::string>(k, world->raws.inorganics[k]->id));
        }
    }

    for (int16_t k = 0; k < max_inorganic; k++) {
        /*if (world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
            world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
            world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
            world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
            world->raws.inorganics[k]->flags.is_set(df::inorganic_flags::SOIL)) {*/
        mineralNames.insert(std::pair<uint16_t, std::string>(k, world->raws.inorganics[k]->id));
        //}
    }
}
std::string embark_assist::index::Index::getInorganicName(uint16_t index, std::unordered_map<uint16_t, std::string> &ingorganicNames, std::string name) {
    const auto iter = ingorganicNames.find(index);
    if (iter != ingorganicNames.end()) {
        name = iter->second;
    }
    return name;
}
void embark_assist::index::Index::writeIndexToDisk(const Roaring& roaring, const std::string prefix) {
    std::size_t expected_size_in_bytes = roaring.getSizeInBytes();
    std::vector<char> buffer(expected_size_in_bytes);
    std::size_t size_in_bytes = roaring.write(buffer.data());

    //std::ofstream stream("yourFile", std::ios::binary);
    //stream.write(reinterpret_cast<const char*>(&buffer), size_in_bytes);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto myfile = std::fstream(index_folder_name + prefix + ".index", std::ios::out | std::ios::binary);
    myfile.write((char*)&buffer[0], size_in_bytes);
    myfile.close();
    auto endTime = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}