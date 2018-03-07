// Copyright 2017 Google Inc.

package com.projecttango.unreal;

import com.google.atap.tangoservice.Tango;
import com.google.atap.tangoservice.Tango.TangoUpdateCallback;
import com.google.atap.tangoservice.TangoCameraMetadata;
import com.google.atap.tangoservice.TangoConfig;
import com.google.atap.tangoservice.TangoCoordinateFramePair;
import com.google.atap.tangoservice.TangoEvent;
import com.google.atap.tangoservice.TangoImage;
import com.google.atap.tangoservice.TangoPointCloudData;
import com.google.atap.tangoservice.TangoPoseData;

import android.os.IBinder;

public class TangoNativeEngineMethodWrapper
{

    private static TangoUpdateCallback mTangoUpdateCallbackProxy = new TangoUpdateCallback() {
        @Override
        public void onPoseAvailable(TangoPoseData pose){
            onPoseAvailableNative(pose);
        }
        @Override
        public void onFrameAvailable(int cameraId) {
            onTextureAvailableNative(cameraId);
        }
        @Override
        public void onTangoEvent(TangoEvent event) {
            onTangoEventNative(event);
        }
        @Override
        public void onPointCloudAvailable(TangoPointCloudData pointCloud) {
            onPointCloudAvailableNative(pointCloud);
        }
        @Override
        public void onImageAvailable(TangoImage image, TangoCameraMetadata metadata, int cameraId) {
            onImageAvailableNative(image, metadata, cameraId);
        }
    };

    static{
        cacheJavaObjects(mTangoUpdateCallbackProxy);
    }

    /**
    * Set cached java objects.
    */
    public static native void cacheJavaObjects(TangoUpdateCallback callbackProxy);

    public static native void onTangoServiceConnected(Tango tango);

    public static native void onPoseAvailableNative(TangoPoseData pose);
    public static native void onPointCloudAvailableNative(TangoPointCloudData pointCloud);
    public static native void onTextureAvailableNative(int cameraId);
    public static native void onImageAvailableNative(TangoImage image, TangoCameraMetadata metadata, int cameraId);
    public static native void onTangoEventNative(TangoEvent event);

    public static native void onApplicationCreated();
    public static native void onApplicationDestroyed();
    public static native void onApplicationPause();
    public static native void onApplicationResume();
    public static native void onApplicationStop();
    public static native void onApplicationStart();

    public static native void onDisplayOrientationChanged();

}
