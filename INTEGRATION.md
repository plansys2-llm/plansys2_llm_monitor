# Integrating `plansys2_llm_monitor` into your project

This document is the integration contract for `plansys2_llm_monitor`. It is
written to be read both by developers and by their coding agents (Claude Code,
Codex, …). It tells you how to wire the LLM replanner into an existing PlanSys2
project and how to add your own monitor backend. It does **not** repeat install
instructions — those live at <https://github.com/plansys2-llm>.

> Conventions in this file: paths and C++/ROS symbols are stable references;
> line numbers are intentionally omitted because they drift. If a symbol or path
> here no longer exists in the code, the docs are stale — fix the docs.

## 1. What this is (one paragraph)

`plansys2_llm_monitor` is **not** a PDDL planner. PlanSys2 keeps using its
classical planner (POPF). This package adds a **recovery** step: when plan
execution fails or perception contradicts the symbolic state, you hand the LLM
the PDDL domain, the PDDL problem and a natural-language `observation`, and it
replies with a classification plus the **predicate deltas** (add/remove) needed
to make the problem state match reality so PlanSys2 can replan. The default
backend is local llama.cpp via `llama_ros`; other backends are pluginlib
plugins.

## 2. The three packages

| Package | Role | You depend on it when… |
|---|---|---|
| `plansys2_monitor_msgs` | `srv/GetProposal`, `msg/Proposal`, `msg/ProposalArray` | always (the wire contract) |
| `plansys2_monitor` | `plansys2::MonitorBase` plugin base + `monitor` lifecycle node + `plansys2::MonitorClient` | you call the monitor or write a plugin |
| `plansys2_llama_monitor` | reference plugin (`plansys2/LLAMAMonitor`) backed by `llama_ros` | you use the default LLM backend |

## 3. Quickstart — call the monitor from your PlanSys2 project

### 3.1 Build dependencies

`package.xml`:

```xml
<depend>plansys2_monitor</depend>
<depend>plansys2_monitor_msgs</depend>
```

`CMakeLists.txt`:

```cmake
find_package(plansys2_monitor REQUIRED)
find_package(plansys2_monitor_msgs REQUIRED)

target_link_libraries(your_node
  plansys2_monitor::plansys2_monitor
  ${plansys2_monitor_msgs_TARGETS}
)
```

### 3.2 Run the monitor node

The monitor is a **lifecycle node** named `monitor`. Launch it with the shipped
launch file (loads `plansys2_monitor/params/monitor_params.yaml`):

```bash
ros2 launch plansys2_monitor monitor_launch.py        # optional: namespace:=/robot1
```

Because it is a lifecycle node it must be transitioned
`configure → activate` before it answers. In the bookstore demo PlanSys2's
lifecycle manager does this; **a standalone integrator must drive the
transitions themselves** (e.g. `ros2 lifecycle set /monitor configure` then
`activate`, or a lifecycle manager). The model server is forked during
`on_configure` when `pre_launch: true`, so the first solve does not pay the
model-load cost.

### 3.3 Call it from C++

```cpp
#include "plansys2_monitor/MonitorClient.hpp"

auto monitor = std::make_shared<plansys2::MonitorClient>();

std::optional<plansys2_monitor_msgs::msg::Proposal> r =
  monitor->getProposal(domain_pddl, problem_pddl, observation, "");

if (!r.has_value()) {
  // timeout, service absent, or LLM output unparseable (ERROR) — give up / retry
} else if (r->classification == plansys2_monitor_msgs::msg::Proposal::CORRECT) {
  // state already consistent; just replan
} else {
  for (const auto & p : r->remove_predicates) {
    problem_expert_->removePredicate(plansys2::Predicate(p));
  }
  for (const auto & p : r->add_predicates) {
    problem_expert_->addPredicate(plansys2::Predicate(p));
  }
  // then ask PlanSys2 to replan
}
```

> **Integrator trap.** `getProposal(domain, problem, observation, ns)`
> has **no separate “prompt” parameter**. The third argument *is* the
> `observation`. In the reference example
> (`plansys2_llm_examples` → `Reception::step()` in
> `src/reception_controller_node.cpp`) a local variable called `prompt` is built
> (task + guidelines + failed action + perception log) and passed straight into
> the `observation` slot. The monitor wraps it into the final LLM prompt for you
> (see §5). Do not also build a full LLM prompt yourself.

You do **not** pass the action-execution log. The `monitor` node subscribes to
`<ns>/actions_hub` (`plansys2_msgs/msg/ActionExecution`), deduplicates the
feedback flood, and injects a summary into the prompt automatically. PlanSys2's
executor already publishes there.

## 4. The public contract

### Service `monitor/get_proposal` — `plansys2_monitor_msgs/srv/GetProposal`

Created relative to the node namespace by `MonitorNode::on_configure`
(`plansys2_monitor/src/plansys2_monitor/MonitorNode.cpp`). Use the service
directly for non-C++ / cross-language callers.

| Direction | Field | Meaning |
|---|---|---|
| Request | `string domain` | PDDL domain text |
| Request | `string problem` | PDDL problem text (current state) |
| Request | `string observation` | natural-language task + guidelines + dynamic facts (see §5) |
| Response | `uint8 status` | `ERROR=0`, `SUCCESS=1`, `RUNNING=2` |
| Response | `plansys2_monitor_msgs/Proposal proposal` | the result (below) |
| Response | `string error_info` | populated when `status == ERROR` |

### `plansys2_monitor_msgs/msg/Proposal`

| Field | Meaning |
|---|---|
| `uint8 classification` | `CORRECT=0`, `MODIFY_PLAN=1`, `MODIFY_DOMAIN=2`, `UNSOLVABLE=3`, `ERROR=4` |
| `string[] add_predicates` / `string[] remove_predicates` | PDDL predicate deltas to apply to the problem |
| `string[] add_instances` | new objects/instances proposed |
| `string[] domain_changes` | proposed domain edits (advisory) |
| `string resolution` | raw LLM text (for logging/debug) |
| `float32 time` | solve wall time, seconds |

Behavioural notes (see `MonitorBase::parseResponse`,
`MonitorNode::get_proposal_service_callback`, `MonitorClient::getProposal`):

- `ERROR` classification (LLM output unparseable) is mapped to
  `status == ERROR` by the node and to `std::nullopt` by `MonitorClient`. A
  C++ caller therefore only ever sees `CORRECT/MODIFY_PLAN/MODIFY_DOMAIN/UNSOLVABLE`
  or no value.
- `MonitorClient::getProposalArray` is currently a stub returning
  empty — only the single-result path is supported.
- Multiple plugins run in parallel (`std::async`); the service returns the
  first entry of the collected `ProposalArray` (iteration order is by monitor id).
  Single-plugin setups (the default `["LLAMA"]`) are unaffected.

## 5. The `observation` convention (performance-critical)

Build the `observation` string **static content first, dynamic content last**:

```text
Your task: <stable description of what the LLM must decide>

Guidelines:
- <stable, domain-specific reasoning rules>
- ...

<dynamic: the failed action, runtime perceptions, sensor readings>
```

Why: `MonitorBase::makePrompt` (`plansys2_monitor/include/plansys2_monitor/MonitorBase.hpp`)
places `observation` between `--- Domain ---` and `--- Problem ---`. With
`cache_prompt: true` (llama_ros default) the LLM reuses the longest common
prefix across calls, so keeping the volatile text at the tail maximises
KV-cache reuse. The repo `README.md` benchmark shows this is ≈×5 fewer tokens
recomputed on a tail-only change, and `pre_launch: true` gives the cold→warm
speedup. This ordering is a contract: a plugin must not reorder
domain/observation/problem in its prompt.

## 6. Configuration (`plansys2_monitor/params/monitor_params.yaml`)

```yaml
monitor:
  ros__parameters:
    propose_timeout: 240.0          # seconds; plugins exceeding this are cancel()ed
    summarize_mode: "limited"      # "limited" = only FINISH/CANCEL action-log entries; "full" = all
    prompt_debug: false            # log the assembled prompt and action summary
    trunc_file: true               # truncate the actions_hub log after each get_proposal; false = accumulate (advanced)
    monitor_plugins: ["LLAMA"]      # ordered list of plugin ids to load
    LLAMA:
      plugin: "plansys2/LLAMAMonitor"
      llm_debug: true              # drain llama_node stdout/stderr into the node log
      pre_launch: true             # start llama_node once at configure (fast warm calls, holds RAM)
      model_yaml: "~/TFG/src/llm/llama_ros/llama_bringup/models/Qwen2.5-3B.yaml"
      # launch_extra_args: []      # extra argv appended to `ros2 llama launch`
      # prompt_extra_args: []      # extra argv appended to `ros2 llama prompt`
      # output_dir: "<tmp>"        # where monitor_node_{domain,problem,observation,prompt,resolution} are written
```

**Integrators must override `model_yaml`** — the default points into the TFG
tree. CPU thread count, context size and the GGUF file are configured in the
`llama_ros` model YAML, not here. `~` is expanded. The `MonitorClient` reads its
own `propose_timeout` parameter (default 150 s) — set it consistently with the
node.

`trunc_file` (default `true`) truncates the `actions_hub` log file after every
`get_solve` consumes it, so each solve is scoped to the just-failed plan
attempt. This bounds the log (it would otherwise grow for the lifetime of the
monitor node) and keeps the prompt's static prefix stable for KV-cache reuse.
Set it `false` only if you deliberately want cross-attempt accumulation
(advanced; grows unbounded, weakens cache reuse). Cross-attempt "the previous
fix did not work" memory should instead be supplied by the caller in the
`observation`, since only the caller knows whether its prior correction
succeeded.

## 7. Writing your own monitor plugin

The fork+exec shape of `LLAMAMonitor` exists so any backend (other local models,
paid APIs) can plug in without dragging its SDK into the monitor process. To add
one:

1. Inherit `plansys2::MonitorBase`
   (`plansys2_monitor/include/plansys2_monitor/MonitorBase.hpp`).
2. Implement the pure-virtuals:
   - `void initialize(const std::string & node_name)`
   - `std::optional<plansys2_monitor_msgs::msg::Proposal> solve(domain, problem, observation, action_file, node_namespace = "", propose_timeout = 120s)`
3. Optionally override `configure(lc_node, plugin_name)` — **call
   `MonitorBase::configure(lc_node, plugin_name)` first** (it reads
   `summarize_mode` / `prompt_debug`), then declare your own
   `plugin_name + ".<param>"` parameters. Optionally override `cancel()`.
4. Reuse the protected helpers — do not reinvent them:
   - `makePrompt(domain, problem, action_summary, observation)` — the canonical
     prompt layout (keep it; see §5).
   - `summarizeActionLog(raw, limited)` — collapses the `actions_hub` log;
     `limited` from `summarize_mode_`.
   - `parseResponse(raw)` — extracts the outermost JSON, maps `classification`,
     fills the predicate arrays, and yields `ERROR` on any parse failure so
     callers can tell “no answer” from “wants a replan”.
5. Cooperate with cancellation: poll `cancel_requested_` during long work.
   `MonitorNode::get_proposal_array` cancels plugins that exceed `propose_timeout`
   and joins their futures ~100 ms later; a plugin that ignores
   `cancel_requested_` will stall the service.
6. Export it (mirror `plansys2_llama_monitor` exactly):
   - End the `.cpp` with
     `PLUGINLIB_EXPORT_CLASS(plansys2::YourMonitor, plansys2::MonitorBase);`
   - Ship a `*_plugin.xml`:
     `<library path="your_lib"><class name="plansys2/YourMonitor" type="plansys2::YourMonitor" base_class_type="plansys2::MonitorBase">`
   - `CMakeLists.txt`:
     `pluginlib_export_plugin_description_file(plansys2_monitor your_plugin.xml)`
   - `package.xml` `<export>`:
     `<plansys2_monitor plugin="${prefix}/your_plugin.xml" />`
7. Select it in params: add the id to `monitor_plugins` and a
   `YOURID: { plugin: "plansys2/YourMonitor", ... }` block. Several plugins may
   be listed; they run concurrently.

Reference implementation:
`plansys2_llama_monitor/src/plansys2_llama_monitor/llama_monitor.cpp`
(`LLAMAMonitor::configure / propose / launch_llm_server / shutdown_llm_server`).

## 8. Gotchas

- **Lifecycle.** `monitor` does nothing until `configure`+`activate`. Standalone
  users must manage the transitions.
- **`llama_ros` required for the default plugin.** `LLAMAMonitor` forks
  `ros2 llama launch <model_yaml>` and `ros2 llama prompt "<text>" -t 0.0`;
  `ros2 llama` (the `llama_cli` exec dependency) must be on `PATH` in the
  node's environment.
- **`pre_launch`.** `true` = one warm `llama_node` for the plugin's lifetime
  (fast, holds RAM); `false` = launch/teardown per solve (frees RAM, slow). If
  `pre_launch: true` but the server died, `solve()` relaunches on the fly.
- **`observation` ordering** is a performance contract (§5), not a style
  preference.
- **Targets.** ROS 2 Jazzy and Rolling are both first-class.

## 9. End-to-end reference

`plansys2_llm_examples` (companion repo) is the worked example:

- `src/reception_controller_node.cpp` → `Reception::step()`, plan-failure path
  (logged `[EXECUTION_FAILURE]`): builds the `observation`, calls
  `MonitorClient::getProposal`, applies the predicate deltas via the
  PlanSys2 `ProblemExpert`, replans.
- `Reception::build_perception_context()`: how the dynamic tail of the
  `observation` is assembled from perception events.
- `launch/bookstore_kobuki_launch.py`: how the `monitor` node is brought up
  alongside PlanSys2.

See <https://github.com/plansys2-llm> for installation and the full demo.
