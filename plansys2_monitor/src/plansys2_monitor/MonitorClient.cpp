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

#include "plansys2_monitor/MonitorClient.hpp"

namespace plansys2
{

MonitorClient::MonitorClient()
{
  node_ = rclcpp::Node::make_shared("monitor_client");

  get_proposal_client_ = node_->create_client<plansys2_monitor_msgs::srv::GetProposal>("monitor/get_proposal");

  double timeout = 150;
  node_->declare_parameter("propose_timeout", timeout);

  node_->get_parameter("propose_timeout", timeout);
  propose_timeout_ = rclcpp::Duration((int32_t)timeout, 0);
  RCLCPP_INFO(
    node_->get_logger(), "Monitor Client created with timeout %g",
    propose_timeout_.seconds());
}

std::optional<plansys2_monitor_msgs::msg::Proposal>
MonitorClient::getProposal(
  const std::string & domain, const std::string & problem,
  const std::string & observation,
  const std::string & node_namespace)
{
  (void)node_namespace;
  while (!get_proposal_client_->wait_for_service(std::chrono::seconds(30))) {
    if (!rclcpp::ok()) {
      return {};
    }
    RCLCPP_ERROR_STREAM(
      node_->get_logger(),
      get_proposal_client_->get_service_name() <<
        " service  client: waiting for service to appear...");
  }
  int32_t timeout = propose_timeout_.seconds();
  if (timeout <= 0) {
    RCLCPP_WARN(node_->get_logger(), "Monitor timeout was %d, falling back to 150s", timeout);
    timeout = 150;
  }

  RCLCPP_DEBUG(node_->get_logger(), "Get Monitor service call with time out %d", timeout);

  auto request = std::make_shared<plansys2_monitor_msgs::srv::GetProposal::Request>();
  request->domain = domain;
  request->problem = problem;
  request->observation = observation;

  auto future_result = get_proposal_client_->async_send_request(request);

  auto outresult = rclcpp::spin_until_future_complete(
    node_, future_result,
    std::chrono::seconds(timeout));
  if (outresult != rclcpp::FutureReturnCode::SUCCESS) {
    if (outresult == rclcpp::FutureReturnCode::TIMEOUT) {
      RCLCPP_ERROR(node_->get_logger(), "Get Monitor service call timed out");
    } else {
      RCLCPP_ERROR(node_->get_logger(), "Get Monitor service call failed");
    }
    return {};
  }

  auto result = *future_result.get();

  if (result.status == plansys2_monitor_msgs::srv::GetProposal::Response::SUCCESS) {
    // Defensive: MonitorNode should have already mapped ERROR classification to
    // response->status = ERROR, but double-check here so the client surface is
    // uniform — any ERROR means the downstream sees std::nullopt.
    if (result.proposal.classification == plansys2_monitor_msgs::msg::Proposal::ERROR) {
      RCLCPP_ERROR(
        node_->get_logger(),
        "Monitor returned ERROR classification (LLM output unparseable)");
      return {};
    }
    return result.proposal;
  } else {
    RCLCPP_ERROR_STREAM(
      node_->get_logger(),
      get_proposal_client_->get_service_name() << ": " <<
        result.error_info);
    return {};
  }
}

std::optional<plansys2_monitor_msgs::msg::ProposalArray>
plansys2::MonitorClient::getProposalArray(
    const std::string & domain,
    const std::string & problem,
    const std::string & observation,
    const std::string & node_namespace)
{
    (void) domain;
    (void) problem;
    (void) observation;
    (void) node_namespace;
    return {};
}

}  // namespace plansys2
