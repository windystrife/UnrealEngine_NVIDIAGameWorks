// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/event.h
 *
 * @brief Value object that represents an event count.
 */

#ifndef GPG_EVENT_H_
#define GPG_EVENT_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <chrono>
#include <memory>
#include <string>

#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class EventImpl;

/**
 * A single data structure containing data about the status of
 * a specific event. Data comprise two types: user-specific (e.g.,
 * whether the user has unlocked the event), and global (e.g., event name).
 *
 * @ingroup ValueType
 */
class GPG_EXPORT Event {
 public:
  Event();
  /**
   * Constructs an <code>Event</code> from a <code>shared_ptr</code> to an
   * <code>EventImpl</code>.
   * Intended for internal use by the API.
   */
  explicit Event(std::shared_ptr<EventImpl const> impl);

  /**
   * Creates a copy of an existing <code>Event</code>.
   */
  Event(Event const &copy_from);

  /**
   * Moves an existing <code>Event</code>.
   */
  Event(Event &&move_from);

  /**
   * Assigns this <code>Event</code> by copying from another one.
   */
  Event &operator=(Event const &copy_from);

  /**
   * Assigns this <code>Event</code> by moving another one into it.
   */
  Event &operator=(Event &&move_from);
  ~Event();

  /**
   * Returns true if this event is populated with data and is
   * accompanied by a successful response status; false for an
   * unpopulated user-created event or for a populated one accompanied by
   * an unsuccessful response status.
   * It must be true for the getter functions on this event (<code>id</code>,
   * <code>Name</code>, <code>Description</code>, etc.) to be usable.
   */
  bool Valid() const;

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to an event in your game client.
   * It can only be called when <code>Event::Valid()</code> returns true.
   */
  std::string const &Id() const;

  /**
   * Returns the short name of the event. Up to 100 characters.
   * It can only be called when <code>Event::Valid()</code> returns true.
   */
  std::string const &Name() const;

  /**
   * Returns the description of the event.
   * It can only be called when <code>Event::Valid()</code> returns true.
   */
  std::string const &Description() const;

  /**
   * Returns the event state: <code>HIDDEN</code> or <code>REVEALED</code>.
   * <code>Event::Valid()</code> must return true for this function to be
   * usable.
   */
  EventVisibility Visibility() const;

  /**
   * Returns the number of times the event has been incremented.
   * <code>Event::Valid()</code> must return true for this function to be
   * usable.
   */
  uint64_t Count() const;

  /**
   * Returns the URL leading to the image of the icon for this
   * event.
   * <code>Event::Valid()</code> must return true for this function to be
   * usable.
   */
  std::string const &ImageUrl() const;

 private:
  std::shared_ptr<EventImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_EVENT_H_
