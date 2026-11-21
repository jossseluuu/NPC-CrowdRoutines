#pragma once
// HTTP API used by the web client (documented in docs/reference/api.md).

#include "server/http.hpp"
#include "server/runner.hpp"

namespace npc::server {

struct AppPaths {
    std::string data_dir = "data";
    std::string web_dir = "web";
    std::string runs_dir = "runs";
};

void register_api(HttpServer& http, SimRunner& runner, const AppPaths& paths, const data::Catalog& catalog,
                  const world::MapLibrary& maps);

} // namespace npc::server
