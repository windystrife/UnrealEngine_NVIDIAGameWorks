// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/turn_based_match_config_builder.h
 *
 * @brief Builder which creates a TurnBasedMatchConfig.
 */

#ifndef GPG_TURN_BASED_MATCH_CONFIG_BUILDER_H_
#define GPG_TURN_BASED_MATCH_CONFIG_BUILDER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include <vector>
#include "gpg/turn_based_match_config.h"
#include "gpg/turn_based_multiplayer_manager.h"

namespace gpg {

/**
 * Builds one or more TurnBasedMatchConfig objects.
 * @ingroup Managers
 */
class GPG_EXPORT TurnBasedMatchConfig::Builder {
 public:
  Builder();

  /**
   * The minimum number of auto-matched players who can join the match.
   * Defaults to 0 when left unspecified. At least one auto-matching player or
   * player id must be added.
   */
  Builder &SetMinimumAutomatchingPlayers(uint32_t minimum_automatching_players);

  /**
   * The maximum number of auto-matched players who can join the match.
   * Left unspecified, defaults to a value equal to the minimum number of
   * auto-matching players.
   */
  Builder &SetMaximumAutomatchingPlayers(uint32_t maximum_automatching_players);

  /**
   * A developer-specific value used to indicate match type or mode. Only
   * players using the same variant can auto-match. Defaults to -1 when left
   * unspecified.
   */
  Builder &SetVariant(uint32_t variant);

  /**
   * A bit mask indicating exclusive roles for players. (E.g. For example, if
   * one player is the attacker, the other must be the defender.) Successful
   * auto-matching requires that the logical product (AND) of the bit mask of
   * any pairing players equals zero.
   * Defaults to 0.
   */
  Builder &SetExclusiveBitMask(uint64_t exclusive_bit_mask);

  /**
   * Adds a player to the list of players to invite to the match. By default, no
   * players are added to the match. The builder cannot create a match until
   * players are added.
   */
  Builder &AddPlayerToInvite(std::string const &player_id);

  /**
   * Adds multiple players to the list of players to invite to the match. By
   * default, no players are added.
   */
  Builder &AddAllPlayersToInvite(std::vector<std::string> const &player_ids);

  /**
   * Populates values from a
   * <code>TurnBasedMultiplayerManager::ShowPlayerSelectUIResponse</code>. Note
   * that this does not populate the variant or the exclusive bit mask.
   */
  Builder &PopulateFromPlayerSelectUIResponse(
      TurnBasedMultiplayerManager::PlayerSelectUIResponse const &response);

  /**
   * Creates a <code>TurnBasedMatchConfig</code> object.
   */
  TurnBasedMatchConfig Create() const;

 private:
  std::shared_ptr<TurnBasedMatchConfigImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_TURN_BASED_MATCH_CONFIG_BUILDER_H_
