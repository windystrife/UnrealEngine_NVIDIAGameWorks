// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/event_manager.h
 *
 *
 *
 * @brief Entry points for Play Games Event functionality.
 */

#ifndef GPG_EVENT_MANAGER_H_
#define GPG_EVENT_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <map>
#include <memory>
#include <vector>

#include "gpg/common.h"
#include "gpg/event.h"
#include "gpg/game_services.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Gets and sets various event-related data.
 * @ingroup Managers
 */
class GPG_EXPORT EventManager {
 public:
  /**
   * <code>Data</code> and <code>ResponseStatus</code> for all events.
   *
   * @ingroup ResponseType
   */
  struct FetchAllResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>FetchAllResponse</code> returns
     * an empty data map.
     */
    ResponseStatus status;

    /**
     * A map containing data for all events associated with the
     * application. The keys are event ids.
     */
    std::map<std::string, Event> data;
  };

  /**
   * Defines a callback type that receives a <code>FetchAllResponse</code>.
   * This callback type is provided to the <code>FetchAll(*)</code> functions
   * below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchAllResponse const &)> FetchAllCallback;

  /**
   * Asynchronously loads all event data for the currently signed-in
   * player. Calls the provided <code>FetchAllCallback</code> on operation
   * completion. Not specifying <code>data_source</code> makes this function
   * call equivalent to calling
   * <code>FetchAll(DataSource data_source, FetchAllCallback)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>.
   */
  void FetchAll(FetchAllCallback callback);

  /**
   * Asynchronously loads all event data for the currently signed-in
   * player. Calls the provided <code>FetchAllCallback</code> on operation
   * completion. Specify data_source as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>.
   */
  void FetchAll(DataSource data_source, FetchAllCallback callback);

  /**
   * Synchronously loads all event data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>.
   * Specifying neither <code>data_source</code> nor <code>timeout</code> makes
   * this function call equivalent to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source,</code>
   * <code>Timeout timeout)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>,
   * and <code>timeout</code> specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking();

  /**
   * Synchronously loads all event data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specify
   * <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Not specifying timeout makes this function call
   * equivalent to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source</code>,
   * <code>Timeout timeout)</code>, with your specified <code>data_source</code>
   * value, and <code>timeout</code> specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source);

  /**
   * Synchronously loads all event data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specify
   * <code>timeout</code> as an
   * arbitrary number of milliseconds. Not specifying <code>data_source</code>
   * makes this function call equivalent to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source,</code>
   * <code>Timeout timeout)</code>, with <code>data_source</code> specified as
   * <code>CACHE_OR_NETWORK</code>, and <code>timeout</code> containing your
   * specified value.
   */
  FetchAllResponse FetchAllBlocking(Timeout timeout);

  /**
   * Synchronously loads all event data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify <code>timeout</code> as an arbitrary
   * number of milliseconds.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source, Timeout timeout);

  /**
   * Contains data and response status for a single event.
   * @ingroup ResponseType
   */
  struct FetchResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is not <code>VALID</code> or
     * <code>VALID_BUT_STALE</code>, <code>FetchResponse</code> returns an empty
     * data map.
     */
    ResponseStatus status;

    /**
     * All data for a specific event.
     */
    Event data;
  };

  /**
   * Defines a callback type that receives a <code>FetchResponse</code>. This
   * callback type is provided to the <code>Fetch(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(const FetchResponse &)> FetchCallback;

  /**
   * Asynchronously loads data for a specific event for the currently
   * signed-in player. Calls the provided <code>FetchCallback</code> on
   * operation completion. Not specifying <code>data_source</code> makes this
   * function call equivalent to calling
   * <code>Fetch(DataSource data_source, const std::string &event_id,</code>
   * <code>FetchCallback)</code>, with <code>data_source</code> specified as
   * <code>CACHE_OR_NETWORK</code>.
   */
  void Fetch(std::string const &event_id, FetchCallback callback);

  /**
   * Asynchronously loads data for a specific event for the currently
   * signed-in player Calls the provided <code>FetchCallback</code> on operation
   * completion. Specify <code>data_source</code> as
   * <code>CACHE_OR_NETWORK</code> or <code>NETWORK_ONLY</code>.
   */
  void Fetch(DataSource data_source,
             std::string const &event_id,
             FetchCallback callback);

  /**
   * Synchronously loads data for a specific event, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>.
   * Leaving <code>data_source</code> and <code>timeout</code> unspecified makes
   * this function call equivalent to calling
   * <code>FetchResponse FetchBlocking(DataSource data_source,</code>
   * <code>Timeout timeout, const std::string &event_id)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>,
   * and <code>timeout</code> specified as 10 years.
   */
  FetchResponse FetchBlocking(std::string const &event_id);

  /**
   * Synchronously loads data for a specific event, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Leaving <code>timeout</code> unspecified makes
   * this function call equivalent to calling
   * <code>FetchResponse FetchBlocking(DataSource data_source,</code>
   * <code>Timeout timeout, const std::string &event_id)</code>, with your
   * specified <code>data_source</code> value, and <code>timeout</code>
   * specified as 10 years.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              std::string const &event_id);

  /**
   * Synchronously loads data for a specific event, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   * Leaving <code>data_source</code> unspecified makes this function call
   * equivalent to calling
   * <code>FetchResponse FetchBlocking(DataSource data_source,</code>
   * <code>Timeout timeout, const std::string &event_id)</code>, with
   * <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>, and
   * <code>timeout</code> containing your specified value.
   */
  FetchResponse FetchBlocking(Timeout timeout, std::string const &event_id);

  /**
   * Synchronously loads data for a specific event, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>. Specify <code>DataSource</code> as
   * <code>CACHE_OR_NETWORK</code> or <code>NETWORK_ONLY</code>. Specify
   * <code>timeout</code> as an arbitrary number of milliseconds.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              Timeout timeout,
                              std::string const &event_id);

  /**
   * Increments an event by 1.
   */
  void Increment(std::string const &event_id);

  /**
   * Increments an event by the given number of steps.
   */
  void Increment(std::string const &event_id, uint32_t steps);

 private:
  friend class GameServicesImpl;
  explicit EventManager(GameServicesImpl *game_services_impl);
  ~EventManager();
  EventManager(EventManager const &) = delete;
  EventManager &operator=(EventManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_EVENT_MANAGER_H_
