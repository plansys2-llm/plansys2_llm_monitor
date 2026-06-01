// Copyright 2019 Intelligent Robotics Lab
// Copyright 2026 Álvaro Valencia
//
// Scaffolding adapted from the PlanSys2 client/node pattern
// (Intelligent Robotics Lab); LLM monitor logic by Álvaro Valencia.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <memory>
#include <fstream>

#include "plansys2_monitor/MonitorNode.hpp"
#include "lifecycle_msgs/msg/state.hpp"

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace plansys2
{

MonitorNode::MonitorNode()
: rclcpp_lifecycle::LifecycleNode("monitor"),
  plugin_loader_("plansys2_monitor", "plansys2::MonitorBase"),
  default_ids_{"LLAMA"},
  default_types_{"plansys2/LLAMAMonitor"},
  propose_timeout_(150s)
{
  declare_parameter("monitor_plugins", default_ids_);
  double timeout = propose_timeout_.seconds();
  declare_parameter("propose_timeout", timeout);
  declare_parameter("trunc_file", true);
}

MonitorNode::~MonitorNode()
{
  proposers_.clear();

  std::vector<std::string> loaded_libraries = plugin_loader_.getRegisteredLibraries();

  for (const auto & library : loaded_libraries) {
    try {
      plugin_loader_.unloadLibraryForClass(library);
      RCLCPP_DEBUG(get_logger(), "Unloaded library: %s", library.c_str());
    } catch (const pluginlib::LibraryUnloadException & e) {
      RCLCPP_WARN(get_logger(), "Failed to unload library %s: %s", library.c_str(), e.what());
    }
  }
}

using CallbackReturnT =
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

CallbackReturnT
MonitorNode::on_configure(const rclcpp_lifecycle::State & state)
{
  (void) state;
  auto node = shared_from_this();
  double timeout;

  RCLCPP_INFO(this->get_logger(), "[%s] Configuring...", get_name());

  get_parameter("monitor_plugins", workers_ids_);
  get_parameter("propose_timeout", timeout);
  get_parameter("trunc_file", trunc_file_);

  std::string nm = get_namespace();
  if (nm != "/") {
    std::filesystem::create_directories("/tmp" + nm);
    action_file_path_ = "/tmp" + nm + "/monitor_action_hub.txt";
  } else {
    nm = "";
    action_file_path_ = "/tmp/monitor_action_hub.txt";
  }

  std::ofstream action_file_(action_file_path_, std::ios::out);
  if (!action_file_) {
    RCLCPP_ERROR(this->get_logger(), "[%s] Error can't open the .txt", get_name());
    return CallbackReturnT::FAILURE;
  }
  action_file_.close();


  propose_timeout_ = rclcpp::Duration((int32_t)timeout, 0);

  if (!workers_ids_.empty()) {
    if (workers_ids_ == default_ids_) {
      for (size_t i = 0; i < default_ids_.size(); ++i) {
        plansys2::declare_parameter_if_not_declared(
        node, default_ids_[i] + ".plugin",
        rclcpp::ParameterValue(default_types_[i]));
      }
    }
    worker_types_.resize(workers_ids_.size());

    for (size_t i = 0; i != worker_types_.size(); i++) {
      try {
        worker_types_[i] = plansys2::get_plugin_type_param(node, workers_ids_[i]);
        plansys2::MonitorBase::Ptr monitor =
        plugin_loader_.createUniqueInstance(worker_types_[i]);

        monitor->configure(node, workers_ids_[i]);

        RCLCPP_INFO(
        this->get_logger(), "Created monitor : %s of type %s",
        workers_ids_[i].c_str(), worker_types_[i].c_str());
        proposers_.insert({workers_ids_[i], monitor});
      } catch (const pluginlib::PluginlibException & ex) {
        RCLCPP_FATAL(this->get_logger(), "Failed to create monitor. Exception: %s", ex.what());
        exit(-1);
      }
    }
  } else {
    RCLCPP_ERROR(this->get_logger(), "[%s] Error not plugin set", get_name());
    return CallbackReturnT::FAILURE;
  }

  RCLCPP_INFO(this->get_logger(), "[%s] Monitor Timeout %g", get_name(), propose_timeout_.seconds());

  get_proposal_service_ = create_service<plansys2_monitor_msgs::srv::GetProposal>(
    "monitor/get_proposal",
    std::bind(
      &MonitorNode::get_proposal_service_callback,
      this, std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3));

  action_subs_ = this->create_subscription<plansys2_msgs::msg::ActionExecution>(
    nm + "/actions_hub", rclcpp::SensorDataQoS().reliable(),
    std::bind(&MonitorNode::action_hub_callback, this, _1));

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
MonitorNode::on_activate(const rclcpp_lifecycle::State & state)
{
  (void) state;
  RCLCPP_INFO(this->get_logger(), "[%s] Activating...", get_name());
  RCLCPP_INFO(this->get_logger(), "[%s] Activated", get_name());
  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
MonitorNode::on_deactivate(const rclcpp_lifecycle::State & state)
{
  (void) state;
  RCLCPP_INFO(this->get_logger(), "[%s] Deactivating...", get_name());
  RCLCPP_INFO(this->get_logger(), "[%s] Deactivated", get_name());

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
MonitorNode::on_cleanup(const rclcpp_lifecycle::State & state)
{
  (void) state;
  RCLCPP_INFO(this->get_logger(), "[%s] Cleaning up...", get_name());
  RCLCPP_INFO(this->get_logger(), "[%s] Cleaned up", get_name());

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
MonitorNode::on_shutdown(const rclcpp_lifecycle::State & state)
{
  (void) state;
  RCLCPP_INFO(this->get_logger(), "[%s] Shutting down...", get_name());
  RCLCPP_INFO(this->get_logger(), "[%s] Shut down", get_name());

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
MonitorNode::on_error(const rclcpp_lifecycle::State & state)
{
  (void) state;
  RCLCPP_ERROR(this->get_logger(), "[%s] Error transition", get_name());

  return CallbackReturnT::SUCCESS;
}

void MonitorNode::action_hub_callback(plansys2_msgs::msg::ActionExecution::UniquePtr msg)
{
  // Dedup vs previous msg: completion must advance ≥0.30 — suppresses FEEDBACK flood.
  if (previous_action_msg_ != nullptr &&
      msg->action == previous_action_msg_->action &&
      msg->type == previous_action_msg_->type &&
      msg->status == previous_action_msg_->status &&
      msg->success == previous_action_msg_->success &&
      std::abs(msg->completion - previous_action_msg_->completion) < 0.30f)
  {
    return;
  }

  std::ofstream action_file_(action_file_path_, std::ios::app);
  if (action_file_.is_open()) {
    action_file_ << "-----------------------------\n";
    action_file_ << "Node ID: " << msg->node_id << "\n";
    action_file_ << "Type: " << msg->type << "\n";

    action_file_ << "Action: " << msg->action << "\n";
    action_file_ << "Arguments:\n";
    for (const auto &arg : msg->arguments) {
      action_file_ << "  - " << arg << "\n";
    }

    action_file_ << "Success: " << (msg->success ? "true" : "false") << "\n";
    action_file_ << "Completion: " << msg->completion << "\n";
    action_file_ << "Status: " << msg->status << "\n";

    action_file_ << "-----------------------------\n\n";
  } else {
    RCLCPP_WARN(this->get_logger(), "Failed to open action hub log file");
  }

  previous_action_msg_ = std::move(msg);
}

void
MonitorNode::get_proposal_service_callback(
  const std::shared_ptr<rmw_request_id_t> request_header,
  const std::shared_ptr<plansys2_monitor_msgs::srv::GetProposal::Request> request,
  const std::shared_ptr<plansys2_monitor_msgs::srv::GetProposal::Response> response)
{
  (void) request_header;
  std::string action_file = read_file(action_file_path_);

  if (trunc_file_) {
    std::ofstream trunc_stream(action_file_path_, std::ios::out);
    if (!trunc_stream) {
      RCLCPP_WARN(this->get_logger(), "[%s] Could not truncate action-hub log", get_name());
    }
  }

  auto proposals = get_proposal_array(request->domain, request->problem, request->observation, action_file);

  if (!proposals.proposal_array.empty()) {
    const auto & first = proposals.proposal_array.front();
    response->proposal = first;
    if (first.classification == plansys2_monitor_msgs::msg::Proposal::ERROR) {
      response->status = plansys2_monitor_msgs::srv::GetProposal::Response::ERROR;
      response->error_info = "Monitor returned ERROR classification (LLM output unparseable)";
    } else {
      response->status = plansys2_monitor_msgs::srv::GetProposal::Response::SUCCESS;
    }
  } else {
    response->status = plansys2_monitor_msgs::srv::GetProposal::Response::ERROR;
    response->error_info = "Resolution not found";
  }
}

plansys2_monitor_msgs::msg::ProposalArray
MonitorNode::get_proposal_array(const std::string & domain, const std::string & problem, const std::string & observation, const std::string & action_file)
{
  std::map<std::string, std::future<std::optional<plansys2_monitor_msgs::msg::Proposal>>> futures;
  std::map<std::string, std::optional<plansys2_monitor_msgs::msg::Proposal>> results;

  for (auto & [monitor_id, monitor] : proposers_) {
    futures[monitor_id] = std::async(std::launch::async,
      &plansys2::MonitorBase::propose, monitor,
      domain, problem, observation, action_file, get_namespace(), propose_timeout_);
  }

  auto start = now();

  size_t pending_result = proposers_.size();
  while (pending_result > 0 && now() - start < propose_timeout_) {
    for (auto & fut : futures) {
      if (results.find(fut.first) == results.end()) {
        if (fut.second.wait_for(1ms) == std::future_status::ready) {
          results[fut.first] = fut.second.get();
          pending_result--;
        }
      }
    }
  }

  for (auto & [monitor_id, monitor] : proposers_) {
    if (results.find(monitor_id) == results.end()) {
      monitor->cancel();
    }
  }
  // 100 ms cushion for LLAMAMonitor::propose poll loop to observe cancel before we join futures.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  for (auto & fut : futures) {
    if (results.find(fut.first) == results.end()) {
      try {
        fut.second.get();
      } catch (const std::exception & e) {
        RCLCPP_WARN_STREAM(
          get_logger(), "Exception while destroying future for "
            << fut.first << ": " << e.what());
      }
    }
  }

  plansys2_monitor_msgs::msg::ProposalArray proposals;
  for (auto & result : results) {
    if (result.second.has_value()) {
      proposals.proposal_array.push_back(result.second.value());
    }
  }

  return proposals;
}

}
