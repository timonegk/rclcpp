// Copyright 2021 Open Source Robotics Foundation, Inc.
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

#ifndef RCLCPP__EXECUTORS__EVENTS_EXECUTOR_EVENT_TYPES_HPP_
#define RCLCPP__EXECUTORS__EVENTS_EXECUTOR_EVENT_TYPES_HPP_

namespace rclcpp
{
namespace executors
{

enum ExecutorEventType
{
  SUBSCRIPTION_EVENT,
  SERVICE_EVENT,
  CLIENT_EVENT,
  WAITABLE_EVENT
};

struct ExecutorEvent
{
  const void * exec_entity_id;
  int gen_entity_id;
  ExecutorEventType type;
  size_t num_events;
};

}  // namespace executors
}  // namespace rclcpp

#endif  // RCLCPP__EXECUTORS__EVENTS_EXECUTOR_EVENT_TYPES_HPP_
