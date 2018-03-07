// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/real_time_room_config.h
 * @brief A configuration object used to create a RealTimeRoom.
 */

#ifndef GPG_REAL_TIME_ROOM_CONFIG_H_
#define GPG_REAL_TIME_ROOM_CONFIG_H_

#include <string>
#include <vector>
#include "gpg/player.h"

namespace gpg {

class RealTimeRoomConfigImpl;

/**
 * A data structure containing the data needed to create a
 * <code>RealTimeRoom</code> object.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT RealTimeRoomConfig {
 public:
  /**
   * Constructs a <code>RealTimeRoomConfig</code> from object a
   * <code>shared_ptr</code> to a <code>RealTimeRoomConfigImpl</code> object.
   * Intended for internal use by the API.
   */
  explicit RealTimeRoomConfig(
      std::shared_ptr<RealTimeRoomConfigImpl const> impl);
  /**
   * A forward declaration of the Builder type.
   * For more information, see documentation on RealTimeRoomConfig::Builder.
   */
  class Builder;
  RealTimeRoomConfig();

  /**
   * Creates a copy of an existing <code>RealTimeRoomConfig</code> object.
   */
  RealTimeRoomConfig(RealTimeRoomConfig const &copy_from);

  /**
   * Moves an existing <code>RealTimeRoomConfig</code> object.
   */
  RealTimeRoomConfig(RealTimeRoomConfig &&move_from);

  /**
   * Assigns this <code>RealTimeRoomConfig</code> object by copying from another
   * one.
   */
  RealTimeRoomConfig &operator=(RealTimeRoomConfig const &copy_from);

  /**
   * Assigns this <code>RealTimeRoomConfig</code> object by moving another one
   * into it.
   */
  RealTimeRoomConfig &operator=(RealTimeRoomConfig &&move_from);

  /**
   * Returns true if this <code>RealTimeRoomConfig</code> object is populated
   * with data. Must return true for the getter functions
   * (<code>PlayerIdsToInvite</code>, <code>MinimumAutoroomingPlayers</code>,
   * etc.) on the <code>RealTimeRoomConfig</code> object to be usable.
   */
  bool Valid() const;

  /**
   * The player IDs to invite to the newly created room. Can only be called if
   * {@link Valid} returns true.
   */
  std::vector<std::string> const &PlayerIdsToInvite() const;

  /**
   * The minimum number of auto-matching players to add to the room. Can only
   * be called if {@link Valid} returns true.
   */
  uint32_t MinimumAutomatchingPlayers() const;

  /**
   * The maximum number of auto-matching players to add to the room. Can only
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
   * A developer-specific value used to indicate room type or mode. Only
   * players using the same value can room. Can only be called if
   * {@link Valid} returns true.
   */
  uint32_t Variant() const;

 private:
  std::shared_ptr<RealTimeRoomConfigImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_REAL_TIME_ROOM_CONFIG_H_
