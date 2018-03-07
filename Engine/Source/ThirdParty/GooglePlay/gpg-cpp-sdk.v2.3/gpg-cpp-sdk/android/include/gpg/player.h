// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/player.h
 *
 * @brief Value object that represents a player.
 */

#ifndef GPG_PLAYER_H_
#define GPG_PLAYER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class PlayerImpl;
class PlayerLevel;

/**
 * A data structure that allows you to access data about a specific player.
 * @ingroup ValueType
 */
class GPG_EXPORT Player {
 public:
  Player();

  /**
   * Constructs a Player from a <code>shared_ptr</code> to a
   * <code>PlayerImpl</code>.
   * Intended for internal use by the API.
   */
  explicit Player(std::shared_ptr<PlayerImpl const> impl);

  /**
   * Creates a copy of an existing Player.
   */
  Player(Player const &copy_from);

  /**
   * Moves an existing Player into a new one.
   */
  Player(Player &&move_from);

  /**
   * Assigns this Player by copying from another one.
   */
  Player &operator=(Player const &copy_from);

  /**
   * Assigns this Player value by moving another one into it.
   */
  Player &operator=(Player &&move_from);
  ~Player();

  /**
   * Returns true when the returned player is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created player or for a populated one accompanied by
   * an unsuccessful response status.
   * It must return true for the getter functions on this object to be usable.
   */
  bool Valid() const;

  /**
   * Returns the <code>Id</code> of the currently signed-in player.
   * <code>Player::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &Id() const;
  /**
   * Returns the Google+ name of the currently signed-in player.
   * <code>Player::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &Name() const;

  /**
   * Returns the URL where the image of this Player's avatar
   * resides. The <code>ImageResolution</code> parameter specifies the
   * resolution of the image. <code>Player::Valid()</code> must return true for
   * this function to be usable.
   */
  std::string const &AvatarUrl(ImageResolution resolution) const;

  /**
   * Returns whether or not this player has level information available. If
   * it returns false, <code>CurrentLevel()</code> and <code>NextLevel()</code>
   * return <code>PlayerLevel</code> objects that are not valid.
   */
  bool HasLevelInfo() const;

  /**
   * Retrieves the current level data for this player, if known. If
   * HasLevelInfo() returns false, this will return a PlayerLevel object for
   * which Valid() also returns false.
   */
  PlayerLevel const &CurrentLevel() const;

  /**
   * Retrieves the next level data for this player, if known. If
   * HasLevelInfo() returns false, this will return a PlayerLevel object for
   * which Valid() also returns false. This is the level that the player is
   * currently working towards. If the player is already at
   * the maximum level they can reach, CurrentLevel() and NextLevel() will
   * return identical values.
   */
  PlayerLevel const &NextLevel() const;

  /**
   * Retrieves the player's current XP total. If HasLevelInfo() returns false,
   * this will return zero. If HasLevelInfo() returns true, the player's current
   * XP total will be in the range CurrentLevel().MinimumXP to
   * CurrentLevel().MaximumXP.
   */
  uint64_t CurrentXP() const;

  /**
   * Retrieves the timestamp at which this player last leveled up. If
   * HasLevelInfo() returns false, or if the player has never leveled up, this
   * will return zero (the epoch).
   */
  Timestamp LastLevelUpTime() const;

  /**
   * Retrieves the title of the player. This is based on actions the player has
   * taken across the Google Play games ecosystem. Note that not all players
   * have titles, and that a player's title may change over time. If a player
   * does not have a title, Title() will return an empty string.
   */
  std::string const &Title() const;

 private:
  friend class PlayerImpl;
  std::shared_ptr<PlayerImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_PLAYER_H_
