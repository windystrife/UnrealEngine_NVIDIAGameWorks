// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/multiplayer_participant.h
 *
 * @brief A configuration object used to create a TurnBasedMatch.
 */

#ifndef GPG_TURN_BASED_MATCH_CONFIG_H_
#define GPG_TURN_BASED_MATCH_CONFIG_H_

#include <string>
#include <vector>
#include "gpg/player.h"

namespace gpg {

class TurnBasedMatchConfigImpl;

/**
 * A data structure containing the data needed to create a
 * <code>TurnBasedMatch</code>.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT TurnBasedMatchConfig {
 public:
  /**
   * Constructs a <code>TurnBasedMatchConfig</code> from a
   * <code>shared_ptr</code> to a <code>TurnBasedMatchConfigImpl</code>.
   * Intended for internal use by the API.
   */
  explicit TurnBasedMatchConfig(
      std::shared_ptr<TurnBasedMatchConfigImpl const> impl);
  /**
   * A forward declaration of the Builder type.
   * For more information, see documentation on TurnBasedMatchConfig::Builder.
   */
  class Builder;
  TurnBasedMatchConfig();

  /**
   * Creates a copy of an existing <code>TurnBasedMatchConfig</code>.
   */
  TurnBasedMatchConfig(TurnBasedMatchConfig const &copy_from);

  /**
   * Moves an existing <code>TurnBasedMatchConfig</code>.
   */
  TurnBasedMatchConfig(TurnBasedMatchConfig &&move_from);

  /**
   * Assigns this <code>TurnBasedMatchConfig</code> by copying from another one.
   */
  TurnBasedMatchConfig &operator=(TurnBasedMatchConfig const &copy_from);

  /**
   * Assigns this <code>TurnBasedMatchConfig</code> by moving another one into
   * it.
   */
  TurnBasedMatchConfig &operator=(TurnBasedMatchConfig &&move_from);

  /**
   * Returns true if this <code>TurnBasedMatchConfig</code> is populated with
   * data. Must return true for the getter functions
   * (<code>PlayerIdsToInvite</code>, <code>MinimumAutomatchingPlayers</code>,
   * etc.) on the <code>TurnBasedMatchConfig</code> object to be usable.
   */
  bool Valid() const;

  /**
   * The player IDs to invite to the newly created match. Can only be called if
   * {@link Valid} returns true.
   */
  std::vector<std::string> const &PlayerIdsToInvite() const;

  /**
   * The minimum number of auto-matching players to add to the match. Can only
   * be called if {@link Valid} returns true.
   */
  uint32_t MinimumAutomatchingPlayers() const;

  /**
   * The maximum number of auto-matching players to add to the match. Can only
   * be called if {@link Valid} returns true.
   */
  uint32_t MaximumAutomatchingPlayers() const;

  /**
   * A bit mask indicating game-specific exclusive roles for the player, such as
   * "attacker" or "defender". The logical product (AND) of any pairing players
   * must equal zero for auto-match. Can only be called if {@link Valid}
   * returns true.
   */
  int64_t ExclusiveBitMask() const;

  /**
   * A developer-specific value used to indicate match type or mode. Only
   * players using the same value can match. Can only be called if
   * {@link Valid} returns true.
   */
  uint32_t Variant() const;

 private:
  std::shared_ptr<TurnBasedMatchConfigImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_TURN_BASED_MATCH_CONFIG_H_
