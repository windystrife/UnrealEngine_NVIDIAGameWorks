// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/video_manager.h
 *
 * @brief Entry points for Play Games Video functionality.
 */

#ifndef GPG_VIDEO_MANAGER_H_
#define GPG_VIDEO_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <functional>
#include <memory>
#include <string>
#include "gpg/capture_overlay_state_listener_helper.h"
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/i_capture_overlay_state_listener.h"
#include "gpg/types.h"
#include "gpg/video_capabilities.h"
#include "gpg/video_capture_state.h"

namespace gpg {

/**
 * Gets and sets various video-related data.
 * @ingroup Managers
 */
class GPG_EXPORT VideoManager {
 public:
  /**
   * Holds data for video capabilities, along with a response status.
   * @ingroup ResponseType
   */
  struct GetCaptureCapabilitiesResponse {
    /**
     * Can be one of the values enumerated in <code>ResponseStatus</code>.
     */
    ResponseStatus status;

    /**
     * Data about the video capabilities of this device.
     */
    VideoCapabilities video_capabilities;
  };

  /**
   * Defines a callback type that receives a GetCaptureCapabilitiesResponse.
   * This callback type is provided to the
   * <code>GetCaptureCapabilities(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(GetCaptureCapabilitiesResponse const &)>
      CaptureCapabilitiesCallback;

  /**
   * Asynchronously fetches the video capabilities of the service, whether the
   * mic or front-facing camera are supported, if the service can write to
   * external storage, and what capture modes and quality levels are available.
   */
  void GetCaptureCapabilities(CaptureCapabilitiesCallback callback);

  /**
   * Synchronously fetches the video capabilities of the service, whether the
   * mic or front-facing camera are supported, if the service can write to
   * external storage, and what capture modes and quality levels are available.
   * Timeout specified as 10 years by default.
   */
  GetCaptureCapabilitiesResponse GetCaptureCapabilitiesBlocking();

  /**
   * Synchronously fetches the video capabilities of the service, whether the
   * mic or front-facing camera are supported, if the service can write to
   * external storage, and what capture modes and quality levels are available.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   */
  GetCaptureCapabilitiesResponse GetCaptureCapabilitiesBlocking(
      Timeout timeout);

  /**
   * Launches the video capture overlay.
   */
  void ShowCaptureOverlay();

  /**
   * Holds data for video capture state, along with a response status.
   * @ingroup ResponseType
   */
  struct GetCaptureStateResponse {
    /**
     * Can be one of the values enumerated in <code>ResponseStatus</code>.
     */
    ResponseStatus status;

    /**
     * Data about the video capture state.
     */
    VideoCaptureState video_capture_state;
  };

  /**
   * Defines a callback type that receives a GetCaptureStateResponse.
   * This callback type is provided to the <code>GetCaptureState(*)</code>
   * functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(GetCaptureStateResponse const &)>
      CaptureStateCallback;

  /**
   * Asynchronously fetches the current state of the capture service.
   * This will inform about whether the capture
   * overlay is visible, if the overlay is actively being used to capture, and
   * much more. See <code>VideoCaptureState</code> for more details.
   */
  void GetCaptureState(CaptureStateCallback callback);

  /**
   * Synchronously fetches the current state of the capture service.
   * This will inform about whether the capture
   * overlay is visible, if the overlay is actively being used to capture, and
   * much more. See <code>VideoCaptureState</code> for more details.
   * Timeout specified as 10 years by default.
   */
  GetCaptureStateResponse GetCaptureStateBlocking();

  /**
   * Synchronously fetches the current state of the capture service.
   * This will inform about whether the capture
   * overlay is visible, if the overlay is actively being used to capture, and
   * much more. See <code>VideoCaptureState</code> for more details.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   */
  GetCaptureStateResponse GetCaptureStateBlocking(Timeout timeout);

  /**
   * Holds whether or not a capture mode (specified in
   * <code>IsCaptureAvailable</code>) is available, along with a response
   * status.
   * @ingroup ResponseType
   */
  struct IsCaptureAvailableResponse {
    /**
     * Can be one of the values enumerated in <code>ResponseStatus</code>.
     */
    ResponseStatus status;

    /**
     * Whether or not a capture mode (specified in
     * <code>IsCaptureAvailable</code>) is available.
     */
    bool is_capture_available;
  };

  /**
   * Defines a callback type that receives a IsCaptureAvailableResponse.
   * This callback type is provided to the <code>IsCaptureAvailable(*)</code>
   * functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(IsCaptureAvailableResponse const &)>
      IsCaptureAvailableCallback;

  /**
   * Asynchronously fetches if the capture service is already in use or not.
   * Use this call to check if a start capture api call will return
   * <code>ResponseStatus::ERROR_VIDEO_ALREADY_CAPTURING</code>. If this returns
   * true, then its safe to start capturing.
   *
   * Do not use this call to check if capture is supported, instead use
   * <code>IsCaptureSupported</code> or <code>GetCaptureCapabilities</code>.
   */
  void IsCaptureAvailable(VideoCaptureMode capture_mode,
                          IsCaptureAvailableCallback callback);

  /**
   * Synchronously fetches if the capture service is already in use or not.
   * Use this call to check if a start capture api call will return
   * <code>ResponseStatus::ERROR_VIDEO_ALREADY_CAPTURING</code>. If this returns
   * true, then its safe to start capturing.
   * Timeout specified as 10 years by default.
   *
   * Do not use this call to check if capture is supported, instead use
   * <code>IsCaptureSupported</code> or <code>GetCaptureCapabilities</code>.
   */
  IsCaptureAvailableResponse IsCaptureAvailableBlocking(
      VideoCaptureMode capture_mode);

  /**
   * Synchronously fetches if the capture service is already in use or not.
   * Use this call to check if a start capture api call will return
   * <code>ResponseStatus::ERROR_VIDEO_ALREADY_CAPTURING</code>. If this returns
   * true, then its safe to start capturing.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   *
   * Do not use this call to check if capture is supported, instead use
   * <code>IsCaptureSupported</code> or <code>GetCaptureCapabilities</code>.
   */
  IsCaptureAvailableResponse IsCaptureAvailableBlocking(
      Timeout timeout, VideoCaptureMode capture_mode);

  /**
   * Synchronous simple check to determine if the device supports capture.
   */
  bool IsCaptureSupported();

  /**
   * Register a listener to listen for changes to the overlay state launched by
   * <code>ShowCaptureOverlay</code>.
   *
   * Note that only one overlay state listener may be active at a time.
   * Calling this method while another overlay state listener was previously
   * registered will replace the original listener with the new one.
   */
  void RegisterCaptureOverlayStateChangedListener(
      ICaptureOverlayStateListener *listener);

  /**
   * Register a listener to listen for changes to the overlay state launched by
   * <code>ShowCaptureOverlay</code>. Takes a
   * <code>CaptureOverlayStateListenerHelper</code> to create the listener.
   *
   * Note that only one overlay state listener may be active at a time.
   * Calling this method while another overlay state listener was previously
   * registered will replace the original listener with the new one.
   */
  void RegisterCaptureOverlayStateChangedListener(
      CaptureOverlayStateListenerHelper helper);

  /**
   * Unregisters this client's overlay state update listener, if any.
   */
  void UnregisterCaptureOverlayStateChangedListener();

 private:
  friend class GameServicesImpl;
  explicit VideoManager(GameServicesImpl *game_services_impl);
  ~VideoManager();
  VideoManager(VideoManager const &) = delete;
  VideoManager &operator=(VideoManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_VIDEO_MANAGER_H_
