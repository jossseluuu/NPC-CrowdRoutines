#pragma once

#include "data/catalog.hpp"
#include "world/mapgen.hpp"

#include <string>

namespace npc::app {

    bool export_docs(const data::Catalog& catalog, const world::MapLibrary& maps, const std::string& out_dir, std::string& error);

}
