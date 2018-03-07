// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/status.h
 *
 * @brief Enumerations used to represent the status of a request.
 */

#ifndef GPG_STATUS_H_
#define GPG_STATUS_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

// GCC Has a bug in which it throws spurious errors on "enum class" types
// when using __attribute__ with them. Suppress this.
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43407
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "gpg/common.h"
#include "gpg/common_error_status.h"

namespace gpg {

/// Returns true if a <code>BaseStatus</code> represents a successful operation.
bool IsSuccess(BaseStatus::StatusCode) GPG_EXPORT;

/// Returns true if authorization completed succesfully.
bool IsSuccess(AuthStatus) GPG_EXPORT;

/// Returns true if an attempted operation resulted in a successful response.
bool IsSuccess(ResponseStatus) GPG_EXPORT;

/// Returns true if a flush completed successfully.
bool IsSuccess(FlushStatus) GPG_EXPORT;

/// Returns true if a UI operation completed successfully.
bool IsSuccess(UIStatus) GPG_EXPORT;

/// Returns true if a multiplayer operation completed successfully.
bool IsSuccess(MultiplayerStatus) GPG_EXPORT;

/// Returns true if an accept quest operation completed successfully.
bool IsSuccess(QuestAcceptStatus) GPG_EXPORT;

/// Returns true if a claim quest milestone operation completed successfully.
bool IsSuccess(QuestClaimMilestoneStatus) GPG_EXPORT;

/// Returns true if a snapshot open operation completed successfully.
bool IsSuccess(SnapshotOpenStatus) GPG_EXPORT;

/// Returns true if a BaseStatus represents an unsuccessful operation.
bool IsError(BaseStatus::StatusCode) GPG_EXPORT;

/// Returns true if auhorization did not complete successfully.
bool IsError(AuthStatus) GPG_EXPORT;

/// Returns true if an attempted operation does not result in a successful
/// response.
bool IsError(ResponseStatus) GPG_EXPORT;

/// Returns true if a flush did not complete successfully.
bool IsError(FlushStatus) GPG_EXPORT;

/// Returns true if a UI operation did not complete successfully.
bool IsError(UIStatus) GPG_EXPORT;

/// Returns true if a Multiplayer operation did not complete successfully.
bool IsError(MultiplayerStatus) GPG_EXPORT;

/// Returns true if an accept quest operation did not complete successfully.
bool IsError(QuestAcceptStatus) GPG_EXPORT;

/// Returns true if a claim quest milestone operation did not complete
/// successfully.
bool IsError(QuestClaimMilestoneStatus) GPG_EXPORT;

/// Returns true if a snapshot open operation did not complete successfully.
bool IsError(SnapshotOpenStatus) GPG_EXPORT;

}  // namespace gpg

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop  // GCC diagnostic ignored "-Wattributes"
#endif

#endif  // GPG_STATUS_H_
