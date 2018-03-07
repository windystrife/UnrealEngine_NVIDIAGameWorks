// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/status.h
 *
 * @brief Enumerations used to represent the status of a request.
 */

#ifndef GPG_COMMON_ERROR_STATUS_H_
#define GPG_COMMON_ERROR_STATUS_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

// GCC Has a bug in which it throws spurious errors on "enum class" types
// when using __attribute__ with them. Suppress this.
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43407
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "gpg/common.h"

namespace gpg {

/**
 *  A struct containing all possible status codes that can be returned by
 *  our APIs. A positive status indicates success, a negative one failure.
 *  The API never directly returns these values, instead doing so via one of
 *  several more specific status enums classes.
 */
struct GPG_EXPORT BaseStatus {
  /**
   * The type of the status values contained within {@link BaseStatus}.
   */
  enum StatusCode {
    VALID = 1,
    VALID_BUT_STALE = 2,
    VALID_WITH_CONFLICT = 3,
    FLUSHED = 4,
    DEFERRED = 5,
    ERROR_LICENSE_CHECK_FAILED = -1,
    ERROR_INTERNAL = -2,
    ERROR_NOT_AUTHORIZED = -3,
    ERROR_VERSION_UPDATE_REQUIRED = -4,
    ERROR_TIMEOUT = -5,
    ERROR_CANCELED = -6,
    ERROR_MATCH_ALREADY_REMATCHED = -7,
    ERROR_INACTIVE_MATCH = -8,
    ERROR_INVALID_RESULTS = -9,
    ERROR_INVALID_MATCH = -10,
    ERROR_MATCH_OUT_OF_DATE = -11,
    ERROR_UI_BUSY = -12,
    ERROR_QUEST_NO_LONGER_AVAILABLE = -13,
    ERROR_QUEST_NOT_STARTED = -14,
    ERROR_MILESTONE_ALREADY_CLAIMED = -15,
    ERROR_MILESTONE_CLAIM_FAILED = -16,
    ERROR_REAL_TIME_ROOM_NOT_JOINED = -17,
    ERROR_LEFT_ROOM = -18,
    // The following error codes map result codes from other APIs that overlap
    // with Play Services CommonErrorCodes values.  The result code is mapped
    // by adding 100 to the value.
    ERROR_NO_DATA = -104,  // -4 taken by ERROR_VERSION_UPDATE_REQUIRED.
    ERROR_NETWORK_OPERATION_FAILED = -106,  // -6 taken by ERROR_CANCELED.
    ERROR_APP_MISCONFIGURED = -108,         // -8 taken by ERROR_INACTIVE_MATCH.
    ERROR_GAME_NOT_FOUND = -109,  // -9 taken by ERROR_INVALID_RESULTS.
    ERROR_INTERRUPTED = -114,     // -14 taken by ERROR_QUEST_NOT_STARTED.
    ERROR_SNAPSHOT_NOT_FOUND = -4000,
    ERROR_SNAPSHOT_CREATION_FAILED = -4001,
    ERROR_SNAPSHOT_CONTENTS_UNAVAILABLE = -4002,
    ERROR_SNAPSHOT_COMMIT_FAILED = -4003,
    ERROR_SNAPSHOT_FOLDER_UNAVAILABLE = -4005,
    ERROR_SNAPSHOT_CONFLICT_MISSING = -4006,
    ERROR_MULTIPLAYER_CREATION_NOT_ALLOWED = -6000,
    ERROR_MULTIPLAYER_NOT_TRUSTED_TESTER = -6001,
    ERROR_MULTIPLAYER_INVALID_MULTIPLAYER_TYPE = -6002,
    ERROR_MULTIPLAYER_DISABLED = -6003,
    ERROR_MULTIPLAYER_INVALID_OPERATION = -6004,
    ERROR_MATCH_INVALID_PARTICIPANT_STATE = -6500,
    ERROR_MATCH_INVALID_MATCH_STATE = -6502,
    ERROR_MATCH_NOT_FOUND = -6506,
    ERROR_MATCH_LOCALLY_MODIFIED = -6507,
    ERROR_VIDEO_NOT_ACTIVE = -9000,   // Not currently available.
    ERROR_VIDEO_UNSUPPORTED = -9001,  // Not currently available.
  };
};

/**
 * The set of possible values representing the result of an attempted
 * operation.
 */
enum class GPG_EXPORT ResponseStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * A network error occurred while attempting to retrieve fresh data, but some
   * locally cached data was available. The data returned may be stale and/or
   * incomplete.
   */
  VALID_BUT_STALE = BaseStatus::VALID_BUT_STALE,
  /**
   * A network error occurred, but the data was successfully modified locally.
   */
  DEFERRED = BaseStatus::DEFERRED,
  /**
   * The application is not licensed to the user.
   */
  ERROR_LICENSE_CHECK_FAILED = BaseStatus::ERROR_LICENSE_CHECK_FAILED,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * The installed version of Google Play services is out of date.
   */
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
  // Not currently available.
  ERROR_VIDEO_NOT_ACTIVE = BaseStatus::ERROR_VIDEO_NOT_ACTIVE,
  // Not currently available.
  ERROR_VIDEO_UNSUPPORTED = BaseStatus::ERROR_VIDEO_UNSUPPORTED,
};

/**
 * The set of possible values representing the result of a flush
 * attempt.
 */
enum class GPG_EXPORT FlushStatus {
  /**
   * A flushing operation was successful.
   */
  FLUSHED = BaseStatus::FLUSHED,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * The installed version of Google Play services is out of date.
   */
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

/**
 * The set of possible values representing the result of an authorization
 * attempt.
 */
enum class GPG_EXPORT AuthStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * The installed version of Google Play services is out of date.
   */
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

/**
 * The set of possible values representing the result of a UI attempt.
 */
enum class GPG_EXPORT UIStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * The installed version of Google Play services is out of date.
   */
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * The user closed the UI, cancelling the operation.
   */
  ERROR_CANCELED = BaseStatus::ERROR_CANCELED,
  /**
   * UI could not be opened.
   */
  ERROR_UI_BUSY = BaseStatus::ERROR_UI_BUSY,
  /**
   * The player left the multiplayer room.
   */
  ERROR_LEFT_ROOM = BaseStatus::ERROR_LEFT_ROOM,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured. See logs for more info.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

/**
 * The set of possible values representing the result of a multiplayer
 * operation.
 */
enum class GPG_EXPORT MultiplayerStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * A network error occurred while attempting to retrieve fresh data, but some
   * locally cached data was available. The data returned may be stale and/or
   * incomplete.
   */
  VALID_BUT_STALE = BaseStatus::VALID_BUT_STALE,
  /**
   * A network error occurred, but the data was successfully modified locally.
   */
  DEFERRED = BaseStatus::DEFERRED,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * The installed version of Google Play services is out of date.
   */
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   *  The specified match has already had a rematch created.
   */
  ERROR_MATCH_ALREADY_REMATCHED = BaseStatus::ERROR_MATCH_ALREADY_REMATCHED,
  /**
   * The match is not currently active. This action cannot be performed on an
   * inactive match.
   */
  ERROR_INACTIVE_MATCH = BaseStatus::ERROR_INACTIVE_MATCH,
  /**
   * The match results provided in this API call are invalid. This covers
   * cases of duplicate results, results for players who are not in the match,
   * etc.
   */
  ERROR_INVALID_RESULTS = BaseStatus::ERROR_INVALID_RESULTS,
  /**
   * The match is invalid.
   */
  ERROR_INVALID_MATCH = BaseStatus::ERROR_INVALID_MATCH,
  /**
   * The match data is out of date, and has been modified on the server.
   */
  ERROR_MATCH_OUT_OF_DATE = BaseStatus::ERROR_MATCH_OUT_OF_DATE,
  /**
   * The message failed to send, as the RTMP room was not joined.
   */
  ERROR_REAL_TIME_ROOM_NOT_JOINED = BaseStatus::ERROR_REAL_TIME_ROOM_NOT_JOINED,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured. See logs for more info.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
  /**
   * The user is not allowed to create a new multiplayer game at this time.
   * This could occur if the user has too many outstanding invitations
   * already.
   */
  ERROR_MULTIPLAYER_CREATION_NOT_ALLOWED =
      BaseStatus::ERROR_MULTIPLAYER_CREATION_NOT_ALLOWED,
  /**
   * The user attempted to invite another user who was not authorized to see
   * the game.
   */
  ERROR_MULTIPLAYER_NOT_TRUSTED_TESTER =
      BaseStatus::ERROR_MULTIPLAYER_NOT_TRUSTED_TESTER,
  /**
   * The match is not the right type to perform this action on.
   */
  ERROR_MULTIPLAYER_INVALID_MULTIPLAYER_TYPE =
      BaseStatus::ERROR_MULTIPLAYER_INVALID_MULTIPLAYER_TYPE,
  /**
   * This game does not have this multiplayer type enabled in the developer
   * console.
   */
  ERROR_MULTIPLAYER_DISABLED = BaseStatus::ERROR_MULTIPLAYER_DISABLED,
  /**
   * This multiplayer operation is not valid, and the server rejected it.
   */
  ERROR_MULTIPLAYER_INVALID_OPERATION =
      BaseStatus::ERROR_MULTIPLAYER_INVALID_OPERATION,
  /**
   * One or more participants in this match are not in valid states.
   */
  ERROR_MATCH_INVALID_PARTICIPANT_STATE =
      BaseStatus::ERROR_MATCH_INVALID_PARTICIPANT_STATE,
  /**
   * The match is not in the correct state to perform the specified action.
   */
  ERROR_MATCH_INVALID_MATCH_STATE = BaseStatus::ERROR_MATCH_INVALID_MATCH_STATE,
  /**
   * The specified match cannot be found.
   */
  ERROR_MATCH_NOT_FOUND = BaseStatus::ERROR_MATCH_NOT_FOUND,
  /**
   * The specified match has been modified locally, and must be sent to the
   * server before this operation can be performed.
   */
  ERROR_MATCH_LOCALLY_MODIFIED = BaseStatus::ERROR_MATCH_LOCALLY_MODIFIED,
};

/**
 * The set of possible values representing the result of an accept-quest
 * operation.
 */
enum class GPG_EXPORT QuestAcceptStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * A network error occurred, but the data was successfully modified locally.
   */
  DEFERRED = BaseStatus::DEFERRED,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * The quest is no longer available.
   */
  ERROR_QUEST_NO_LONGER_AVAILABLE = BaseStatus::ERROR_QUEST_NO_LONGER_AVAILABLE,
  /**
   * The quest has not started.
   */
  ERROR_QUEST_NOT_STARTED = BaseStatus::ERROR_QUEST_NOT_STARTED,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured. See logs for more info.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

/**
 * The set of possible values representing the result of a claim-milestone
 * operation.
 */
enum class GPG_EXPORT QuestClaimMilestoneStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * A network error occurred, but the data was successfully modified locally.
   */
  DEFERRED = BaseStatus::DEFERRED,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * The milestone is already claimed.
   */
  ERROR_MILESTONE_ALREADY_CLAIMED = BaseStatus::ERROR_MILESTONE_ALREADY_CLAIMED,
  /**
   * The milestone claim failed.
   */
  ERROR_MILESTONE_CLAIM_FAILED = BaseStatus::ERROR_MILESTONE_CLAIM_FAILED,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured. See logs for more info.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

/**
 * The set of possible values representing errors which are common to all
 * operations.  These error values must be included in every Status value set.
 */
enum class GPG_EXPORT CommonErrorStatus {
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The application is misconfigured. See logs for more info.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

/**
 * The set of possible values representing the result of a snapshot open
 * operation.
 */
enum class GPG_EXPORT SnapshotOpenStatus {
  /**
   * The operation was successful.
   */
  VALID = BaseStatus::VALID,
  /**
   * The operation was successful, but a conflict was detected.
   */
  VALID_WITH_CONFLICT = BaseStatus::VALID_WITH_CONFLICT,
  /**
   * An internal error occurred.
   */
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  /**
   * The player is not authorized to perform the operation.
   */
  ERROR_NOT_AUTHORIZED = BaseStatus::ERROR_NOT_AUTHORIZED,
  /**
   * Timed out while awaiting the result.
   */
  ERROR_TIMEOUT = BaseStatus::ERROR_TIMEOUT,
  /**
   * A network error occurred, and there is no data available locally.
   */
  ERROR_NO_DATA = BaseStatus::ERROR_NO_DATA,
  /**
   * A network error occurred during an operation that requires network
   * access.
   */
  ERROR_NETWORK_OPERATION_FAILED = BaseStatus::ERROR_NETWORK_OPERATION_FAILED,
  /**
   * The specified snapshot was not found.
   */
  ERROR_SNAPSHOT_NOT_FOUND = BaseStatus::ERROR_SNAPSHOT_NOT_FOUND,
  /**
   * The attempt to create a snapshot failed.
   */
  ERROR_SNAPSHOT_CREATION_FAILED = BaseStatus::ERROR_SNAPSHOT_CREATION_FAILED,
  /**
   * An error occurred while attempting to open the contents of a snapshot.
   */
  ERROR_SNAPSHOT_CONTENTS_UNAVAILABLE =
      BaseStatus::ERROR_SNAPSHOT_CONTENTS_UNAVAILABLE,
  /**
   * The attempt to commit the change to the snapshot failed.
   */
  ERROR_SNAPSHOT_COMMIT_FAILED = BaseStatus::ERROR_SNAPSHOT_COMMIT_FAILED,
  /**
   * The root folder for snapshots could not be found or created.
   */
  ERROR_SNAPSHOT_FOLDER_UNAVAILABLE =
      BaseStatus::ERROR_SNAPSHOT_FOLDER_UNAVAILABLE,
  /**
   * The snapshot conflict being resolved does not exist.
   */
  ERROR_SNAPSHOT_CONFLICT_MISSING = BaseStatus::ERROR_SNAPSHOT_CONFLICT_MISSING,
  /**
   * The application is misconfigured. See logs for more info.
   */
  ERROR_APP_MISCONFIGURED = BaseStatus::ERROR_APP_MISCONFIGURED,
  /**
   * The specified game ID was not recognized by the server.
   */
  ERROR_GAME_NOT_FOUND = BaseStatus::ERROR_GAME_NOT_FOUND,
  /**
   * A blocking call was interrupted while waiting and did not run to
   * completion.
   */
  ERROR_INTERRUPTED = BaseStatus::ERROR_INTERRUPTED,
};

}  // namespace gpg

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop  // GCC diagnostic ignored "-Wattributes"
#endif

#endif  // GPG_COMMON_ERROR_STATUS_H_
