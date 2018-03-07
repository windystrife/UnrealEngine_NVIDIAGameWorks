// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/builder.h
 *
 * @brief Used to construct a GameServices object.
 */

#ifndef GPG_BUILDER_H_
#define GPG_BUILDER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <functional>
#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/multiplayer_invitation.h"
#include "gpg/platform_configuration.h"
#include "gpg/quest.h"
#include "gpg/turn_based_match.h"

namespace gpg {

class BuilderImpl;
enum class LogLevel;  // Defined in gpg/types.h

/**
 * Used for creating and configuring an instance of the GameServices class.
 */
class GPG_EXPORT GameServices::Builder {
 public:
  Builder();
  ~Builder();

  /**
   * Takes a platform configuration and attempts to return a newly instantiated
   * GameServices object.  Will return nullptr if given an invalid
   * PlatformConfiguration (i.e. !platform.Valid()), and may also return nullptr
   * if another GameServices object has already been instantiated.
   *
   * For more information, see the documentation on IosPlatformConfiguration,
   * AndroidPlatformConfiguration, and PlatformConfiguration:
   * https://developers.google.com/games/services/cpp/api/platform__configuration_8h
   */
  std::unique_ptr<GameServices> Create(PlatformConfiguration const &platform);

  /**
   * The type of logging callback that can be provided to the SDK.
   * @ingroup Callbacks
   */
  typedef std::function<void(LogLevel, std::string const &)> OnLogCallback;

  /**
   * @deprecated Prefer SetOnLog and SetDefaultOnLog.
   * Registers a callback which will be used to perform logging.
   * min_level specifies the minimum log level at which the callback is invoked.
   * Possible levels are: VERBOSE, INFO, WARNING, and ERROR.
   */
  Builder &SetLogging(OnLogCallback callback,
                      LogLevel min_level) GPG_DEPRECATED;

  /**
   * @deprecated Prefer SetOnLog and SetDefaultOnLog.
   * Registers a callback which will be used to perform logging.
   * the same as calling SetLogging(OnLogCallback, LogLevel) with a LogLevel of
   * INFO.
   */
  Builder &SetLogging(OnLogCallback callback) GPG_DEPRECATED;

  /**
   * Registers a callback which will be used to perform logging.
   * min_level specifies the minimum log level at which the callback is invoked.
   * Possible levels are: VERBOSE, INFO, WARNING, and ERROR.
   */
  Builder &SetOnLog(OnLogCallback callback, LogLevel min_level);

  /**
   * Registers a callback which will be used to perform logging.
   * This is equivalent to calling SetOnLog(OnLogCallback, LogLevel) with a
   * LogLevel of INFO.
   */
  Builder &SetOnLog(OnLogCallback callback);

  /**
   * Specifies that logging should use the DEFAULT_ON_LOG_CALLBACK at the
   * specified log level. min_level specifies the minimum log level at which the
   * default callback is invoked.
   * Possible levels are: VERBOSE, INFO, WARNING, and ERROR.
   * This is equivalent to calling SetOnLog(OnLogCallback, LogLevel) with
   * OnLogCallback set to DEFAULT_ON_LOG_CALLBACK and a LogLevel of min_level.
   */
  Builder &SetDefaultOnLog(LogLevel min_level);

  /**
   * The type of the authentication action started callback that can be provided
   * to the SDK.
   * @ingroup Callbacks
   */
  typedef std::function<void(AuthOperation)> OnAuthActionStartedCallback;

  /**
   * Registers a callback to be called when authorization has begun.
   */
  Builder &SetOnAuthActionStarted(OnAuthActionStartedCallback callback);

  /**
   * The type of authentication action finished callback that can be provided to
   * the SDK.
   * @ingroup Callbacks
   */
  typedef std::function<void(AuthOperation, AuthStatus)>
      OnAuthActionFinishedCallback;

  /**
   * Registers a callback to be called when authorization has finished.
   */
  Builder &SetOnAuthActionFinished(OnAuthActionFinishedCallback callback);

  /**
   * The type of the multiplayer invitation callback that can be provided to the
   * SDK. Valid() only returns true for the MultiplayerInvitation on UPDATED
   * events.
   * @ingroup Callbacks
   */
  typedef std::function<void(
      MultiplayerEvent, std::string, MultiplayerInvitation)>
      OnMultiplayerInvitationEventCallback;

  /**
   * Registers a callback to be called when an event occurrs for a multiplayer
   * invitation.
   */
  Builder &SetOnMultiplayerInvitationEvent(
      OnMultiplayerInvitationEventCallback callback);

  /**
   * The type of the turn based multiplayer event callback that can be provided
   * to the SDK. Valid() only returns true for the TurnBasedMatch parameter on
   * UPDATED events.
   * @ingroup Callbacks
   */
  typedef std::function<void(
      MultiplayerEvent event, std::string, TurnBasedMatch)>
      OnTurnBasedMatchEventCallback;

  /**
   * Registers a callback to be called when an event occurrs for a turn based
   * multiplayer match.
   */
  Builder &SetOnTurnBasedMatchEvent(OnTurnBasedMatchEventCallback callback);

  /**
   * The type of the quest completed callback that can be provided to the
   * SDK. Provides the completed quest.
   * @ingroup Callbacks
   */
  typedef std::function<void(Quest quest)> OnQuestCompletedCallback;

  /**
   * Registers a callback to be called when a quest changes to the state
   * QuestState::COMPLETED.
   */
  Builder &SetOnQuestCompleted(OnQuestCompletedCallback callback);

  /**
   * Enable Snapshots. This is equivalent to
   * <code>AddOauthScope(kSnapshotScope)</code>.
   * See {@link SnapshotManager} for more details.
   */
  Builder &EnableSnapshots();

  /**
   * Scopes beyond the required Play Games scope to request.
   * Details on authorization scopes at
   * https://developers.google.com/+/api/oauth#scopes.
   */
  Builder &AddOauthScope(std::string const &scope);

  /**
   * Sets whether a "connecting" popup should be displayed automatically
   * at the start of the sign-in flow. By default this is enabled.
   */
  Builder &SetShowConnectingPopup(bool show_popup);

 private:
  Builder(Builder const &) = delete;
  Builder &operator=(Builder const &) = delete;

  std::unique_ptr<BuilderImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_BUILDER_H_
