// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/android_initialization.h
 *
 * @brief Android specific initialization functions.
 */

#ifndef GPG_ANDROID_INITIALIZATION_H_
#define GPG_ANDROID_INITIALIZATION_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <android/native_activity.h>
#include <jni.h>
#include "gpg/common.h"

// Forward declaration of android_app from android_native_app_glue.h.
struct android_app;

namespace gpg {

/**
 * AndroidInitialization includes three initialization functions, exactly one of
 * which must be called. In the case of a standard Java Activity, JNI_OnLoad
 * should be used. In the case of a NativeActivity where JNI_OnLoad will not be
 * called, either android_main or ANativeActivity_onCreate should be used.
 * android_main is used when building a NativeActivity using
 * android_native_app_glue.h. ANativeActivity_onCreate is used when building a
 * NativeActivity using just native_activity.h. android_native_app_glue.h and
 * native_activity.h are default Android headers.
 *
 * The appropriate initialization function must be called exactly once before
 * any AndroidPlatformConfiguration instance methods are called, and it must be
 * called before a GameServices object is instantiated. It is permitted to
 * instantiate a AndroidPlatformConfiguration before one of the intialization
 * calls (for example, if the configuration object has global scope), as long as
 * no methods are called before the initialization call. These methods need be
 * called only once in the lifetime of the calling program, not once per
 * GameServices object created.
 */
struct GPG_EXPORT AndroidInitialization {
  /**
   * When using Play Game Services with a standard Java Activity, JNI_OnLoad
   * should be called when the dynamic library's JNI_OnLoad is called.
   */
  static void JNI_OnLoad(JavaVM *jvm);

  /**
   * When using Play Game Services with a NativeActivity which is based on
   * android_native_app_glue.h, android_main should be called during your
   * activity's android_main, before any other Play Game Services calls.
   */
  static void android_main(struct android_app *app);

  /**
   * When using Play Game Services with a NativeActivity which is based on only
   * native_activity.h, ANativeActivity_onCreate should be called during your
   * activity's ANativeActivity_onCreate, before any other Play Game Services
   * calls.
   */
  static void ANativeActivity_onCreate(ANativeActivity *native_activity,
                                       void *savedState,
                                       size_t savedStateSize);
};

}  // namespace gpg

#endif  // GPG_ANDROID_INITIALIZATION_H_
