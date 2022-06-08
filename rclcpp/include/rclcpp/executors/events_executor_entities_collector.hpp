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

#ifndef RCLCPP__EXECUTORS__EVENTS_EXECUTOR_ENTITIES_COLLECTOR_HPP_
#define RCLCPP__EXECUTORS__EVENTS_EXECUTOR_ENTITIES_COLLECTOR_HPP_

#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "rclcpp/executors/event_waitable.hpp"
#include "rclcpp/executors/events_executor_event_types.hpp"
#include "rclcpp/executors/timers_manager.hpp"
#include "rclcpp/node_interfaces/node_base_interface.hpp"

namespace rclcpp
{
namespace executors
{
typedef std::map<rclcpp::CallbackGroup::WeakPtr,
    rclcpp::node_interfaces::NodeBaseInterface::WeakPtr,
    std::owner_less<rclcpp::CallbackGroup::WeakPtr>> WeakCallbackGroupsToNodesMap;

// forward declaration of EventsExecutor to avoid circular dependency
class EventsExecutor;

/**
 * @brief This class provides a waitable object that is used for managing the
 * entities (i.e. nodes and their subscriptions, timers, services, etc)
 * added to an EventsExecutor.
 * The add/remove node APIs are used when a node is added/removed from
 * the associated EventsExecutor and result in setting/unsetting the
 * events callbacks and adding timers to the timers manager.
 *
 * Being this class derived from Waitable, it can be used to wake up an
 * executor thread while it's spinning.
 * When this occurs, the execute API takes care of handling changes
 * in the entities currently used by the executor.
 */
class EventsExecutorEntitiesCollector final
  : public EventWaitable,
  public std::enable_shared_from_this<EventsExecutorEntitiesCollector>
{
public:
  RCLCPP_SMART_PTR_DEFINITIONS(EventsExecutorEntitiesCollector)

  // Constructor
  RCLCPP_PUBLIC
  EventsExecutorEntitiesCollector(
    EventsExecutor * executor);

  // Destructor
  RCLCPP_PUBLIC
  ~EventsExecutorEntitiesCollector();

  // Initialize entities collector
  RCLCPP_PUBLIC
  void init();

  // The purpose of "execute" is handling the situation of a new entity added to
  // a node, while the executor is already spinning.
  // This consists in setting that entitiy's callback.
  // If a entity is removed from a node, we should unset its callback
  RCLCPP_PUBLIC
  void
  execute(std::shared_ptr<void> & data) override;

  RCLCPP_PUBLIC
  void
  add_node(
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr);

  RCLCPP_PUBLIC
  void
  remove_node(
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr);

  /// Add a callback group to the entities collector
  /**
   * \see rclcpp::Executor::add_callback_group
   */
  RCLCPP_PUBLIC
  void
  add_callback_group(
    rclcpp::CallbackGroup::SharedPtr group_ptr,
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr);

  /// Add a callback group to the entities collector
  /**
   * \see rclcpp::Executor::add_callback_group
   * \return boolean whether the node from the callback group is new
   */
  RCLCPP_PUBLIC
  void
  add_callback_group(
    rclcpp::CallbackGroup::SharedPtr group_ptr,
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr,
    WeakCallbackGroupsToNodesMap & weak_groups_to_nodes);

  /// Remove a callback group from the entities collector
  /**
   * \see rclcpp::Executor::remove_callback_group
   */
  RCLCPP_PUBLIC
  void
  remove_callback_group(
    rclcpp::CallbackGroup::SharedPtr group_ptr);

  /// Remove a callback group from the entities collector
  /**
   * \see rclcpp::Executor::remove_callback_group_from_map
   */
  RCLCPP_PUBLIC
  void
  remove_callback_group_from_map(
    rclcpp::CallbackGroup::SharedPtr group_ptr,
    WeakCallbackGroupsToNodesMap & weak_groups_to_nodes);

  RCLCPP_PUBLIC
  std::vector<rclcpp::CallbackGroup::WeakPtr>
  get_all_callback_groups();

  /// Get manually added callback groups that belong to the entities collector
  /**
   * \see rclcpp::Executor::get_manually_added_callback_groups()
   */
  RCLCPP_PUBLIC
  std::vector<rclcpp::CallbackGroup::WeakPtr>
  get_manually_added_callback_groups();

  /// Get autmatically added callback groups that belong to the entities collector
  /**
   * \see rclcpp::Executor::get_automatically_added_callback_groups_from_nodes()
   */
  RCLCPP_PUBLIC
  std::vector<rclcpp::CallbackGroup::WeakPtr>
  get_automatically_added_callback_groups_from_nodes();

  ///
  /**
   * Get the subscription shared pointer corresponding
   * to a subscription identifier
   */
  RCLCPP_PUBLIC
  rclcpp::SubscriptionBase::SharedPtr
  get_subscription(const void * subscription_id);

  ///
  /**
   * Get the client shared pointer corresponding
   * to a client identifier
   */
  RCLCPP_PUBLIC
  rclcpp::ClientBase::SharedPtr
  get_client(const void * client_id);

  ///
  /**
   * Get the service shared pointer corresponding
   * to a service identifier
   */
  RCLCPP_PUBLIC
  rclcpp::ServiceBase::SharedPtr
  get_service(const void * service_id);

  ///
  /**
   * Get the waitable shared pointer corresponding
   * to a waitable identifier
   */
  RCLCPP_PUBLIC
  rclcpp::Waitable::SharedPtr
  get_waitable(const void * waitable_id);

  ///
  /**
   * Add a weak pointer to a waitable
   */
  RCLCPP_PUBLIC
  void
  add_waitable(rclcpp::Waitable::SharedPtr waitable);

  RCLCPP_PUBLIC
  std::shared_ptr<void>
  take_data()
  {
    // This waitable doesn't handle any data
    return nullptr;
  }

private:
  void
  set_callback_group_entities_callbacks(rclcpp::CallbackGroup::SharedPtr group);

  void
  unset_callback_group_entities_callbacks(rclcpp::CallbackGroup::SharedPtr group);

  void
  set_guard_condition_callback(rclcpp::GuardCondition * guard_condition);

  void
  unset_guard_condition_callback(rclcpp::GuardCondition * guard_condition);

  std::function<void(size_t)>
  create_entity_callback(void * exec_entity_id, ExecutorEventType type);

  std::function<void(size_t, int)>
  create_waitable_callback(void * waitable_id);

  /// Return true if the node belongs to the collector
  /**
   * \param[in] group_ptr a node base interface shared pointer
   * \return boolean whether a node belongs the collector
   */
  bool
  has_node(
    const rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr,
    const WeakCallbackGroupsToNodesMap & weak_groups_to_nodes) const;

  /// Add all callback groups that can be automatically added by any executor
  /// and is not already associated with an executor from nodes
  /// that are associated with executor
  /**
   * \see rclcpp::Executor::add_callback_groups_from_nodes_associated_to_executor()
   */
  void
  add_callback_groups_from_nodes_associated_to_executor();

  void
  set_entities_event_callbacks_from_map(
    const WeakCallbackGroupsToNodesMap & weak_groups_to_nodes);

  // maps callback groups to nodes.
  WeakCallbackGroupsToNodesMap weak_groups_associated_with_executor_to_nodes_;
  // maps callback groups to nodes.
  WeakCallbackGroupsToNodesMap weak_groups_to_nodes_associated_with_executor_;

  typedef std::map<rclcpp::node_interfaces::NodeBaseInterface::WeakPtr,
      rclcpp::GuardCondition *,
      std::owner_less<rclcpp::node_interfaces::NodeBaseInterface::WeakPtr>>
    WeakNodesToGuardConditionsMap;
  WeakNodesToGuardConditionsMap weak_nodes_to_guard_conditions_;

  /// List of weak nodes registered in the events executor
  std::list<rclcpp::node_interfaces::NodeBaseInterface::WeakPtr> weak_nodes_;

  // Maps: entity identifiers to weak pointers from the entities registered in the executor
  // so in the case of an event providing and ID, we can retrieve and own the corresponding
  // entity while it performs work
  std::unordered_map<const void *, rclcpp::SubscriptionBase::WeakPtr> weak_subscriptions_map_;
  std::unordered_map<const void *, rclcpp::ServiceBase::WeakPtr> weak_services_map_;
  std::unordered_map<const void *, rclcpp::ClientBase::WeakPtr> weak_clients_map_;
  std::unordered_map<const void *, rclcpp::Waitable::WeakPtr> weak_waitables_map_;

  /// Executor using this entities collector object
  EventsExecutor * associated_executor_ = nullptr;
  /// Instance of the timers manager used by the associated executor
  TimersManager::SharedPtr timers_manager_;
};

}  // namespace executors
}  // namespace rclcpp

#endif  // RCLCPP__EXECUTORS__EVENTS_EXECUTOR_ENTITIES_COLLECTOR_HPP_
