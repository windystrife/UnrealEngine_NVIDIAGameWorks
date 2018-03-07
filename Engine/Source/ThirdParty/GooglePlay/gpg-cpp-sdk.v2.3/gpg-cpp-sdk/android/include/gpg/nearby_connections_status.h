// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/nearby_connections_status.h
 *
 * @brief Enumerations used to represent the status of a request.
 */

#ifndef GPG_NEARBY_CONNECTIONS_STATUS_H_
#define GPG_NEARBY_CONNECTIONS_STATUS_H_

#include "gpg/common_error_status.h"

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

namespace gpg {

/**
 *  The set of possible values representing the result of an API-initialization
 *  attempt.  If the API client has been disconnected, the callback will be
 *  called with InitializationStatus::DISCONNECTED to notify the user that all
 *  API calls will not be authorized successfully until the underlying
 *  GoogleApiClient is reconnected.
 */
enum class GPG_EXPORT InitializationStatus {
  VALID = BaseStatus::VALID,
  ERROR_INTERNAL = BaseStatus::ERROR_INTERNAL,
  ERROR_VERSION_UPDATE_REQUIRED = BaseStatus::ERROR_VERSION_UPDATE_REQUIRED,
  DISCONNECTED = BaseStatus::ERROR_NOT_AUTHORIZED,
};

}  // namespace gpg

#endif  // GPG_NEARBY_CONNECTIONS_STATUS_H_
