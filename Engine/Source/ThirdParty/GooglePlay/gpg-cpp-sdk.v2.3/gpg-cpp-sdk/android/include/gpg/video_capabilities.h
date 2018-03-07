// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/video_capabilities.h
 *
 * @brief Value object that contains information on what capabilities the
 * current device has for video recording.
 */

#ifndef GPG_VIDEO_CAPABILITIES_H_
#define GPG_VIDEO_CAPABILITIES_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class VideoCapabilitiesImpl;

/**
 * A data structure which allows access to information on what capabilities the
 * current device has for video recording.
 */
class GPG_EXPORT VideoCapabilities {
 public:
  VideoCapabilities();

  /**
   * Explicit constructor.
   */
  explicit VideoCapabilities(std::shared_ptr<VideoCapabilitiesImpl const> impl);

  /**
   * Copy constructor for copying an existing VideoCapabilities object into a
   * new one.
   */
  VideoCapabilities(VideoCapabilities const &copy_from);

  /**
   * Constructor for moving an existing VideoCapabilities object into a new one.
   * r-value-reference version.
   */
  VideoCapabilities(VideoCapabilities &&move_from);

  /**
   * Assignment operator for assigning this VideoCapabilities object's value
   * from another VideoCapabilities object.
   */
  VideoCapabilities &operator=(VideoCapabilities const &copy_from);

  /**
   * Assignment operator for assigning this VideoCapabilities object's value
   * from another VideoCapabilities object.
   * r-value-reference version.
   */
  VideoCapabilities &operator=(VideoCapabilities &&move_from);
  ~VideoCapabilities();

  /**
   * Returns true when the returned VideoCapabilities object is populated with
   * data and is accompanied by a successful response status; false for an
   * unpopulated user-created VideoCapabilities object or for a populated one
   * accompanied by an unsuccessful response status.
   * It must be true for the getter functions on this VideoCapabilities object
   * to be usable.
   */
  bool Valid() const;

  /**
   * Returns whether the device has a front-facing camera and we can use it.
   */
  bool IsCameraSupported() const;

  /**
   * Returns whether the device has a microphone and we can use it.
   */
  bool IsMicSupported() const;

  /**
   * Returns whether the device has an external storage device and we can use
   * it.
   */
  bool IsWriteStorageSupported() const;

  /**
   * Returns whether the device supports the given capture mode.
   */
  bool SupportsCaptureMode(VideoCaptureMode capture_mode) const;

  /**
   * Returns whether the device supports the given quality level.
   */
  bool SupportsQualityLevel(VideoQualityLevel quality_level) const;

  /**
   * Checks if the capture mode and quality level are supported, as well as
   * camera, mic, and storage write.
   */
  bool IsFullySupported(VideoCaptureMode capture_mode,
                        VideoQualityLevel quality_level) const;

 private:
  std::shared_ptr<VideoCapabilitiesImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_VIDEO_CAPABILITIES_H_
