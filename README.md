# plansys2_llm_solver

LLM-assisted replanner for [PlanSys2](https://github.com/PlanSys2/ros2_planning_system). When plan execution fails or a perception contradicts the symbolic state, it queries an LLM and returns the predicate deltas needed to recover — it does **not** replace PlanSys2's PDDL planner (POPF), it complements it. Default backend: local llama.cpp via [`llama_ros`](https://github.com/mgonzs13/llama_ros).

Part of the [`plansys2-llm`](https://github.com/plansys2-llm) project.

## What this package provides

- **`plansys2_solver`** — pluginlib base (`plansys2::SolverBase`) and the lifecycle node that loads and runs the LLM plugins.
- **`plansys2_solver_msgs`** — ROS messages and service definitions used between the solver node and its callers.
- **`plansys2_llama_solver`** — default plugin backed by local llama.cpp via `llama_ros`.

Plugins are loaded via pluginlib; multiple can be configured at once and run in parallel. New backends (e.g. ChatGPT through the OpenAI API) plug in by inheriting from `plansys2::SolverBase`.

## `observation` field convention

The service `GetSolve.srv` accepts an `observation` string with free-form content. The solver places it immediately after `--- Domain ---` in the prompt (the `--- Task & Observations ---` block).

For best KV-cache reuse across consecutive solver calls (`cache_prompt: true` in `llama_ros`), construct the observation with **static content first, dynamic content last**:

```text
Your task: <stable description of what the LLM should do>

Guidelines:
- <domain-specific reasoning rules, stable across calls>
- ...

<dynamic content — failed action, runtime perceptions, sensor readings>
```

Because the cache reuses the longest common prefix between consecutive prompts, putting stable instructions at the top extends the cacheable region and avoids recomputing identical text on every replan.

## Installation and usage

This is one of two repositories that compose the project. **The full installation and usage instructions live in the organization home:**

> https://github.com/plansys2-llm

The companion repository (`plansys2_llm_examples`) provides the bookstore demo that exercises this replanner.

## License

Apache 2.0
