// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/player_manager.h
 *
 * @brief Entry points for Play Games Player functionality.
 */

#ifndef GPG_PLAYER_MANAGER_H_
#define GPG_PLAYER_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/player.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Gets and sets various player-related data.
 * @ingroup Managers
 */
class GPG_EXPORT PlayerManager {
 public:
  /**
   * Holds all player data, along with a response status.
   * @ingroup ResponseType
   */
  struct FetchSelfResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, FetchSelfResponse data object comes
     * back empty.
     */
    ResponseStatus status;

    /**
     * Data associated with this player.
     */
    Player data;
  };

  /**
   * Defines a callback type that receives a FetchSelfResponse. This callback
   * type is provided to the <code>FetchSelf(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchSelfResponse const &)> FetchSelfCallback;

  /**
   * Asynchronously loads all data for all currently signed-in players.
   * Calls the provided FetchSelfCallback on operation completion.
   * Not specifying data_source makes this function call equivalent to calling
   * <code>FetchSelf(DataSource data_source, FetchSelfCallback callback)</code>,
   * with data_source specified as CACHE_OR_NETWORK.
   */
  void FetchSelf(FetchSelfCallback callback);

  /**
   * Asynchronously loads all data for all currently signed-in players.
   * Calls the provided FetchSelfCallback on operation completion.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void FetchSelf(DataSource data_source, FetchSelfCallback callback);

  /**
   * Synchronously loads all data for all currently signed-in players,
   * directly returning the FetchSelfResponse.
   * Not specifying data_source or timeout makes this function call
   * equivalent to calling
   * FetchSelfResponse FetchSelfBlocking(DataSource data_source, Timeout
   * timeout), with data_source specified as CACHE_OR_NETWORK, and timeout
   * specified as 10 years.
   */
  FetchSelfResponse FetchSelfBlocking();

  /**
   * Synchronously loads all data for all currently signed-in players,
   * directly returning the FetchSelfResponse. Specify data_source as
   * CACHE_OR_NETWORK or NETWORK_ONLY. Not specifying timeout makes this
   * function call equivalent to calling
   * FetchSelfResponse FetchSelfBlocking(DataSource data_source,
   * Timeout timeout), with your specified data_source value, and timeout
   * specified as 10 years.
   */
  FetchSelfResponse FetchSelfBlocking(DataSource data_source);

  /**
   * Synchronously loads all data for all currently signed-in players,
   * directly returning the FetchSelfResponse. Specify timeout as
   * an arbitrary number of milliseconds. Not specifying data_source makes this
   * function call equivalent to calling
   * FetchSelfResponse FetchSelfBlocking(DataSource data_source, Timeout
   * timeout), with data_source specified as CACHE_OR_NETWORK, and timeout
   * containing your specified value.
   */
  FetchSelfResponse FetchSelfBlocking(Timeout timeout);

  /**
   * Synchronously loads all data for all currently signed-in
   * players, directly returning the FetchSelfResponse. Specify
   * data_source as CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout as an
   * arbitrary number of milliseconds.
   */
  FetchSelfResponse FetchSelfBlocking(DataSource data_source, Timeout timeout);

  /**
   * <code>data</code> and <code>ResponseStatus</code> for a specific
   * Player. The data value is only <Code>Valid()</code> if
   * <code>IsSuccess(status)</code> returns true.
   *
   * @ingroup ResponseType
   */
  struct FetchResponse {
    ResponseStatus status;
    Player data;
  };

  /**
   * Defines a callback type that receives a FetchResponse. This callback type
   * is provided to the <code>Fetch(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchResponse const &)> FetchCallback;

  /**
   * Asynchronously loads all data for a specific player.
   * Calls the provided FetchCallback on operation completion.
   * Not specifying data_source makes this function call equivalent to
   * calling <code>Fetch(DataSource data_source std::string const &player_id,
   * FetchCallback callback)</code>, with data_source specified as
   * CACHE_OR_NETWORK.
   */
  void Fetch(std::string const &player_id, FetchCallback callback);

  /**
   * Asynchronously loads all data for a specific player.
   * Calls the provided FetchCallback on operation completion.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void Fetch(DataSource data_source,
             std::string const &player_id,
             FetchCallback callback);

  /**
   * Synchronously loads all data for a specific player, directly returning
   * the FetchResponse.
   * Specifying neither data_source nor timeout makes this
   * function call equivalent to calling
   * FetchResponse FetchBlocking(DataSource data_source, timeout timeout),
   * with DataSource specified as CACHE_OR_NETWORK, and timeout specified as 10
   * years.
   */
  FetchResponse FetchBlocking(std::string const &player_id);

  /**
   * Synchronously loads all data for a specific player, directly returning
   * the FetchResponse. Specify data_source as
   * CACHE_OR_NETWORK or NETWORK_ONLY. Leaving timeout unspecified makes this
   * function call equivalent to calling
   * FetchResponse FetchBlocking(DataSource data_source, Timeout timeout),
   * with your specified data_source value, and timeout specified as 10 years.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              std::string const &player_id);

  /**
   * Synchronously loads all data for a specific player, directly returning
   * the FetchResponse. Specify timeout as an arbitrary number of
   * milliseconds.
   * Leaving data_source unspecified makes this function call equivalent to
   * calling FetchResponse FetchBlocking(DataSource data_source, Timeout
   * timeout), with data_source specified as CACHE_OR_NETWORK and timeout,
   * containing your specified value.
   */
  FetchResponse FetchBlocking(Timeout timeout, std::string const &player_id);

  /**
   * Synchronously loads all data for a specific player, directly returning
   * returning the FetchResponse. Specify data_source
   * as CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout as an arbitrary number
   * of milliseconds.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              Timeout timeout,
                              std::string const &player_id);

  /**
   * A response which contains a vector of players. If
   * <code>!IsSuccess(status)</code> then the vector will be empty.
   * @ingroup ResponseType
   */
  struct FetchListResponse {
    ResponseStatus status;
    std::vector<Player> data;
  };

  /**
   * Defines a callback type that receives a FetchListResponse. This callback
   * type is provided to the <code>Fetch(*)</code> functions
   * below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchListResponse const &)> FetchListCallback;

  /**
   * Asynchronously loads all recently played players. Calls the provided
   * FetchCallback on operation completion. Specify data_source as either
   * CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void FetchRecentlyPlayed(DataSource data_source, FetchListCallback callback);

  /**
   * Overload of {@link FetchRecentlyPlayed} which uses a default data_source
   * of CACHE_OR_NETWORK.
   */
  void FetchRecentlyPlayed(FetchListCallback callback);

  /**
   * Blocking version of {@link FetchRecentlyPlayed}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  FetchListResponse FetchRecentlyPlayedBlocking(DataSource data_source,
                                                Timeout timeout);

  /**
   * Overload of {@link FetchRecentlyPlayedBlocking} which uses a default
   * timeout of 10 years.
   */
  FetchListResponse FetchRecentlyPlayedBlocking(DataSource data_source);

  /**
   * Overload of {@link FetchRecentlyPlayedBlocking} which uses a default
   * data_source of CACHE_OR_NETWORK.
   */
  FetchListResponse FetchRecentlyPlayedBlocking(Timeout timeout);

  /**
   * Overload of {@link FetchRecentlyPlayedBlocking} which uses a default
   * data_source of CACHE_OR_NETWORK and a default timeout of 10 years.
   */
  FetchListResponse FetchRecentlyPlayedBlocking();

  /**
   * Asynchronously loads all players which have connected to the current game,
   * and which the signed-in player has permission to know about. Calls the
   * provided FetchCallback on operation completion. Specify data_source as
   * either CACHE_OR_NETWORK or NETWORK_ONLY.
   *
   * Note that this function may return {@link Player} objects where
   * {@link Player.HasLevelInfo} returns false. In such cases, level information
   * can be retreived by re-requesting the given player through the
   * {@link Fetch} API.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  void FetchConnected(DataSource data_source, FetchListCallback callback);

  /**
   * Overload of {@link FetchConnected} which uses a default data_source
   * of CACHE_OR_NETWORK.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  void FetchConnected(FetchListCallback callback);

  /**
   * Blocking version of {@link FetchConnected}. Allows the caller to specify a
   * timeout in ms. After the specified time elapses, the function returns
   * <code>ERROR_TIMEOUT</code>. Note that on iOS this blocking version cannot
   * be called from the UI thread, as the underlying plus service which provides
   * the data must run operations on the UI thread.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchConnectedBlocking(DataSource data_Source,
                                           Timeout timeout) GPG_DEPRECATED;

  /**
   * Overload of {@link FetchConnectedBlocking} which uses a default timeout
   * of 10 years.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchConnectedBlocking(DataSource data_source)
      GPG_DEPRECATED;

  /**
   * Overload of {@link FetchConnectedBlocking} which uses a default
   * data_source of CACHE_OR_NETWORK.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchConnectedBlocking(Timeout timeout) GPG_DEPRECATED;

  /**
   * Overload of {@link FetchConnectedBlocking} which uses a default
   * data_source of CACHE_OR_NETWORK and a default timeout of 10 years.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchConnectedBlocking() GPG_DEPRECATED;

  /**
   * Asynchronously loads all players which are in the local player's circles
   * (and can receive invites from the local player). Note that if an
   * invitable player has not connected to the current game, they may not
   * receive any notification that they have been invited.
   *
   * Calls the provided FetchCallback on operation completion. Specify
   * data_source as either CACHE_OR_NETWORK or NETWORK_ONLY.
   *
   * Note that this function may return {@link Player} objects where
   * {@link Player.HasLevelInfo} returns false. In such cases, level information
   * can be retreived by re-requesting the given player through the
   * {@link Fetch} API.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  void FetchInvitable(DataSource data_source,
                      FetchListCallback callback) GPG_DEPRECATED;

  /**
   * Overload of {@link FetchInvitable} which uses a default data_source
   * of CACHE_OR_NETWORK.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  void FetchInvitable(FetchListCallback callback) GPG_DEPRECATED;

  /**
   * Blocking version of {@link FetchInvitable}. Allows the caller to specify
   * a timeout in ms. After the specified time elapses, the function returns
   * <code>ERROR_TIMEOUT</code>. Note that on iOS this blocking version cannot
   * be called from the UI thread, as the underlying plus service which provides
   * the data must run operations on the UI thread.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchInvitableBlocking(DataSource data_source,
                                           Timeout timeout) GPG_DEPRECATED;

  /**
   * Overload of {@link FetchInvitableBlocking} which uses a default timeout
   * of 10 years.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchInvitableBlocking(DataSource data_source)
      GPG_DEPRECATED;

  /**
   * Overload of {@link FetchInvitableBlocking} which uses a default
   * data_source of CACHE_OR_NETWORK.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchInvitableBlocking(Timeout timeout) GPG_DEPRECATED;

  /**
   * Overload of {@link FetchInvitableBlocking} which uses a default
   * data_source of CACHE_OR_NETWORK and a default timeout of 10 years.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  FetchListResponse FetchInvitableBlocking() GPG_DEPRECATED;

 private:
  friend class GameServicesImpl;
  explicit PlayerManager(GameServicesImpl *game_services_impl);
  ~PlayerManager();
  PlayerManager(PlayerManager const &) = delete;
  PlayerManager &operator=(PlayerManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_PLAYER_MANAGER_H_
