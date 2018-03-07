// Copyright 2017 Google Inc.

package com.projecttango.unreal;

import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.util.Log;

import java.io.File;
/**
 * Ensures that the correct libtango_client_api.so is loaded.
 */
public class TangoInitialization {
  /**
   * tango_client_api is not yet loaded.
   */
  public static final int ARCH_UNLOADED = -2;

  /**
   * Whatever SO is on the LD_LIBRARY_PATH.  Usually a fallback symlink in /system/lib.
   */
  public static final int ARCH_FALLBACK = -1;

  /**
   * Whatever SO is in the default folder.
   */
  public static final int ARCH_DEFAULT = 0;

  /**
   * ARM, 64-bit.
   */
  public static final int ARCH_ARM64 = 1;

  /**
   * ARM, 32-bit.
   */
  public static final int ARCH_ARM32 = 2;

  /**
   * Intel, 64-bit.
   */
  public static final int ARCH_X86_64 = 3;

  /**
   * Intel, 32-bit.
   */
  public static final int ARCH_X86 = 4;

  /**
   * Caches the loaded SO architecture.
   */
  private static int mLoadedSoArch = ARCH_UNLOADED;

  /**
   * Load the right tango_client_api SO for the architecture.  Call this before calling into the
   * Tango API.
   *
   * @return The loaded architecture.
   */
  public static boolean loadLibrary() {
    if (mLoadedSoArch != ARCH_UNLOADED) {
      Log.i("TangoInitialization", "loadLibrary -- Already loaded the so with arch " +
              mLoadedSoArch);
      return true;
    }

    int loadedSoId = ARCH_UNLOADED;
    String basePath = "/data/data/com.google.tango/libfiles/";
    if (!new File(basePath).exists()) {
      basePath = "/data/data/com.projecttango.tango/libfiles/";
    }
    Log.i("TangoInitialization", "loadLibrary -- basePath: " + basePath);

    if (loadedSoId == ARCH_UNLOADED) {
      try {
        System.load(basePath + "arm64-v8a/libtango_client_api.so");
        loadedSoId = ARCH_ARM64;
        Log.i("TangoInitialization",
                "loadLibrary -- Successfully loaded arm64-v8a/libtango_client_api.");
      } catch (UnsatisfiedLinkError e) {
      }
    }
    if (loadedSoId == ARCH_UNLOADED) {
      try {
        System.load(basePath + "armeabi-v7a/libtango_client_api.so");
        loadedSoId = ARCH_ARM32;
        Log.i("TangoInitialization",
                "loadLibrary -- Successfully loaded armeabi-v7a/libtango_client_api.");
      } catch (UnsatisfiedLinkError e) {
      }
    }
    if (loadedSoId == ARCH_UNLOADED) {
      try {
        System.load(basePath + "x86_64/libtango_client_api.so");
        loadedSoId = ARCH_X86_64;
        Log.i("TangoInitialization",
                "loadLibrary -- Successfully loaded x86_64/libtango_client_api.");
      } catch (UnsatisfiedLinkError e) {
      }
    }
    if (loadedSoId == ARCH_UNLOADED) {
      try {
        System.load(basePath + "x86/libtango_client_api.so");
        loadedSoId = ARCH_X86;
        Log.i("TangoInitialization",
                "loadLibrary -- Successfully loaded x86/libtango_client_api.");
      } catch (UnsatisfiedLinkError e) {
      }
    }
    if (loadedSoId == ARCH_UNLOADED) {
      try {
        System.load(basePath + "default/libtango_client_api.so");
        loadedSoId = ARCH_DEFAULT;
        Log.i("TangoInitialization",
                "loadLibrary -- Successfully loaded default/libtango_client_api.");
      } catch (UnsatisfiedLinkError e) {
      }
    }
    if (loadedSoId == ARCH_UNLOADED) {
      try {
        System.loadLibrary("tango_client_api");
        loadedSoId = ARCH_FALLBACK;
        Log.i("TangoInitialization",
                "loadLibrary -- Successfully loaded falback tango_client_api.");
      } catch (UnsatisfiedLinkError e) {
      }
    }

    mLoadedSoArch = loadedSoId;
    Log.i("TangoInitialization", "loadLibrary -- Loaded so arch " + mLoadedSoArch);
    return mLoadedSoArch != ARCH_UNLOADED;
  }

  /**
   * Connect to the Tango service.  This is a light wrapper around Context.bindService for Tango.
   *
   * @param context Context to call bindService on.
   * @param connection ServiceConnection to pass to bindService.
   * @return false if there is no TangoService, otherwise the result from calling bindService.
   */
  public static boolean bindService(Context context, ServiceConnection connection) {
    Intent intent = new Intent();
    intent.setClassName("com.google.tango", "com.google.atap.tango.TangoService");
    boolean hasJavaService = (context.getPackageManager().resolveService(intent, 0) != null);

    // User doesn't have the latest packagename for TangoCore, fallback to the previous name.
    if (!hasJavaService) {
      intent = new Intent();
      intent.setClassName("com.projecttango.tango", "com.google.atap.tango.TangoService");
      hasJavaService = (context.getPackageManager().resolveService(intent, 0) != null);
    }

    // User doesn't have a Java-fied TangoCore at all; fallback to the deprecated approach
    // of doing nothing and letting the native side auto-init to the system-service version
    // of Tango.
    if (!hasJavaService) {
      return false;
    }

    return context.bindService(intent, connection, Context.BIND_AUTO_CREATE);
  }
}
