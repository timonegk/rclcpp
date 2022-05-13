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

#ifndef RCLCPP__EXPERIMENTAL__BUFFERS__SIMPLE_EVENTS_QUEUE_HPP_
#define RCLCPP__EXPERIMENTAL__BUFFERS__SIMPLE_EVENTS_QUEUE_HPP_

#include <queue>
#include <utility>

#include "rclcpp/experimental/buffers/events_queue.hpp"

namespace rclcpp
{
namespace experimental
{
namespace buffers
{

/**
 * @brief This class implements an EventsQueue as a simple wrapper around a std::queue.
 * It does not perform any checks about the size of queue, which can grow
 * unbounded without being pruned.
 * The simplicity of this implementation makes it suitable for optimizing CPU usage.
 */
class SimpleEventsQueue : public EventsQueue
{
public:
  RCLCPP_PUBLIC
  ~SimpleEventsQueue() = default;

  /**
   * @brief push event into the queue
   * @param event The event to push into the queue
   */
  RCLCPP_PUBLIC
  virtual
  void
  push(const rclcpp::executors::ExecutorEvent & event)
  {
    event_queue_.push(event);
  }

  /**
   * @brief removes front event from the queue.
   */
  RCLCPP_PUBLIC
  virtual
  void
  pop()
  {
    event_queue_.pop();
  }

  /**
   * @brief gets the front event from the queue
   * @return the front event
   */
  RCLCPP_PUBLIC
  virtual
  rclcpp::executors::ExecutorEvent
  front() const
  {
    return event_queue_.front();
  }

  /**
   * @brief Test whether queue is empty
   * @return true if the queue's size is 0, false otherwise.
   */
  RCLCPP_PUBLIC
  virtual
  bool
  empty() const
  {
    return event_queue_.empty();
  }

  /**
   * @brief Returns the number of elements in the queue.
   * @return the number of elements in the queue.
   */
  RCLCPP_PUBLIC
  virtual
  size_t
  size() const
  {
    return event_queue_.size();
  }

  /**
   * @brief Initializes the queue
   */
  RCLCPP_PUBLIC
  virtual
  void
  init()
  {
    // Make sure the queue is empty when we start
    std::queue<rclcpp::executors::ExecutorEvent> local_queue;
    std::swap(event_queue_, local_queue);
  }

  /**
   * @brief gets a queue with all events accumulated on it since
   * the last call. The member queue is empty when the call returns.
   * @return std::queue with events
   */
  RCLCPP_PUBLIC
  virtual
  std::queue<rclcpp::executors::ExecutorEvent>
  pop_all_events()
  {
    std::queue<rclcpp::executors::ExecutorEvent> local_queue;
    std::swap(event_queue_, local_queue);
    return local_queue;
  }

  /**
   * @brief gets a single entity event from the queue
   * and decrements the event counter.
   * @return a single event
   */
  RCLCPP_PUBLIC
  virtual
  rclcpp::executors::ExecutorEvent
  get_single_event()
  {
    rclcpp::executors::ExecutorEvent & front_event = event_queue_.front();

    if (front_event.num_events > 1) {
      // We have more than a single event for the entity.
      // Decrement the counter by one, keeping the event in the front.
      front_event.num_events--;
    } else {
      // We have a single event, pop it from queue.
      event_queue_.pop();
    }

    // Make sure we return a single event for the entity.
    rclcpp::executors::ExecutorEvent single_event = front_event;
    single_event.num_events = 1;

    return single_event;
  }

private:
  std::queue<rclcpp::executors::ExecutorEvent> event_queue_;
};

}  // namespace buffers
}  // namespace experimental
}  // namespace rclcpp


#endif  // RCLCPP__EXPERIMENTAL__BUFFERS__SIMPLE_EVENTS_QUEUE_HPP_
