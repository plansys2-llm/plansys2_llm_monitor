# CLAUDE.md

Guidance for coding agents (Claude Code, Codex, …) working in this repo.

## What this repo is

`plansys2_llm_solver` is a **reusable PlanSys2 component**, not an application:
a pluginlib solver base (`plansys2::SolverBase`), a `solver` lifecycle node, a
`plansys2::SolverClient`, the `plansys2_solver_msgs` wire contract, and the
default `plansys2/LLAMASolver` plugin (llama.cpp via `llama_ros`). It is
consumed by **external** PlanSys2 projects. Treat its public surface as an API.

## Build / test

Colcon workspace, ROS 2 **Jazzy or Rolling** (both first-class):

```bash
colcon build --packages-up-to plansys2_llama_solver
colcon test  --packages-select plansys2_solver plansys2_llama_solver
```

## Hard rules

1. **The public surface is an API.** Do not change without flagging it in the
   PR/commit: `srv/GetSolve`, `msg/Solver`, `msg/SolverArray`; the
   `plansys2::SolverBase` virtual signatures; `plansys2::SolverClient` /
   `SolverInterface`; the `solver/get_solve` service name; param names in
   `plansys2_solver/params/solver_params.yaml`.
2. **Preserve the prompt layout** in `SolverBase::makePrompt`
   (domain → observation → problem → action log). The static-first /
   dynamic-last ordering is a KV-cache performance contract — see
   `INTEGRATION.md` §5 and the `README.md` benchmark. Do not reorder it or move
   the JSON schema to the tail.
3. **New backends are plugins.** Add a backend by inheriting `SolverBase` and
   following `plansys2_llama_solver` exactly (pluginlib export `.xml`,
   `PLUGINLIB_EXPORT_CLASS`, CMake `pluginlib_export_plugin_description_file`,
   `package.xml` `<export>`). Reuse `makePrompt` / `summarizeActionLog` /
   `parseResponse`; honor `cancel_requested_`.
4. **Docs must not lie.** If you change a symbol/path/param referenced in
   `INTEGRATION.md` or `README.md`, update those docs in the same change. Docs
   reference symbols and paths, never line numbers.

## Depth lives in INTEGRATION.md

`INTEGRATION.md` is the single source of truth for the integration contract,
the public API tables, the `observation` convention, plugin authoring, config
and gotchas. Read it before changing behavior; do not duplicate its content
here.

## Key files

- `plansys2_solver/include/plansys2_solver/SolverBase.hpp` — plugin base, `makePrompt`, `parseResponse`
- `plansys2_solver/src/plansys2_solver/SolverNode.cpp` — `solver/get_solve` service, parallel plugin run, `actions_hub` log
- `plansys2_solver/src/plansys2_solver/SolverClient.cpp` — caller API
- `plansys2_solver_msgs/{srv/GetSolve.srv,msg/Solver.msg}` — wire contract
- `plansys2_llama_solver/src/plansys2_llama_solver/llama_solver.cpp` — reference plugin
- `plansys2_solver/params/solver_params.yaml` — configuration
