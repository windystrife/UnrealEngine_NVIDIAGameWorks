// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/snapshot_metadata.h
 *
 * @brief Value object that represents the completion of a task or goal.
 */

#ifndef GPG_SNAPSHOT_METADATA_H_
#define GPG_SNAPSHOT_METADATA_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <chrono>
#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class SnapshotMetadataImpl;

/**
 * A single data structure that allows you to access data about the status of
 * a specific snapshot metadata. Unlike other value types, a
 * <code>SnapshotMetadata</code> is not strictly immutable.
 * <code>SnapshotManager</code> APIs can "close" the snapshot, changing
 * the result of the IsOpen() method; such APIs universally require that the
 * <code>SnapshotMetadata</code> passed in IsOpen().
 * @ingroup ValueType
 */
class GPG_EXPORT SnapshotMetadata {
 public:
  SnapshotMetadata();
  /**
   * Constructs a <code>SnapshotMetadata</code> object from a
   * <code>shared_ptr</code> to a <code>SnapshotMetadataImpl</code>.
   * Intended for internal use by the API.
   */
  explicit SnapshotMetadata(std::shared_ptr<SnapshotMetadataImpl> impl);

  /**
   * Creates a copy of an existing <code>SnapshotMetadata</code> object.
   */
  SnapshotMetadata(SnapshotMetadata const &copy_from);

  /**
   * Moves an existing <code>SnapshotMetadata</code> object.
   */
  SnapshotMetadata(SnapshotMetadata &&move_from);

  /**
   * Assigns this <code>SnapshotMetadata</code> object by copying from another
   * one.
   */
  SnapshotMetadata &operator=(SnapshotMetadata const &copy_from);

  /**
   * Assigns this <code>SnapshotMetadata</code> object by moving another one
   * into it.
   */
  SnapshotMetadata &operator=(SnapshotMetadata &&move_from);
  ~SnapshotMetadata();

  /**
   * Returns true when the returned snapshot metadata is populated with data and
   * is accompanied by a successful response status; false for an
   * unpopulated user-created snapshot or for a populated one accompanied by
   * an unsuccessful response status.
   * This function must return true for the getter functions (<code>id</code>,
   * <code>Name</code>, <code>Description</code>, etc.) on this snapshot to be
   * usable.
   */
  bool Valid() const;

  /**
   * Returns true when the returned snapshot metadata has been loaded with
   * matching file data. Data related operations such as <code>Read</code>,
   * <code>Commit</code>, and <code>Resolve</code> will only work if the
   * object has file data. Despite being const, this value changes to reflect
   * the underlying data of the snapshot metadata. For example, calling
   * <code>Commit</code> on the operation will result in IsOpen() returning
   * false;
   */
  bool IsOpen() const;

  /**
   * Returns the file name and the unique identifier of the snapshot. Snapshot
   * names must be between 1 and 100 non-URL-reserved characters
   * (a-z, A-Z, 0-9, or the symbols "-", ".", "_", or "~").
   */
  std::string const &FileName() const;

  /**
   * Returns a concise description of your snapshot metadata.
   * <code>SnapshotMetadata::Valid()</code> must return true for this function
   * to be usable.
   */
  std::string const &Description() const;

  /**
   * Returns the played time associated with this snapshot metadata.
   */
  gpg::Duration PlayedTime() const;

  /**
   * Returns the time at which the entry was last modified (expressed as
   * milliseconds since the Unix epoch).
   */
  Timestamp LastModifiedTime() const;

  /**
   * Returns the set progress value associated with this snapshot metadata. The
   * progress value is used in automatic conflict resolution.
   */
  int64_t ProgressValue() const;

  /**
   * Returns cover image url.
   */
  std::string const &CoverImageURL() const;

 private:
  friend class SnapshotMetadataImpl;

  std::shared_ptr<SnapshotMetadataImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_SNAPSHOT_METADATA_H_
