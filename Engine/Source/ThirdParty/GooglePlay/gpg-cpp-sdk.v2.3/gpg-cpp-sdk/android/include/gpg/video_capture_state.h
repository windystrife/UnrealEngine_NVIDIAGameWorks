// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/video_capture_state.h
 *
 * @brief Value object that contains information on the current video capture
 * state.
 */

#ifndef GPG_VIDEO_CAPTURE_STATE_H_
#define GPG_VIDEO_CAPTURE_STATE_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class VideoCaptureStateImpl;

/**
 * A data structure which allows access to the current state of video capture.
 */
class GPG_EXPORT VideoCaptureState {
 public:
  VideoCaptureState();

  /**
   * Explicit constructor.
   */
  explicit VideoCaptureState(std::shared_ptr<VideoCaptureStateImpl const> impl);

  /**
   * Copy constructor for copying an existing VideoCaptureState object into a
   * new one.
   */
  VideoCaptureState(VideoCaptureState const &copy_from);

  /**
   * Constructor for moving an existing VideoCaptureState object into a new one.
   * r-value-reference version.
   */
  VideoCaptureState(VideoCaptureState &&move_from);

  /**
   * Assignment operator for assigning this VideoCaptureState object's value
   * from another VideoCaptureState object.
   */
  VideoCaptureState &operator=(VideoCaptureState const &copy_from);

  /**
   * Assignment operator for assigning this VideoCaptureState object's value
   * from another VideoCaptureState object.
   * r-value-reference version.
   */
  VideoCaptureState &operator=(VideoCaptureState &&move_from);
  ~VideoCaptureState();

  /**
   * Returns true when the returned VideoCaptureState object is populated with
   * data and is accompanied by a successful response status; false for an
   * unpopulated user-created VideoCaptureState object or for a populated one
   * accompanied by an unsuccessful response status.
   * It must be true for the getter functions on this VideoCaptureState object
   * to be usable.
   */
  bool Valid() const;

  /**
   * Returns whether the service is currently capturing or not.
   */
  bool IsCapturing() const;

  /**
   * Returns the capture mode of the current capture.
   */
  VideoCaptureMode CaptureMode() const;

  /**
   * Returns the quality level of the current capture.
   */
  VideoQualityLevel QualityLevel() const;

  /**
   * Returns whether the capture overlay is currently visible or not. This also
   * indicates the capture overlay is being used by the user and background
   * capture will fail.
   */
  bool IsOverlayVisible() const;

  /**
   * Returns whether the capture is currently paused or not. Will always be
   * <code>false</code> if <code>IsCapturing()</code> if <code>false</code>.
   */
  bool IsPaused() const;

 private:
  std::shared_ptr<VideoCaptureStateImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_VIDEO_CAPTURE_STATE_H_
