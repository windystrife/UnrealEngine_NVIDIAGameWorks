// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/snapshot_metadata_change.h
 *
 * @brief Value object that represents the completion of a task or goal.
 */

#ifndef GPG_SNAPSHOT_METADATA_CHANGE_H_
#define GPG_SNAPSHOT_METADATA_CHANGE_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class SnapshotMetadataChangeImpl;

/**
 * A single data structure which allows you to access data about the status of
 * a specific snapshot.
 * @ingroup ValueType
 */
class GPG_EXPORT SnapshotMetadataChange {
 public:
  class Builder;
  class CoverImage;
  SnapshotMetadataChange();
  /**
   * Constructs a <code>SnapshotMetadataChange</code> from a
   * <code>shared_ptr</code> to <code>SnapshotMetadataChangeImpl</code>.
   * Intended for internal use by the API.
   */
  explicit SnapshotMetadataChange(
      std::shared_ptr<SnapshotMetadataChangeImpl const> impl);

  /**
   * Creates a copy of an existing <code>SnapshotMetadataChange</code>.
   */
  SnapshotMetadataChange(SnapshotMetadataChange const &copy_from);

  /**
   * Moves an existing <code>SnapshotMetadataChange</code>.
   */
  SnapshotMetadataChange(SnapshotMetadataChange &&move_from);

  /**
   * Assigns this <code>SnapshotMetadataChange</code> by copying from another
   * one.
   */
  SnapshotMetadataChange &operator=(SnapshotMetadataChange const &copy_from);

  /**
   * Assigns this <code>SnapshotMetadataChange</code> by moving another one into
   * it.
   */
  SnapshotMetadataChange &operator=(SnapshotMetadataChange &&move_from);
  ~SnapshotMetadataChange();

  /**
   * The snapshot metadata change is valid and was created with the builder.
   */
  bool Valid() const;

  /**
   * The description of the snapshot metadata.
   */
  std::string const &Description() const;

  /**
   * The description of the snapshot metadata will be modified with this new
   * description.
   */
  bool DescriptionIsChanged() const;

  /**
   * The played time of the snapshot metadata.
   */
  gpg::Duration PlayedTime() const;

  /**
   * The played time of the snapshot metadata will be modified with this new
   * duration.
   */
  bool PlayedTimeIsChanged() const;

  /**
   * The progress value of the snapshot metadata.
   */
  int64_t ProgressValue() const;

  /**
   * The progress value of the snapshot metadata will be modified with this new
   * progress value.
   */
  bool ProgressValueIsChanged() const;

  /**
   * Image data that will be assigned to the snapshot.
   */
  SnapshotMetadataChange::CoverImage Image() const;

  /**
   * The cover image of the snapshot metadata will be modified with this new
   * cover image.
   */
  bool ImageIsChanged() const;

 private:
  std::shared_ptr<SnapshotMetadataChangeImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_SNAPSHOT_METADATA_CHANGE_H_
