#include "world/building.hpp"

#include <cmath>

namespace npc_crowd::world {

// ---------------------------------------------------------------------------------------------
// OpeningHours
// ---------------------------------------------------------------------------------------------
namespace {

const OpeningHours::DaySchedule& schedule_for(const OpeningHours& hours, int week_day, int day_of_year) {
    for (const auto& [day, schedule] : hours.exceptions)
        if (day == day_of_year) return schedule;
    const int d = ((week_day % 7) + 7) % 7;
    return hours.days[static_cast<std::size_t>(d)];
}

bool in_break(const OpeningHours::DaySchedule& s, float hour) {
    for (const auto& [from, to] : s.breaks)
        if (hour >= from && hour < to) return true;
    return false;
}

/// Horario que puede cruzar medianoche (p. ej. 20:00 -> 03:00).
bool schedule_open(const OpeningHours::DaySchedule& s, float hour) {
    if (s.closed) return false;
    const bool open = s.open_hour <= s.close_hour ? (hour >= s.open_hour && hour < s.close_hour)
                                                  : (hour >= s.open_hour || hour < s.close_hour);
    return open && !in_break(s, hour);
}

} // namespace

bool OpeningHours::is_open(float world_hour, int week_day, int day_of_year) const {
    const float h = std::fmod(std::fmod(world_hour, 24.f) + 24.f, 24.f);
    // Un horario nocturno del día anterior puede seguir abierto pasada la medianoche.
    const auto& yesterday = schedule_for(*this, week_day + 6, day_of_year - 1);
    if (!yesterday.closed && yesterday.open_hour > yesterday.close_hour && h < yesterday.close_hour) return true;
    return schedule_open(schedule_for(*this, week_day, day_of_year), h);
}

float OpeningHours::time_until_open(float world_hour, int week_day, int day_of_year) const {
    if (is_open(world_hour, week_day, day_of_year)) return 0.f;
    // Búsqueda en pasos de 15 minutos durante la próxima semana.
    constexpr float step = 0.25f;
    for (float t = step; t <= 24.f * 7.f; t += step) {
        const float h = world_hour + t;
        const int extra_days = static_cast<int>(std::floor(h / 24.f));
        if (is_open(h, week_day + extra_days, day_of_year + extra_days)) return t;
    }
    return -1.f; // nunca abre
}

float OpeningHours::time_until_close(float world_hour, int week_day, int day_of_year) const {
    if (!is_open(world_hour, week_day, day_of_year)) return 0.f;
    constexpr float step = 0.25f;
    for (float t = step; t <= 24.f * 7.f; t += step) {
        const float h = world_hour + t;
        const int extra_days = static_cast<int>(std::floor(h / 24.f));
        if (!is_open(h, week_day + extra_days, day_of_year + extra_days)) return t;
    }
    return -1.f; // abierto permanentemente
}

// ---------------------------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------------------------
bool Building::is_open(float world_hour, int week_day, int day_of_year) const {
    if (!is_operational) return false;
    return is_24h || hours.is_open(world_hour, week_day, day_of_year);
}

std::optional<std::uint32_t> Building::find_available_job(std::uint8_t education_level) const {
    for (const auto& job : jobs)
        if (job.required_education <= education_level && job.worker_ids.size() < job.max_workers) return job.id;
    return std::nullopt;
}

std::optional<std::uint32_t> Building::find_available_housing(std::uint32_t household_size) const {
    for (const auto& h : households) {
        if (!h.resident_ids.empty()) continue;
        // Capacidad aproximada por zona interior; sin zona, cualquier hogar vacío sirve.
        std::uint32_t capacity = household_size;
        for (const auto& z : zones)
            if (z.id == h.zone_id) capacity = z.capacity;
        if (capacity >= household_size) return h.id;
    }
    return std::nullopt;
}

Vec2 Building::get_random_entrance() const {
    if (entrances.empty()) return position;
    // Determinista: elige una entrada estable en función del id del edificio.
    return entrances[id % entrances.size()];
}

Vec2 Building::get_entrance_for_zone(std::uint32_t zone_id) const {
    for (const auto& z : zones) {
        if (z.id != zone_id) continue;
        const Vec2 zone_world = position + z.position;
        if (entrances.empty()) return zone_world;
        const Vec2* best = &entrances.front();
        for (const auto& e : entrances)
            if (length_sq(e - zone_world) < length_sq(*best - zone_world)) best = &e;
        return *best;
    }
    return get_random_entrance();
}

// ---------------------------------------------------------------------------------------------
// BuildingTypeConfig (tabla data-driven con los tipos más usados; el resto recibe un valor por defecto)
// ---------------------------------------------------------------------------------------------
namespace {

OpeningHours make_hours(float open, float close, bool weekends_open, bool sunday_open = false) {
    OpeningHours h;
    for (std::size_t d = 0; d < 7; ++d) {
        h.days[d].open_hour = open;
        h.days[d].close_hour = close;
        h.days[d].closed = (d == 5 && !weekends_open) || (d == 6 && !(weekends_open && sunday_open));
    }
    return h;
}

BuildingTypeConfig make_config(BuildingType type, const char* name, Vec2 size, std::uint32_t capacity, std::uint32_t floors,
                               OpeningHours hours, bool is_24h, float visit, float work, float live) {
    BuildingTypeConfig c;
    c.type = type;
    c.name = name;
    c.typical_size = size;
    c.base_capacity = capacity;
    c.max_floors = floors;
    c.default_hours = std::move(hours);
    c.is_24h = is_24h;
    c.visit_attraction = visit;
    c.work_attraction = work;
    c.live_attraction = live;
    return c;
}

std::unordered_map<BuildingType, BuildingTypeConfig> build_config_table() {
    using T = BuildingType;
    const auto office = make_hours(8.f, 19.f, false);
    const auto shop = make_hours(9.f, 21.f, true, false);
    const auto daily = make_hours(7.f, 23.f, true, true);
    const auto school = make_hours(8.f, 17.f, false);
    const auto night = make_hours(20.f, 3.f, true, true);
    const OpeningHours always{};

    std::unordered_map<T, BuildingTypeConfig> t;
    auto add = [&t](BuildingTypeConfig c) { t.emplace(c.type, std::move(c)); };
    //   tipo                     nombre                 tamaño        cap    pisos horario 24h   visita trabajo vivir
    add(make_config(T::House,          "House",           {12, 10},       5,  2, always, true,  .1f, .1f, 1.f));
    add(make_config(T::ApartmentBlock, "Apartment block", {30, 20},     120,  6, always, true,  .1f, .2f, .9f));
    add(make_config(T::HighRise,       "High-rise",       {35, 35},     600, 30, always, true,  .1f, .3f, .7f));
    add(make_config(T::OfficeSmall,    "Small office",    {20, 15},      50,  3, office, false, .2f, 1.f, 0.f));
    add(make_config(T::OfficeMedium,   "Medium office",   {35, 25},     200,  8, office, false, .2f, 1.f, 0.f));
    add(make_config(T::OfficeLarge,    "Large office",    {50, 40},    1000, 20, office, false, .2f, 1.f, 0.f));
    add(make_config(T::CorporateHQ,    "Corporate HQ",    {70, 50},    2500, 40, office, false, .3f, 1.f, 0.f));
    add(make_config(T::Coworking,      "Coworking",       {25, 20},      80,  3, daily,  false, .4f, .8f, 0.f));
    add(make_config(T::ConvenienceStore,"Convenience store",{10, 8},     20,  1, daily,  false, .9f, .3f, 0.f));
    add(make_config(T::Supermarket,    "Supermarket",     {50, 35},     300,  1, shop,   false, 1.f, .5f, 0.f));
    add(make_config(T::Mall,           "Mall",            {120, 90},   3000,  3, shop,   false, 1.f, .6f, 0.f));
    add(make_config(T::Factory,        "Factory",         {80, 60},     400,  2, make_hours(6.f, 22.f, true), false, .1f, .9f, 0.f));
    add(make_config(T::Warehouse,      "Warehouse",       {90, 60},     100,  1, always, true,  .1f, .7f, 0.f));
    add(make_config(T::Hospital,       "Hospital",        {90, 70},     800,  8, always, true,  .6f, 1.f, 0.f));
    add(make_config(T::Clinic,         "Clinic",          {25, 20},      60,  2, make_hours(8.f, 20.f, true), false, .6f, .8f, 0.f));
    add(make_config(T::Pharmacy,       "Pharmacy",        {10, 8},       15,  1, shop,   false, .7f, .3f, 0.f));
    add(make_config(T::Kindergarten,   "Kindergarten",    {25, 20},      80,  1, school, false, .3f, .6f, 0.f));
    add(make_config(T::PrimarySchool,  "Primary school",  {50, 40},     500,  2, school, false, .3f, .7f, 0.f));
    add(make_config(T::SecondarySchool,"Secondary school",{60, 45},     800,  3, school, false, .3f, .7f, 0.f));
    add(make_config(T::HighSchool,     "High school",     {60, 45},     900,  3, school, false, .3f, .7f, 0.f));
    add(make_config(T::University,     "University",      {120, 90},   5000,  5, make_hours(8.f, 21.f, true), false, .5f, .8f, 0.f));
    add(make_config(T::Library,        "Library",         {30, 25},     150,  2, make_hours(9.f, 20.f, true), false, .7f, .3f, 0.f));
    add(make_config(T::PoliceStation,  "Police station",  {35, 25},     120,  3, always, true,  .2f, .8f, 0.f));
    add(make_config(T::FireStation,    "Fire station",    {35, 25},      60,  2, always, true,  .1f, .8f, 0.f));
    add(make_config(T::CityHall,       "City hall",       {50, 40},     300,  4, office, false, .5f, .8f, 0.f));
    add(make_config(T::Museum,         "Museum",          {60, 45},     400,  3, make_hours(10.f, 19.f, true, true), false, .9f, .3f, 0.f));
    add(make_config(T::Cinema,         "Cinema",          {40, 30},     500,  2, make_hours(16.f, 24.f, true, true), false, .9f, .3f, 0.f));
    add(make_config(T::Park,           "Park",            {100, 80},   2000,  1, always, true,  1.f, .1f, 0.f));
    add(make_config(T::Restaurant,     "Restaurant",      {15, 12},      60,  1, make_hours(12.f, 24.f, true, true), false, 1.f, .4f, 0.f));
    add(make_config(T::Cafe,           "Cafe",            {10, 8},       30,  1, make_hours(7.f, 20.f, true, true), false, 1.f, .3f, 0.f));
    add(make_config(T::Bar,            "Bar",             {12, 10},      50,  1, night,  false, .9f, .3f, 0.f));
    add(make_config(T::Nightclub,      "Nightclub",       {30, 25},     400,  1, night,  false, .8f, .3f, 0.f));
    add(make_config(T::TrainStation,   "Train station",   {80, 40},    2000,  2, make_hours(5.f, 1.f, true, true), false, .6f, .5f, 0.f));
    add(make_config(T::MetroStation,   "Metro station",   {20, 15},     800,  1, make_hours(6.f, 1.f, true, true), false, .6f, .3f, 0.f));
    add(make_config(T::Gym,            "Gym",             {30, 20},     120,  2, make_hours(6.f, 23.f, true, true), false, .7f, .3f, 0.f));
    return t;
}

} // namespace

const std::unordered_map<BuildingType, BuildingTypeConfig>& BuildingTypeConfig::all_configs() {
    static const auto table = build_config_table();
    return table;
}

const BuildingTypeConfig& BuildingTypeConfig::get(BuildingType type) {
    const auto& table = all_configs();
    if (auto it = table.find(type); it != table.end()) return it->second;
    static const BuildingTypeConfig fallback =
        make_config(BuildingType::VacantLot, "Generic building", {20, 20}, 50, 1, make_hours(9.f, 18.f, false), false, .3f, .3f, 0.f);
    return fallback;
}

// ---------------------------------------------------------------------------------------------
// BuildingRegistry
// ---------------------------------------------------------------------------------------------
std::vector<std::uint32_t> BuildingRegistry::find_by_type(BuildingType type) const {
    std::vector<std::uint32_t> out;
    for (const auto& [id, b] : buildings)
        if (b.type == type) out.push_back(id);
    return out;
}

std::vector<std::uint32_t> BuildingRegistry::find_near(Vec2 pos, float radius) const {
    std::vector<std::uint32_t> out;
    const float r2 = radius * radius;
    for (const auto& [id, b] : buildings)
        if (length_sq(b.position - pos) <= r2) out.push_back(id);
    return out;
}

std::vector<std::uint32_t> BuildingRegistry::find_with_capacity(std::uint32_t min_capacity) const {
    std::vector<std::uint32_t> out;
    for (const auto& [id, b] : buildings)
        if (b.can_accommodate(min_capacity)) out.push_back(id);
    return out;
}

std::vector<std::uint32_t> BuildingRegistry::find_hiring(std::uint8_t education_level) const {
    std::vector<std::uint32_t> out;
    for (const auto& [id, b] : buildings)
        if (b.find_available_job(education_level)) out.push_back(id);
    return out;
}

std::vector<std::uint32_t> BuildingRegistry::find_with_housing(std::uint32_t household_size) const {
    std::vector<std::uint32_t> out;
    for (const auto& [id, b] : buildings)
        if (b.find_available_housing(household_size)) out.push_back(id);
    return out;
}

std::uint32_t BuildingRegistry::total_capacity() const {
    std::uint32_t total = 0;
    for (const auto& [id, b] : buildings) total += b.max_capacity;
    return total;
}

std::uint32_t BuildingRegistry::total_occupancy() const {
    std::uint32_t total = 0;
    for (const auto& [id, b] : buildings) total += b.current_occupancy;
    return total;
}

float BuildingRegistry::average_occupancy_ratio() const {
    if (buildings.empty()) return 0.f;
    float sum = 0.f;
    for (const auto& [id, b] : buildings)
        sum += b.max_capacity ? static_cast<float>(b.current_occupancy) / static_cast<float>(b.max_capacity) : 0.f;
    return sum / static_cast<float>(buildings.size());
}

std::unordered_map<BuildingType, std::uint32_t> BuildingRegistry::count_by_type() const {
    std::unordered_map<BuildingType, std::uint32_t> out;
    for (const auto& [id, b] : buildings) ++out[b.type];
    return out;
}

} // namespace npc_crowd::world
