// export-docs: generates Markdown reference pages from the data catalog so the wiki never drifts from data/.
#include "docs_export.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace npc::app {

namespace {

std::string fmt_hours(const std::vector<data::HourWindow>& ws) {
    std::ostringstream o;
    for (std::size_t i = 0; i < ws.size(); ++i) {
        if (i) o << ", ";
        char buf[48];
        std::snprintf(buf, sizeof buf, "%02d:%02d–%02d:%02d", static_cast<int>(ws[i].start), static_cast<int>((ws[i].start - static_cast<int>(ws[i].start)) * 60),
                      static_cast<int>(ws[i].end), static_cast<int>((ws[i].end - static_cast<int>(ws[i].end)) * 60));
        o << buf;
        if (ws[i].bonus != 1.f) o << " (" << ws[i].bonus << ")";
    }
    return ws.empty() ? "—" : o.str();
}

std::string hh(float h) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", static_cast<int>(h), static_cast<int>(std::round((h - static_cast<int>(h)) * 60)) % 60);
    return buf;
}

std::string tr(const json::Value& i18n, const char* section, const std::string& id) {
    const auto s = i18n[section].str(id);
    return s.empty() ? id : s;
}

void write(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

const char* kHeader = "<!-- Generado por `npc_crowd_routines export-docs` a partir de data/. No editar a mano. -->\n\n";

} // namespace

bool export_docs(const data::Catalog& cat, const world::MapLibrary& lib, const std::string& out_dir, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    auto r = json::parse_file(cat.data_dir + "/i18n/es.json");
    if (!r.ok()) { error = r.error; return false; }
    const json::Value& es = r.value;

    // ---- Activities and substates
    {
        std::ostringstream o;
        o << kHeader << "# Actividades y subestados\n\n"
          << "Cada residente está siempre en **una actividad principal** y **uno de sus subestados**. "
          << cat.activities.size() << " actividades y " << cat.substates.size() << " subestados en total.\n\n"
          << "Columnas: *elegible* = la IA por utilidad puede escogerla; *agenda* = solo dentro de su bloque de agenda; "
          << "subestados *auto* los asigna el motor según el contexto (grupo, transporte, eventos...).\n\n";
        o << "| Actividad | id | Elegible | Lugar | Duración (min) | Horas preferidas |\n|---|---|---|---|---|---|\n";
        for (const auto& a : cat.activities) {
            std::string where;
            switch (a.location) {
                case data::LocationKind::Home: where = "casa"; break;
                case data::LocationKind::Travel: where = "en tránsito"; break;
                case data::LocationKind::Work: where = "trabajo"; break;
                case data::LocationKind::School: where = "centro de estudios"; break;
                case data::LocationKind::Anywhere: where = "cualquiera"; break;
                case data::LocationKind::Venues: {
                    for (int v : a.venues) {
                        const auto& name = cat.categories[static_cast<std::size_t>(v)];
                        if (name.rfind("home_", 0) == 0) continue; // pseudo-categories, listed below in words
                        where += (where.empty() ? "" : ", ") + name;
                    }
                    if (a.own_home) where += ", casa";
                    if (a.friend_home) where += ", casa de una amistad";
                    if (a.family_home) where += ", casa de un familiar";
                    break;
                }
            }
            o << "| " << a.icon << " " << tr(es, "activity", a.id) << " | `" << a.id << "` | " << (a.choosable ? (a.scheduled ? "agenda" : "sí") : "no")
              << " | " << where << " | " << (a.choosable ? std::to_string(static_cast<int>(a.min_minutes)) + "–" + std::to_string(static_cast<int>(a.max_minutes)) : "—")
              << " | " << fmt_hours(a.hours) << " |\n";
        }
        for (const auto& a : cat.activities) {
            o << "\n## " << a.icon << " " << tr(es, "activity", a.id) << " (`" << a.id << "`)\n\n| Subestado | id | Cuándo |\n|---|---|---|\n";
            for (int si : a.substates) {
                const auto& s = cat.substates[static_cast<std::size_t>(si)];
                std::string when;
                if (s.automatic) when = "auto (contexto)";
                else if (!s.venues.empty()) {
                    when = "en ";
                    for (std::size_t k = 0; k < s.venues.size(); ++k) when += (k ? ", " : "") + cat.categories[static_cast<std::size_t>(s.venues[k])];
                } else when = "genérico";
                if (!s.hours.empty()) when += " · " + fmt_hours(s.hours);
                o << "| " << tr(es, "substate", s.id) << " | `" << s.id << "` | " << when << " |\n";
            }
        }
        write(out_dir + "/actividades.md", o.str());
    }

    // ---- Archetypes and profiles
    {
        std::ostringstream o;
        o << kHeader << "# Perfiles y arquetipos\n\n## Perfiles de rutina (" << cat.profiles.size() << ")\n\n"
          << "| Perfil | id | Peso | Edad | Lugar de trabajo/estudio | Días | Inicio | Horas |\n|---|---|---:|---|---|---|---|---|\n";
        const char* days[] = {"lun–vie", "lun–sáb", "rotativos", "aleatorios", "—"};
        for (const auto& p : cat.profiles) {
            std::string wp;
            for (std::size_t k = 0; k < p.workplaces.size(); ++k) wp += (k ? ", " : "") + cat.categories[static_cast<std::size_t>(p.workplaces[k])];
            std::string start = p.shifts.empty() ? hh(p.start_mean) + " ± " + std::to_string(static_cast<int>(p.start_sd * 60)) + " min" : "turnos";
            if (!p.shifts.empty()) { start += " "; for (std::size_t k = 0; k < p.shifts.size(); ++k) start += (k ? "/" : "") + hh(p.shifts[k]); }
            o << "| " << tr(es, "profile", p.id) << " | `" << p.id << "` | " << p.weight << " | " << p.age[0] << "–" << p.age[1] << " | " << (wp.empty() ? "—" : wp)
              << " | " << days[static_cast<int>(p.days)] << " | " << (p.days == data::WorkDays::None && !p.care ? "—" : start) << " | "
              << (p.days == data::WorkDays::None && !p.care ? "—" : std::to_string(p.duration_mean).substr(0, 4)) << " |\n";
        }
        o << "\n## Arquetipos de personalidad (" << cat.archetypes.size() << ")\n\n"
          << "Cada residente combina un arquetipo principal (70 %) y uno secundario (30 %); sus rasgos se muestrean con una normal "
          << "de desviación " << cat.trait_sd << " alrededor de esa mezcla. Valores 0–100.\n\n| Arquetipo | id |";
        for (auto t : data::kTraitNames) o << " " << tr(es, "trait", t) << " |";
        o << " Preferencias |\n|---|---|";
        for (std::size_t k = 0; k < data::kTraitCount; ++k) o << "---:|";
        o << "---|\n";
        for (const auto& a : cat.archetypes) {
            o << "| " << tr(es, "archetype", a.id) << " | `" << a.id << "` |";
            for (float v : a.traits) o << " " << static_cast<int>(std::round(v * 100)) << " |";
            std::string prefs;
            for (std::size_t k = 0; k < a.prefs.size(); ++k)
                if (a.prefs[k] != 1.f) prefs += (prefs.empty() ? "" : ", ") + tr(es, "activity", cat.activities[k].id) + " ×" + std::to_string(a.prefs[k]).substr(0, 3);
            o << " " << (prefs.empty() ? "—" : prefs) << " |\n";
        }
        write(out_dir + "/poblacion.md", o.str());
    }

    // ---- Buildings and districts
    {
        std::ostringstream o;
        const char* dn[] = {"L", "M", "X", "J", "V", "S", "D"};
        o << kHeader << "# Edificios y barrios\n\n## Tipos de edificio (" << cat.building_types.size() << ")\n\n"
          << "| Edificio | id | Categoría | Plantas | Viviendas | Empleos | Visitantes | Horario |\n|---|---|---|---|---|---|---:|---|\n";
        for (const auto& b : cat.building_types) {
            std::string hours;
            if (b.open24h) hours = b.residential() ? "—" : "24 h";
            else {
                std::string last;
                for (int d = 0; d < 7; ++d) {
                    const auto& dh = b.hours[static_cast<std::size_t>(d)];
                    const std::string cur = dh.closed() ? "cerrado" : hh(dh.open) + "–" + hh(dh.close);
                    if (cur != last) { hours += (hours.empty() ? "" : "; ") + std::string(dn[d]) + " " + cur; last = cur; }
                }
            }
            auto range = [](const int r[2]) { return r[1] > 0 ? std::to_string(r[0]) + "–" + std::to_string(r[1]) : std::string("—"); };
            o << "| " << tr(es, "building", b.id) << " | `" << b.id << "` | " << cat.categories[static_cast<std::size_t>(b.category)] << " | " << b.floors[0] << "–"
              << b.floors[1] << " | " << range(b.households) << " | " << range(b.jobs) << " | " << b.visitors << " | " << hours << " |\n";
        }
        o << "\n## Tipos de barrio (" << cat.districts.size() << ")\n\n| Barrio | id | Densidad | Edificios más probables |\n|---|---|---:|---|\n";
        for (const auto& d : cat.districts) {
            auto w = d.weights;
            std::sort(w.begin(), w.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
            std::string top;
            for (std::size_t k = 0; k < std::min<std::size_t>(5, w.size()); ++k)
                top += (k ? ", " : "") + tr(es, "building", cat.building_types[static_cast<std::size_t>(w[k].first)].id);
            o << "| " << tr(es, "district", d.id) << " | `" << d.id << "` | " << d.density << " | " << top << " |\n";
        }
        o << "\n## Plantillas de mapa (" << lib.templates.size() << ")\n\n| Plantilla | id | Separación (m) | Deformación | Avenidas | Agua |\n|---|---|---|---|---|---|\n";
        for (const auto& t : lib.templates)
            o << "| " << tr(es, "template", t.id) << " | `" << t.id << "` | " << t.spacing[0] << "×" << t.spacing[1] << " | jitter " << t.jitter << ", warp " << t.warp
              << " | " << (t.avenue_every ? "cada " + std::to_string(t.avenue_every) : std::string("no")) << (t.diagonal ? " + diagonal" : "") << " | " << t.water << " |\n";
        o << "\n| Tamaño | id | Dimensiones | Población sugerida |\n|---|---|---|---:|\n";
        for (const auto& s : lib.sizes)
            o << "| " << tr(es, "size", s.id) << " | `" << s.id << "` | " << s.width << " × " << s.height << " m | " << s.population << " |\n";
        write(out_dir + "/edificios-y-mapas.md", o.str());
    }

    // ---- Events
    {
        std::ostringstream o;
        o << kHeader << "# Eventos (" << cat.events.size() << ")\n\n"
          << "Tasa base = eventos por día simulado con caos 1 (se multiplica por el nivel de caos, la hora y el fin de semana).\n\n"
          << "| Evento | id | Categoría | Tasa/día | Lugar | Radio (m) | Duración (min) | Efectos | Reacciones |\n|---|---|---|---:|---|---|---|---|---|\n";
        const char* locs[] = {"calle", "edificio", "barrio", "cualquiera", "toda la ciudad"};
        for (const auto& e : cat.events) {
            std::string fx;
            auto add = [&](const std::string& s) { fx += (fx.empty() ? "" : ", ") + s; };
            if (e.effects.block_roads) add("corta calles");
            if (e.effects.close_buildings) add("cierra edificios");
            if (e.effects.evacuate) add("evacuación");
            if (e.effects.conflict) add("conflicto");
            if (e.effects.crowd[1] > 0) add("multitud " + std::to_string(e.effects.crowd[0]) + "–" + std::to_string(e.effects.crowd[1]));
            if (e.effects.weather >= 0) add(std::string("tiempo: ") + data::kWeatherNames[static_cast<std::size_t>(e.effects.weather)]);
            if (e.effects.car_speed < 1) add("coches ×" + std::to_string(e.effects.car_speed).substr(0, 4));
            if (e.effects.walk_speed < 1) add("peatones ×" + std::to_string(e.effects.walk_speed).substr(0, 4));
            if (e.effects.needs[data::Stress] > 0) add("estrés +" + std::to_string(e.effects.needs[data::Stress]).substr(0, 4) + "/h");
            std::string resp;
            const auto& rr = e.response;
            const std::pair<const char*, float> rs[] = {{"observar", rr.watch}, {"curiosear", rr.investigate}, {"huir", rr.flee}, {"evacuar", rr.evacuate}, {"refugiarse", rr.shelter}, {"ayudar", rr.help}};
            for (const auto& [n, w] : rs) if (w > 0) resp += (resp.empty() ? "" : ", ") + std::string(n);
            o << "| " << e.icon << " " << tr(es, "event", e.id) << " | `" << e.id << "` | " << e.category << " | " << e.base_rate << " | "
              << locs[static_cast<int>(e.location)] << " | " << (e.location == data::EventLocation::Citywide ? std::string("—") : std::to_string(static_cast<int>(e.radius[0])) + "–" + std::to_string(static_cast<int>(e.radius[1])))
              << " | " << static_cast<int>(e.duration[0]) << "–" << static_cast<int>(e.duration[1]) << " | " << (fx.empty() ? "—" : fx) << " | " << (resp.empty() ? "—" : resp) << " |\n";
        }
        write(out_dir + "/eventos.md", o.str());
    }
    return true;
}

} // namespace npc::app
