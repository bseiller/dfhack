
#include "DataDefs.h"
#include "df/inorganic_raw.h"
#include "df/global_objects.h"
#include "df/world.h"
#include "df/world_data.h"

#include "inorganics_information.h"


using df::global::world;

embark_assist::inorganics::inorganics_information* embark_assist::inorganics::inorganics_information::instance = nullptr;

embark_assist::inorganics::inorganics_information::inorganics_information(df::world *world) {
    init(world);
}

void embark_assist::inorganics::inorganics_information::init(df::world *world) {
    for (uint16_t i = world->raws.inorganics.size() - 1; i >= 0; i--) {
        if (!world->raws.inorganics[i]->flags.is_set(df::inorganic_flags::GENERATED)) {
            max_inorganics = i;
            break;
        }
    }
    max_inorganics++;

    std::vector<bool> metals;
    metals.resize(max_inorganics, false);

    std::vector<bool> economics;
    economics.resize(max_inorganics, false);

    std::vector<bool> minerals;
    minerals.resize(max_inorganics, false);

    is_relevant_mineral.resize(max_inorganics, false);

    for (uint16_t k = 0; k < max_inorganics; k++) {
        for (uint16_t l = 0; l < world->raws.inorganics[k]->metal_ore.mat_index.size(); l++) {
            const int metalIndex = world->raws.inorganics[k]->metal_ore.mat_index[l];
            if (!metals[metalIndex]) {
                metals[metalIndex] = true;
                metal_names.insert(std::pair<uint16_t, std::string>(metalIndex, world->raws.inorganics[world->raws.inorganics[k]->metal_ore.mat_index[l]]->id));
                metal_indices.push_back(metalIndex);
            }
        }
    }
    metal_indices.sort();

    for (int16_t k = 0; k < max_inorganics; k++) {
        if (world->raws.inorganics[k]->economic_uses.size() != 0 && !world->raws.inorganics[k]->material.flags.is_set(df::material_flags::IS_METAL)) {
            if (!economics[k]) {
                economics[k] = true;
                economic_names.insert(std::pair<uint16_t, std::string>(k, world->raws.inorganics[k]->id));
                economic_indices.push_back(k);
            }
        }
    }
    economic_indices.sort();

    for (int16_t k = 0; k < max_inorganics; k++) {
        const df::inorganic_raw* raw = world->raws.inorganics[k];
        if (!minerals[k]) {
            minerals[k] = true;
            mineral_names.insert(std::pair<uint16_t, std::string>(k, world->raws.inorganics[k]->id));
            if (
                // true || 
                raw->environment.location.size() != 0 ||
                raw->environment_spec.mat_index.size() != 0 ||
                raw->flags.is_set(df::inorganic_flags::SEDIMENTARY) ||
                raw->flags.is_set(df::inorganic_flags::IGNEOUS_EXTRUSIVE) ||
                raw->flags.is_set(df::inorganic_flags::IGNEOUS_INTRUSIVE) ||
                raw->flags.is_set(df::inorganic_flags::METAMORPHIC) ||
                raw->flags.is_set(df::inorganic_flags::SOIL)) {

                mineral_indices.push_back(k);
                is_relevant_mineral[k] = true;
            }
        }
    }
    mineral_indices.sort();

    initialized = true;
}

embark_assist::inorganics::inorganics_information::~inorganics_information() {
    clear();
}

void embark_assist::inorganics::inorganics_information::clear() {
    initialized = false;
    metal_names.clear();
    metal_names.reserve(0);
    metal_indices.clear();
    metal_indices.resize(0);

    economic_names.clear();
    economic_names.reserve(0);
    economic_indices.clear();
    economic_indices.resize(0);

    mineral_names.clear();
    mineral_names.reserve(0);
    mineral_indices.clear();
    mineral_indices.resize(0);

    is_relevant_mineral.clear();
    is_relevant_mineral.reserve(0);
}

const embark_assist::inorganics::inorganics_information& embark_assist::inorganics::inorganics_information::get_instance() {
    static inorganics_information instance(world);
    embark_assist::inorganics::inorganics_information::instance = &instance;
    if (!instance.initialized) {
        instance.init(world);
    }
    return instance;
}

void embark_assist::inorganics::inorganics_information::reset() {
    if (embark_assist::inorganics::inorganics_information::instance != nullptr) {
        embark_assist::inorganics::inorganics_information::instance->clear();
    }
}

const uint16_t embark_assist::inorganics::inorganics_information::get_max_inorganics() const {
    return max_inorganics;
}

const std::list<uint16_t>& embark_assist::inorganics::inorganics_information::get_metal_indices() const {
    return metal_indices;
}

const std::list<uint16_t>& embark_assist::inorganics::inorganics_information::get_economic_indices() const {
    return economic_indices;
}

const std::list<uint16_t>& embark_assist::inorganics::inorganics_information::get_mineral_indices() const {
    return mineral_indices;
}

const std::vector<bool>* embark_assist::inorganics::inorganics_information::get_relevant_minerals() const {
    return &is_relevant_mineral;
}

const std::unordered_map<uint16_t, std::string>& embark_assist::inorganics::inorganics_information::get_metal_names() const {
    return metal_names;
}

const std::unordered_map<uint16_t, std::string>& embark_assist::inorganics::inorganics_information::get_economic_names() const {
    return economic_names;
}

const std::unordered_map<uint16_t, std::string>& embark_assist::inorganics::inorganics_information::get_mineral_names() const {
    return mineral_names;
}
