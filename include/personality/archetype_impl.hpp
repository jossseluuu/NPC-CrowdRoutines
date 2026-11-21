#pragma once

// Implementación header-only del módulo de personalidad.
// Todas las funciones son `inline` para poder incluir este archivo desde varias unidades de compilación.

#include "archetype.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <deque>
#include <random>
#include <string>

namespace npc_crowd::personality {

namespace detail {

struct ArchetypeValues {
    float risk_aversion, sociability, patience, conscientiousness, curiosity, aggression, empathy;
    float work, shop, leisure, study, medical, social;
};

inline PersonalityArchetype make_archetype(const char* name, const char* description, const ArchetypeValues& v) {
    PersonalityArchetype a(name);
    a.description = description;
    a.risk_aversion = v.risk_aversion;
    a.sociability = v.sociability;
    a.patience = v.patience;
    a.conscientiousness = v.conscientiousness;
    a.curiosity = v.curiosity;
    a.aggression = v.aggression;
    a.empathy = v.empathy;
    a.attraction_to_work = v.work;
    a.attraction_to_shop = v.shop;
    a.attraction_to_leisure = v.leisure;
    a.attraction_to_study = v.study;
    a.attraction_to_medical = v.medical;
    a.attraction_to_social = v.social;
    // Mantener los rasgos base sincronizados con los factores de decisión.
    auto sync = [&a](const char* trait, float value) {
        for (auto& t : a.traits) if (t.name == trait) t.set(value);
    };
    sync("RiskAversion", v.risk_aversion);
    sync("RiskTaking", 1.f - v.risk_aversion);
    sync("Sociability", v.sociability);
    sync("Extraversion", v.sociability);
    sync("Patience", v.patience);
    sync("Conscientiousness", v.conscientiousness);
    sync("WorkEthic", v.conscientiousness);
    sync("Curiosity", v.curiosity);
    sync("Openness", v.curiosity);
    sync("Aggression", v.aggression);
    sync("Empathy", v.empathy);
    sync("Agreeableness", v.empathy);
    return a;
}

/// Arquetipos registrados en tiempo de ejecución. std::deque mantiene estables los punteros devueltos.
inline std::deque<PersonalityArchetype>& custom_registry() {
    static std::deque<PersonalityArchetype> registry;
    return registry;
}

} // namespace detail

//                                                                       risk  soc   pat   cons  cur   aggr  emp   work shop leis stud med  soc
inline const PersonalityArchetype& get_stable_worker_archetype() {
    static const auto a = detail::make_archetype("StableWorker", "Trabajador estable y puntual",
        {.65f, .45f, .65f, .85f, .40f, .25f, .55f, 1.5f, .9f, .8f, .5f, 1.f, .9f});
    return a;
}
inline const PersonalityArchetype& get_student_archetype() {
    static const auto a = detail::make_archetype("Student", "Estudiante impulsivo y curioso",
        {.30f, .75f, .35f, .45f, .80f, .40f, .55f, .6f, 1.f, 1.4f, 1.6f, .7f, 1.4f});
    return a;
}
inline const PersonalityArchetype& get_caregiver_archetype() {
    static const auto a = detail::make_archetype("Caregiver", "Cuidador empático",
        {.70f, .60f, .80f, .75f, .45f, .15f, .90f, .9f, 1.2f, .7f, .5f, 1.4f, 1.2f});
    return a;
}
inline const PersonalityArchetype& get_retired_archetype() {
    static const auto a = detail::make_archetype("Retired", "Jubilado tranquilo",
        {.75f, .55f, .85f, .60f, .45f, .15f, .65f, .2f, 1.2f, 1.2f, .4f, 1.6f, 1.1f});
    return a;
}
inline const PersonalityArchetype& get_entrepreneur_archetype() {
    static const auto a = detail::make_archetype("Entrepreneur", "Emprendedor que asume riesgos",
        {.20f, .65f, .40f, .80f, .75f, .45f, .45f, 1.8f, .8f, .7f, .9f, .6f, 1.1f});
    return a;
}
inline const PersonalityArchetype& get_extrovert_archetype() {
    static const auto a = detail::make_archetype("Extrovert", "Sociable extrovertido",
        {.40f, .90f, .50f, .50f, .65f, .30f, .70f, .9f, 1.3f, 1.6f, .7f, .8f, 1.9f});
    return a;
}
inline const PersonalityArchetype& get_introvert_archetype() {
    static const auto a = detail::make_archetype("Introvert", "Introvertido cauteloso",
        {.70f, .15f, .70f, .70f, .55f, .15f, .55f, 1.1f, .6f, .7f, 1.2f, .9f, .4f});
    return a;
}
inline const PersonalityArchetype& get_anxious_archetype() {
    static const auto a = detail::make_archetype("Anxious", "Ansioso / hipervigilante",
        {.90f, .35f, .30f, .65f, .30f, .20f, .60f, 1.f, .8f, .6f, .8f, 1.5f, .7f});
    return a;
}
inline const PersonalityArchetype& get_lazy_archetype() {
    static const auto a = detail::make_archetype("Lazy", "Perezoso / baja energía",
        {.55f, .45f, .60f, .15f, .30f, .25f, .45f, .4f, .9f, 1.5f, .3f, .7f, 1.f});
    return a;
}
inline const PersonalityArchetype& get_hypochondriac_archetype() {
    static const auto a = detail::make_archetype("Hypochondriac", "Hipocondríaco",
        {.85f, .40f, .40f, .60f, .35f, .20f, .55f, .8f, 1.f, .6f, .6f, 2.f, .7f});
    return a;
}
inline const PersonalityArchetype& get_risk_seeker_archetype() {
    static const auto a = detail::make_archetype("RiskSeeker", "Buscador de riesgo",
        {.05f, .60f, .25f, .35f, .90f, .70f, .30f, .7f, .9f, 1.7f, .5f, .3f, 1.2f});
    return a;
}

inline std::vector<const PersonalityArchetype*> get_all_archetypes() {
    std::vector<const PersonalityArchetype*> result{
        &get_stable_worker_archetype(), &get_student_archetype(),  &get_caregiver_archetype(),
        &get_retired_archetype(),       &get_entrepreneur_archetype(), &get_extrovert_archetype(),
        &get_introvert_archetype(),     &get_anxious_archetype(),  &get_lazy_archetype(),
        &get_hypochondriac_archetype(), &get_risk_seeker_archetype()};
    for (const auto& custom : detail::custom_registry()) result.push_back(&custom);
    return result;
}

inline void register_archetype(PersonalityArchetype archetype) {
    detail::custom_registry().push_back(std::move(archetype));
}

inline PersonalityArchetype::PersonalityArchetype(const std::string& name_) : name(name_) {
    // Rasgos base (valor, mínimo, máximo, peso en la decisión).
    traits = {
        {"RiskAversion", "Aversión al riesgo", 0.5f, 0.f, 1.f, 1.2f},
        {"Sociability", "Necesidad social", 0.5f, 0.f, 1.f, 1.1f},
        {"Patience", "Paciencia", 0.5f, 0.f, 1.f, 1.0f},
        {"Conscientiousness", "Escrupulosidad", 0.5f, 0.f, 1.f, 1.0f},
        {"Curiosity", "Curiosidad", 0.5f, 0.f, 1.f, 0.9f},
        {"Aggression", "Agresividad", 0.5f, 0.f, 1.f, 0.8f},
        {"Empathy", "Empatía", 0.5f, 0.f, 1.f, 1.3f},
        {"Openness", "Apertura a la experiencia", 0.5f, 0.f, 1.f, 0.9f},
        {"Neuroticism", "Neuroticismo", 0.5f, 0.f, 1.f, 0.7f},
        {"Extraversion", "Extraversión", 0.5f, 0.f, 1.f, 1.1f},
        {"Agreeableness", "Amabilidad", 0.5f, 0.f, 1.f, 1.0f},
        {"SelfEsteem", "Autoestima", 0.5f, 0.f, 1.f, 0.9f},
        {"Dominance", "Dominancia", 0.5f, 0.f, 1.f, 0.8f},
        {"Independence", "Independencia", 0.5f, 0.f, 1.f, 0.9f},
        {"Traditionalism", "Tradicionalismo", 0.5f, 0.f, 1.f, 0.7f},
        {"InnovationSeeking", "Búsqueda de innovación", 0.5f, 0.f, 1.f, 0.8f},
        {"WorkEthic", "Ética de trabajo", 0.5f, 0.f, 1.f, 1.2f},
        {"RiskTaking", "Toma de riesgos", 0.5f, 0.f, 1.f, 0.9f},
    };

    trait_weights = {{"Work", 1.0f}, {"Shop", 1.0f}, {"Leisure", 1.0f}, {"Study", 1.0f}, {"Medical", 1.0f}, {"Social", 1.0f}};

    activity_preferences = {
        {Activity::Work, 1.0f},      {Activity::Study, 0.8f}, {Activity::Leisure, 1.0f}, {Activity::Home, 0.9f},
        {Activity::Breakfast, 0.7f}, {Activity::Eat, 0.9f},   {Activity::Sleep, 1.0f},   {Activity::Commute, 0.6f},
        {Activity::Talk, 0.8f},      {Activity::Care, 0.7f},
    };

    sex_weights = {{Sex::Male, 0.495f}, {Sex::Female, 0.495f}, {Sex::Other, 0.01f}};

    profile_weights = {
        {RoutineProfile::DayWorker, 0.3f}, {RoutineProfile::ShiftWorker, 0.1f}, {RoutineProfile::Student, 0.15f},
        {RoutineProfile::Care, 0.1f},      {RoutineProfile::Retired, 0.15f},    {RoutineProfile::Flexible, 0.2f},
    };
}

inline float PersonalityArchetype::get_trait_value(const std::string& trait_name) const {
    for (const auto& trait : traits)
        if (trait.name == trait_name) return trait.value;
    return 0.5f;
}

inline void PersonalityArchetype::mutate(float mutation_rate, std::mt19937& rng) {
    std::uniform_real_distribution<float> chance(0.f, 1.f);
    std::uniform_real_distribution<float> delta(-1.f, 1.f); // simétrico: no hay deriva hacia arriba

    for (auto& trait : traits)
        if (chance(rng) < mutation_rate) trait.set(trait.value + delta(rng) * 0.3f); // ±0.3

    auto nudge = [&](float& value, float max_value) {
        if (chance(rng) < mutation_rate) value = std::clamp(value + delta(rng) * 0.2f, 0.f, max_value);
    };
    nudge(risk_aversion, 1.f); nudge(sociability, 1.f); nudge(patience, 1.f); nudge(conscientiousness, 1.f);
    nudge(curiosity, 1.f);     nudge(aggression, 1.f);  nudge(empathy, 1.f);
    nudge(attraction_to_work, 2.f);  nudge(attraction_to_shop, 2.f);    nudge(attraction_to_leisure, 2.f);
    nudge(attraction_to_study, 2.f); nudge(attraction_to_medical, 2.f); nudge(attraction_to_social, 2.f);
}

inline PersonalityArchetype PersonalityArchetype::crossover(const PersonalityArchetype& a, const PersonalityArchetype& b,
                                                            std::mt19937& rng) {
    std::uniform_real_distribution<float> chance(0.f, 1.f);
    PersonalityArchetype child = a;
    child.name = a.name + "x" + b.name;

    // Cruce uniforme: cada rasgo se hereda de uno de los dos progenitores.
    const std::size_t shared = std::min(a.traits.size(), b.traits.size());
    for (std::size_t i = 0; i < shared; ++i)
        if (chance(rng) < 0.5f) child.traits[i] = b.traits[i];

    auto blend = [](float x, float y) { return x * 0.5f + y * 0.5f; };
    child.risk_aversion = blend(a.risk_aversion, b.risk_aversion);
    child.sociability = blend(a.sociability, b.sociability);
    child.patience = blend(a.patience, b.patience);
    child.conscientiousness = blend(a.conscientiousness, b.conscientiousness);
    child.curiosity = blend(a.curiosity, b.curiosity);
    child.aggression = blend(a.aggression, b.aggression);
    child.empathy = blend(a.empathy, b.empathy);
    child.attraction_to_work = blend(a.attraction_to_work, b.attraction_to_work);
    child.attraction_to_shop = blend(a.attraction_to_shop, b.attraction_to_shop);
    child.attraction_to_leisure = blend(a.attraction_to_leisure, b.attraction_to_leisure);
    child.attraction_to_study = blend(a.attraction_to_study, b.attraction_to_study);
    child.attraction_to_medical = blend(a.attraction_to_medical, b.attraction_to_medical);
    child.attraction_to_social = blend(a.attraction_to_social, b.attraction_to_social);
    return child;
}

inline PersonalityArchetype PersonalityArchetype::random_archetype(std::mt19937& rng) {
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    PersonalityArchetype result("Random");
    for (auto& trait : result.traits) trait.randomize(rng); // aleatoriza los rasgos existentes, sin duplicarlos

    result.risk_aversion = unit(rng);
    result.sociability = unit(rng);
    result.patience = unit(rng);
    result.conscientiousness = unit(rng);
    result.curiosity = unit(rng);
    result.aggression = unit(rng);
    result.empathy = unit(rng);
    result.attraction_to_work = unit(rng) * 2.f;
    result.attraction_to_shop = unit(rng) * 2.f;
    result.attraction_to_leisure = unit(rng) * 2.f;
    result.attraction_to_study = unit(rng) * 2.f;
    result.attraction_to_medical = unit(rng) * 2.f;
    result.attraction_to_social = unit(rng) * 2.f;
    return result;
}

inline void PersonalityArchetype::apply_to_agent(Agent& agent) const {
    // Los factores de decisión son la fuente de verdad. Agent no tiene campos de agresividad/empatía todavía,
    // así que no se reutiliza ningún otro campo para guardarlos.
    agent.risk_aversion = risk_aversion;
    agent.sociability = sociability;
    agent.patience = patience;
    agent.conscientiousness = conscientiousness;
    agent.curiosity = curiosity;
}

} // namespace npc_crowd::personality
