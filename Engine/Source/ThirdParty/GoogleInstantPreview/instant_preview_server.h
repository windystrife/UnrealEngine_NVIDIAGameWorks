#ifndef INSTANT_PREVIEW_IP_SERVER_INSTANT_INSTANT_PREVIEW_SERVER_H_
#define INSTANT_PREVIEW_IP_SERVER_INSTANT_INSTANT_PREVIEW_SERVER_H_

#include <string>
#include <stdint.h>

namespace instant_preview {

typedef struct Pose {
  float transform[16];
  
  Pose() {
    transform[0] = 1.0f;
    transform[1] = 0.0f;
    transform[2] = 0.0f;
    transform[3] = 0.0f;
    
    transform[4] = 0.0f;
    transform[5] = 1.0f;
    transform[6] = 0.0f;
    transform[7] = 0.0f;
    
    transform[8] = 0.0f;
    transform[9] = 0.0f;
    transform[10] = 1.0f;
    transform[11] = 0.0f;
    
    transform[12] = 0.0f;
    transform[13] = 0.0f;
    transform[14] = 0.0f;
    transform[15] = 1.0f;
  }
} Pose;

typedef struct Rect {
  double right;
  double left;
  double top;
  double bottom;
} Rect;

// Specifies the view of an eye relative to the reference pose.
typedef struct EyeView {
  Pose eye_pose;
  Rect eye_fov;
} EyeView;

typedef struct ReferencePose {
  Pose pose;
  double timestamp;
} ReferencePose;

typedef struct ControllerState {
  int connection_state;
  float orientation[4]; // w, x, y, z, same memory layout as unity's Quaternion.
  float gyro[3];
  float accel[3];
  float touch_pos[2];
  bool is_touching;
  bool app_button_state;
  bool click_button_state;
} ControllerState;

// Left eye is drawn into the left half of the returned stream
// Right eye is drawn into the right half of the returned stream.
// num_active_eyes will be 1, and only the first eye_pose will be
// valid for monocular rendering.  In this case the result should
// should use the full returned stream.
typedef struct EyeViews {
  int num_active_eyes;
  EyeView eye_views[2];
} EyeViews;

typedef enum Result {
  RESULT_FAILURE = 0,
  RESULT_SUCCESS,
  RESULT_NO_DATA,
  RESULT_FRAME_DROP,
	RESULT_NO_ADB,
} Result;

typedef enum PixelFormat {
  PIXEL_FORMAT_RGBA = 0,
  PIXEL_FORMAT_BGRA = 1,
  PIXEL_FORMAT_ARGB = 2,
  PIXEL_FORMAT_ABGR = 3,
  PIXEL_FORMAT_UYVY = 4,
} PixelFormat;

typedef enum TouchAction {
  TOUCH_ACTION_DOWN = 0,
  TOUCH_ACTION_MOVE = 1,
  TOUCH_ACTION_CANCEL = 2,
  TOUCH_ACTION_UP = 3,
} TouchAction;

// Forward declaration
class Session;

/**
 * Session Listener to be implemented by instant preview consumers.
 */
class SessionListener {
public:
  virtual ~SessionListener() {};

  /**
   * Inform the instant preview consumer that a new session has started.
   * Note that no pose data will be available until after this function
   * returns.
   */
  virtual void on_session_started(Session* session) = 0;

  /**
   * Inform the instant preview consumer that the given session has ended.
   */
  virtual void on_session_ended(Session* session) = 0;
};

/**
 * Touch event listener to be implemented by instant preview consumers.
 */
class GestureListener {
 public:
  /**
   * Inform the consumer that a single tap has occured.
   */
  virtual void on_tap(float /* x */, float /* y */) {}

  /**
   * Inform the consumer that a long press has occured.
   */
  virtual void on_long_press(float /* x */, float /* y */) {}

  /**
  * Inform the consumer that a touch event has occured.
  */
  virtual void on_touch_event(TouchAction /* action */, float /* x */, float /* y */, int /* touch_id */) {}
};

/**
* class representing a single device session.
*/
class Session {
 public:
  virtual Result get_latest_pose(ReferencePose* reference_pose) = 0;
  virtual Result reset_origin_to(ReferencePose* reference_pose) = 0;
  virtual Result reset_origin_to_current() = 0;
  virtual Result get_eye_views(EyeViews* eye_views) = 0;
  virtual Result get_controller_state(ControllerState* controller_state) = 0;
  // No ownership transfer.
  virtual void set_gesture_listener(GestureListener* gesture_listener) = 0;
  virtual void set_neck_model_scale(float neck_scale) = 0;
  
  virtual Result get_is_video_requested(bool* video_requested) = 0;
  virtual Result send_frame(const uint8_t* pixels,
                            PixelFormat format,
                            int width,
                            int height,
                            int stride,
                            const ReferencePose& reference_pose,
                            int bitrate_kbps = 8000,
                            bool force_keyframe = false) = 0;
};

/**
 * Instant preview server class definition.
 */
class Server {
 public:
  virtual ~Server() {}

  virtual Result start(const std::string &serving_address, SessionListener* listener, bool adb_reverse, const char* adb_path) = 0;
  virtual Result stop() = 0;
};

}  // namespace instant_preview

#endif  // INSTANT_PREVIEW_IP_SERVER_INSTANT_INSTANT_PREVIEW_SERVER_H_
