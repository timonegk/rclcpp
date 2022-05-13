// Copyright 2020 Open Source Robotics Foundation, Inc.
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

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "rclcpp/exceptions/exceptions.hpp"
#include "rclcpp/executors/events_executor.hpp"

using namespace std::chrono_literals;

using rclcpp::executors::EventsExecutor;

EventsExecutor::EventsExecutor(
  rclcpp::experimental::buffers::EventsQueue::UniquePtr events_queue,
  const rclcpp::ExecutorOptions & options)
: rclcpp::Executor(options)
{
  // Get ownership of the queue used to store events.
  if (!events_queue) {
    throw std::invalid_argument("events_queue can't be a null pointer");
  }
  events_queue_ = std::move(events_queue);

  // Create timers manager
  timers_manager_ = std::make_shared<TimersManager>(context_);

  // Create entities collector
  entities_collector_ = std::make_shared<EventsExecutorEntitiesCollector>(this);
  entities_collector_->init();

  // Setup the executor notifier to wake up the executor when some guard conditions are tiggered.
  // The added guard conditions are guaranteed to not go out of scope before the executor itself.
  executor_notifier_ = std::make_shared<EventsExecutorNotifyWaitable>();
  executor_notifier_->add_guard_condition(shutdown_guard_condition_.get());
  executor_notifier_->add_guard_condition(&interrupt_guard_condition_);

  entities_collector_->add_waitable(executor_notifier_);
}

void
EventsExecutor::spin()
{
  if (spinning.exchange(true)) {
    throw std::runtime_error("spin() called while already spinning");
  }
  RCPPUTILS_SCOPE_EXIT(this->spinning.store(false););

  timers_manager_->start();

  while (rclcpp::ok(context_) && spinning.load()) {
    // Wait until we get an event
    ExecutorEvent event;
    bool has_event = events_queue_->dequeue(event);
    if (has_event) {
      this->execute_event(event);
    }
  }

  // Stop the timers manager thread when we are done spinning
  timers_manager_->stop();
}

void
EventsExecutor::spin_some(std::chrono::nanoseconds max_duration)
{
  // In this context a 0 input max_duration means no duration limit
  if (std::chrono::nanoseconds(0) == max_duration) {
    max_duration = timers_manager_->MAX_TIME;
  }

  return this->spin_some_impl(max_duration, false);
}

void
EventsExecutor::spin_all(std::chrono::nanoseconds max_duration)
{
  if (max_duration <= 0ns) {
    throw std::invalid_argument("max_duration must be positive");
  }
  return this->spin_some_impl(max_duration, true);
}

void
EventsExecutor::spin_some_impl(std::chrono::nanoseconds max_duration, bool exhaustive)
{
  if (spinning.exchange(true)) {
    throw std::runtime_error("spin_some() called while already spinning");
  }

  RCPPUTILS_SCOPE_EXIT(this->spinning.store(false););

  auto start = std::chrono::steady_clock::now();

  auto max_duration_not_elapsed = [max_duration, start]() {
      auto elapsed_time = std::chrono::steady_clock::now() - start;
      return elapsed_time < max_duration;
    };

  size_t ready_events_at_start = 0;
  size_t executed_events = 0;

  if (!exhaustive) {
    // Get the number of events ready at start
    ready_events_at_start = events_queue_->size();
  }

  while (rclcpp::ok(context_) && spinning.load() && max_duration_not_elapsed()) {
    // Execute first ready event from queue if exists
    if (exhaustive || (executed_events < ready_events_at_start)) {
      bool has_event = !events_queue_->empty();

      if (has_event) {
        ExecutorEvent event;
        bool ret = events_queue_->dequeue(event);
        if (ret) {
            this->execute_event(event);
            executed_events++;
        }
        continue;
      }
    }

    bool timer_executed;

    if (exhaustive) {
      // Execute timer if is ready
      timer_executed = timers_manager_->execute_head_timer();
    } else {
      // Execute timer if was ready at start
      timer_executed = timers_manager_->execute_head_timer(start);
    }

    if (timer_executed) {
      continue;
    }

    // If there's no more work available, exit
    break;
  }
}

void
EventsExecutor::spin_once_impl(std::chrono::nanoseconds timeout)
{
  // In this context a negative input timeout means no timeout
  if (timeout < 0ns) {
    timeout = timers_manager_->MAX_TIME;
  }

  // Select the smallest between input timeout and timer timeout
  auto next_timer_timeout = timers_manager_->get_head_timeout();
  if (next_timer_timeout < timeout) {
    timeout = next_timer_timeout;
  }

  ExecutorEvent event;
  bool has_event = events_queue_->dequeue(event, timeout);

  // If we wake up from the wait with an event, it means that it
  // arrived before any of the timers expired.
  if (has_event) {
    this->execute_event(event);
  } else {
    timers_manager_->execute_head_timer();
  }
}

void
EventsExecutor::add_node(
  rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr, bool notify)
{
  // This field is unused because we don't have to wake up the executor when a node is added.
  (void) notify;

  // Add node to entities collector
  entities_collector_->add_node(node_ptr);
}

void
EventsExecutor::add_node(std::shared_ptr<rclcpp::Node> node_ptr, bool notify)
{
  this->add_node(node_ptr->get_node_base_interface(), notify);
}

void
EventsExecutor::remove_node(
  rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr, bool notify)
{
  // This field is unused because we don't have to wake up the executor when a node is removed.
  (void)notify;

  // Remove node from entities collector.
  // This will result in un-setting all the event callbacks from its entities.
  // After this function returns, this executor will not receive any more events associated
  // to these entities.
  entities_collector_->remove_node(node_ptr);
}

void
EventsExecutor::remove_node(std::shared_ptr<rclcpp::Node> node_ptr, bool notify)
{
  this->remove_node(node_ptr->get_node_base_interface(), notify);
}

void
EventsExecutor::execute_event(const ExecutorEvent & event)
{
  switch (event.type) {
    case SUBSCRIPTION_EVENT:
      {
        auto subscription = entities_collector_->get_subscription(event.exec_entity_id);

        if (subscription) {
          for (size_t i = 0; i < event.num_events; i++) {
            execute_subscription(subscription);
          }
        }
        break;
      }

    case SERVICE_EVENT:
      {
        auto service = entities_collector_->get_service(event.exec_entity_id);

        if (service) {
          for (size_t i = 0; i < event.num_events; i++) {
            execute_service(service);
          }
        }
        break;
      }

    case CLIENT_EVENT:
      {
        auto client = entities_collector_->get_client(event.exec_entity_id);

        if (client) {
          for (size_t i = 0; i < event.num_events; i++) {
            execute_client(client);
          }
        }
        break;
      }

    case WAITABLE_EVENT:
      {
        auto waitable = entities_collector_->get_waitable(event.exec_entity_id);

        if (waitable) {
          for (size_t i = 0; i < event.num_events; i++) {
            auto data = waitable->take_data();
            waitable->execute(data);
          }
        }
        break;
      }
  }
}

void
EventsExecutor::add_callback_group(
  rclcpp::CallbackGroup::SharedPtr group_ptr,
  rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr,
  bool notify)
{
  // This field is unused because we don't have to wake up
  // the executor when a callback group is added.
  (void)notify;
  entities_collector_->add_callback_group(group_ptr, node_ptr);
}

void
EventsExecutor::remove_callback_group(
  rclcpp::CallbackGroup::SharedPtr group_ptr, bool notify)
{
  // This field is unused because we don't have to wake up
  // the executor when a callback group is removed.
  (void)notify;
  entities_collector_->remove_callback_group(group_ptr);
}

std::vector<rclcpp::CallbackGroup::WeakPtr>
EventsExecutor::get_all_callback_groups()
{
  return entities_collector_->get_all_callback_groups();
}

std::vector<rclcpp::CallbackGroup::WeakPtr>
EventsExecutor::get_manually_added_callback_groups()
{
  return entities_collector_->get_manually_added_callback_groups();
}

std::vector<rclcpp::CallbackGroup::WeakPtr>
EventsExecutor::get_automatically_added_callback_groups_from_nodes()
{
  return entities_collector_->get_automatically_added_callback_groups_from_nodes();
}
