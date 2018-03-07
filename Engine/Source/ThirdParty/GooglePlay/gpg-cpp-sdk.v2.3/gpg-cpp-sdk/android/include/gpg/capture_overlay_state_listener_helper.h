// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/capture_overlay_state_listener_helper.h
 * @brief Builds an interface for listening to changes in video capture state.
 */

#ifndef GPG_CAPTURE_OVERLAY_STATE_LISTENER_HELPER_H_
#define GPG_CAPTURE_OVERLAY_STATE_LISTENER_HELPER_H_

#include <memory>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/types.h"

namespace gpg {

class CaptureOverlayStateListenerHelperImpl;

/**
 * Defines a helper which can be used to provide ICaptureOverlayStateListener
 * callbacks to the SDK without defining the full ICaptureOverlayStateListener
 * interface. Callbacks configured on this object will be invoked by the Video
 * APIs as described in VideoManager.
 * Callbacks not explicitly set will do nothing.
 */
class GPG_EXPORT CaptureOverlayStateListenerHelper {
 public:
  CaptureOverlayStateListenerHelper();

  /**
   * Constructs a CaptureOverlayStateListenerHelper from a
   * <code>shared_ptr</code> to a
   * <code>CaptureOverlayStateListenerHelperImpl</code>.
   * Intended for internal use by the API.
   */
  CaptureOverlayStateListenerHelper(
      std::shared_ptr<CaptureOverlayStateListenerHelperImpl> impl);

  /**
   * <code>OnCaptureOverlayStateChangedCallback</code> is called when the video
   * capture overlay changes state.
   */
  typedef std::function<void(VideoCaptureOverlayState overlay_state)>
      OnCaptureOverlayStateChangedCallback;

  /**
   * Set the OnCaptureOverlayStateChangedCallback.
   */
  CaptureOverlayStateListenerHelper &SetOnCaptureOverlayStateChangedCallback(
      OnCaptureOverlayStateChangedCallback callback);

 private:
  friend class CaptureOverlayStateListenerHelperImpl;
  std::shared_ptr<CaptureOverlayStateListenerHelperImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_CAPTURE_OVERLAY_STATE_LISTENER_HELPER_H_
