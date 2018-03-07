// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/multiplayer_invitation.h
 * @brief Value object that represents an invitation to a turn based match.
 */

#ifndef GPG_MULTIPLAYER_INVITATION_H_
#define GPG_MULTIPLAYER_INVITATION_H_

#include <string>
#include <vector>
#include "gpg/multiplayer_participant.h"
#include "gpg/types.h"

namespace gpg {

class TurnBasedMatchImpl;
class RealTimeRoomImpl;

/**
 * A data structure containing data about the current state of an invitation to
 * a turn-based match.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT MultiplayerInvitation {
 public:
  MultiplayerInvitation();

  /**
   * Constructs a MultiplayerInvitation from a
   * <code>shared_ptr</code> to a <code>TurnBasedMatchImpl</code>.
   * Intended for internal use by the API.
   */
  explicit MultiplayerInvitation(
      std::shared_ptr<TurnBasedMatchImpl const> impl);

  /**
   * Constructs a MultiplayerInvitation from a
   * <code>shared_ptr</code> to a <code>RealTimeRoomImpl</code>.
   * Intended for internal use by the API.
   */
  explicit MultiplayerInvitation(std::shared_ptr<RealTimeRoomImpl const> impl);

  /**
   * Creates a copy of an existing MultiplayerInvitation.
   */
  MultiplayerInvitation(MultiplayerInvitation const &copy_from);

  /**
   * Moves an existing MultiplayerInvitation into a new one.
   */
  MultiplayerInvitation(MultiplayerInvitation &&move_from);

  /**
   * Assigns this MultiplayerInvitation by copying from another
   * one.
   */
  MultiplayerInvitation &operator=(MultiplayerInvitation const &copy_from);

  /**
   * Assigns this MultiplayerInvitation by moving another one into
   * it.
   */
  MultiplayerInvitation &operator=(MultiplayerInvitation &&move_from);

  /**
   * Returns true if this MultiplayerInvitation is populated with
   * data. Must be true in order for the getter functions (<code>Id</code>,
   * <code>Variant</code>, etc.) on this MultiplayerInvitation
   * object to be usable.
   */
  bool Valid() const;

  /**
   * Returns an ID that uniquely identifies this
   * MultiplayerInvitation. {@link Valid} must return true for this
   * function to be usable.
   */
  std::string const &Id() const;

  /**
   * Returns a game-specific variant identifier that a game can use to identify
   * game mode. {@link Valid} must return true for this function to be usable.
   */
  uint32_t Variant() const;

  /**
   * Returns the number of available auto-matching slots for the match for which
   * this object is an invitation. This value is equal to the number of
   * auto-matching slots with which the match was created, minus the number of
   * participants who have already been added via auto-matching. {@link Valid}
   * must return true for this function to be usable.
   */
  uint32_t AutomatchingSlotsAvailable() const;

  /**
   * Returns the time at which the TurnBasedMatch for this
   * invitation was created (expressed as milliseconds since the Unix epoch).
   * {@link Valid} must return true for this function to be usable.
   */
  Timestamp CreationTime() const;

  /**
   * Returns the participant who invited the local participant to the
   * TurnBasedMatch for this invitation. {@link Valid} must return
   * true for this function to be usable.
   */
  MultiplayerParticipant InvitingParticipant() const;

  /**
   * A vector of all participants in the TurnBasedMatch for this
   * invitation. {@link Valid} must return true for this function to be usable.
   */
  std::vector<MultiplayerParticipant> const &Participants() const;

  /**
   * Identifies whether this invitation is for a RealTimeRoom or a
   * TurnBasedMatch.
   */
  MultiplayerInvitationType Type() const;

 private:
  friend class TurnBasedMatchImpl;
  friend class RealTimeRoomImpl;

  std::shared_ptr<TurnBasedMatchImpl const> turn_based_match_impl_;
  std::shared_ptr<RealTimeRoomImpl const> real_time_room_impl_;
};

}  // namespace gpg

#endif  // GPG_MULTIPLAYER_INVITATION_H_
