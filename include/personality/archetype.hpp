#pragma once

#include "core/types.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

namespace npc_crowd::personality {

/// Rasgo de personalidad individual
struct PersonalityTrait {
    std::string name;
    std::string description;
    float value = 0.5f;        // 0.0 - 1.0 (opuesto extremes)
    float min_value = 0.f;
    float max_value = 1.f;
    float weight = 1.f;        // Importancia en decisión
    bool is_continuous = true; // O discreto (categorías)
    
    // Para rasgos discretos
    std::vector<std::pair<std::string, float>> categories; // {label, value}
    
    // Métodos
    float get() const { return value; }
    void set(float v) { value = std::clamp(v, min_value, max_value); }
    void randomize(std::mt19937& rng) {
        if (is_continuous) {
            std::uniform_real_distribution<float> dist(min_value, max_value);
            value = dist(rng);
        } else {
            // Seleccionar categoría aleatoria
            if (!categories.empty()) {
                std::uniform_int_distribution<std::size_t> ci(0, categories.size() - 1);
                value = categories[ci(rng)].second;
            }
        }
    }
    
    // Interpolación entre dos valores
    static PersonalityTrait lerp(const PersonalityTrait& a, const PersonalityTrait& b, float t) {
        PersonalityTrait result = a;
        result.value = a.value * (1.f - t) + b.value * t;
        return result;
    }
};

/// Arquetipo de personalidad (perfil predefinido)
struct PersonalityArchetype {
    std::string name;
    std::string description;
    std::vector<PersonalityTrait> traits; // 20+ rasgos base
    std::unordered_map<std::string, float> trait_weights; // Pesos por actividad
    std::unordered_map<Activity, float> activity_preferences; // Preferencia por actividad
    std::unordered_map<Sex, float> sex_weights; // Distribución por sexo
    std::unordered_map<RoutineProfile, float> profile_weights; // Distribución por perfil
    
    // Factores de decisión
    float risk_aversion = 0.5f;
    float sociability = 0.5f;
    float patience = 0.5f;
    float conscientiousness = 0.5f;
    float curiosity = 0.5f;
    float aggression = 0.5f;
    float empathy = 0.5f;
    
    // Atracción a actividades específicas
    float attraction_to_work = 1.f;
    float attraction_to_shop = 1.f;
    float attraction_to_leisure = 1.f;
    float attraction_to_study = 1.f;
    float attraction_to_medical = 1.f;
    float attraction_to_social = 1.f;
    
    // Constructor por defecto con 20 rasgos
    PersonalityArchetype(const std::string& name_);
    
    // Obtener valor de rasgo por nombre
    float get_trait_value(const std::string& trait_name) const;
    
    // Aplicar mutación genética
    void mutate(float mutation_rate, std::mt19937& rng);
    
    // Cross-over con otro arquetipo
    static PersonalityArchetype crossover(const PersonalityArchetype& a, const PersonalityArchetype& b, std::mt19937& rng);
    
    // Generar variante aleatoria
    static PersonalityArchetype random_archetype(std::mt19937& rng);
    
    // Convertir a valores para Agent struct
    void apply_to_agent(Agent& agent) const;
};

// Arquetipos predefinidos

/// Arquetipo: Trabajador estable
inline const PersonalityArchetype& get_stable_worker_archetype();

/// Arquetipo: Estudiante impulsivo
inline const PersonalityArchetype& get_student_archetype();

/// Arquetipo: Cuidador empático
inline const PersonalityArchetype& get_caregiver_archetype();

/// Arquetipo: Jubilado tranquilo
inline const PersonalityArchetype& get_retired_archetype();

/// Arquetipo: Emprendedor riskeador
inline const PersonalityArchetype& get_entrepreneur_archetype();

/// Arquetipo: Sociable extrovertido
inline const PersonalityArchetype& get_extrovert_archetype();

/// Arquetipo: Introvert cauteloso
inline const PersonalityArchetype& get_introvert_archetype();

/// Arquetipo: Ansioso/hipervigilante
inline const PersonalityArchetype& get_anxious_archetype();

/// Arquetipo: Perezoso / baja energía
inline const PersonalityArchetype& get_lazy_archetype();

/// Arquetipo: Hipocondríaco
inline const PersonalityArchetype& get_hypochondriac_archetype();

/// Arquetipo: Buscador de riesgo
inline const PersonalityArchetype& get_risk_seeker_archetype();

/// Obtener todos los arquetipos registrados
inline std::vector<const PersonalityArchetype*> get_all_archetypes();

/// Registrar arquetipo personalizado
inline void register_archetype(PersonalityArchetype archetype);

} // namespace npc_crowd::personality

#include "archetype_impl.hpp"
