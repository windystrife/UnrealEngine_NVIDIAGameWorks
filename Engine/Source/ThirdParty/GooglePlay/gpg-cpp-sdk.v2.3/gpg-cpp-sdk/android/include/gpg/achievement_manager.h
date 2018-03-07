// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/achievement_manager.h
 *
 * @brief Entry points for Play Games Achievement functionality.
 */

#ifndef GPG_ACHIEVEMENT_MANAGER_H_
#define GPG_ACHIEVEMENT_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <vector>
#include "gpg/achievement.h"
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Gets and sets various achievement-related data.
 * @ingroup Managers
 */
class GPG_EXPORT AchievementManager {
 public:
  /**
   * Holds all data for all achievements, along with a response status.
   * @ingroup ResponseType
   */
  struct FetchAllResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>FetchAllResponse</code>'s data
     * vector is empty.
     */
    ResponseStatus status;

    /**
     * A vector containing data for all achievements associated with the
     * application.
     */
    std::vector<Achievement> data;
  };

  /**
   * Defines a callback type that receives a <code>FetchAllResponse</code>.
   * This callback type is provided to the <code>FetchAll(*)</code> functions
   * below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchAllResponse const &)> FetchAllCallback;

  /**
   * Asynchronously loads all achievement data for the currently signed-in
   * player. Calls the provided <code>FetchAllCallback</code> upon operation
   * completion. Not specifying <code>data_source</code> makes this function
   * call equivalent to calling
   * <code>FetchAll(DataSource data_source, FetchAllCallback)</code>, with
   * <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>.
   */
  void FetchAll(FetchAllCallback callback);

  /**
   * Asynchronously loads all achievement data for the currently signed-in
   * player. Calls the provided <code>FetchAllCallback</code> upon operation
   * completion. Specify <code>data_source</code> as
   * <code>CACHE_OR_NETWORK</code> or <code>NETWORK_ONLY</code>.
   */
  void FetchAll(DataSource data_source, FetchAllCallback callback);

  /**
   * Synchronously loads all achievement data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specifying
   * neither <code>data_source</code> nor <code>timeout</code> makes this
   * function call equivalent to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with <code>data_source</code> specified as
   * <code>CACHE_OR_NETWORK</code>, and <code>timeout</code> specified as 10
   * years.
   */
  FetchAllResponse FetchAllBlocking();

  /**
   * Synchronously loads all achievement data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Not specifying <code>timeout</code> makes this
   * function call equivalent to calling
   * <code>FetchAllBlocking FetchAllResponse(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with your specified <code>data_source</code> value, and
   * <code>timeout</code> specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source);

  /**
   * Synchronously loads all achievement data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specify
   * <code>timeout</code> as an arbitrary number of milliseconds.
   * Not specifying <code>data_source</code> makes this function call equivalent
   * to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>,
   * and <code>timeout</code> containing your specified value.
   */
  FetchAllResponse FetchAllBlocking(Timeout timeout);

  /**
   * Synchronously loads all achievement data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specify
   * <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify timeout as an arbitrary number of
   * milliseconds.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source, Timeout timeout);

  /**
   * Contains data and response status for a single achievement.
   * @ingroup ResponseType
   */
  struct FetchResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>FetchResponse</code>'s
     * data vector will be empty.
     */
    ResponseStatus status;

    /**
     * All data for a specific achievement.
     */
    Achievement data;
  };

  /**
   * Defines a callback type that receives a <code>FetchResponse</code>.
   * This callback type is provided to the <code>Fetch(*)</code> functions
   * below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchResponse const &)> FetchCallback;

  /**
   * Asynchronously loads data for a specific achievement for the currently
   * signed-in player.
   * Calls the provided <code>FetchCallback</code> upon operation completion.
   * Not specifying <code>data_source</code> makes this function call
   * equivalent to calling
   * <code>Fetch(DataSource data_source, std::string const &id, </code>
   * <code>FetchCallback)</code>, with <code>data_source</code> specified as
   * <code>CACHE_OR_NETWORK</code>.
   */
  void Fetch(std::string const &achievement_id, FetchCallback callback);

  /**
   * Asynchronously loads data for a specific achievement for the currently
   * signed-in player. Calls the provided <code>FetchCallback</code> on
   * operation completion. Specify data_source as <code>CACHE_OR_NETWORK</code>
   * or <code>NETWORK_ONLY</code>.
   */
  void Fetch(DataSource data_source,
             std::string const &achievement_id,
             FetchCallback callback);

  /**
   * Synchronously loads data for a specific achievement, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>. Leaving <code>data_source</code> and
   * <code>timeout</code> unspecified makes this function call equivalent to
   * calling
   * <code>FetchResponse FetchBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, std::string const &id)</code>, with
   * <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>,
   * and <code>timeout</code> specified as 10 years.
   */
  FetchResponse FetchBlocking(std::string const &achievement_id);

  /**
   * Synchronously loads data for a specific achievement, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>. Specify <code>data_source</code> as
   * <code>CACHE_OR_NETWORK</code> or <code>NETWORK_ONLY</code>. Leaving
   * <code>timeout</code> unspecified makes this function call equivalent to
   * calling
   * <code>FetchResponse FetchBlocking(DataSource data_source, </code>
   * <code> Timeout timeout, std::string const &id)</code>, with your
   * specified <code>data_source</code> value, and <code>timeout</code>
   * specified as 10 years.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              std::string const &achievement_id);

  /**
   * Synchronously loads data for a specific achievement, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>. Specify <code>timeout</code> as an arbitrary
   * number of milliseconds. Leaving <code>data_source</code> unspecified makes
   * this function call equivalent to calling
   * <code>FetchResponse FetchBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, std::string const &id)</code>, with
   * <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>, and
   * <code>timeout</code> containing your specified value.
   */
  FetchResponse FetchBlocking(Timeout timeout,
                              std::string const &achievement_id);

  /**
   * Synchronously loads data for a specific achievement, identified by string
   * id, for the currently signed-in player; directly returns the
   * <code>FetchResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify <code>timeout</code> as an arbitrary
   * number of milliseconds.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              Timeout timeout,
                              std::string const &achievement_id);

  /**
   * Increments an achievement by the given number of steps. Leaving the
   * increment undefined causes its value to default to 1. The achievement
   * must be an incremental achievement. Once an achievement reaches the
   * maximum number of steps, it is unlocked automatically. Any further
   * increments are ignored.
   */
  void Increment(std::string const &achievement_id);

  /**
   * Increments an achievement by the given number of steps. The achievement
   * must be an incremental achievement. Once an achievement reaches at least
   * the maximum number of steps, it will be unlocked automatically. Any further
   * increments will be ignored.
   */
  void Increment(std::string const &achievement_id, uint32_t steps);

  /**
   * Reveal a hidden achievement to the currently signed-in player. If the
   * achievement has already been unlocked, this will have no effect.
   */
  void Reveal(std::string const &achievement_id);

  /**
   * Set an achievement to have at least the given number of steps completed.
   * Calling this method while the achievement already has more steps than the
   * provided value is a no-op. Once the achievement reaches the maximum number
   * of steps, the achievement is automatically unlocked, and any further
   * mutation operations are ignored.
   */
  void SetStepsAtLeast(std::string const &achievement_id, uint32_t steps);

  /**
   * Unlocks an achievement for the currently signed in player. If the
   * achievement is hidden, the SDK reveals it, as well.
   */
  void Unlock(std::string const &achievement_id);

  /**
   * Defines a callback type that receives a <code>UIStatus</code>.  This
   * callback type is provided to the <code>ShowAllUI*</code> function below.
   * @ingroup Callbacks
   */
  typedef std::function<void(UIStatus const &)> ShowAllUICallback;

  /**
   * Presents to the user a UI that displays information about all achievements.
   * It asynchronously calls <code>ShowAllUICallback</code>.
   */
  void ShowAllUI(ShowAllUICallback callback);

  /**
   * Presents to the user a UI that displays information about all achievements.
   * It synchronously returns a <code>UIStatus</code>.  Not specifying
   * <code>timeout</code> makes this function call equivalent to calling
   * <code>ShowAllUIBlocking(Timeout timeout)</code> with <code>timeout</code>
   * specified as 10 years.
   */
  UIStatus ShowAllUIBlocking();

  /**
   * Presents to the user a UI that displays information about all achievements.
   * It synchronously returns a <code>UIStatus</code>.  Specify
   * <code>timeout</code> as an arbitrary number of milliseconds.
   */
  UIStatus ShowAllUIBlocking(Timeout timeout);

  /**
   * @deprecated Prefer ShowAllUI(ShowAllUICallback callback).
   * Presents a UI to the user that displays information about all achievements.
   * The UI is shown asynchronously on all platforms.
   */
  void ShowAllUI() GPG_DEPRECATED;

 private:
  friend class GameServicesImpl;
  explicit AchievementManager(GameServicesImpl *game_services_impl);
  ~AchievementManager();
  AchievementManager(AchievementManager const &) = delete;
  AchievementManager &operator=(AchievementManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_ACHIEVEMENT_MANAGER_H_
