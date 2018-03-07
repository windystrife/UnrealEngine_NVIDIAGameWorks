// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/android_support.h
 *
 * @brief Support functions used when running on pre- Android 4.0 devices.
 */

#ifndef GPG_ANDROID_SUPPORT_H_
#define GPG_ANDROID_SUPPORT_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include "gpg/common.h"

namespace gpg {

/**
 * Functions which enable pre- Android 4.0 support.
 *
 * <h1>Android Lifecycle Callbacks</h1>
 *
 * For apps which target Android 2.3 or 3.x devices (API Version prior to 14),
 * Play Game Services has no way to automatically receive Activity lifecycle
 * callbacks. In these cases, Play Game Services relies on the owning Activity
 * to notify it of lifecycle events. Any Activity which owns a GameServices
 * object should call the AndroidSupport::* functions from within their own
 * lifecycle callback functions. The arguments in these functions match those
 * provided by Android, so no additional processing is necessary.
 *
 * For apps which target android 4.0+ (API Version greater than or equal to 14),
 * most of these function calls are unnecessary. For such apps only the
 * OnActivityResult function must be called.
 *
 * <h1>Example code for a pre- Android 4.0 game using a Java Activity</h1>
 * In your Java Activity, please add the following. You may need to merge this
 * code with your existing lifecycle functions.
 * \code
 * import android.app.Activity;
 * import android.content.Intent;
 *
 * public class YourActivity extends Activity {
 *   protected void onCreate(Bundle savedInstanceState){
 *     super.onCreate(savedInstanceState);
 *     nativeOnActivityCreated(this, savedInstanceState);
 *   }
 *
 *   protected void onDestroy(){
 *     super.onDestroy();
 *     nativeOnActivityDestroyed(this);
 *   }
 *
 *   protected void onPause(){
 *     super.onPause();
 *     nativeOnActivityPaused(this);
 *   }
 *
 *   protected void onResume(){
 *     super.onResume();
 *     nativeOnActivityResumed(this);
 *   }
 *
 *   protected void onStart(){
 *     super.onStart();
 *     nativeOnActivityStarted(this);
 *   }
 *
 *   protected void onStop(){
 *     super.onStop();
 *     nativeOnActivityStopped(this);
 *   }
 *
 *   protected void onSaveInstanceState(Bundle outState) {
 *     super.onSaveInstanceState(outState);
 *     nativeOnActivitySaveInstanceState(this, outState);
 *   }
 *
 *   protected void onActivityResult(int requestCode,
 *                                   int resultCode,
 *                                   Intent data) {
 *     super.onActivityResult(requestCode, resultCode, data);
 *     nativeOnActivityResult(this, requestCode, resultCode, data);
 *   }
 *
 *   // Implemented in C++.
 *   private static native void nativeOnActivityCreated(
 *       Activity activity, Bundle savedInstanceState);
 *   private static native void nativeOnActivityDestroyed(Activity activity);
 *   private static native void nativeOnActivityPaused(Activity activity);
 *   private static native void nativeOnActivityResumed(Activity activity);
 *   private static native void nativeOnActivitySaveInstanceState(
 *       Activity activity,
 *       Bundle outState);
 *   private static native void nativeOnActivityStarted(Activity activity);
 *   private static native void nativeOnActivityStopped(Activity activity);
 *   private static native void nativeOnActivityResult(
 *       Activity activity,
 *       int requestCode,
 *       int resultCode,
 *       Intent data);
 * }
 * \endcode
 * Then, in your native library, add the following forwarding functions.
 * \code
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityCreated(
 *     JNIEnv *env,
 *     jobject thiz,
 *     jobject activity,
 *     jobject saved_instance_state) {
 *   gpg::AndroidSupport::OnActivityCreated(env,
 *                                          activity,
 *                                          saved_instance_state);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityDestroyed(
 *     JNIEnv *env, jobject thiz, jobject activity) {
 *   gpg::AndroidSupport::OnActivityDestroyed(env, activity);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityPaused(
 *     JNIEnv *env, jobject thiz, jobject activity) {
 *   gpg::AndroidSupport::OnActivityPaused(env, activity);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityResumed(
 *     JNIEnv *env, jobject thiz, jobject activity) {
 *   gpg::AndroidSupport::OnActivityResumed(env, activity);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivitySaveInstanceState(
 *     JNIEnv *env, jobject thiz, jobject activity, jobject out_state) {
 *   gpg::AndroidSupport::OnActivitySaveInstanceState(env, activity, out_state);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityStarted(
 *     JNIEnv *env, jobject thiz, jobject activity) {
 *   gpg::AndroidSupport::OnActivityStarted(env, activity);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityStopped(
 *     JNIEnv *env, jobject thiz, jobject activity) {
 *   gpg::AndroidSupport::OnActivityStopped(env, activity);
 * }
 *
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityResult(
 *     JNIEnv *env,
 *     jobject thiz,
 *     jobject activity,
 *     jint request_code,
 *     jint result_code,
 *     jobject data) {
 *   gpg::AndroidSupport::OnActivityResult(
 *       env, activity, request_code, result_code, data);
 * }
 * \endcode
 *
 * <h1>Example code for an Android 4.0+ game using a Java Activity</h1>
 * In your Java Activity, please add the following. You may need to merge this
 * code with your existing lifecycle functions.
 * \code
 * public class YourActivity extends Activity {
 *   protected void onActivityResult(int requestCode,
 *                                   int resultCode,
 *                                   Intent data) {
 *     super.onActivityResult(requestCode, resultCode, data);
 *     nativeOnActivityResult(this, requestCode, resultCode, data);
 *   }
 *
 *   // Implemented in C++.
 *   private static native void nativeOnActivityResult(
 *       Activity activity,
 *       int requestCode,
 *       int resultCode,
 *       Intent data);
 * }
 * \endcode
 * Then, in your native library, add the following forwarding functions.
 * \code
 * void Java_com_example_yourapp_YourActivity_nativeOnActivityResult(
 *     JNIEnv *env,
 *     jobject thiz,
 *     jobject activity,
 *     jint request_code,
 *     jint result_code,
 *     jobject data) {
 *   gpg::AndroidSupport::OnActivityResult(
 *       env, activity, request_code, result_code, data);
 * }
 * \endcode
 */
struct GPG_EXPORT AndroidSupport {
  /**
   * Should be called to forward data from your Java activity's
   * onActivityCreated. Only necessary for Android 2.3.x support.
   */
  static void OnActivityCreated(JNIEnv *env,
                                jobject activity,
                                jobject saved_instance_state);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivityDestroyed. Only necessary for Android 2.3.x support.
   */
  static void OnActivityDestroyed(JNIEnv *env, jobject activity);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivityPaused. Only necessary for Android 2.3.x support.
   */
  static void OnActivityPaused(JNIEnv *env, jobject activity);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivityResumed. Only necessary for Android 2.3.x support.
   */
  static void OnActivityResumed(JNIEnv *env, jobject activity);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivitySaveInstanceState. Only necessary for Android 2.3.x support.
   */
  static void OnActivitySaveInstanceState(JNIEnv *env,
                                          jobject activity,
                                          jobject out_state);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivityStarted. Only necessary for Android 2.3.x support.
   */
  static void OnActivityStarted(JNIEnv *env, jobject activity);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivityStopped. Only necessary for Android 2.3.x support.
   */
  static void OnActivityStopped(JNIEnv *env, jobject activity);

  /**
   * Should be called to forward data from your Java activity's
   * OnActivityResult.
   */
  static void OnActivityResult(JNIEnv *env,
                               jobject activity,
                               jint request_code,
                               jint result_code,
                               jobject result);
};

}  // namespace gpg

#endif  // GPG_ANDROID_SUPPORT_H_
