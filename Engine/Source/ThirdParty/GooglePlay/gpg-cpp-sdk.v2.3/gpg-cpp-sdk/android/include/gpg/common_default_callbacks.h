// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/common_default_callbacks.h
 *
 * @brief Non games-specific default callback functions. Games specific
 *        functionality can be found in default_callbacks.h.
 */

#ifndef GPG_COMMON_DEFAULT_CALLBACKS_H_
#define GPG_COMMON_DEFAULT_CALLBACKS_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <functional>
#include <string>
#include "gpg/common.h"

namespace gpg {

enum class LogLevel;  // Defined in gpg/types.h

/**
 * This is the default value for Builder::SetLogging. By default, logs are
 * written in a platform-specific manner (i.e., to the Android log or NSLog).
 */
void DEFAULT_ON_LOG(LogLevel level, std::string const &message) GPG_EXPORT;

}  // namespace gpg

#endif  // GPG_COMMON_DEFAULT_CALLBACKS_H_
