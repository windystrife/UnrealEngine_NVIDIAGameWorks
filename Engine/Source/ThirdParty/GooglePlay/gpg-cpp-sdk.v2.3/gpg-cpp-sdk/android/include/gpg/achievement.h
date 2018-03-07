// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/achievement.h
 * @brief Value object that represents the completion of a task or goal.
 */

#ifndef GPG_ACHIEVEMENT_H_
#define GPG_ACHIEVEMENT_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <chrono>
#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class AchievementImpl;

/**
 * A single data structure which allows you to access data about the status of
 * a specific achievement. Data comprise two types: user-specific (e.g.,
 * whether the user has unlocked the achievement, etc.) and global (e.g.,
 * achievement name).
 * @ingroup ValueType
 */
class GPG_EXPORT Achievement {
 public:
  Achievement();
  /**
   * Constructs an Achievement from a <code>shared_ptr</code> to
   * an <code>AchievementImpl</code>.Intended for internal use by the API.
   */
  explicit Achievement(std::shared_ptr<AchievementImpl const> impl);

  /**
   * Creates a copy of an existing Achievement.
   */
  Achievement(Achievement const &copy_from);

  /**
   * Moves an existing Achievement.
   */
  Achievement(Achievement &&move_from);

  /**
   * Assigns this Achievement by copying from another one.
   */
  Achievement &operator=(Achievement const &copy_from);

  /**
   * Assigns this Achievement by moving another one into it.
   */
  Achievement &operator=(Achievement &&move_from);
  ~Achievement();

  /**
   * Returns true when the returned achievement is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created achievement or for a populated one accompanied by
   * an unsuccessful response status.
   * It must be true for the getter functions on this achievement (id, Name,
   * Description, etc.) to be usable.
   */
  bool Valid() const;

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to an achievement in your game client.
   * It can only be called when Achievement::Valid() returns true.
   */
  std::string const &Id() const;

  /**
   * Returns the short name of the achievement. Up to 100 characters.
   * It can only be called when Achievement::Valid() returns true.
   */
  std::string const &Name() const;

  /**
   * Returns a concise description of your achievement. Usually tells player how
   * to earn the achievement. Up to 500 characters.
   * It can only be called when Achievement::Valid() returns true.
   */
  std::string const &Description() const;

  /**
   * Returns the achievement type: <code>INCREMENTAL</code> or
   * <code>STANDARD</code>.
   * It can only be called when Achievement::Valid() returns true. More
   * information is available <a
   * href="../../common/concepts/achievements#incremental_achievements">
   * here</a>.
   */
  AchievementType Type() const;

  /**
   * Returns the achievement state: <code>HIDDEN</code>, <code>REVEALED</code>,
   * or <code>UNLOCKED</code>.
   * It can only be called when Achievement::Valid() returns true. More
   * information is available <a
   * href="../../common/concepts/achievements#state"> here</a>.
   */
  AchievementState State() const;

  /**
   * Returns the number of steps the player has taken toward unlocking an
   * incremental achievement.
   * It can only be called when Achievement::Valid() returns true.
   */
  uint32_t CurrentSteps() const;

  /**
   * Returns the number of steps required, in total, for the player to unlock a
   * given incremental achievement.
   * It can only be called when Achievement::Valid() returns true.
   */
  uint32_t TotalSteps() const;

  /**
   * The number of experience points awarded by this achievement.
   * It can only be called when Achievement::Valid() returns true.
   */
  uint64_t XP() const;

  /**
   * Returns the URL leading to the image of the revealed icon for this
   * Achievement.  This icon is intended to be shown when the Achievement has
   * been revealed, but not yet unlocked.
   * This function can only be called when Achievement::Valid() returns true.
   */
  std::string const &RevealedIconUrl() const;

  /**
   * Returns the URL leading to the image of the unlocked icon for this
   * Achievement.  This icon is intended to be shown when the Achievement has
   * been unlocked (and hence also revealed).
   * This function can only be called when Achievement::Valid() returns true.
   */
  std::string const &UnlockedIconUrl() const;

  /**
   * Returns time at which the entry was last modified (expressed as
   * milliseconds since the Unix epoch).
   * It can only be called when Achievement::Valid() returns true.
   */
  Timestamp LastModifiedTime() const;

  /**
   * @deprecated Prefer LastModifiedTime.
   */
  Timestamp LastModified() const GPG_DEPRECATED { return LastModifiedTime(); }

 private:
  std::shared_ptr<AchievementImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_ACHIEVEMENT_H_
