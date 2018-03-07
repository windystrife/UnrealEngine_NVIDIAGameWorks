// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/leaderboard.h
 *
 * @brief Value object that represents a single leaderboard.
 */

#ifndef GPG_LEADERBOARD_H_
#define GPG_LEADERBOARD_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class LeaderboardImpl;

/**
 * A single data structure allowing you to access data about the status of a
 * specific leaderboard, such as its name and validity.
 * @ingroup ValueType
 */
class GPG_EXPORT Leaderboard {
 public:
  Leaderboard();

  /**
   * Explicit constructor.
   */
  explicit Leaderboard(std::shared_ptr<LeaderboardImpl const> impl);

  /**
   * Copy constructor for copying an existing leaderboard into a new one.
   */
  Leaderboard(Leaderboard const &copy_from);

  /**
   * Constructor for moving an existing leaderboard into a new one.
   * r-value-reference version.
   */
  Leaderboard(Leaderboard &&move_from);

  /**
   * Assignment operator for assigning this leaderboard's value from another
   * leaderboard.
   */
  Leaderboard &operator=(Leaderboard const &copy_from);

  /**
   * Assignment operator for assigning this leaderboard's value from another
   * leaderboard.
   * r-value-reference version.
   */
  Leaderboard &operator=(Leaderboard &&move_from);
  ~Leaderboard();

  /**
   * Returns true when the returned leaderboard is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created leaderboard or for a populated one accompanied by
   * an unsuccessful response status.
   * It must be true for the getter functions on this leaderboard (id, Name,
   * Description, etc.) to be usable.
   */

  bool Valid() const;

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to a leaderboard in your game client.
   * It can only be called when Leaderboard::Valid() returns true.
   */
  std::string const &Id() const;

  /**
   * Returns the short name of the leaderboard. Up to 100 characters.
   * It can only be called when Achievement::Valid() returns true.
   */
  std::string const &Name() const;

  /**
   * Returns whether ascending or descending order is better.
   * Possible values are "LARGER_IS_BETTER" or "SMALLER_IS_BETTER".
   * LARGER_IS_BETTER places larger scores at the top (descending).
   * SMALLER_IS_BETTER places smaller scores at the top (ascending).
   * It can only be called when Achievement::Valid() returns true.
   */
  LeaderboardOrder Order() const;

  /**
   * Returns the URL leading to the image of the icon for this Leaderboard.
   * It can only be called when Leaderboard::Valid() returns true.
   */
  std::string const &IconUrl() const;

 private:
  std::shared_ptr<LeaderboardImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_LEADERBOARD_H_
