// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/types.h
 *
 * @brief Assorted types.
 */

#ifndef GPG_TYPES_H_
#define GPG_TYPES_H_

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

#include <chrono>
#include "gpg/common.h"

namespace gpg {

/**
 * Data type used in specifying timeout periods for attempted operations.
 */

typedef std::chrono::milliseconds Timeout;

/**
 * Data type used to specify timestamps. Relative to the epoch (1/1/1970).
 */

typedef std::chrono::milliseconds Timestamp;

/**
 * Data type used to specify durations in milliseconds.
 */

typedef std::chrono::milliseconds Duration;

/**
 * Values specifying where to get data from when retrieving achievement,
 * leaderboard, and other data.
 * When the setting is CACHE_OR_NETWORK, the system resorts to the local
 * cache when no network connection is available.
 */
enum class GPG_EXPORT DataSource {
  /**
   * Use either the cache or the network. (The system resorts to the local
   * cache when no network connection is available.)
   */
  CACHE_OR_NETWORK = 1,
  /**
   * Force a refresh of data from the network.
   * The request will fail if no network connection is available.
   */
  NETWORK_ONLY = 2

};

/**
 * Values used to specify the level of logging.
 */
enum class GPG_EXPORT LogLevel {
  /**
   * All log messages.
   */
  VERBOSE = 1,
  /**
   * All log messages besides verbose.
   */
  INFO = 2,
  /**
   * Warnings and errors only.
   */
  WARNING = 3,
  /**
   * Errors Only.
   */
  ERROR = 4
};

/**
 * Values used to specify the type of authorization operation to perform.
 */
enum class GPG_EXPORT AuthOperation {
  /**
   * Sign in.
   */
  SIGN_IN = 1,
  /**
   * Sign out.
   */
  SIGN_OUT = 2
};

/**
 * Values used to specify the resolution at which to fetch a particular image.
 */
enum class GPG_EXPORT ImageResolution {
  /**
   * Icon-sized resolution.
   */
  ICON = 1,
  /**
   * High resolution.
   */
  HI_RES = 2
};

/**
 * Values used to specify achievement type.
 * A player makes gradual progress (steps) toward an incremental achievement.
 * He or she completes a standard achievement in a single step.
 */

enum class GPG_EXPORT AchievementType {
  /**
   * Standard achievement - completes in a single step.
   */
  STANDARD = 1,
  /**
   * Incremental achievement - completes in a multiple steps.
   */
  INCREMENTAL = 2
};

/**
 * Values used to specify achievement state.
 * A hidden achievement is one whose existence a player has not yet discovered.
 * make him or her aware of it.
 * A revealed achievement is one that the player knows about, but has not yet
 * earned.
 * An unlocked achievement is one that the player has earned.
 */

enum class GPG_EXPORT AchievementState {
  // These are ordered such that only increasing transitions are possible
  /**
   * Not visible to the player.
   */
  HIDDEN = 1,
  /**
   * Visible to the player, but not yet unlocked.
   */
  REVEALED = 2,
  /**
   * The player has achieved the requirements for this achievement.
   */
  UNLOCKED = 3
};

/**
 * Values specifying whether an event is hidden to the player,
 * or visible to them.
 */

enum class GPG_EXPORT EventVisibility {
  /**
   * Not visible to the player.
   */
  HIDDEN = 1,
  /**
   * Visible to the player.
   */
  REVEALED = 2,
};

/**
 * Values specifying how whether larger or smaller scores should be interpreted
 * as better in the context of a leaderboard.
 */
enum class GPG_EXPORT LeaderboardOrder {
  /**
   * Larger is better.
   */
  LARGER_IS_BETTER = 1,
  /**
   * Smaller is better.
   */
  SMALLER_IS_BETTER = 2
};

/**
 * Values specifying whether rankings are displayed on a leaderboard in order
 * of score or player.
 */
enum class GPG_EXPORT LeaderboardStart {
  /**
   * Start the leaderboard at the top.
   */
  TOP_SCORES = 1,
  /**
   * Start the leaderboard at the player's score, centered.
   */
  PLAYER_CENTERED = 2
};

/**
 * Values that specify the period of time that a leaderboard covers.
 */
enum class GPG_EXPORT LeaderboardTimeSpan {
  /**
   * Daily.
   */
  DAILY = 1,
  /**
   * Weekly.
   */
  WEEKLY = 2,
  /**
   * All time.
   */
  ALL_TIME = 3
};

/**
 * Values that specify whether a leaderboard can be viewed by anyone with a
 * Google+ account (public), or only members of a player's Google+ circles
 * (social).
 */

enum class GPG_EXPORT LeaderboardCollection {
  /**
   * Visible to all.
   */
  PUBLIC = 1,
  /**
   * Visible to members of a player's social graph only.
   * @deprecated - Returned list will be empty.  See:
   * https://android-developers.googleblog.com/2016/12/games-authentication-adopting-google.html
   */
  SOCIAL = 2
};

/**
 * Values used to specify the state of a participant within a
 * <code>TurnBasedMatch</code>.
 */

enum class GPG_EXPORT ParticipantStatus {
  /**
   * Participant has been invited.
   */
  INVITED = 1,
  /**
   * Participant has joined.
   */
  JOINED = 2,
  /**
   * Participant has declined their invitation.
   */
  DECLINED = 3,
  /**
   * Participant has left the match.
   */
  LEFT = 4,
  /**
   * Participant has not yet been invited.
   */
  NOT_INVITED_YET = 5,
  /**
   * Participant has finished.
   */
  FINISHED = 6,
  /**
   * Participant is unresponsive.
   */
  UNRESPONSIVE = 7
};

/**
 * Values used to specify the outcome of a <code>TurnBasedMatch</code>
 * for a participant.
 */

enum class GPG_EXPORT MatchResult {
  /**
   * The particicpant did not agree to the match.
   */
  DISAGREED = 1,
  /**
   * The participant disconnected.
   */
  DISCONNECTED = 2,
  /**
   * The participant lost.
   */
  LOSS = 3,
  /**
   * There is no result.
   */
  NONE = 4,
  /**
   * The match was a draw.
   */
  TIE = 5,
  /**
   * The participant won.
   */
  WIN = 6
};

/**
 * Values used to specify the status of a <code>TurnBasedMatch</code> for the
 * local participant.
 */

enum class GPG_EXPORT MatchStatus {
  /**
   * Opponent(s) have been invited.
   */
  INVITED = 1,
  /**
   * It is not the participant's turn.
   */
  THEIR_TURN = 2,
  /**
   * It is the participant's turn.
   */
  MY_TURN = 3,
  /**
   * The match is pending completion.
   */
  PENDING_COMPLETION = 4,
  /**
   * The match is completed.
   */
  COMPLETED = 5,
  /**
   * The match was cancelled.
   */
  CANCELED = 6,
  /**
   * The match has expired.
   */
  EXPIRED = 7
};

/**
 * A struct containing flags which can be provided to
 * {@link QuestManager::FetchList} in order to filter the results returned.
 */
struct GPG_EXPORT QuestFetchFlags {
  enum : int32_t {
    UPCOMING = 1 << 0,
    OPEN = 1 << 1,
    ACCEPTED = 1 << 2,
    COMPLETED = 1 << 3,
    COMPLETED_NOT_CLAIMED = 1 << 4,
    EXPIRED = 1 << 5,
    ENDING_SOON = 1 << 6,
    FAILED = 1 << 7,
    ALL = -1
  };
};

/** Values used to specify the <code>Quest</code> state. */

enum class GPG_EXPORT QuestState {
  /**
   * The quest is upcoming.
   */
  UPCOMING = 1,
  /**
   * The quest is open.
   */
  OPEN = 2,
  /**
   * The quest has been accepted by the participant.
   */
  ACCEPTED = 3,
  /**
   * The quest has been completed.
   */
  COMPLETED = 4,
  /**
   * The quest has expired.
   */
  EXPIRED = 5,
  /**
   * The quest has been failed.
   */
  FAILED = 6
};

/** Values used to specify the QuestMilestone state. */

enum class GPG_EXPORT QuestMilestoneState {
  /**
   * The quest milestone has not been started.
   */
  NOT_STARTED = 1,  // Note that this value is new in v1.2.
  /**
   * The quest milestone has not been completed.
   */
  NOT_COMPLETED = 2,
  /**
   * The quest milestone has been completed, but not yet claimed.
   */
  COMPLETED_NOT_CLAIMED = 3,
  /**
   * The quest milestone has been claimed.
   */
  CLAIMED = 4
};

/**
 * Values used to specify the type of update being reported by a
 * multiplayer callback.
 *
 * @see <code>gpg::GameServices::Builder::SetOnTurnBasedMatchEvent</code>
 * @see <code>gpg::GameServices::Builder::SetOnMultiplayerInvitationEvent</code>
 */
enum class GPG_EXPORT MultiplayerEvent {
  /** A multiplayer match was updated while the app was running. */
  UPDATED = 1,
  /**
   * A multiplayer match was updated, and the app was launched in response to
   * this update.
   */
  UPDATED_FROM_APP_LAUNCH = 2,
  /** A match has been removed from the device, and should no longer be used. */
  REMOVED = 3
};

/**
 * @deprecated Prefer MultiplayerEvent.
 */
typedef MultiplayerEvent TurnBasedMultiplayerEvent GPG_DEPRECATED;

/**
 * Values which identify the type of a <code>MultiplayerInvitation</code>.
 */
enum class GPG_EXPORT MultiplayerInvitationType {
  /**
   * Turn-based multiplayer match.
   */
  TURN_BASED = 1,
  /**
   * Real-time multiplayer match.
   */
  REAL_TIME = 2
};

/**
 * Values representing the current status of a RealTimeRoom.
 */
enum class GPG_EXPORT RealTimeRoomStatus {
  /**
   * The room has one or more players that have been invited and have not
   * responded yet.
   */
  INVITING = 1,
  /**
   * The room is waiting for clients to connect to each other.
   */
  CONNECTING = 2,
  /**
   * One or more slots in the room are waiting to be filled by auto-matching.
   */
  AUTO_MATCHING = 3,
  /**
   * The room is active and connections are established.
   */
  ACTIVE = 4,
  /**
   * The room has been deleted.
   */
  DELETED = 5
};

/** Values used to specify the Snapshot conflict resolution policy.
 *
 * @see <code>gpg::SnapshotManager::Open</code>
 */
enum class GPG_EXPORT SnapshotConflictPolicy {
  /**
   * In the case of a conflict, the result will be returned to the app for
   * resolution.
   */
  MANUAL = 1,
  /**
   * In the case of a conflict, the snapshot with the longest played time will
   * be used.
   */
  LONGEST_PLAYTIME = 2,
  /**
   * In the case of a conflict, the last known good version of this snapshot
   * will be used.
   */
  LAST_KNOWN_GOOD = 3,
  /**
   * In the case of a conflict, the most recently modified version of this
   * snapshot will be used.
   */
  MOST_RECENTLY_MODIFIED = 4,
  /**
   * In the case of a conflict, the snapshot with the highest progress value
   * will be used.
   */
  HIGHEST_PROGRESS = 5
};

/**
 * Values indicating the type of video capture being performed.
 */
enum class GPG_EXPORT VideoCaptureMode {
  /**
   * An unknown value to return when capture mode is not available.
   */
  UNKNOWN = -1,
  /**
   * Capture device audio and video to a local file.
   */
  FILE = 0,
  /**
   * Capture device audio and video, and stream it live.
   * Not currently supported in the Native SDK.
   */
  STREAM = 1  // Not currently supported in the Native SDK.
};

/**
 * Values indicating the quality of the video capture.
 */
enum class GPG_EXPORT VideoQualityLevel {
  /**
   * An unknown value to return when quality level is not available.
   */
  UNKNOWN = -1,
  /**
   * SD quality: Standard def resolution (e.g. 480p) and a low bit rate
   * (e.g. 1-2Mbps).
   */
  SD = 0,
  /**
   * HD quality: DVD HD resolution (i.e. 720p) and a medium bit rate
   * (e.g. 3-4Mbps).
   */
  HD = 1,
  /**
   * Extreme HD quality: BluRay HD resolution (i.e. 1080p) and a high bit rate
   * (e.g. 6-8Mbps).
   */
  XHD = 2,
  /**
   * Full HD quality: 2160P resolution and high bit rate, e.g. 10-12Mbps.
   */
  FULLHD = 3
};

/**
 * Values indicating the state of the video capture overlay UI.
 */
enum class GPG_EXPORT VideoCaptureOverlayState {
  /**
   * State used to indicate that the state of the capture overlay is unknown.
   * This usually indicates an error.
   */
  UNKNOWN = -1,
  /**
   * State used to indicate that the capture overlay is drawn on the screen and
   * visible to the user.
   */
  SHOWN = 1,
  /**
   * State used to indicate that the user has initiated capture via the capture
   * overlay.
   */
  STARTED = 2,
  /**
   * State used to indicate that the user has stopped capturing via the capture
   * overlay.
   */
  STOPPED = 3,
  /**
   * State used to indicate that the user has dismissed the capture overlay and
   * it is no longer visible.
   */
  DISMISSED = 4
};

}  // namespace gpg

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop  // GCC diagnostic ignored "-Wattributes"
#endif

#endif  // GPG_TYPES_H_
