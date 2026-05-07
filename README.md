# plansys2_llm_solver

Custom plan-solver plugin for [PlanSys2](https://github.com/PlanSys2/ros2_planning_system) that delegates PDDL planning to a large language model running locally via [`llama_ros`](https://github.com/mgonzs13/llama_ros) (llama.cpp).

Part of the [`plansys2-llm`](https://github.com/plansys2-llm) project.

## What this package provides

- **`plansys2_solver`** — base plan-solver interface and the C++ glue with PlanSys2's planner pipeline.
- **`plansys2_solver_msgs`** — ROS messages and actions used to talk to the underlying LLM service.
- **`plansys2_llama_solver`** — concrete implementation backed by `llama_cli` from `llama_ros`.

The solver is loaded as a PlanSys2 plugin (`plansys2/PlanSolverBase`), so it can be swapped for the default POPF planner via configuration.

## Installation and usage

This is one of two repositories that compose the project. **The full installation, configuration and demo instructions live in the organization home:**

> https://github.com/plansys2-llm

The companion repository (`plansys2_llm_examples`) provides the bookstore demo that exercises this solver end-to-end.

## License

Apache 2.0
