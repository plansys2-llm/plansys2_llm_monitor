cmake_minimum_required(VERSION 3.5)
project(plansys2_monitor_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Proposal.msg"
  "msg/ProposalArray.msg"
  "srv/GetProposal.srv"
)

ament_export_dependencies(rosidl_default_runtime)

ament_package()
