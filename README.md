# plansys2_llm_solver

LLM-assisted replanner for [PlanSys2](https://github.com/PlanSys2/ros2_planning_system). When plan execution fails or a perception contradicts the symbolic state, it queries an LLM and returns the predicate deltas needed to recover — it does **not** replace PlanSys2's PDDL planner (POPF), it complements it. Default backend: local llama.cpp via [`llama_ros`](https://github.com/mgonzs13/llama_ros).

Part of the [`plansys2-llm`](https://github.com/plansys2-llm) project.

## What this package provides

- **`plansys2_solver`** — pluginlib base (`plansys2::SolverBase`) and the lifecycle node that loads and runs the LLM plugins.
- **`plansys2_solver_msgs`** — ROS messages and service definitions used between the solver node and its callers.
- **`plansys2_llama_solver`** — default plugin backed by local llama.cpp via `llama_ros`.

Plugins are loaded via pluginlib; multiple can be configured at once and run in parallel. New backends (e.g. ChatGPT through the OpenAI API) plug in by inheriting from `plansys2::SolverBase`.

## Integrating this into your project

👉 **[`INTEGRATION.md`](INTEGRATION.md)** is the integration contract: how to
call the solver from an existing PlanSys2 project, the `solver/get_solve`
service and message contract, the `observation` convention, configuration, and
how to write your own solver plugin. It is written to be read by developers and
by coding agents alike. Agents working *in this repo* should also read
[`CLAUDE.md`](CLAUDE.md) (mirrored as [`AGENTS.md`](AGENTS.md)).

## Benchmark

On a Raspberry Pi 5 16 GB running `Qwen2.5-3B-Q4_K_M` on CPU (4 threads), 3 050-token prompt:

| Call | Prompt eval | Wall | Cached tokens |
|---|---|---|---|
| First (cold KV cache) | 156.3 s | 3:19 | 0 / 3 391 |
| Identical replan (warm) | 0.49 s | 43 s | 3 390 / 3 391 |
| Replan with new tail line | 3.7 s | 47.8 s | 3 381 / 3 418 |

Cold → warm: **×4.7 faster**. Replan with a tail change: **×5.2 fewer tokens recomputed** vs the same prompt structured with the JSON schema at the bottom.

### How to reproduce these numbers

The `observation` argument to `GetSolve.srv` should be built with **static content first, dynamic content last**. The solver places it immediately after `--- Domain ---` in the prompt (the `--- Task & Observations ---` block), so `cache_prompt: true` (default in `llama_ros`) can reuse the longest common prefix between consecutive calls:

```text
Your task: <stable description of what the LLM should do>

Guidelines:
- <domain-specific reasoning rules, stable across calls>
- ...

<dynamic content — failed action, runtime perceptions, sensor readings>
```

The first speed-up (cold → warm) comes for free once `pre_launch: true` keeps the `llama_node` alive between calls. The second (×5.2 on the tail change) requires the static-first / dynamic-last layout above.

## Installation and usage

This is one of two repositories that compose the project. **The full installation and usage instructions live in the organization home:**

> https://github.com/plansys2-llm

The companion repository (`plansys2_llm_examples`) provides the bookstore demo that exercises this replanner.

## License

Apache 2.0
