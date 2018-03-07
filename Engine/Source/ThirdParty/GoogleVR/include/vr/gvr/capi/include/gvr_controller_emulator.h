/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VR_GVR_CAPI_INCLUDE_GVR_CONTROLLER_EMULATOR_H_
#define VR_GVR_CAPI_INCLUDE_GVR_CONTROLLER_EMULATOR_H_
#include "vr/gvr/capi/include/gvr_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Creates and initializes a gvr_controller_context instance which can be used
/// to invoke the Daydream Controller API functions. Use this function when you
/// are trying to use a phone with the Controller Emulator app as the controller
/// on Windows or Mac.
///
/// @param options The API options. To get the defaults, use
///     gvr_controller_get_default_options().
/// @param port_num The port number the Controller Emulator socket is forwarded
///     to. This is usually done by using "adb forward tcp:port_num tcp:7003".
///     WARNING: the caller is responsible for making sure the pointer
///     remains valid for the lifetime of this object.
/// @return A pointer to the initialized API, or NULL if an error occurs.
gvr_controller_context* gvr_controller_create_and_init_emulator(
    int32_t options, const int port_num);

#ifdef __cplusplus
}  // extern "C"
#endif

// Convenience C++ wrapper.
#if defined(__cplusplus) && !defined(GVR_NO_CPP_WRAPPER)

#include <memory>

namespace gvr {

class ControllerEmulatorApi : public ControllerApi {
 public:
  /// Creates an (uninitialized) ControllerEmulatorApi object. You must
  /// initialize it by calling InitEmulator() before interacting with it.
  ControllerEmulatorApi() : ControllerApi() {}

  /// Initializes the Controller API for controller emulator
  /// For more information, see: gvr_controller_create_and_init_emulator().
  bool InitEmulator(int32_t options, const int port_num) {
    cobject_ = gvr_controller_create_and_init_emulator(options, port_num);
    return cobject_ != nullptr;
  }
};

}  // namespace gvr
#endif  // #if defined(__cplusplus) && !defined(GVR_NO_CPP_WRAPPER)
#endif  // VR_GVR_CAPI_INCLUDE_GVR_CONTROLLER_EMULATOR_H_