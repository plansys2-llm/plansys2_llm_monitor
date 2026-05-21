// Copyright 2026 Álvaro Valencia
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

#ifndef PLANSYS2_MONITOR__MONITORCLIENT_HPP_
#define PLANSYS2_MONITOR__MONITORCLIENT_HPP_

#include <optional>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>

#include "plansys2_monitor/MonitorInterface.hpp"

#include "plansys2_monitor_msgs/srv/get_proposal.hpp"

#include "rclcpp/rclcpp.hpp"

namespace plansys2
{
class MonitorClient : public MonitorInterface
{
public:
    MonitorClient();

    std::optional<plansys2_monitor_msgs::msg::Proposal> getProposal(
        const std::string & domain, const std::string & problem,
        const std::string & observation,
        const std::string & node_namespace = "") override;


    std::optional<plansys2_monitor_msgs::msg::ProposalArray> getProposalArray(
        const std::string & domain, const std::string & problem,
        const std::string & observation,
        const std::string & node_namespace = "") override;

private:
    rclcpp::Client<plansys2_monitor_msgs::srv::GetProposal>::SharedPtr
        get_proposal_client_;

    rclcpp::Node::SharedPtr node_;
    rclcpp::Duration propose_timeout_ = rclcpp::Duration(300, 0);
};

}

#endif  // PLANSYS2_MONITOR__MONITORCLIENT_HPP_