#pragma once

#include "data/catalog.hpp"
#include "world/mapgen.hpp"

#include <string>

namespace npc::app {

/// Writes Markdown reference tables (activities, population, buildings/maps, events) generated from data/.
bool export_docs(const data::Catalog& catalog, const world::MapLibrary& maps, const std::string& out_dir, std::string& error);

} // namespace npc::app
