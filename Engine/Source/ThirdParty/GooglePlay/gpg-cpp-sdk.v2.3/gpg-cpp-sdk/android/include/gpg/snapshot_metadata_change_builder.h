// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/snapshot_metadata_change_builder.h
 *
 * @brief Builder which creates a SnapshotMetadataChange.
 */

#ifndef GPG_SNAPSHOT_METADATA_CHANGE_BUILDER_H_
#define GPG_SNAPSHOT_METADATA_CHANGE_BUILDER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include <vector>
#include "gpg/snapshot_metadata_change.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Builds one or more SnapshotMetadataChange objects.
 * @ingroup Managers
 */
class GPG_EXPORT SnapshotMetadataChange::Builder {
 public:
  Builder();
  /**
   * Constructs a <code>Builder</code> from a <code>shared_ptr</code> to
   * <code>BuilderImpl</code>. Intended for internal use by the API.
   */
  explicit Builder(std::shared_ptr<SnapshotMetadataChangeImpl> impl);

  /**
   * Creates a copy of an existing <code>Builder</code>.
   */
  Builder(Builder const &copy_from);

  /**
   * Moves an existing <code>Builder</code>.
   */
  Builder(Builder &&move_from);

  /**
   * Assigns this <code>Builder</code> by copying from another one.
   */
  Builder &operator=(Builder const &copy_from);

  /**
   * Assigns this <code>Builder</code> by moving another one into it.
   */
  Builder &operator=(Builder &&move_from);

  /**
   * Sets the concise description of the snapshot metadata_change.
   */
  Builder &SetDescription(std::string const &description);

  /**
   * Sets the played time of the snapshot metadata_change.
   */
  Builder &SetPlayedTime(gpg::Duration played_time);

  /**
   * Sets the progress value of the snapshot metadata_change.
   */
  Builder &SetProgressValue(int64_t progress_value);

  /**
   * The raw bytes of the encoded png cover image of the snapshot metadata
   * change. Cover image must be less than 800 kb. Image must be set every
   * commit or it will be cleared.
   */
  Builder &SetCoverImageFromPngData(std::vector<uint8_t> png_data);

  /**
   * Creates a SnapshotMetadataChange.
   */
  SnapshotMetadataChange Create() const;

 private:
  std::shared_ptr<SnapshotMetadataChangeImpl> impl_;
  Builder &SetCoverImage(std::vector<uint8_t> cover_image,
                         std::string mime_type,
                         int width,
                         int height);
};

}  // namespace gpg

#endif  // GPG_SNAPSHOT_METADATA_CHANGE_BUILDER_H_
