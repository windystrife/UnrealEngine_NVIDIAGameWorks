// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/turn_based_match.h
 *
 * @brief Value object that represents a turn based multiplayer match.
 */

#ifndef GPG_TURN_BASED_MATCH_H_
#define GPG_TURN_BASED_MATCH_H_

#include <string>
#include <vector>
#include "gpg/multiplayer_participant.h"
#include "gpg/participant_results.h"
#include "gpg/types.h"

namespace gpg {

class TurnBasedMatchImpl;

/**
 * A data structure containing data about the current state of a
 * <code>TurnBasedMatch</code>.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT TurnBasedMatch {
 public:
  TurnBasedMatch();

  /**
   * Constructs a <code>TurnBasedMatch</code> from a <code>shared_ptr</code> to
   * a <code>TurnBasedMatchImpl</code>.
   * Intended for internal use by the API.
   */
  explicit TurnBasedMatch(std::shared_ptr<TurnBasedMatchImpl const> impl);

  /**
   * Creates a copy of an existing <code>TurnBasedMatch</code>.
   */
  TurnBasedMatch(TurnBasedMatch const &copy_from);

  /**
   * Moves an existing <code>TurnBasedMatch</code>.
   */
  TurnBasedMatch(TurnBasedMatch &&move_from);

  /**
   * Assigns this <code>TurnBasedMatch</code> by copying from another one.
   */
  TurnBasedMatch &operator=(TurnBasedMatch const &copy_from);

  /**
   * Assigns this <code>TurnBasedMatch</code> by moving another one into it.
   */
  TurnBasedMatch &operator=(TurnBasedMatch &&move_from);

  /**
   * Returns true if this <code>TurnBasedMatch</code> is populated with data.
   * Must return true for the getter functions on the
   * <code>TurnBasedMatch</code> object (<code>Id</code>,
   * <code>CreationTime</code>, etc...) to be usable.
   */
  bool Valid() const;

  /**
   * Returns an ID that uniquely identifies this <code>TurnBasedMatch</code>.
   * Use with <code>TurnBasedMultiplayerManager::FetchMatch</code> to retrieve
   * this match at a later point. {@link Valid} must return true for this
   * function to be usable.
   */
  std::string const &Id() const;

  /**
   * Returns the time at which this <code>TurnBasedMatch was created</code>
   * (expressed as milliseconds since the Unix epoch). {@link Valid} must return
   * true for this function to be usable.
   */
  std::chrono::milliseconds CreationTime() const;

  /**
   * Returns the participant who created this match. {@link Valid} must return
   * true for this function to be usable.
   */
  MultiplayerParticipant CreatingParticipant() const;

  /**
   * Returns the time at which this <code>TurnBasedMatch</code> was last
   * updated (expressed as milliseconds since the Unix epoch). {@link Valid}
   * must return true for this function to be usable.
   */
  Timestamp LastUpdateTime() const;

  /**
   * Returns the participant who most recently updated this match. {@link Valid}
   * must return true for this function to be usable.
   */
  MultiplayerParticipant LastUpdatingParticipant() const;

  /**
   * Returns the participant whose turn it is to update this match.
   * {@link Valid} must return true for this function to be usable.
   */
  MultiplayerParticipant PendingParticipant() const;

  /**
   * Returns the status of the match for the local participant. The status
   * determines which actions can be taken on the match. {@link Valid} must
   * return true for this function to be usable.
   */
  MatchStatus Status() const;

  /**
   * Returns the number of available auto-matching slots for the match. This
   * number is equal to the number of auto-matching slots with which the match
   * was created, minus the number of participants who have already been added
   * via auto-matching. {@link Valid} must return true for this function to be
   * usable.
   */
  uint32_t AutomatchingSlotsAvailable() const;

  /**
   * Returns a game-specific variant identifier that can be used by a game to
   * identify different game modes. {@link Valid} must return true for
   * this function to be usable.
   */
  uint32_t Variant() const;

  /**
   * Returns a server-generated summary of the state of the match. {@link Valid}
   * must return true for this function to be usable.
   */
  std::string const &Description() const;

  /**
   * A number indicating how many matches preceded this one via rematching. Is
   * set to 1 on the first match, and incremented by one on each rematch.
   */
  uint32_t Number() const;

  /**
   * A strictly incrementing ID, updated each time a match is modified.
   */
  uint32_t Version() const;

  /**
   * Returns the results for the match. Results can be set via
   * <code>TurnBasedMultiplayerManager::TakeMyTurn</code>,
   * <code>TurnBasedMultiplayerManager::FinishDuringMyTurn</code>, and other
   * related functions. Always use
   * <code>ParticipantResults().WithResult(...)</code> to create a new
   * <code>ParticipantResults</code> object consistent with
   * any existing ones. {@link Valid} must return true for this function to be
   * usable.
   */
  gpg::ParticipantResults const &ParticipantResults() const;

  /**
   * A vector of all participants in this match. {@link Valid} must return true
   * for this function to be usable.
   */
  std::vector<MultiplayerParticipant> const &Participants() const;

  /**
   * True if this is the first turn of a rematch, and data from the previous
   * {@link Valid} must return true for this function to be usable.
   */
  bool HasPreviousMatchData() const;

  /**
   * Data from the previous match, if HasPreviousMatchData(). Can only be called
   * if {@link Valid} returns true.
   */
  std::vector<uint8_t> const &PreviousMatchData() const;

  /**
   * True if this object has data that were set during a previous turn.
   * {@link Valid} must return true for this function to be usable.
   */
  bool HasData() const;

  /**
   * Returns the match data, if <code>HasData()</code> is true. {@link Valid}
   * must return true for this function to be usable.
   */
  std::vector<uint8_t> const &Data() const;

  /**
   * Returns true if this match has been rematched.
   */
  bool HasRematchId() const;

  /**
   * Returns the ID of the match which is a rematch of this match, if available.
   */
  std::string const &RematchId() const;

  /**
   * A helper function which picks a valid participant from the set of joined,
   * invitable, and auto-matching participants. If this function is always used
   * to select the next participant, play will proceed through all participants
   * in order, repeating if necessary. This function must only be called if
   * {@link Status()} is {@link MatchStatus::MY_TURN}, as this is the only time
   * that the result of this function can be meaningfully used. If called
   * incorrectly, this function will return an invalid participant
   * (<code>MultiplayerParticipant::Valid() == false</code>).
   */
  MultiplayerParticipant SuggestedNextParticipant() const;

 private:
  friend class TurnBasedMatchImpl;
  std::shared_ptr<TurnBasedMatchImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_TURN_BASED_MATCH_H_
