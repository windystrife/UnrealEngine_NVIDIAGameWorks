// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/common.h
 *
 * @brief Common functionality for public header files.
 */

#ifndef GPG_COMMON_H_
#define GPG_COMMON_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

/**
 * Used to define default visibility on our public symbols.
 */
#define GPG_EXPORT __attribute__((visibility("default")))

/**
 * Marks deprecated methods and functions.
 */
#define GPG_DEPRECATED __attribute__((deprecated))

#endif  // GPG_COMMON_H_
