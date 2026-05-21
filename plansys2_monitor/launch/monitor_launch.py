# Copyright 2026 Álvaro Valencia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    namespace = LaunchConfiguration('namespace')
    pkg_share = get_package_share_directory('plansys2_monitor')
    params_file = os.path.join(pkg_share, 'params', 'monitor_params.yaml')

    declare_namespace_cmd = DeclareLaunchArgument(
        'namespace',
        default_value='',
        description='Namespace')

    # Specify the actions
    monitor_cmd = Node(
        package='plansys2_monitor',
        executable='monitor_node',
        name='monitor',
        namespace=namespace,
        output='screen',
        parameters=[params_file])

    # Create the launch description and populate
    ld = LaunchDescription()

    ld.add_action(declare_namespace_cmd)

    # Declare the launch options
    ld.add_action(monitor_cmd)

    return ld
