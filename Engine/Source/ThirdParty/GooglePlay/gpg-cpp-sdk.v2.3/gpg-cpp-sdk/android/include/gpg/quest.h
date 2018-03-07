// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/quest.h
 *
 * @brief Value object that represents a single guest.
 */

#ifndef GPG_QUEST_H_
#define GPG_QUEST_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class QuestImpl;
class QuestMilestone;

/**
 * A single data structure allowing you to access data about the status of a
 * specific quest.
 * @ingroup ValueType
 */
class GPG_EXPORT Quest {
 public:
  Quest();

  /**
   * Constructs a <code>Quest</code> from a <code>shared_ptr</code> to a
   * <code>QuestImpl</code>.
   * Intended for internal use by the API.
   */
  explicit Quest(std::shared_ptr<QuestImpl const> impl);

  /**
   * Creates a copy of an existing <code>Quest</code>.
   */
  Quest(Quest const &copy_from);

  /**
   * Moves an existing <code>Quest</code>.
   */
  Quest(Quest &&move_from);

  /**
   * Assigns this <code>Quest</code> from another one.
   */
  Quest &operator=(Quest const &copy_from);

  /**
   * Assigns this <code>Quest</code> by moving another one into it.
   */
  Quest &operator=(Quest &&move_from);
  ~Quest();

  /**
   * Returns true when the returned quest is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created quest or for a populated one accompanied by
   * an unsuccessful response status.
   * This function must return true for the getter functions on this quest to
   * be usable.
   */
  bool Valid() const;

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to a quest in your game client.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &Id() const;

  /**
   * Returns the name of the quest.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &Name() const;

  /**
   * Returns the description of the quest.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &Description() const;

  /**
   * Returns the URL leading to the image of the icon for this quest.
   * Returns an empty string if the quest has no icon.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &IconUrl() const;

  /**
   * Returns the URL leading to the banner image for this quest.
   * Returns an empty string if the quest has no banner.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &BannerUrl() const;

  /**
   * Returns the latest milestone information associated with this quest.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  QuestMilestone CurrentMilestone() const;

  /**
   * Returns the quest state: <code>UPCOMING, OPEN, ACCEPTED, COMPLETED,
   * EXPIRED,</code> or <code>FAILED</code>.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  QuestState State() const;

  /**
   * Returns the time (in milliseconds since epoch) at which this quest
   * will be available for players to accept.
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  Timestamp StartTime() const;

  /**
   * Returns the time (in milliseconds since epoch) at which this quest
   * will expire. It will always be true that Start() < Expiration().
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  Timestamp ExpirationTime() const;

  /**
   * Returns the time (in milliseconds since epoch) at which this quest was
   * accepted by the player, or 0 if the player did not accept the quest.
   * When the quest has been accepted, Start() <= Accepted() <= Expiration().
   * <code>Quest::Valid()</code> must return true for this function to be
   * usable.
   */
  Timestamp AcceptedTime() const;

 private:
  std::shared_ptr<QuestImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_QUEST_H_
