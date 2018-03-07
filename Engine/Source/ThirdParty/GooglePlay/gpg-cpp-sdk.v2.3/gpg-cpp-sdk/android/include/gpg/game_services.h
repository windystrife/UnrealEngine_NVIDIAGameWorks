// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/game_services.h
 *
 * @brief The main entry point for Play Games.
 */

#ifndef GPG_GAME_SERVICES_H_
#define GPG_GAME_SERVICES_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <functional>
#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/platform_configuration.h"
#include "gpg/status.h"
#include "gpg/types.h"

namespace gpg {

class AchievementManager;
class BuilderImpl;
class EventManager;
class GameServicesImpl;
class LeaderboardManager;
class PlayerManager;
class QuestManager;
class RealTimeMultiplayerManager;
class SnapshotManager;
class StatsManager;
class TurnBasedMultiplayerManager;
class VideoManager;

/**
 * The starting point for interacting with Google Play Games.
 *
 * <h3>Game Services Lifecycle</h3>
 *
 * An instance of the GameServices class is created via a GameServices::Builder.
 * When created, the instance is initially not signed into the Game Services
 * (that is, IsAuthorized() will return false). A silent sign-in attempt is
 * initiated in the background, and may succeed if a user was signed in at the
 * end of a previous session. Until the completion of this silent sign-in
 * attempt, any user authentication UI (for example, sign-in and/or sign-out
 * buttons) should be disabled or hidden.
 *
 * At the completion of this silent sign-in attempt, the OnAuthActionFinished
 * callback for the GameServices instance (registered with
 * GameServices::Builder::SetOnAuthActionFinished) will be notified. If the
 * callback arguments reflect a succesful sign-in attempt, the instance can be
 * assumed to be connected to Game Services (that is, IsAuthorized() will return
 * true), and sign-out UI should be enabled. If the callback argument reflects a
 * failed sign-in attempt, sign-in UI should be enabled.
 *
 * Explicit sign-out via the SignOut() method should be invoked only at user
 * request. This requests a transition to a signed out state. The completion of
 * this transition is indicated by an invocation of the OnAuthActionFinished
 * callback. Until such a time as this callback is invoked, other GameServices
 * APIs (including StartAuthorizationUI()) should not be called.
 *
 * When a GameServices instance is destructed, it will block until there are no
 * pending operations to avoid data loss. If this block-on-destruction behavior
 * is not desired, a Flush() should be issued and the GameServices instance
 * should be kept alive until the Flush() completes.
 *
 * For GameServices to function properly on Android versions less than 4.0, the
 * owning Activity must call lifecycle callbacks. See AndroidSupport.
 */
class GPG_EXPORT GameServices {
 public:
  /**
   * A forward declaration of the Builder type.
   * For more information, see documentation on GameServices::Builder.
   */
  class Builder;

  GameServices() = delete;
  ~GameServices();

  /// Brings up a platform-specific user authorization flow.
  void StartAuthorizationUI();

  /**
   * Allows you to explicitly check the current authorization state.
   * SDK consumers are encouraged to register for AUTH_ACTION_* callbacks to
   * handle authorization state changes, rather than polling.
   */
  bool IsAuthorized();

  /**
   * Begins the asynchronous sign-out process.
   * After calling SignOut, you should not call any operations on GameServices
   * until you receive the OnAuthActionFinishedCallback indicating a successful
   * sign-out.
   */
  void SignOut();

  /**
   * Provides a reference to the AchievementManager object used for accessing
   * and manipulating achievements.
   */
  AchievementManager &Achievements();

  /**
   * Provides a const reference to the AchievementManager object used for
   * accessing and manipulating achievements.
   */
  AchievementManager const &Achievements() const;

  /**
   * Provides a reference to the EventManager object used for accessing
   * and manipulating events.
   */
  EventManager &Events();

  /**
   * Provides a const reference to the EventManager object used for accessing
   * and manipulating events.
   */
  EventManager const &Events() const;

  /**
   * Provides a reference to the LeaderboardManager object used for accessing
   * and manipulating achievements.
   */
  LeaderboardManager &Leaderboards();

  /**
   * Provides a const reference to the LeaderboardManager object used for
   * accessing and manipulating achievements.
   */
  LeaderboardManager const &Leaderboards() const;

  /**
   * Provides a reference to the SnapshotManager object used for accessing
   * and manipulating snapshots.
   */
  SnapshotManager &Snapshots();

  /**
   * Provides a const reference to the SnapshotManager object used for accessing
   * and manipulating snapshots.
   */
  SnapshotManager const &Snapshots() const;

  /**
   * Provides a reference to the StatsManager object used for accessing
   * game and player statistics.
   */
  StatsManager &Stats();

  /**
   * Provides a const reference to the StatsManager object used for accessing
   * game and player statistics.
   */
  StatsManager const &Stats() const;

  /**
   * Provides a reference to the PlayerManager object, which allows access
   * to information about players.
   */
  PlayerManager &Players();

  /**
   * Provides a const reference to the PlayerManager object, which allows access
   * to information about players.
   */
  PlayerManager const &Players() const;

  /**
   * Provides a reference to the QuestManager object, which allows access
   * to information about quests.
   */
  QuestManager &Quests();

  /**
   * Provides a const reference to the QuestManager object, which allows access
   * to information about quests.
   */
  QuestManager const &Quests() const;

  /**
   * Provides a reference to the TurnBasedMultiplayerManager object, which
   * allows access to TBMP related methods.
   */
  TurnBasedMultiplayerManager &TurnBasedMultiplayer();

  /**
   * Provides a const reference to the TurnBasedMultiplayerManager object, which
   * allows access to TBMP related methods.
   */
  TurnBasedMultiplayerManager const &TurnBasedMultiplayer() const;

  /**
   * Provides a reference to the RealTimeMultiplayerManager object, which
   * allows access to RTMP related methods.
   */
  RealTimeMultiplayerManager &RealTimeMultiplayer();

  /**
   * Provides a const reference to the RealTimeMultiplayerManager object, which
   * allows access to RTMP related methods.
   */
  RealTimeMultiplayerManager const &RealTimeMultiplayer() const;

  /**
   * Provides a reference to the VideoManager object, which allows access to
   * video related methods.
   */
  VideoManager &Video();

  /**
   * Provides a const reference to the VideoManager object, which allows access
   * to video related methods.
   */
  VideoManager const &Video() const;

  /**
   * Defines a callback type that receives the result (status) of a Flush
   * operation. Used in Flush().
   * @ingroup Callbacks
   */
  typedef std::function<void(FlushStatus)> FlushCallback;

  /**
   * Asynchronously flushes the main dispatch queue, and returns the status of
   * the flush to the provided FlushCallback. Possible statuses are: FLUSHED,
   * ERROR_INTERNAL, and ERROR_VERSION_UPDATE_REQUIRED.
   */
  void Flush(FlushCallback callback);

  /**
   * Synchronously flushes and gets a result (status) of the flush.
   * Possible statuses are: FLUSHED, ERROR_INTERNAL, ERROR_NOT_AUTHORIZED,
   * ERROR_VERSION_UPDATE_REQUIRED, and ERROR_TIMEOUT. Leaving this timeout
   * unspecified makes this function call equivalent to calling
   * FlushStatus FlushBlocking(Timeout), with Timeout specified as
   * 10 years.
   */
  FlushStatus FlushBlocking();

  /**
   * Synchronously flushes and gets a result (status) of the flush.
   * Possible statuses are: FLUSHED, ERROR_INTERNAL, ERROR_NOT_AUTHORIZED,
   * ERROR_VERSION_UPDATE_REQUIRED, and ERROR_TIMEOUT. Specify the timeout
   * as an arbitrary number of milliseconds.
   */
  FlushStatus FlushBlocking(Timeout timeout);

 private:
  GameServices(std::unique_ptr<BuilderImpl> builder_impl,
               PlatformConfiguration const &platform);
  GameServices(GameServices const &) = delete;
  GameServices &operator=(GameServices const &) = delete;

  std::shared_ptr<GameServicesImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_GAME_SERVICES_H_
