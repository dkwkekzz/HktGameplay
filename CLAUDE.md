# CLAUDE.md

## Project Overview

HktCore is the core module of the **HktGameplay** Unreal Engine 5 plugin — a deterministic multiplayer game framework implementing an Intent-Simulation-Presentation (ISP) architecture for server-authoritative RTS/MOBA-style games.

This worktree covers `Source/HktCore/`, the pure C++ virtual machine and data layer. The full plugin lives at `E:/WS/UE5/HktProto/Plugins/HktGameplay/`.

## Architecture

**Six modules** compose the plugin:

| Module | Type | Purpose |
|--------|------|---------|
| **HktCore** | Runtime | Deterministic VM, Stash (entity data), Physics |
| **HktRuntime** | Runtime | Game loop, networking, relevancy |
| **HktPresentation** | Runtime | View layer (read-only, delegate-driven) |
| **HktAsset** | Runtime | Data assets, input action config |
| **HktInsights** | DeveloperTool | Debug/profiling UI |
| **HktInsightsEditor** | Editor | Editor extensions |

**Data flow:** Client Input → IntentBuilder → Server RPC → VM Execution → Relevancy-filtered Batch → Client Presentation

## HktCore Specifics

- **Pure C++ VM** — zero UObject/UWorld dependencies, enabling perfect determinism
- **32-bit instruction encoding** (OpCode + Operands + Immediate values)
- **16 registers** (32-bit): R0-R9 general, R10-R15 special (Self, Target, Spawned, Hit, Iter, Flag)
- **Three-phase pipeline**: Build (create VMs) → Execute (run bytecode, max 10K instructions/tick) → Cleanup (apply writes, deallocate)
- **Stash system**: MasterStash (server, full state) / VisibleStash (client, partial read-only) using SOA layout
- **Generational handles** prevent use-after-free
- **Max 256 VMs/frame, 1024 entities, 128 properties/entity**

## Build & Dependencies

**Build files:** `*.Build.cs` per module (standard UE5 plugin structure)

**HktCore dependencies:** Core, CoreUObject, GameplayTags (+ HktInsights in non-Shipping)

**External dependencies:** HktBase (git submodule), MassGameplay, MassAI, Niagara, EnhancedInput

## Code Conventions

- **Prefixes:** `F` (structs/pure C++), `A` (Actors), `U` (UObjects), `I` (interfaces), `E` (enums)
- **Booleans:** `b` prefix (e.g., `bIsActive`)
- **Camel case** for properties and functions
- **ALL_CAPS** for constants
- **Key patterns:** Pool (VM runtime), Builder (IntentBuilder, FlowBuilder), Observer (delegates for Model→View), SOA (Stash properties), Interface Segregation (many small interfaces)

## Key Types (HktCoreTypes.h)

- `FHktEntityId` — Entity identifier
- `FHktIntentEvent` — Client→Server action
- `FHktEntitySnapshot` — Entity state snapshot
- `FHktFrameBatch` — Server→Client batch
- `FHktCellChangeEvent` — Relevancy event

## Key Classes

- `FHktVMProgram` — Compiled bytecode program
- `FHktVMRuntime` — VM instance state
- `FHktVMInterpreter` — Bytecode executor
- `FHktVMProcessor` — VM pool + execution manager
- `FHktMasterStash` / `FHktVisibleStash` — Server/client entity data
- `FHktPhysicsWorld` — Simple collision detection

## Testing

- `HktCollisionTests.h/cpp` — Collision detection test suite
- Physics math unit tests for vector/ray operations

## Important Constraints

1. **Determinism is paramount** — VM logic must produce identical results given identical inputs
2. **No UE5 runtime in HktCore** — keep the VM pure C++ (no UObject, UWorld, etc.)
3. **Server-authoritative** — clients cannot manipulate what they see; server filters all data
4. **Thread safety** — intent queue uses locks for multi-threaded access
5. **FlowBuilder programs register at startup** — no hot reloading
6. **Loose coupling** — Model↔View communication uses delegates only; Presentation is read-only
