// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/quest_milestone.h
 *
 * @brief Value object that represents a single guest.
 */

#ifndef GPG_QUEST_MILESTONE_H_
#define GPG_QUEST_MILESTONE_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include <vector>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class QuestImpl;

/**
 * A single data structure allowing you to access data about the status of a
 * specific quest milestone.
 * @ingroup ValueType
 */
class GPG_EXPORT QuestMilestone {
 public:
  QuestMilestone();

  /**
   * Constructs a <code>QuestMilestone</code> from a shared_ptr to a
   * <code>QuestMilestoneImpl</code>.
   * Intended for internal use by the API.
   */
  explicit QuestMilestone(std::shared_ptr<QuestImpl const> impl);

  /**
   * Creates a copy of an existing <code>QuestMilestone</code>.
   */
  QuestMilestone(QuestMilestone const &copy_from);

  /**
   * Moves an existing <code>QuestMilestone</code>.
   */
  QuestMilestone(QuestMilestone &&move_from);

  /**
   * Assigns this <code>QuestMilestone</code> by copying from another one.
   */
  QuestMilestone &operator=(QuestMilestone const &copy_from);

  /**
   * Assigns this <code>QuestMilestone</code> by moving another one into it.
   */
  QuestMilestone &operator=(QuestMilestone &&move_from);
  ~QuestMilestone();

  /**
   * Returns true when the returned milestone is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created milestone or for a populated one accompanied by
   * an unsuccessful response status.
   * This function must return true for the getter functions on this object
   * to be usable.
   */
  bool Valid() const;

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to a milestone in your game client.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  std::string const &Id() const;

  /**
   * Returns the id for the quest that owns this milestone. This is a unique
   * string that the Google Play Developer Console generated beforehand.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  std::string const &QuestId() const;

  /**
   * Returns the id for the event associated with this milestone. Incrementing
   * this event will progress the milestone. This is a unique string that the
   * Google Play Developer Console generated beforehand.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  std::string const &EventId() const;

  /**
   * Returns the milestone state: <code>NOT_STARTED, NOT_COMPLETED,
   * COMPLETED_NOT_CLAIMED,</code> or <code>CLAIMED</code>.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  QuestMilestoneState State() const;

  /**
   * Returns current progress of the milestone, as a counter of the associated
   * event.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  uint64_t CurrentCount() const;

  /**
   * Returns the target event count for the milestone.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  uint64_t TargetCount() const;

  /**
   * Returns the developer-defined data for completion of this milestone. This
   * data is set in the Google Play Developer Console.
   * <code>QuestMilestone::Valid()</code> must return true for this function
   * to be usable.
   */
  std::vector<uint8_t> const &CompletionRewardData() const;

 private:
  std::shared_ptr<QuestImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_QUEST_MILESTONE_H_
