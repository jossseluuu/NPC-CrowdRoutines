# ADR 0001: Compare OOP and SoA behind one simulation contract

- **Status:** Accepted
- **Context:** The project needs to compare object-oriented updates with data-oriented storage without changing the simulated rules between runs.
- **Decision:** Keep domain policy outside storage. Provide OOP and SoA state providers that receive identical initialization, fixed-step settings, random streams and timestamped commands. Compare output snapshots and report the full machine/build configuration with timing.
- **Consequences:** SoA can optimize sequential batches and cache locality; OOP provides a straightforward reference model. Some systems may need backend-specific iteration adapters, but behavior remains governed by shared policies. Current code has the first storage comparison; a shared command/replay contract and full parity work remain future stages.
- **Revisit when:** A backend cannot implement a system without duplicating policy, or deterministic parity becomes ambiguous.
