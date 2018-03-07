// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/score.h
 *
 * @brief Value object that represents a single leaderboard score.
 */

#ifndef GPG_SCORE_H_
#define GPG_SCORE_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"

namespace gpg {

class ScoreImpl;

/**
 * Single data structure which allows you to access data about a player's
 * score.
 * @ingroup ValueType
 */
class GPG_EXPORT Score {
 public:
  Score();

  /**
   * Explicit constructor.
   */
  explicit Score(std::shared_ptr<ScoreImpl const> impl);

  /**
   * Copy constructor for copying an existing score into a new one.
   */
  Score(Score const &copy_from);

  /**
   * Constructor for moving an existing score into a new one.
   * r-value-reference version.
   */
  Score(Score &&move_from);

  /**
   * Assignment operator for assigning this score's value from another score.
   */
  Score &operator=(Score const &copy_from);

  /**
   * Assignment operator for assigning this score's value from another score.
   * r-value-reference version.
   */
  Score &operator=(Score &&move_from);
  ~Score();

  /**
   * Returns true when the returned score is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created score or for a populated one accompanied by
   * an unsuccessful response status.
   * It must be true for the getter functions on this object to be usable.
   */
  bool Valid() const;

  /**
   * Returns rank of the player's score compared to those of other players.
   */
  uint64_t Rank() const;

  /**
   * Returns the player's score.
   */
  uint64_t Value() const;

  /**
   * Returns score-related developer-specified metadata, if any was set for
   * this score.
   */
  std::string const &Metadata() const;

 private:
  std::shared_ptr<ScoreImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_SCORE_H_
