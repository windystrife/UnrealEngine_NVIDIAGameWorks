// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/snapshot_metadata_change.h
 *
 * @brief Value object that represents the completion of a task or goal.
 */

#ifndef GPG_SNAPSHOT_METADATA_CHANGE_COVER_IMAGE_H_
#define GPG_SNAPSHOT_METADATA_CHANGE_COVER_IMAGE_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "gpg/common.h"
#include "gpg/snapshot_metadata_change.h"
#include "gpg/types.h"

namespace gpg {

class SnapshotMetadataChangeCoverImageImpl;

/**
 * A single data structure which allows you to access data about the status of
 * a specific cover image.
 * @ingroup ValueType
 */
class GPG_EXPORT SnapshotMetadataChange::CoverImage {
 public:
  /**
   * Constructs a <code>CoverImage</code> from a <code>shared_ptr</code> to
   * a <code>CoverImageImpl</code>. Intended for internal use by the API.
   */
  explicit CoverImage(
      std::shared_ptr<SnapshotMetadataChangeCoverImageImpl const> impl);

  /**
   * Creates a copy of an existing <code>CoverImage</code>.
   */
  CoverImage(CoverImage const &copy_from);

  /**
   * Moves an existing <code>CoverImage</code>.
   */
  CoverImage(CoverImage &&move_from);

  /**
   * Assigns this <code>CoverImage</code> by moving another one into it.
   */
  CoverImage &operator=(CoverImage const &copy_from);

  /**
   * Assignment operator for assigning this CoverImage's
   * value from another CoverImage.
   * r-value-reference version.
   */
  CoverImage &operator=(CoverImage &&move_from);
  ~CoverImage();

  /**
   * The image data to set as the cover image. The format of this vector of
   * bytes is defined by the MimeType() of the cover image.
   */
  std::vector<uint8_t> const &Data() const;

  /**
   * The mime-type of the image file to set as the cover image.
   * Example "image/png".
   */
  std::string const &MimeType() const;

  /**
   * The width of the image in pixels.
   */
  int Width() const;

  /**
   * The height of the image in pixels.
   */
  int Height() const;

 private:
  std::shared_ptr<SnapshotMetadataChangeCoverImageImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_SNAPSHOT_METADATA_CHANGE_COVER_IMAGE_H_
