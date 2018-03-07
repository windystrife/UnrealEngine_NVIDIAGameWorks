// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/stats_manager.h
 *
 * @brief Entry points for Play Games Stats functionality.
 */

#ifndef GPG_STATS_MANAGER_H_
#define GPG_STATS_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/playerstats.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Gets and sets various stats-related data.
 * @ingroup Managers
 */
class GPG_EXPORT StatsManager {
 public:
  /**
   * Holds all PlayerStats data, along with a response status.
   * @ingroup ResponseType
   */
  struct FetchForPlayerResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, FetchForPlayerResponse data object comes
     * back empty.
     */
    ResponseStatus status;

    /**
     * Data associated with the statistics collected for this player.
     */
    PlayerStats data;
  };

  /**
   * Defines a callback type that receives a FetchForPlayerResponse. This
   * callback type is provided to the <code>FetchForPlayer(*)</code> functions
   * below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchForPlayerResponse const &)>
      FetchForPlayerCallback;

  /**
   * Asynchronously loads all stats data for the currently signed-in player.
   * Calls the provided FetchForPlayerCallback on operation completion.
   * Not specifying data_source makes this function call equivalent to calling
   * <code>FetchForPlayer(DataSource data_source,
   * FetchForPlayerCallback callback)</code>, with data_source specified as
   * CACHE_OR_NETWORK.
   */
  void FetchForPlayer(FetchForPlayerCallback callback);

  /**
   * Asynchronously loads all stats data for the currently signed-in player.
   * Calls the provided FetchForPlayerCallback on operation completion.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void FetchForPlayer(DataSource data_source, FetchForPlayerCallback callback);

  /**
   * Synchronously loads all stats data for the currently signed-in player,
   * directly returning the FetchForPlayerResponse.
   * Not specifying data_source or timeout makes this function call
   * equivalent to calling
   * FetchForPlayerResponse FetchForPlayerBlocking(DataSource data_source,
   * Timeout timeout), with data_source specified as CACHE_OR_NETWORK, and
   * timeout specified as 10 years.
   */
  FetchForPlayerResponse FetchForPlayerBlocking();

  /**
   * Synchronously loads all stats data for the currently signed-in player,
   * directly returning the FetchForPlayerResponse. Specify data_source as
   * CACHE_OR_NETWORK or NETWORK_ONLY. Not specifying timeout makes this
   * function call equivalent to calling
   * FetchForPlayerResponse FetchForPlayerBlocking(DataSource data_source,
   * Timeout timeout), with your specified data_source value, and timeout
   * specified as 10 years.
   */
  FetchForPlayerResponse FetchForPlayerBlocking(DataSource data_source);

  /**
   * Synchronously loads all stats data for the currently signed-in player,
   * directly returning the FetchForPlayerResponse. Specify timeout as
   * an arbitrary number of milliseconds. Not specifying data_source makes this
   * function call equivalent to calling
   * FetchForPlayerResponse FetchForPlayerBlocking(DataSource data_source,
   * Timeout timeout), with data_source specified as CACHE_OR_NETWORK, and
   * timeout containing your specified value.
   */
  FetchForPlayerResponse FetchForPlayerBlocking(Timeout timeout);

  /**
   * Synchronously loads all stats data for the currently signed-in
   * player, directly returning the FetchForPlayerResponse. Specify
   * data_source as CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout as an
   * arbitrary number of milliseconds.
   */
  FetchForPlayerResponse FetchForPlayerBlocking(DataSource data_source,
                                                Timeout timeout);

 private:
  friend class GameServicesImpl;
  explicit StatsManager(GameServicesImpl *game_services_impl);
  ~StatsManager();
  StatsManager(StatsManager const &) = delete;
  StatsManager &operator=(StatsManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_STATS_MANAGER_H_
