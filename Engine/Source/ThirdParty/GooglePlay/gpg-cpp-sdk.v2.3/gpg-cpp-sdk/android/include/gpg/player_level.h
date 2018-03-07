// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/player_level.h
 *
 * @brief Value object that represents a player level.
 */

#ifndef GPG_PLAYER_LEVEL_H_
#define GPG_PLAYER_LEVEL_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class PlayerLevelImpl;

/**
 * A single data structure containing data about player's level.
 * @ingroup ValueType
 */
class GPG_EXPORT PlayerLevel {
 public:
  PlayerLevel();

  /**
   * Constructs a PlayerLevel from a shared_ptr to a PlayerLevelImpl. This is
   * used internally, and is not intended for use by consumers of this API.
   */
  explicit PlayerLevel(std::shared_ptr<PlayerLevelImpl const> impl);

  /**
   * Copy constructor for copying an existing player level into a new one.
   */
  PlayerLevel(PlayerLevel const &copy_from);

  /**
   * Constructor for moving an existing player level into a new one.
   * r-value-reference version.
   */
  PlayerLevel(PlayerLevel &&move_from);

  /**
   * Assignment operator for assigning this player level's value from
   * another player level.
   */
  PlayerLevel &operator=(PlayerLevel const &copy_from);

  /**
   * Assignment operator for assigning this player level's value
   * from another player level.
   * r-value-reference version
   */
  PlayerLevel &operator=(PlayerLevel &&move_from);
  ~PlayerLevel();

  /**
   * Returns true when the returned player level is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created player or for a populated one accompanied by
   * an unsuccessful response status.
   * It must be true for the getter functions on this object to be usable.
   */
  bool Valid() const;

  /**
   * Returns the number for this level, e.g. "level 10".
   */
  uint32_t LevelNumber() const;

  /**
   * Returns the minimum XP value needed to attain this level, inclusive.
   */
  uint64_t MinimumXP() const;

  /**
   * Returns the maximum XP value represented by this level, exclusive.
   */
  uint64_t MaximumXP() const;

 private:
  friend class PlayerLevelImpl;
  std::shared_ptr<PlayerLevelImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_PLAYER_LEVEL_H_
