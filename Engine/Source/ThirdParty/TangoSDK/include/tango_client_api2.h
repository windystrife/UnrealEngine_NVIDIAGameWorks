// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef TANGO_CLIENT_API2_TANGO_CLIENT_API2_H_
#define TANGO_CLIENT_API2_TANGO_CLIENT_API2_H_

#include <jni.h>

#include <tango_client_api.h>

#ifdef __cplusplus
extern "C" {
#endif

TangoErrorType TangoService_Experimental_getPlaneByUVCoord(
    int cameraId, const TangoPoseData* camera_pose, const double uvCoord[2],
    TangoPlaneData* planeData);

// *planes is expected to be NULL, i.e. it is app's responsibility to free
// *planes before passing it to the function.
//
// If success, *planes points to an array of TangoPlaneData, and *plane_num is
// the size of the array.
//
// Memory of *planes is owned by system. App should use TangoPlaneData_free to
// free the memory.
TangoErrorType TangoService_Experimental_getPlanes(TangoPlaneData** planes,
                                                   size_t* plane_num);

TangoErrorType TangoService_isSupported(bool* isSupported);

// Helper for TangoPlaneData
void TangoPlaneData_free(TangoPlaneData* planes, size_t plane_num);

void TangoService_CacheTangoObject(JNIEnv* env, jobject jTangoObj);

void TangoService_CacheJavaObjects(JNIEnv* env, jobject jTangoUpdateCallback);

void TangoService_JavaCallback_OnPoseAvailable(JNIEnv* env,
                                               jobject jTangoPoseData);

void TangoService_JavaCallback_OnPointCloudAvailable(JNIEnv* env,
                                                     jobject jTangoPointCloud);

void TangoService_JavaCallback_OnTangoEvent(JNIEnv* env, jobject jTangoEvent);

void TangoService_JavaCallback_OnTextureAvailable(int cameraId);

void TangoService_JavaCallback_OnImageAvailable(JNIEnv* env, int cameraId,
                                                jobject jTangoImage,
                                                jobject jTangoCameraMetadata);

// Lighting information
TangoErrorType TangoService_getPixelIntensity(uint8_t* yuvImage, int width,
                                              int height, int rowStride,
                                              float* out_float);

TangoErrorType TangoService_getLuminance(int64_t exposureDurationNs,
                                         int sensitivityIso, float lensAperture,
                                         float* out_float);

#ifdef __cplusplus
}
#endif

#endif  // TANGO_CLIENT_API2_TANGO_CLIENT_API2_H_
