// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/multiplayer_participant.h
 *
 * @brief Value object that represents a participant in a multiplayer match.
 */

#ifndef GPG_MULTIPLAYER_PARTICIPANT_H_
#define GPG_MULTIPLAYER_PARTICIPANT_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/player.h"
#include "gpg/types.h"

namespace gpg {

class MultiplayerParticipantImpl;

/**
 * A data structure containing data about a participant in a multiplayer match.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT MultiplayerParticipant {
 public:
  MultiplayerParticipant();

  /**
   * Constructs a <code>MultiplayerParticipant</code> from a
   * <code>shared_ptr</code> to a <code>MultiplayerParticipantImpl</code>.
   * Intended for internal use by the API.
   */
  explicit MultiplayerParticipant(
      std::shared_ptr<MultiplayerParticipantImpl const> impl);

  /**
   * Creates a copy of an existing <code>MultiplayerParticipant</code>.
   */
  MultiplayerParticipant(MultiplayerParticipant const &copy_from);

  /**
   * Moves an existing <code>MultiplayerParticipant</code>.
   */
  MultiplayerParticipant(MultiplayerParticipant &&move_from);

  /**
   * Assigns this <code>MultiplayerParticipant</code> by copying from another
   * one.
   */
  MultiplayerParticipant &operator=(MultiplayerParticipant const &copy_from);

  /**
   * Assigns this <code>MultiplayerParticipant</code> by moving another one
   * into it.
   */
  MultiplayerParticipant &operator=(MultiplayerParticipant &&move_from);
  ~MultiplayerParticipant();

  /**
   * Returns true if this <code>MultiplayerParticipant</code> is populated
   * with data. Must be true in order for the getter functions
   * (<code>DisplayName</code>, <code>AvatarUrl</code>, <code>Id</code>, etc.)
   * on this <code>MultiplayerParticipant</code> to be usable.
   */
  bool Valid() const;

  /**
   * The display name for this <code>MultiplayerParticipant</code>.
   * {@link Valid} must return true for this function to be usable.
   */
  std::string const &DisplayName() const;

  /**
   * Returns the URL where the image of this
   * <code>MultiplayerParticipant</code>'s avatar resides. The
   * <code>ImageResolution</code> parameter specifies the resolution of the
   * returned image. Specify either ICON or HI_RES for the resolution.
   * {@link Valid} must return true for this function
   * to be usable.
   */
  std::string const &AvatarUrl(ImageResolution resolution) const;

  /**
   * The <code>Id</code> of this <code>MultiplayerParticipant</code>.
   * <code>Id</code>'s are only valid in the scope of a single match, and are
   * different from <code>Player::Id()</code>'s. {@link Valid} must return true
   * for this function to be usable.
   */
  std::string const &Id() const;

  /**
   * Whether a Player is associated with this
   * <code>MultiplayerParticipant</code>. {@link Valid} must return true for
   * this function to be usable.
   */
  bool HasPlayer() const;

  /**
   * The Player associated with this
   * <code>MultiplayerParticipant</code>. {@link Valid} and
   * {@link HasPlayer} must both return true for this function to be usable.
   */
  gpg::Player Player() const;

  /**
   * The status of this <code>MultiplayerParticipant</code> with respect to the
   * match. {@link Valid} must return true for this function to be usable.
   */
  ParticipantStatus Status() const;

  /**
   * Whether this participant has a result for this match. If false,
   * {@link MatchResult} and {@link MatchRank} do not return valid data.
   * {@link Valid} must return true for this function to be usable.
   */
  bool HasMatchResult() const;

  /**
   * The result of the match for this <code>MultiplayerParticipant</code>.
   * {@link Valid} must return true for this function to be usable. If
   * <code>HasMatchResult()</code> does not return true, this function returns
   * <code>MatchResult::None</code>.
   */
  gpg::MatchResult MatchResult() const;

  /**
   * The rank for this <code>MultiplayerParticipant</code> within its match.
   * {@link Valid} must return true for this function to be usable. If
   * <code>HasMatchResult()</code> does not return true, this function returns
   * 0.
   */
  uint32_t MatchRank() const;

  /**
   * Whether this participant is connected to a <code>RealTimeRoom</code>.
   * Always false if this is a participant from a <code>TurnBasedMatch</code>.
   */
  bool IsConnectedToRoom() const;

 private:
  friend class MultiplayerParticipantImpl;
  std::shared_ptr<MultiplayerParticipantImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_MULTIPLAYER_PARTICIPANT_H_
