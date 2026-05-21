# CLAUDE.md

Guidance for coding agents (Claude Code, Codex, …) working in this repo.

## What this repo is

`plansys2_llm_monitor` is a **reusable PlanSys2 component**, not an application:
a pluginlib monitor base (`plansys2::MonitorBase`), a `monitor` lifecycle node, a
`plansys2::MonitorClient`, the `plansys2_monitor_msgs` wire contract, and the
default `plansys2/LLAMAMonitor` plugin (llama.cpp via `llama_ros`). It is
consumed by **external** PlanSys2 projects. Treat its public surface as an API.

## Build / test

Colcon workspace, ROS 2 **Jazzy or Rolling** (both first-class):

```bash
colcon build --packages-up-to plansys2_llama_monitor
colcon test  --packages-select plansys2_monitor plansys2_llama_monitor
```

## Hard rules

1. **The public surface is an API.** Do not change without flagging it in the
   PR/commit: `srv/GetProposal`, `msg/Proposal`, `msg/ProposalArray`; the
   `plansys2::MonitorBase` virtual signatures; `plansys2::MonitorClient` /
   `MonitorInterface`; the `monitor/get_proposal` service name; param names in
   `plansys2_monitor/params/monitor_params.yaml`.
2. **Preserve the prompt layout** in `MonitorBase::makePrompt`
   (domain → observation → problem → action log). The static-first /
   dynamic-last ordering is a KV-cache performance contract — see
   `INTEGRATION.md` §5 and the `README.md` benchmark. Do not reorder it or move
   the JSON schema to the tail.
3. **New backends are plugins.** Add a backend by inheriting `MonitorBase` and
   following `plansys2_llama_monitor` exactly (pluginlib export `.xml`,
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

- `plansys2_monitor/include/plansys2_monitor/MonitorBase.hpp` — plugin base, `makePrompt`, `parseResponse`
- `plansys2_monitor/src/plansys2_monitor/MonitorNode.cpp` — `monitor/get_proposal` service, parallel plugin run, `actions_hub` log
- `plansys2_monitor/src/plansys2_monitor/MonitorClient.cpp` — caller API
- `plansys2_monitor_msgs/{srv/GetProposal.srv,msg/Proposal.msg}` — wire contract
- `plansys2_llama_monitor/src/plansys2_llama_monitor/llama_monitor.cpp` — reference plugin
- `plansys2_monitor/params/monitor_params.yaml` — configuration
