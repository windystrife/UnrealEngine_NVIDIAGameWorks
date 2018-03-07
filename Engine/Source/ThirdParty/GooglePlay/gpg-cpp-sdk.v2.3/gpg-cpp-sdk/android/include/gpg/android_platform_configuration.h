// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/android_platform_configuration.h
 *
 * @brief Platform-specific builder configuration for Android.
 */

#ifndef GPG_ANDROID_PLATFORM_CONFIGURATION_H_
#define GPG_ANDROID_PLATFORM_CONFIGURATION_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <jni.h>
#include <functional>
#include <memory>
#include "gpg/common.h"
#include "gpg/quest.h"
#include "gpg/snapshot_metadata.h"

namespace gpg {

class AndroidPlatformConfigurationImpl;

/**
 * The platform configuration used when creating an instance of the GameServices
 * class on Android.
 */
class GPG_EXPORT AndroidPlatformConfiguration {
 public:
  AndroidPlatformConfiguration();
  ~AndroidPlatformConfiguration();

  /**
   * You must set this to an Android Activity that is active for the lifetime of
   * your application. If you do not also set something for
   * SetOptionalIntentHandlerForUI, then this activity will also be used to
   * launch UI, and must implement Activity.onActivityResult(). Forward the
   * result to AndroidSupport::OnActivityResult. This function is declared in
   * android_support.h.
   */
  AndroidPlatformConfiguration &SetActivity(jobject android_app_activity);

  /**
   * Optionally set an IntentHandler function if you don't want to use your main
   * Activity to launch Play Games UI. Provide a function that can start a
   * provided UI intent at any point, using startActivityForResult.
   *
   * The activity used to launch this intent must implement
   * Activity.onActivityResult(). Forward the result to
   * AndroidSupport::OnActivityResult. This function is declared in
   * android_support.h.
   *
   */
  typedef std::function<void(jobject)> IntentHandler;

  /**
   * Optionally set an IntentHandler function if you don't want to use your main
   * Activity to launch Play Games UI. Provide a function that can start a
   * provided UI intent at any point, using startActivityForResult.
   *
   * The activity used to launch this intent must implement
   * Activity.onActivityResult(). Forward the result to
   * AndroidSupport::OnActivityResult. This function is declared in
   * android_support.h.
   */
  AndroidPlatformConfiguration &SetOptionalIntentHandlerForUI(
      IntentHandler intent_handler);

  /**
   * Sets the View to use as a content view for popups.
   */
  AndroidPlatformConfiguration &SetOptionalViewForPopups(jobject android_view);

  /**
   * The callback type used with {@link SetOnLaunchedWithSnapshot}.
   * @ingroup callbacks
   */
  typedef std::function<void(SnapshotMetadata)> OnLaunchedWithSnapshotCallback;

  /**
   * The default callback called when the app is launched from the Play Games
   * Destination App by selecting a snapshot. Logs the ID of the quest. This can
   * be overriden by setting a new callback with
   * {@link SetOnLaunchedWithSnapshot}.
   */
  static void DEFAULT_ON_LAUNCHED_WITH_SNAPSHOT(SnapshotMetadata snapshot);

  /**
   * Registers a callback that will be called if the app is launched from the
   * Play Games Destination App by selecting a snapshot.
   */
  AndroidPlatformConfiguration &SetOnLaunchedWithSnapshot(
      OnLaunchedWithSnapshotCallback callback);

  /**
   * The callback type used with {@link SetOnLaunchedWithQuest}.
   * @ingroup callbacks
   */
  typedef std::function<void(Quest)> OnLaunchedWithQuestCallback;

  /**
   * The default callback called when the app is launched from the Play Games
   * Destination App by selecting a quest. Logs the ID of the quest. This can be
   * overridden by setting a new callback with {@link SetOnLaunchedWithQuest}.
   */
  static void DEFAULT_ON_LAUNCHED_WITH_QUEST(Quest quest);

  /**
   * Registers a callback that will be called if the app is launched from the
   * Play Games Destination App by selecting a quest.
   */
  AndroidPlatformConfiguration &SetOnLaunchedWithQuest(
      OnLaunchedWithQuestCallback callback);

  /**
   * Returns true if all required values were provided to the
   * AndroidPlatformConfiguration. In this case, the only required value is the
   * Activity.
   */
  bool Valid() const;

 private:
  AndroidPlatformConfiguration(AndroidPlatformConfiguration const &) = delete;
  AndroidPlatformConfiguration &operator=(
      AndroidPlatformConfiguration const &) = delete;

  friend class GameServicesImpl;
  friend class NearbyConnectionsImpl;
  std::unique_ptr<AndroidPlatformConfigurationImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_ANDROID_PLATFORM_CONFIGURATION_H_
