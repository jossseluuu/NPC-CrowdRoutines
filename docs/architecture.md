# Architecture and simulation scope

## Product shape

This repository is intended to become a repeatable city-scale simulation lab, not only a crowd animation. The native simulation owns state and time. A renderer consumes snapshots and sends commands (pause, step, speed, event injection, selection) through a narrow transport layer. The browser preview currently demonstrates heterogeneous routines, social links, simple road routing and compound population queries with its own model; it is not yet connected to the native process.

```text
Application shell
  ├─ Simulation coordinator ── world clock / seeded random streams
  │    ├─ Population and lifecycle
  │    ├─ Needs, utility decisions, schedules
  │    ├─ Navigation, traffic, local movement
  │    ├─ Events, perception, propagation
  │    └─ Telemetry, snapshots, replay
  ├─ OOP backend / SoA backend (same domain contract)
  └─ Presentation adapter ── browser / SDL2-ImGui
```

The coordinator advances fixed simulation ticks. Presentation frame rate must not decide simulation state. Speed multipliers change the mapping between simulation time and wall time; they do not change the fixed-step rules. All externally visible changes enter through timestamped commands so a recorded run can be reproduced.

## Native module boundaries

| Module | Owns | Depends on |
| --- | --- | --- |
| `core` | IDs, units, vectors, clock, configuration, deterministic random streams | standard library |
| `population` | citizen profiles, household links, spawn/despawn, age and lifecycle | core |
| `needs` | energy, hunger, rest, social contact, hygiene, bladder, comfort, health, safety, money | core, population |
| `schedule` | work shifts, school, errands, home commitments, leisure windows | core, population |
| `decision` | Utility AI, personality modifiers, action selection and cooldowns | needs, schedule, world state |
| `world` | districts, buildings, land use, roads, sidewalks, transit, weather | core |
| `navigation` | hierarchical routes, travel costs, closures, replanning and route cache | world |
| `movement` | steering, collision avoidance, queues, density and pedestrian flow | navigation, spatial index |
| `events` | event sources, affected areas, perception, response rules and propagation | world, population |
| `storage` | OOP and SoA implementations behind a common population view | core, population |
| `telemetry` | tick/frame percentiles, counters, memory estimates and export | storage, coordinator |
| `presentation` | renderer, inspector, overlays and user commands | snapshots and command API |

Keep policy and presentation out of the storage backends. A backend should answer the same domain operations and produce the same state for a fixed seed and command stream. Use stable entity IDs; never expose vector positions as persistent identity.

## Citizen model

The current native `Agent` carries identity, position, velocity, home/work anchors, activity, age, job category, needs, condition, and personality values. The browser inspector has a broader prototype profile. The long-term schema should group data by update cadence and access pattern rather than build one ever-growing entity class:

- **Identity and social graph:** stable ID, household, employer, school, district, relationships, dependents, role, age band.
- **Needs and condition:** energy, hunger, hydration, rest, social contact, hygiene, bladder, comfort, health, pain, safety, stress, fatigue, fear, curiosity, boredom, temperature, money.
- **Personality and skills:** sociability, risk aversion, patience, conscientiousness, curiosity, trust, empathy, mobility, driving and local knowledge.
- **Routine:** work/school hours, commute mode, recurring errands, preferred venues, meal windows, sleep window and flexible time budget.
- **Mobility:** current mode, speed class, accessibility needs, current route, next waypoint, group membership and congestion exposure.
- **Memory and belief:** known incidents, perceived danger, trusted sources, destination familiarity, last update time and confidence.
- **Social context:** household members, companions, leaders/followers, caregiving duty, shared destination and separation response.

Values should declare units, range, update cadence, persistence, and whether the value is authored, derived, or stochastic. Needs are not all updated every tick: use scheduled coarse updates for slow variables and event-driven updates for abrupt changes.

## Decision and routine pipeline

At each decision interval, candidate actions are generated from schedule and local world context. Each action receives a score from need relief, schedule urgency, travel cost, personal preference, social obligation, perceived risk, affordability, accessibility, and uncertainty. Apply hysteresis and minimum commitment windows to prevent destination thrashing. Escalate decision frequency for emergencies and keep routine citizens on a cheaper cadence. Record the chosen action and its top contributing terms for inspection and replay.

Example action candidates include sleep, go to work, stay at work, eat at home, buy food, visit a venue, accompany a dependent, return home, wait, reroute, shelter, investigate, help, or evacuate. Actions should be data-driven and configurable, not a chain of hard-coded time checks.

## Navigation and crowd model

Use a multi-layer graph: district connectors, road/transit graph, pedestrian links, building entrances, and final approach paths. Route cost combines travel time, mode changes, fare, accessibility, crowd density, closures, weather and perceived risk. Cache routes by origin/destination/mode and invalidate only affected edges after world changes. Local avoidance operates in world space with a spatial index, while bottleneck capacity and queues live on graph edges and entrances. Static obstacles must be authoritative collision geometry; a clamped map boundary is not an obstacle system.

## World events and reactions

An event has a stable ID, type, start/end time, footprint, severity, source, confidence, affected world links, and response policy. Separate the actual hazard from what each person knows. Perception depends on distance, visibility, hearing, attention, trust, and communication delay. Reactions can propagate through household ties, nearby witnesses, workplaces, public alerts and social groups. Track exposure and response latency so panic is an outcome to measure rather than an automatic global state switch.

Event families to plan for: traffic collisions, road works, broken transit, weather, power outage, fire, medical emergency, public gathering, service outage, security warning and evacuation. Each family should modify only relevant world systems, and each should have a decay/recovery path.

## Determinism, backends and performance

Use independent seeded random streams for population generation, schedules, world events and optional presentation effects. A replay header should store build version, seed, backend, fixed-step size, configuration hash and command log. Cross-backend comparisons must start from identical initial snapshots and receive identical commands.

Benchmark warm-up separately from measured ticks. Report p50/p95/p99, maximum, tick overruns, agent count, event load, compiler/build mode, CPU, memory source, and whether the renderer is included. Keep allocation and memory accounting explicit. A 2 ms target is a measured acceptance threshold on declared hardware, not a hard-coded promise.

## Delivery stages

1. **Interactive slice (current):** browser preview, agent inspector, heterogeneous profiles and schedules, household and social links, simple road routes, pursuit/flee/conversation states, compound AND/OR filters, manual disruption triggers, CSV export; native fixed-step OOP/SoA benchmark with per-agent wake, sleep, breakfast and work schedules.
2. **Shared native domain:** move routines, needs, events and spatial queries behind backend-neutral systems; build a snapshot/command boundary and make replay deterministic.
3. **Walkable district:** authored sidewalks/buildings, obstacle constraints, route planner, transit stops, queues and density telemetry.
4. **Emergent response:** per-agent perception, event-specific decisions, social/household propagation, staged evacuation and recovery.
5. **Scale and research:** 500–25,000 population sweeps, repeatable hardware reports, profiling, level of detail, parallel batches and documented architectural decisions.

Do not claim a stage complete based on a visual mock. Mark functionality complete only when it runs through the native domain and has observable output in the inspector or exported run data.
