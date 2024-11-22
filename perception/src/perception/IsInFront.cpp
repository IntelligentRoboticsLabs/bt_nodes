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

#include "perception/IsInFront.hpp"

namespace perception
{

using pl = perception_system::PerceptionListener;

IsInFront::IsInFront(const std::string & xml_tag_name, const BT::NodeConfiguration & conf)
: BT::ConditionNode(xml_tag_name, conf)
{
  std::string what;
  
  config().blackboard->get("node", node_);
  getInput("target", target_);
  getInput("conficende", confidence_);
  getInput("what", what);
  getInput("entity_to_identify", entity_);

  if (what == "person") {
    node_->add_activation("perception_system/perception_people_detection");
  } else if (what == "object") {
    node_->add_activation("perception_system/perception_object_detection");
  } else {
    RCLCPP_ERROR(node_->get_logger(), "Unknown what: %s. Activating generic", what.c_str());
    node_->add_activation("perception_system/perception_object_detection");
  }
}
BT::NodeStatus IsInFront::tick()
{
  RCLCPP_DEBUG(node_->get_logger(), "IsInFront ticked");

  std::vector<perception_system_interfaces::msg::Detection> detections;
  perception_system_interfaces::msg::Detection detection;
  
  config().blackboard->get(target_, detection);

  rclcpp::spin_some(node_->get_node_base_interface());
  
  detections = pl::getInstance(node_)->get_by_features(detection, confidence_);

  if (detections.empty()) {
    RCLCPP_ERROR(node_->get_logger(), "No detections found");
    setOutput("direction", -1); // If no detections, just turn right bu default
    return BT::NodeStatus::FAILURE;
  }

  detection = detections[0];

  // double dx = detection.center3d.position.x;
  // double dy = detection.center3d.position.y;
  // double yaw = atan2(dx, -dy);
  double yaw = atan2(detection.center3d.position.y, detection.center3d.position.x);
  yaw = yaw * 180.0 / M_PI;

  if (std::abs((yaw)) > 5.0) { // If the angle is greater than 5 degrees, the detection is not in front
    if (yaw > 0) {
      setOutput("direction", 1);
    } else {
      setOutput("direction", -1);
    }
    return BT::NodeStatus::FAILURE;
  }

  // The detection is in front
  // Publish the detection
  pl::getInstance(node_)->publishTF(detection, entity_);
  setOutput("direction", 0);
  return BT::NodeStatus::SUCCESS;
  

}

}  // namespace perception

BT_REGISTER_NODES(factory) {
  factory.registerNodeType<perception::IsInFront>("IsInFront");
}
