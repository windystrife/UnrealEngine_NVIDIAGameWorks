// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/real_time_room_config_builder.h
 *
 * @brief Builder that creates a RealTimeRoomConfig object.
 */

#ifndef GPG_REAL_TIME_ROOM_CONFIG_BUILDER_H_
#define GPG_REAL_TIME_ROOM_CONFIG_BUILDER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include <vector>
#include "gpg/real_time_multiplayer_manager.h"
#include "gpg/real_time_room_config.h"

namespace gpg {

/**
 * Builds one or more RealTimeRoomConfig objects.
 * @ingroup Managers
 */
class GPG_EXPORT RealTimeRoomConfig::Builder {
 public:
  Builder();

  /**
   * The minimum number of auto-matched players who can join the room.
   * Defaults to 0 when left unspecified. At least one auto-matching player or
   * player id must be added.
   */
  Builder &SetMinimumAutomatchingPlayers(uint32_t minimum_automatching_players);

  /**
   * The maximum number of auto-matched players who can join the room.
   * Left unspecified, defaults to a value equal to the minimum number of
   * auto-matching players.
   */
  Builder &SetMaximumAutomatchingPlayers(uint32_t maximum_automatching_players);

  /**
   * A developer-specific value used to indicate room type or mode. Only
   * players using the same variant can auto-match. Defaults to -1 when left
   * unspecified.
   */
  Builder &SetVariant(uint32_t variant);

  /**
   * A bit mask indicating exclusive roles for players. (For example,
   * one player as attacker, the other as defender.) Successful
   * auto-matching requires that the logical product (AND) of the bit mask of
   * any paired players equals 0.
   * Defaults to 0.
   */
  Builder &SetExclusiveBitMask(uint64_t exclusive_bit_mask);

  /**
   * Adds a player to the list of players to invite to the room. By default, no
   * players are added to the room. The builder cannot create a room until
   * players are added.
   */
  Builder &AddPlayerToInvite(std::string const &player_id);

  /**
   * Adds multiple players to the list of players to invite to the room. By
   * default, no players are added.
   */
  Builder &AddAllPlayersToInvite(std::vector<std::string> const &player_ids);

  /**
   * Populates values obtained by the
   * <code>RealTimeMultiplayerManager::PlayerSelectUIResponse</code> method.
   * Note that this does not populate the variant or the exclusive bit mask.
   */
  Builder &PopulateFromPlayerSelectUIResponse(
      RealTimeMultiplayerManager::PlayerSelectUIResponse const &response);

  /**
   * Creates a <code>RealTimeRoomConfig</code> object.
   */
  RealTimeRoomConfig Create() const;

 private:
  std::shared_ptr<RealTimeRoomConfigImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_REAL_TIME_ROOM_CONFIG_BUILDER_H_
