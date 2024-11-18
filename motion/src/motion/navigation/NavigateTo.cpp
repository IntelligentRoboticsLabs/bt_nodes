// Copyright 2024 Intelligent Robotics Lab
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

#include "motion/navigation/NavigateTo.hpp"

namespace navigation
{

NavigateTo::NavigateTo(
  const std::string & xml_tag_name, const std::string & action_name,
  const BT::NodeConfiguration & conf)
: motion::BtActionNode<
    nav2_msgs::action::NavigateToPose, rclcpp_cascade_lifecycle::CascadeLifecycleNode>(
    xml_tag_name, action_name, conf),
  tf_buffer_(),
  tf_listener_(tf_buffer_)
{

  callback_group_ = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  callback_executor_.add_callback_group(callback_group_, node_->get_node_base_interface());

  set_truncate_distance_client_ =
    node_->create_client<navigation_system_interfaces::srv::SetTruncateDistance>(
    "navigation_system_node/set_truncate_distance", rmw_qos_profile_services_default,
    callback_group_);
}

void NavigateTo::on_tick()
{
  RCLCPP_DEBUG(node_->get_logger(), "NavigateTo ticked");
  geometry_msgs::msg::PoseStamped goal;
  geometry_msgs::msg::TransformStamped map_to_goal;
  bool is_truncated;
  std::string tf_frame, xml_path;

  getInput("tf_frame", tf_frame);
  getInput("will_finish", will_finish_);
  getInput("is_truncated", is_truncated);

  if (tf_frame.length() > 0) { // There is a TF to go, ignore coordinates
    RCLCPP_INFO(node_->get_logger(), "Transforming %s to %s", "map", tf_frame.c_str());
    try {
      map_to_goal = tf_buffer_.lookupTransform("map", tf_frame, tf2::TimePointZero);
    } catch (const tf2::TransformException & ex) {
      RCLCPP_WARN(
        node_->get_logger(), "Could not transform %s to %s: %s", "map", tf_frame.c_str(), ex.what());
      setStatus(BT::NodeStatus::RUNNING);
    }
    
    goal.pose.position.x = map_to_goal.transform.translation.x;
    goal.pose.position.y = map_to_goal.transform.translation.y;
    goal.pose.orientation.x = map_to_goal.transform.rotation.x;
    goal.pose.orientation.y = map_to_goal.transform.rotation.y;
    goal.pose.orientation.z = map_to_goal.transform.rotation.z;
    goal.pose.orientation.w = map_to_goal.transform.rotation.w;

  } else { // No TF, use coordinates

    getInput("x", goal.pose.position.x);
    getInput("y", goal.pose.position.y);
    RCLCPP_INFO(node_->get_logger(), "Setting goal to x: %f, y: %f", goal.pose.position.x, goal.pose.position.y);
    
    goal.pose.orientation.w = 1.0;
    goal.pose.orientation.x = 0.0;
    goal.pose.orientation.y = 0.0;
    goal.pose.orientation.z = 0.0;
  }

  goal.header.frame_id = "map";

  if (!set_truncate_distance_client_->wait_for_service(std::chrono::seconds(1))) {
    RCLCPP_WARN(node_->get_logger(), "Waiting for action server to be up...");
    setStatus(BT::NodeStatus::RUNNING);
  }

  RCLCPP_INFO(
    node_->get_logger(), "Sending goal: x: %f, y: %f, qx: %f, qy: %f, qz: %f qw: %f. Frame: %s",
    goal.pose.position.x, goal.pose.position.y, goal.pose.orientation.x, goal.pose.orientation.y,
    goal.pose.orientation.z, goal.pose.orientation.w, goal.header.frame_id.c_str());

  if (is_truncated) {
    double distance_tolerance;
    getInput("distance_tolerance", distance_tolerance);
    xml_path = generate_xml_file(nav_to_pose_truncated_xml, distance_tolerance);
    goal_.behavior_tree = xml_path;
  }

  goal_.pose = goal;
}

BT::NodeStatus NavigateTo::on_success()
{
  RCLCPP_INFO(node_->get_logger(), "Navigation succeeded");
  if (will_finish_) {
    return BT::NodeStatus::SUCCESS;
  }
  goal_updated_ = true;
  on_tick();
  on_new_goal_received();
  return BT::NodeStatus::RUNNING;
}

BT::NodeStatus NavigateTo::on_aborted()
{
  if (will_finish_) {
    return BT::NodeStatus::FAILURE;
  }
  on_tick();
  on_new_goal_received();
  return BT::NodeStatus::RUNNING;
}

BT::NodeStatus NavigateTo::on_cancelled()
{
  if (will_finish_) {
    return BT::NodeStatus::SUCCESS;
  }
  on_tick();
  on_new_goal_received();
  RCLCPP_INFO(node_->get_logger(), "Navigation cancelled");
  return BT::NodeStatus::RUNNING;
}

}  // namespace navigation

#include "behaviortree_cpp_v3/bt_factory.h"

BT_REGISTER_NODES(factory)
{
  BT::NodeBuilder builder = [](const std::string & name, const BT::NodeConfiguration & config) {
      return std::make_unique<navigation::NavigateTo>(name, "/navigate_to_pose", config);
  };

  factory.registerBuilder<navigation::NavigateTo>("NavigateTo", builder);
}
