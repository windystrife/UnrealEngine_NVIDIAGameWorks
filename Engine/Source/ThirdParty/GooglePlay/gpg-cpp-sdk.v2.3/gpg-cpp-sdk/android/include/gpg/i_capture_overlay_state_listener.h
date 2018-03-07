// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/i_capture_overlay_state_listener.h
 * @brief An interface for listening to changes in video capture state.
 */

#ifndef GPG_I_CAPTURE_OVERLAY_STATE_LISTENER_H_
#define GPG_I_CAPTURE_OVERLAY_STATE_LISTENER_H_

namespace gpg {

/**
 * Defines an interface that can deliver events relating to changes in video
 * capture state.
 */
class GPG_EXPORT ICaptureOverlayStateListener {
 public:
  virtual ~ICaptureOverlayStateListener() {}

  /**
   * <code>OnCaptureOverlayStateChanged</code> is called when the video capture
   * overlay changes state.
   *
   * @param overlay_state The current capture overlay state.
   */
  virtual void OnCaptureOverlayStateChanged(
      VideoCaptureOverlayState overlay_state) = 0;
};

}  // namespace gpg

#endif  // GPG_I_CAPTURE_OVERLAY_STATE_LISTENER_H_
