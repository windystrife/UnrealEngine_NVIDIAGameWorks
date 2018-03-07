// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/debug.h
 *
 * @brief Helper functions that support logging of Game Services types.
 * @brief DebugString is provided for major public types, converting them to
 * log-friendly strings.
 * @brief Additionally, operator<< is provided for each of these types for easy
 * use with streams.commands.
 * @note These strings are for use only in logging and debugging. They are
 * not intended as user-facing output.
 */

#ifndef GPG_DEBUG_H_
#define GPG_DEBUG_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <string>
#include "gpg/common.h"
#include "gpg/common_error_status.h"
#include "gpg/types.h"

#ifndef GPG_SPECIFIC_DEBUG

#include "gpg/status.h"
#define ACHIEVEMENTS_DEBUG
#define EVENTS_DEBUG
#define LEADERBOARDS_DEBUG
#define MULTIPLAYER_DEBUG
#define PLAYERS_DEBUG
#define QUESTS_DEBUG
#define SNAPSHOTS_DEBUG
#endif

#ifdef ACHIEVEMENTS_DEBUG

#include "gpg/achievement.h"

namespace gpg {

// Achievements

/// Returns a human-readable achievement type.
std::string DebugString(AchievementType type) GPG_EXPORT;

/// Returns a human-readable achievement state.
std::string DebugString(AchievementState state) GPG_EXPORT;

/// Returns a human-readable achievement.
std::string DebugString(Achievement const &achievement) GPG_EXPORT;

/// Writes a human-readable achievement type to an output stream.
std::ostream &operator<<(std::ostream &os, AchievementType type) GPG_EXPORT;

/// Writes a human-readable achievement state to an output stream.
std::ostream &operator<<(std::ostream &os, AchievementState state) GPG_EXPORT;

/// Writes a human-readable achievement to an output stream.
std::ostream &operator<<(std::ostream &os,
                         Achievement const &achievement) GPG_EXPORT;

}  // namespace gpg
#endif

#ifdef EVENTS_DEBUG

#include "gpg/event.h"
namespace gpg {
// Events

/// Returns all event data in human-readable form.
std::string DebugString(Event const &event) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Event const &event) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, EventVisibility vis) GPG_EXPORT;

}  // namespace gpg

#endif

#ifdef LEADERBOARDS_DEBUG

#include "gpg/leaderboard.h"

#include "gpg/score.h"
#include "gpg/score_page.h"
#include "gpg/score_summary.h"

namespace gpg {

// Leaderboards

/// Returns the value of order in human-readable form.
std::string DebugString(LeaderboardOrder order) GPG_EXPORT;

/// Returns all leaderboard data in human-readable form.
std::string DebugString(Leaderboard const &leaderboard) GPG_EXPORT;

/// Returns the value of start in human-readable form.
std::string DebugString(LeaderboardStart start) GPG_EXPORT;

/// Returns the value of time_span in human-readable form.
std::string DebugString(LeaderboardTimeSpan time_span) GPG_EXPORT;

/// Returns the value of collection in human-readable form.
std::string DebugString(LeaderboardCollection collection) GPG_EXPORT;

/// Returns all score data in human-readable form.
std::string DebugString(Score const &score) GPG_EXPORT;

/// Returns all score-page data in human-readable form.
std::string DebugString(ScorePage const &score_page) GPG_EXPORT;

/// Returns all score entries in human-readable form.
std::string DebugString(ScorePage::Entry const &entry) GPG_EXPORT;

/// Returns all score summary data in human-readable form.
std::string DebugString(ScoreSummary const &summary) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, LeaderboardOrder order) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         Leaderboard const &leaderboard) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, LeaderboardStart start) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         LeaderboardTimeSpan time_span) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         LeaderboardCollection collection) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Score const &score) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         ScorePage const &score_page) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         ScorePage::Entry const &entry) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         ScoreSummary const &summary) GPG_EXPORT;

}  // namespace gpg

#endif

#ifdef MULTIPLAYER_DEBUG
#include "gpg/multiplayer_invitation.h"
#include "gpg/multiplayer_participant.h"
#include "gpg/real_time_room.h"
#include "gpg/turn_based_match.h"
#include "gpg/turn_based_match_config.h"

namespace gpg {

// Multiplayer

/// Returns data for a turn-based match in human-readable form.
std::string DebugString(TurnBasedMatch const &match) GPG_EXPORT;

/// Returns data for a multiplayer invitation in human-readable form.
std::string DebugString(MultiplayerInvitation const &invitation) GPG_EXPORT;

/// Returns data for a turn-based match configuration object in human-readable
/// form.
std::string DebugString(TurnBasedMatchConfig const &config) GPG_EXPORT;

/// Returns a multiplayer participant in human-readable form.
std::string DebugString(MultiplayerParticipant const &participant) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         TurnBasedMatch const &match) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         MultiplayerInvitation const &invitation) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         TurnBasedMatchConfig const &config) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         MultiplayerParticipant const &participant) GPG_EXPORT;

/// Returns RealTimeRoom in human-readable form
std::string DebugString(RealTimeRoom room) GPG_EXPORT;

/// Returns RealTimeRoomStatus in human-readable form
std::string DebugString(RealTimeRoomStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         RealTimeRoomStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, RealTimeRoom room) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, ParticipantStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, MatchResult result) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, MatchStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, MultiplayerEvent event) GPG_EXPORT;

}  // namespace gpg

#endif

#ifdef PLAYERS_DEBUG

#include "gpg/player.h"
#include "gpg/player_level.h"

namespace gpg {
// Players

/// Returns all player data in human-readable form.
std::string DebugString(Player const &player) GPG_EXPORT;

/// Returns player level info in human-readable form.
std::string DebugString(PlayerLevel const &player) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Player const &player) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         PlayerLevel const &player) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, ImageResolution res) GPG_EXPORT;

}  // namespace gpg

#endif

#ifdef QUESTS_DEBUG

#include "gpg/quest.h"
#include "gpg/quest_milestone.h"

namespace gpg {
// Quests

/// Returns a human-readable quest state.
std::string DebugString(QuestState const &quest_state) GPG_EXPORT;

/// Returns a human-readable milestone state.
std::string DebugString(QuestMilestoneState const &milestone_state) GPG_EXPORT;

/// Returns all quest data in human-readable form.
std::string DebugString(Quest const &quest) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         QuestState const &quest_state) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         QuestMilestoneState const &milestone_state) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Quest const &quest) GPG_EXPORT;

}  // namespace gpg

#endif

#ifdef SNAPSHOTS_DEBUG

#include "gpg/snapshot_metadata.h"
#include "gpg/snapshot_metadata_change.h"
#include "gpg/snapshot_metadata_change_cover_image.h"

namespace gpg {
// Snapshots

/// Returns data from a snapshot cover image in human-readable form.
std::string DebugString(SnapshotMetadataChange::CoverImage const &image)
    GPG_EXPORT;

/// Returns data from a snapshot metadata object in human-readable form.
std::string DebugString(SnapshotMetadata const &metadata) GPG_EXPORT;

/// Returns data from a snapshot metadata change in human-readable form.
std::string DebugString(SnapshotMetadataChange const &change) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         SnapshotMetadataChange::CoverImage const &image)
    GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         SnapshotMetadata const &metadata) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         SnapshotMetadataChange const &change) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         SnapshotConflictPolicy policy) GPG_EXPORT;
}  // namespace gpg

#endif

namespace gpg {
// Status

/// Returns a BaseStatus in human-readable form.
std::string DebugString(BaseStatus::StatusCode status) GPG_EXPORT;

/// Returns authorization status in human-readable form.
std::string DebugString(AuthStatus status) GPG_EXPORT;

/// Returns response status in human-readable form.
std::string DebugString(ResponseStatus status) GPG_EXPORT;

/// Returns flush status in human-readable form.
std::string DebugString(FlushStatus status) GPG_EXPORT;

/// Returns UI status in human-readable form.
std::string DebugString(UIStatus status) GPG_EXPORT;

/// Returns multiplayer status in human-readable form.
std::string DebugString(MultiplayerStatus status) GPG_EXPORT;

/// Returns quest accept status in human-readable form.
std::string DebugString(QuestAcceptStatus status) GPG_EXPORT;

/// Returns quest claim milestone status in human-readable form.
std::string DebugString(QuestClaimMilestoneStatus status) GPG_EXPORT;

/// Returns snapshot open status in human-readable form.
std::string DebugString(SnapshotOpenStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         BaseStatus::StatusCode status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, AuthStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, ResponseStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, FlushStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, UIStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, MultiplayerStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, QuestAcceptStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         QuestClaimMilestoneStatus status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os,
                         SnapshotOpenStatus status) GPG_EXPORT;

}  // namespace gpg

namespace gpg {
// Types

/// Returns timeout in human-readable form.
std::string DebugString(Timeout timeout) GPG_EXPORT;

/// Returns timestamp in human-readable form.
std::string DebugString(Timestamp timestamp) GPG_EXPORT;

/// Returns duration in human-readable form.
std::string DebugString(Duration duration) GPG_EXPORT;

/// Returns data source in human-readable form.
std::string DebugString(DataSource source) GPG_EXPORT;

/// Returns log level in human-readable form.
std::string DebugString(LogLevel level) GPG_EXPORT;

/// Returns authorization operation in human-readable form.
std::string DebugString(AuthOperation op) GPG_EXPORT;

/// Returns image resolution selection in human-readable form.
std::string DebugString(ImageResolution res) GPG_EXPORT;

/// Returns event visibility in human-readable form
std::string DebugString(EventVisibility vis) GPG_EXPORT;

/// Returns participant status in human-readable form
std::string DebugString(ParticipantStatus status) GPG_EXPORT;

/// Returns match result in human-readable form
std::string DebugString(MatchResult result) GPG_EXPORT;

/// Returns match status in human-readable form
std::string DebugString(MatchStatus status) GPG_EXPORT;

/// Returns turn-based multiplayer event in human-readable form
std::string DebugString(MultiplayerEvent event) GPG_EXPORT;

/// Returns snapshot conflict policy in human-readable form
std::string DebugString(SnapshotConflictPolicy policy) GPG_EXPORT;

/// Returns video capture mode in human-readable form
std::string DebugString(VideoCaptureMode mode) GPG_EXPORT;

/// Returns video quality level in human-readable form
std::string DebugString(VideoQualityLevel level) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Timeout timeout) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Timestamp timestamp) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, Duration duration) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, DataSource status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, LogLevel status) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, AuthOperation op) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, VideoCaptureMode mode) GPG_EXPORT;

/// Provided for easy use of the corresponding debug string with streams.
std::ostream &operator<<(std::ostream &os, VideoQualityLevel level) GPG_EXPORT;

}  // namespace gpg

#endif  // GPG_DEBUG_H_
