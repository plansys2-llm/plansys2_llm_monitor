cmake_minimum_required(VERSION 3.20)
project(plansys2_monitor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(plansys2_core REQUIRED)
find_package(plansys2_msgs REQUIRED)
find_package(plansys2_monitor_msgs REQUIRED)
find_package(plansys2_domain_expert REQUIRED)
find_package(plansys2_problem_expert REQUIRED)
find_package(pluginlib REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(nlohmann_json REQUIRED)

set(MONITOR_SOURCES
  src/plansys2_monitor/MonitorNode.cpp
  src/plansys2_monitor/MonitorClient.cpp
)
add_library(${PROJECT_NAME} SHARED ${MONITOR_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")
target_link_libraries(${PROJECT_NAME}
  PUBLIC
  ${lifecycle_msgs_TARGETS}
  ${plansys2_msgs_TARGETS}
  ${plansys2_monitor_msgs_TARGETS}
  pluginlib::pluginlib
  rclcpp::rclcpp
  rclcpp_lifecycle::rclcpp_lifecycle
  nlohmann_json::nlohmann_json
)

if(TARGET plansys2_core::plansys2_core)
  target_link_libraries(${PROJECT_NAME}
    PUBLIC
    plansys2_core::plansys2_core
    plansys2_domain_expert::plansys2_domain_expert
    plansys2_problem_expert::plansys2_problem_expert
  )
else()
  ament_target_dependencies(${PROJECT_NAME}
    PUBLIC
    plansys2_core
    plansys2_domain_expert
    plansys2_problem_expert
  )
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC "PLUGINLIB__DISABLE_BOOST_FUNCTIONS")

add_executable(monitor_node
  src/monitor_node.cpp
)
target_include_directories(monitor_node PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
)
target_compile_definitions(monitor_node PUBLIC "PLUGINLIB__DISABLE_BOOST_FUNCTIONS")
target_link_libraries(monitor_node
  PRIVATE
  ${PROJECT_NAME}
  rclcpp::rclcpp
)

add_executable(monitor_client
  src/monitor_client.cpp
)
target_include_directories(monitor_client PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
)
target_compile_definitions(monitor_client PUBLIC "PLUGINLIB__DISABLE_BOOST_FUNCTIONS")
target_link_libraries(monitor_client
  PRIVATE
  ${PROJECT_NAME}
  rclcpp::rclcpp
)

install(DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY params
  DESTINATION share/${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(TARGETS monitor_node monitor_client
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_index_cpp REQUIRED)
endif()

ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME})
ament_export_dependencies(
  rclcpp
  rclcpp_lifecycle
  plansys2_core
  ament_index_cpp
  plansys2_msgs
  plansys2_monitor_msgs
  plansys2_domain_expert
  plansys2_problem_expert
  pluginlib
  lifecycle_msgs
  nlohmann_json
)

ament_package()
