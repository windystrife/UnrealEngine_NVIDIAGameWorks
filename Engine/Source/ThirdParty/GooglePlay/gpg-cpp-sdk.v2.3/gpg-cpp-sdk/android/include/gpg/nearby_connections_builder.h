// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/nearby_connections_builder.h
 *
 * @brief Builds <code>NearbyConnections</code> objects.
 */

#ifndef GPG_NEARBY_CONNECTIONS_BUILDER_H_
#define GPG_NEARBY_CONNECTIONS_BUILDER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include "gpg/nearby_connections.h"
#include "gpg/nearby_connections_status.h"
#include "gpg/platform_configuration.h"

namespace gpg {

class NearbyConnectionsBuilderImpl;
enum class LogLevel;  // Defined in gpg/types.h

/**
 * Builder class used to construct NearbyConnections objects.
 */
class GPG_EXPORT NearbyConnections::Builder {
 public:
  Builder();
  ~Builder();

  /**
   * Specifies the platform for which <code>Builder</code> is to create the
   * <code>NearbyConnections</code> object, and then attempts to create it. If
   * successful, it will return a <code>unique_ptr</code> to the
   * <code>NearbyConnections</code> object. For more information, see the
   * documentation on
   * <a
   * href="https://developers.google.com/games/services/cpp/api/platform__configuration_8h">
   * platform configuration.</a>
   */
  std::unique_ptr<NearbyConnections> Create(
      PlatformConfiguration const &platform);

  /**
   * Sets a client ID for this API, which are returned by callbacks. The client
   * ID allows a single object to register as a listener for multiple
   * <code>NearbyConnection</code> instances, and to tell which callbacks are
   * being returned for which instances. This ID does nothing on Android.
   */
  Builder &SetClientId(int64_t client_id);

  /**
   * Sets a service ID used when advertising. This ID does nothing on Android,
   * but other platforms may require it to be able to advertise.
   */
  Builder &SetServiceId(std::string const &service_id);

  /**
   * A callback that the app invokes on initializing the API, or when
   * initialization fails.
   * @ingroup Callbacks
   */
  typedef std::function<void(InitializationStatus)>
      OnInitializationFinishedCallback;

  /**
   * Registers a callback that the app calls when initialization finishes. The
   * app must call it prior to calling <code>Create</code>. The app may invoke
   * the callback multiple times. For example, if a user gets a phone call and
   * then returns to the app, <code>NearbyConnections</code> will reinitialize
   * and then call this callback again. Note that this callback must be called
   * before you can use a <code>NearbyConnections</code> object.
   */
  Builder &SetOnInitializationFinished(
      OnInitializationFinishedCallback callback);

  /**
   * The type of logging callback that can be provided to the SDK.
   * @ingroup Callbacks
   */
  typedef std::function<void(LogLevel, std::string const &)> OnLogCallback;

  /**
   * Registers a callback that will perform logging.
   * min_level specifies the minimum log level.
   * In ascending order, possible levels are: <code>VERBOSE</code>,
   * <code>INFO</code>, <code>WARNING</code>, and <code>ERROR</code>.
   */
  Builder &SetOnLog(OnLogCallback callback, LogLevel min_level);

  /**
   * Registers a callback that will perform logging.
   * This is equivalent to calling <code>SetOnLog(OnLogCallback,
   * LogLevel)</code> with a LogLevel of <code>INFO</code>.
   */
  Builder &SetOnLog(OnLogCallback callback);

  /**
   * Specifies that logging should use the <code>DEFAULT_ON_LOG_CALLBACK</code>
   * at the specified log level. <code>min_level</code> specifies the minimum
   * log level at which the app invokes the default callback. Possible levels
   * are: <code>VERBOSE</code>, <code>INFO</code>, <code>WARNING</code>, and
   * <code>ERROR</code>.
   * This specification is equivalent to calling <code>SetOnLog(OnLogCallback,
   * LogLevel)</code> with <code>OnLogCallback</code> set to
   * <code>DEFAULT_ON_LOG_CALLBACK</code> and a <code>LogLevel</code> of
   * <code>min_level</code>.
   */
  Builder &SetDefaultOnLog(LogLevel min_level);

 private:
  Builder(Builder const &) = delete;
  Builder &operator=(Builder const &) = delete;

  std::unique_ptr<NearbyConnectionsBuilderImpl> impl_;
  bool built_;
};

}  // namespace gpg

#endif  // GPG_NEARBY_CONNECTIONS_BUILDER_H_
