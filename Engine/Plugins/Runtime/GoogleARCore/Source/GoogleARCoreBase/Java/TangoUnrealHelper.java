// Copyright 2017 Google Inc.

package com.projecttango.unreal;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Point;
import android.hardware.Camera;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.Display;
import android.view.Surface;

/**
 * Makes it easier to work with Tango from within Unreal.
 */
public class TangoUnrealHelper {
    // Note: keep this in sync with required tangocore features
    static final int TANGO_MINIMUM_VERSION_CODE = 14694;

    private static final String INTENT_CLASS_PACKAGE = "com.google.tango";
    // Key string for load/save Area Description Files.
    private static final String AREA_LEARNING_PERMISSION =
            "ADF_LOAD_SAVE_PERMISSION";

    // Permission request action.
    public static final int REQUEST_CODE_TANGO_PERMISSION = 77897;
    public static final int REQUEST_CODE_TANGO_IMPORT = 77898;
    public static final int REQUEST_CODE_TANGO_EXPORT = 77899;
    private static final String REQUEST_PERMISSION_ACTION =
            "android.intent.action.REQUEST_TANGO_PERMISSION";

    // On current Tango devices, camera id 0 is the color camera.
    private static final int COLOR_CAMERA_ID = 0;
    private static final String TAG = "TangoUnrealHelper";

    private static final String TANGO_PERMISSION_ACTIVITY =
            "com.google.atap.tango.RequestPermissionActivity";
    private static final String AREA_DESCRIPTION_IMPORT_EXPORT_ACTIVITY =
            "com.google.atap.tango.RequestImportExportActivity";
    private static final String EXTRA_KEY_SOURCEUUID = "SOURCE_UUID";
    private static final String EXTRA_KEY_SOURCEFILE = "SOURCE_FILE";
    private static final String EXTRA_KEY_DESTINATIONFILE = "DESTINATION_FILE";

    private static boolean hasTangoPermission(Context context, String permissionType) {
        Uri uri = Uri.parse("content://com.google.atap.tango.PermissionStatusProvider/" +
                permissionType);
        Cursor cursor = context.getContentResolver().query(uri, null, null, null, null);
        if (cursor == null) {
            return false;
        } else {
            return true;
        }
    }

    private void getTangoPermission(String permissionType) {
        Intent intent = new Intent();
        intent.setPackage(INTENT_CLASS_PACKAGE);
        intent.setAction(REQUEST_PERMISSION_ACTION);
        intent.putExtra("PERMISSIONTYPE", permissionType);

        // After the permission activity is dismissed, we will receive a callback
        // function onActivityResult() with user's result.
        mParent.startActivityForResult(intent, REQUEST_CODE_TANGO_PERMISSION);
    }

    private Activity mParent;
    private DisplayManager displayManager;

    public TangoUnrealHelper(Activity unrealActivity) {
        mParent = unrealActivity;
    }

    public void initDisplayManager() {
        displayManager = (DisplayManager) mParent.getSystemService(mParent.DISPLAY_SERVICE);
        if(displayManager != null){
            displayManager.registerDisplayListener(new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int displayId) {
                }

                @Override
                public void onDisplayChanged(int displayId) {
                    synchronized (this) {
                        TangoNativeEngineMethodWrapper.onDisplayOrientationChanged();
                    }
                }

                @Override
                public void onDisplayRemoved(int displayId) {
                }
            }, null);
        }
    }

    public int getDisplayRotation() {
        Display display = mParent.getWindowManager().getDefaultDisplay();
        return display.getRotation();
    }

    public int getColorCameraRotation() {
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(COLOR_CAMERA_ID, info);
        return info.orientation;
    }

    public int getDeviceDefaultOrientation() {
        Display display = mParent.getWindowManager().getDefaultDisplay();

        int ret = mParent.getResources().getConfiguration().orientation + display.getRotation();
        ret = ret % 4;

        // odd number represents portrait, even number represents landscape.
        return ret;
    }

    public int getTangoCoreVersionCode() {
      try {
        PackageManager pm = mParent.getPackageManager();
        PackageInfo info = pm.getPackageInfo(getTangoCorePackageName(), 0);
        return info.versionCode;
      } catch (NameNotFoundException e) {
        return 0;
      }
    }

    public boolean isTangoCorePresent() {
        int version = getTangoCoreVersionCode();
        return version > 0;
	}

    public boolean isTangoCoreUpToDate() {
        int version = getTangoCoreVersionCode();
        return version > TANGO_MINIMUM_VERSION_CODE;
    }

    public String getTangoCoreVersionName() {
      try {
        PackageManager pm = mParent.getPackageManager();
        PackageInfo info = pm.getPackageInfo(getTangoCorePackageName(), 0);
        return info.versionName;
      } catch (NameNotFoundException e) {
        return "";
      }
    }

    private String getTangoCorePackageName() {
      final String tangoCorePackage = "com.google.tango";
      final String tangoCoreOldPackage = "com.projecttango.tango";

      try {
        PackageManager pm = mParent.getPackageManager();
        pm.getPackageInfo(tangoCorePackage, 0);
        Log.d(TAG, "Using package:" + tangoCorePackage);
        return tangoCorePackage;
      } catch (NameNotFoundException e) {
        Log.d(TAG, "Using package:" + tangoCoreOldPackage);
        return tangoCoreOldPackage;
      }
    }
}
