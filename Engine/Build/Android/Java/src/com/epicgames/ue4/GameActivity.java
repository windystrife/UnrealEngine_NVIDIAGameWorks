// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import java.io.File;
import android.hardware.SensorManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import java.lang.Override;
import java.util.Arrays;
import java.util.Map;
import java.util.HashMap;
import java.util.ArrayList;
import java.text.DecimalFormat;

import android.annotation.TargetApi;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

import android.os.Vibrator;
import android.os.SystemClock;
import android.os.Handler;
import android.os.HandlerThread;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.PendingIntent;
import android.app.AlarmManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.Spinner;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Spanned;
import android.text.method.PasswordTransformationMethod;
import android.text.TextWatcher;
import android.view.inputmethod.EditorInfo;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.IntentSender.SendIntentException;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.FeatureInfo;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.provider.Settings.Secure;
import android.content.pm.PackageInfo;


import android.media.AudioManager;
import android.util.DisplayMetrics;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

import android.media.AudioManager;

import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import com.google.android.gms.auth.GoogleAuthUtil;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.games.Games;

import com.google.android.gms.plus.Plus;

import com.google.vr.sdk.samples.permission.PermissionHelper;

import java.net.URL;
import java.net.HttpURLConnection;

import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

import com.epicgames.ue4.GooglePlayStoreHelper;
import com.epicgames.ue4.GooglePlayLicensing;

// Console commands listener, only for debug builds
import com.epicgames.ue4.ConsoleCmdReceiver;

import android.os.Build;

// TODO: use the resources from the UE4 lib project once we've got the packager up and running
//import com.epicgames.ue4.R;

import com.epicgames.ue4.DownloadShim;

// used in new virtual keyboard
import android.view.inputmethod.InputMethodManager;

import android.graphics.Rect;
import android.view.ViewTreeObserver;

import java.lang.reflect.Method;

import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.util.AttributeSet;
import android.graphics.Color;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;
import android.content.BroadcastReceiver;
//$${gameActivityImportAdditions}$$

//$${gameActivityPostImportAdditions}$$

//Extending NativeActivity so that this Java class is instantiated
//from the beginning of the program.  This will allow the user
//to instantiate other Java libraries from here, that the user
//can then use the functions from C++
//NOTE -- This class is not necessary for the UnrealEngine C++ code
//  to startup, as this is handled through the base NativeActivity class.
//  This class's functionality is to provide a way to instantiate other
//  Java libraries at the startup of the program and store references 
//  to them in this class.

public class GameActivity extends NativeActivity implements SurfaceHolder.Callback2,
															GoogleApiClient.ConnectionCallbacks,
															GoogleApiClient.OnConnectionFailedListener,
															SensorEventListener
{
	private SensorManager sensorManager;
	private Sensor accelerometer;
	private Sensor magnetometer;
	private Sensor gyroscope;
	
	private final float[] rotationMatrix = new float[9];
	private final float[] orientationAngles = new float[3];

	static float[] last_accelerometer = new float[]{0, 0, 0};
	static float[] last_magnetometer = new float[]{0, 0, 0};
	// Buffered, historical, motion data.
	static float[] last_tilt = new float[]{0, 0, 0};
	static float[] last_gravity = new float[]{0, 0, 0};

	static boolean first_acceleration_sample = true;
	static final float SampleDecayRate = 0.85f;
	public static Logger Log = new Logger("UE4");
	
	public static final int DOWNLOAD_ACTIVITY_ID = 80001; // so we can identify the activity later
	public static final int DOWNLOAD_NO_RETURN_CODE = 0; // we didn't get a return code - will need to log and debug as this shouldn't happen
	public static final int DOWNLOAD_FILES_PRESENT = 1;  // we already had the files we needed
	public static final int DOWNLOAD_COMPLETED_OK = 2; // downloaded ok (practically the same as above)
	public static final int DOWNLOAD_USER_QUIT = 3;    // user aborted the download
	public static final int DOWNLOAD_FAILED = 4;
	public static final int DOWNLOAD_INVALID = 5;
	public static final int DOWNLOAD_NO_PLAY_KEY = 6;
	public static final String DOWNLOAD_RETURN_NAME = "Result";

	public static enum VirtualKeyboardCommand { VK_CMD_NONE, VK_CMD_SHOW, VK_CMD_HIDE };
	public static VirtualKeyboardCommand lastVirtualKeyboardCommand = VirtualKeyboardCommand.VK_CMD_NONE;
	public static final int lastVirtualKeyboardCommandDelay = 200;
	
	static GameActivity _activity;
	static Bundle _bundle;

	protected Dialog mSplashDialog;
	private int noActionAnimID = -1;

	// Console
	private static final String CONSOLE_SPINNER_ITEMS[] = {"Common Console Commands", "stat FPS", "stat Anim","stat OpenGLRHI","stat VulkanRHI","stat DumpEvents","stat DumpFrame",
		"stat DumpHitches","stat Engine","stat Game","stat Grouped","stat Hitches","stat InitViews","stat LightRendering",
		"stat Memory","stat Particles","stat SceneRendering","stat SceneUpdate","stat ShadowRendering","stat Slow",
		"stat Streaming","stat StreamingDetails","stat Unit","stat UnitGraph", "stat StartFile", "stat StopFile", "GameVer", "show PostProcessing", "stat AndroidCPU"};
	AlertDialog consoleAlert;
	LinearLayout consoleAlertLayout;
	Spinner consoleSpinner;
	EditText consoleInputBox;
	ArrayList<String> consoleHistoryList;
	int consoleHistoryIndex;
	float consoleDistance;
	float consoleVelocity;

	// Virtual keyboard
	AlertDialog virtualKeyboardAlert;
	EditText virtualKeyboardInputBox;
	String virtualKeyboardPreviousContents;

	//virtual keyboard input - custom EditText
	VirtualKeyboardInput newVirtualKeyboardInput;

	//layout for displaying virtual keyboard input
	private LinearLayout virtualKeyboardLayout;

	//container for SurfaceView and virtual keyboard input
	private FrameLayout containerFrameLayout;

	//handler for virtual keyboard show/hide events
	private Handler virtualKeyboardHandler;

	// Keep a reference to the main content view so we can bring up the virtual keyboard without an editbox
	private View mainView;
	private boolean bKeyboardShowing;

	private View mainDecorView;
	private Rect mainDecorViewRect;

	// Console commands receiver
	ConsoleCmdReceiver consoleCmdReceiver;

	// default the PackageDataInsideApk to an invalid value to make sure we don't get it too early
	private static int PackageDataInsideApkValue = -1;
	private static int HasOBBFiles = -1;
	
	// depthbuffer preference from manifest
	int DepthBufferPreference = 0;
	
	/** AssetManger reference - populated on start up and used when the OBB is packed into the APK */
	private AssetManager			AssetManagerReference;
	
	private GoogleApiClient googleClient;

	// layout required by popups, e.g ads, native controls
	LinearLayout activityLayout;

	/** Request code to use when launching the Google Services resolution activity */
    private static final int GOOGLE_SERVICES_REQUEST_RESOLVE_ERROR = 1001;

	/** Unique tag for the error dialog fragment */
    private static final String DIALOG_ERROR = "dialog_error";

	/** Unique ID to identify Google Play Services error dialog */
	private static final int PLAY_SERVICES_DIALOG_ID = 1;
	
	private static String appPackageName = "";

	/** Check to see if we have all the files */
	private boolean HasAllFiles = false;
	
	/** Check to see if we should be verifying the files once we have them */
	public boolean VerifyOBBOnStartUp = false;

	/** Use ExternalFilesDir for UE4Game files */
    private boolean UseExternalFilesDir = false;

	/** Flag to ensure we have finished startup before allowing nativeOnActivityResult to get called */
	private boolean InitCompletedOK = false;
	
	private boolean ShouldHideUI = false;

	/** Whether this application is for distribution */
	private boolean IsForDistribution = false;
	

	/** Application build configuration */
	private String BuildConfiguration = "";

	/** Whether we are in VRMode */
	private boolean IsInVRMode = false;

	/** Implement this if app wants to handle IAB activity launch. For e.g use DaydreamApi for transitions **/
	GooglePlayStoreHelper.PurchaseLaunchCallback purchaseLaunchCallback = null;

	/** Used for SurfaceHolder.setFixedSize buffer scaling workaround on early Amazon devices and some others */
	private boolean bUseSurfaceView = false;
	private SurfaceView MySurfaceView;
	private int DesiredHolderWidth = 0;
	private int DesiredHolderHeight = 0;

	/** Discovered Vulkan Version and Level from getSystemAvailableFeatures() */
	private int VulkanVersion = 0;
	private int VulkanLevel = 0;

	/** Used for LocalNotification support*/
	private boolean localNotificationAppLaunched = false;
	private String	localNotificationLaunchActivationEvent = "";
	private int		localNotificationLaunchFireDate = 0;
	
	public int DeviceRotation = -1;

	enum EAlertDialogType
	{
		None,
		Console,
		Keyboard
	}
	private EAlertDialogType CurrentDialogType = EAlertDialogType.None;
	
	public boolean IsInVRMode()
	{
		return IsInVRMode;
	}

	public GooglePlayStoreHelper.PurchaseLaunchCallback getPurchaseLaunchCallback()
	{
		return purchaseLaunchCallback;
	}

	/** Access singleton activity for game. **/
	public static GameActivity Get()
	{
		return _activity;
	}
	
	/**
	Get the SDK level of the OS we are running in.
	We do this instead of accessing the SDK_INT
	with JNI from C++ as the new ART runtime seems to have
	problems dynamically finding/loading static inner classes.
	*/
	public static final int ANDROID_BUILD_VERSION = android.os.Build.VERSION.SDK_INT;
	
	private StoreHelper IapStoreHelper;

//$${gameActivityClassAdditions}$$

	@Override
	public void onStart()
	{
		super.onStart();
		
		if (!BuildConfiguration.equals("Shipping"))
		{
			// Create console command broadcast listener
			Log.debug( "Creating console command broadcast listener");
			consoleCmdReceiver = new ConsoleCmdReceiver(this);
			registerReceiver(consoleCmdReceiver, new IntentFilter(Intent.ACTION_RUN));
		}
		
//$${gameActivityOnStartAdditions}$$
		Log.debug("==================================> Inside onStart function in GameActivity");
	}

	public int getDeviceDefaultOrientation() 
	{
		// WindowManager windowManager =  (WindowManager) getSystemService(WINDOW_SERVICE);
		WindowManager windowManager =  getWindowManager();

		Configuration config = getResources().getConfiguration();

		int rotation = windowManager.getDefaultDisplay().getRotation();
		switch (rotation)
		{
			case Surface.ROTATION_0:	DeviceRotation = 0;		break;
			case Surface.ROTATION_90:	DeviceRotation = 90;	break;
			case Surface.ROTATION_180:	DeviceRotation = 180;	break;
			case Surface.ROTATION_270:	DeviceRotation = 270;	break;
		}

		if ( ((rotation == android.view.Surface.ROTATION_0 || rotation == android.view.Surface.ROTATION_180) &&
				config.orientation == Configuration.ORIENTATION_LANDSCAPE)
			|| ((rotation == android.view.Surface.ROTATION_90 || rotation == android.view.Surface.ROTATION_270) &&    
				config.orientation == Configuration.ORIENTATION_PORTRAIT)) 
		{
			return Configuration.ORIENTATION_LANDSCAPE;
		}
		else 
		{
			return Configuration.ORIENTATION_PORTRAIT;
		}
	}

	private int getResourceId(String VariableName, String ResourceName, String PackageName)
	{
		try {
			return getResources().getIdentifier(VariableName, ResourceName, PackageName);
		}
		catch (Exception e) {
			e.printStackTrace();
			return -1;
		} 
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
		accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
		magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
		gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
		// create splashscreen dialog (if launched by SplashActivity)
		Bundle intentBundle = getIntent().getExtras();
		if (intentBundle != null)
		{
			ShouldHideUI = intentBundle.getString("ShouldHideUI") != null;
			if (intentBundle.getString("UseSplashScreen") != null)
			{
				try {
					// try to get the splash theme (can't use R.style.UE4SplashTheme since we don't know the package name until runtime)
					int SplashThemeId = getResources().getIdentifier("UE4SplashTheme", "style", getPackageName());
					mSplashDialog = new Dialog(this, SplashThemeId);
					mSplashDialog.setCancelable(false);
					if (ShouldHideUI)
					{
						View decorView = mSplashDialog.getWindow().getDecorView(); 
						// only do this on KitKat and above
						if(android.os.Build.VERSION.SDK_INT >= 19) {
							decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
														| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
														| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
														| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
														| View.SYSTEM_UI_FLAG_FULLSCREEN
														| View.SYSTEM_UI_FLAG_IMMERSIVE);  // NOT sticky.. will be set to sticky later! 
						}
					}
					mSplashDialog.show();
				}
				catch (Exception e) {
					e.printStackTrace();
				}
				try {
					noActionAnimID = getResources().getIdentifier("noaction", "anim", getPackageName());
				}
				catch (Exception e) {
					e.printStackTrace();
				}
			}
			try {
				noActionAnimID = getResources().getIdentifier("noaction", "anim", getPackageName());
			}
			catch (Exception e) {
				e.printStackTrace();
			}
		}

		//Check for target sdk.  If 23 or higher then warn that permission handling may mean features don't work if user denies them.
		int targetSdkVersion = 0;
		try 
		{
			PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
			targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;
		}
		catch (PackageManager.NameNotFoundException e) 
		{
			Log.debug(e.getMessage());
		}

		if (ANDROID_BUILD_VERSION >= 23 && targetSdkVersion >= 23) //23 is the API level (Marshmallow) where runtime permission handling is available
		{
			Log.debug("Target SDK is " + targetSdkVersion + ".  This may cause issues if permissions are denied by the user." );				
		}


		
		// Suppress java logs in Shipping builds
		if (nativeIsShippingBuild())
		{
			Logger.SuppressLogs();
		}
		else
		{
			//For non-shipping builds we need to request this permission bc we write to the sdcard for logs
			PermissionHelper.acquirePermissions(new String[] {"android.permission.WRITE_EXTERNAL_STORAGE"}, this);
		}

		_activity = this;

		// layout required by popups, e.g ads, native controls
		MarginLayoutParams params = new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
		params.setMargins(0, 0, 0, 0);
		activityLayout = new LinearLayout(_activity);
		_activity.setContentView(activityLayout, params);

/*
		// Turn on and unlock screen.. Assumption is that this
		// will only really have an effect when for debug launching
		// as otherwise the screen is already unlocked.
		this.getWindow().addFlags(
			WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON |
//			WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
			WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
		// On some devices we can also unlock a key-locked screen by disabling the
		// keylock guard. To be safe we only do this on < Android 3.2. As the API
		// is deprecated from 3.2 onward.
		if (ANDROID_BUILD_VERSION < 13)
		{
			android.app.KeyguardManager keyman = (android.app.KeyguardManager)getSystemService(KEYGUARD_SERVICE);
			android.app.KeyguardManager.KeyguardLock keylock = keyman.newKeyguardLock("Unlock");
			keylock.disableKeyguard();
		}
*/

/*
		// log a list of input devices for debugging
		{
			int[] deviceIds = InputDevice.getDeviceIds();
			for (int deviceIndex=0; deviceIndex < deviceIds.length; deviceIndex++)
			{
				InputDevice inputDevice = InputDevice.getDevice(deviceIds[deviceIndex]);
				Log.debug("Device index " + deviceIndex + ": (deviceId=" + inputDevice.getId() + 
				", controllerNumber=" + inputDevice.getControllerNumber() + ", sources=" + String.format("%08x", inputDevice.getSources()) +
				", vendorId=" + String.format("%04x", inputDevice.getVendorId()) + ", productId=" + String.format("%04x", inputDevice.getProductId()) + 
				", descriptor=" + inputDevice.getDescriptor() +	", deviceName=" + inputDevice.getName() + ")");

				// is it a joystick?
				if ((inputDevice.getSources() & InputDevice.SOURCE_JOYSTICK) != 0)
				{
					Log.debug("Gamepad detected: (deviceIndex=" + deviceIndex + ", deviceId=" + inputDevice.getId() + ", deviceName=" + inputDevice.getName() + ")");
				}
			}
		}
*/

		// tell Android that we want volume controls to change the media volume, aka music
		setVolumeControlStream(AudioManager.STREAM_MUSIC);

		// Look for Vulkan support if Nougat or later
		if (ANDROID_BUILD_VERSION >= 24)
		{
			FeatureInfo[] features = getPackageManager().getSystemAvailableFeatures();
			for (FeatureInfo feature : features) {
				if (feature.name != null)
				{
					if (feature.name.equals("android.hardware.vulkan.level"))
					{
						// since we may not be compiled against android-24 or higher, use .toString to get the version field
						String dump = feature.toString();
						int index = dump.indexOf("v=");
						if (index >= 0)
						{
							dump = dump.substring(index+2);
							index = dump.indexOf(" ");
							if (index >= 0)
							{
								VulkanLevel = Integer.parseInt(dump.substring(0, index));
								Log.debug("Vulkan level: " + VulkanLevel);
							}
						}
					}
					else
					if (feature.name.equals("android.hardware.vulkan.version"))
					{
						// since we may not be compiled against android-24 or higher, use .toString to get the version field
						String dump = feature.toString();
						int index = dump.indexOf("v=");
						if (index >= 0)
						{
							dump = dump.substring(index+2);
							index = dump.indexOf(" ");
							if (index >= 0)
							{
								VulkanVersion = Integer.parseInt(dump.substring(0, index));
								int VersionMajor = (VulkanVersion >> 22) & 0x03ff;
								int VersionMinor = (VulkanVersion >> 12) & 0x03ff;
								int VersionPatch = VulkanVersion & 0x0fff;
								Log.debug("Vulkan version: " + VersionMajor + "." + VersionMinor + "." + VersionPatch);
							}
						}
					}
				}
			}
		}

		// is this a native landscape device (tablet, tv)?
		if ( getDeviceDefaultOrientation() == Configuration.ORIENTATION_LANDSCAPE )
		{
			boolean bForceLandscape = false;

			// check for a Google TV by checking system feature support
			if (getPackageManager().hasSystemFeature("com.google.android.tv"))
			{
				Log.debug( "Detected Google TV, will default to landscape" );
				bForceLandscape = true;
			} else

			// check NVidia devices
			if (android.os.Build.MANUFACTURER.equals("NVIDIA"))
			{
				// is it a Shield? (checking exact model)
				if (android.os.Build.MODEL.equals("SHIELD"))
				{
					Log.debug( "Detected NVidia Shield, will default to landscape" );
					bForceLandscape = true;
				}
			} else

			// check Ouya
			if (android.os.Build.MANUFACTURER.equals("OUYA"))
			{
				// only one so far (ouya_1_1) but check prefix anyway
				if (android.os.Build.MODEL.toLowerCase().startsWith("ouya_"))
				{
					Log.debug( "Detected Ouya console (" + android.os.Build.MODEL + "), will default to landscape" );
					bForceLandscape = true;
				}
			} else

			// check Amazon devices
			if (android.os.Build.MANUFACTURER.equals("Amazon"))
			{
				// is it a Kindle Fire TV? (Fire TV FAQ says AFTB, but to check for AFT)
				if (android.os.Build.MODEL.startsWith("AFT"))
				{
					Log.debug( "Detected Kindle Fire TV (" + android.os.Build.MODEL + "), will default to landscape" );
					bForceLandscape = true;
				}
			}

			// apply the force request if we found a device above
			if (bForceLandscape)
			{
				Log.debug( "Setting screen orientation to landscape because we have detected landscape device" );
				_activity.setRequestedOrientation( android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE );
			}
		}
		
		// Grab a reference to the asset manager
		AssetManagerReference = this.getAssets();

		// Read metadata from AndroidManifest.xml
		appPackageName = getPackageName();
		String ProjectName = getPackageName();
		ProjectName = ProjectName.substring(ProjectName.lastIndexOf('.') + 1);
		try {
			ApplicationInfo ai = getPackageManager().getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;
			_bundle = bundle;

			if ((ai.flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) 
			{
				IsForDistribution = true;
			}

			// Get the preferred depth buffer size from AndroidManifest.xml
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.DepthBufferPreference"))
			{
				DepthBufferPreference = bundle.getInt("com.epicgames.ue4.GameActivity.DepthBufferPreference");
				Log.debug( "Found DepthBufferPreference = " + DepthBufferPreference);
			}
			else
			{
				Log.debug( "Did not find DepthBufferPreference, using default.");
			}

			// Determine if data is embedded in APK from AndroidManifest.xml
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bPackageDataInsideApk"))
			{
				PackageDataInsideApkValue = bundle.getBoolean("com.epicgames.ue4.GameActivity.bPackageDataInsideApk") ? 1 : 0;
				Log.debug( "Found bPackageDataInsideApk = " + PackageDataInsideApkValue);
			}
			else
			{
				PackageDataInsideApkValue = 0;
				Log.debug( "Did not find bPackageDataInsideApk, using default.");
			}

			// Get the project name from AndroidManifest.xml
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.ProjectName"))
			{
				ProjectName = bundle.getString("com.epicgames.ue4.GameActivity.ProjectName");
				Log.debug( "Found ProjectName = " + ProjectName);
			}
			else
			{
				Log.debug( "Did not find ProjectName, using package name = " + ProjectName);
			}
			
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bHasOBBFiles"))
			{
				HasOBBFiles = bundle.getBoolean("com.epicgames.ue4.GameActivity.bHasOBBFiles") ? 1 : 0;
				Log.debug( "Found bHasOBBFiles = " + HasOBBFiles);
			}
			else
			{
				HasOBBFiles = 0;
				Log.debug( "Did not find bHasOBBFiles, using default.");
			}
			
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp"))
			{
				VerifyOBBOnStartUp = bundle.getBoolean("com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp");
				Log.debug( "Found bVerifyOBBOnStartUp = " + VerifyOBBOnStartUp);
			}
			else
			{
				VerifyOBBOnStartUp = false;
				Log.debug( "Did not find bVerifyOBBOnStartUp, using default.");
			}
				
			if(bundle.containsKey("com.epicgames.ue4.GameActivity.bShouldHideUI"))
			{
				ShouldHideUI = bundle.getBoolean("com.epicgames.ue4.GameActivity.bShouldHideUI");
				Log.debug( "UI hiding set to " + ShouldHideUI);
			}
			else
			{
				Log.debug( "UI hiding not found. Leaving as " + ShouldHideUI);
			}
			if(bundle.containsKey("com.epicgames.ue4.GameActivity.BuildConfiguration"))
			{
				BuildConfiguration = bundle.getString("com.epicgames.ue4.GameActivity.BuildConfiguration");
				Log.debug( "BuildConfiguration set to " + BuildConfiguration);
			}
			else
			{
				Log.debug( "BuildConfiguration not found" );
			}

			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bUseExternalFilesDir"))
            {
                UseExternalFilesDir = bundle.getBoolean("com.epicgames.ue4.GameActivity.bUseExternalFilesDir");
                Log.debug( "UseExternalFilesDir set to " + UseExternalFilesDir);
            }
            else
            {
                Log.debug( "bUseExternalFilesDir not found. Leaving as " + UseExternalFilesDir);
            }

//$${gameActivityReadMetadataAdditions}$$
		}
		catch (NameNotFoundException e)
		{
			Log.debug( "Failed to load meta-data: NameNotFound: " + e.getMessage());
		}
		catch (NullPointerException e)
		{
			Log.debug( "Failed to load meta-data: NullPointer: " + e.getMessage());
		}

		Log.debug("APK path: " + getPackageResourcePath());
		Log.debug("OBB in APK: " + (PackageDataInsideApkValue==1));
		nativeSetGlobalActivity(UseExternalFilesDir, PackageDataInsideApkValue==1, getPackageResourcePath());

		// tell the engine if this is a portrait app
		nativeSetWindowInfo(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT, DepthBufferPreference);

		// get the full language code, like en-US
		// note: this may need to be Locale.getDefault().getLanguage()
		String Language = java.util.Locale.getDefault().toString();

		Log.debug( "Android version is " + android.os.Build.VERSION.RELEASE );
		Log.debug( "Android manufacturer is " + android.os.Build.MANUFACTURER );
		Log.debug( "Android model is " + android.os.Build.MODEL );
		Log.debug( "OS language is set to " + Language );

		nativeSetAndroidVersionInformation( android.os.Build.VERSION.RELEASE, android.os.Build.MANUFACTURER, android.os.Build.MODEL, Language );

		try
		{
			int Version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
			int PatchVersion = 0;
			nativeSetObbInfo(ProjectName, getApplicationContext().getPackageName(), Version, PatchVersion);
		}
		catch (Exception e)
		{
			// if the above failed, then, we can't use obbs
			Log.debug("==================================> PackageInfo failure getting .obb info: " + e.getMessage());
		}
		
		// enable the physical volume controls to the game
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);

		AlertDialog.Builder builder;

		consoleInputBox = new EditText(this);
		consoleInputBox.setInputType(0x00080001); // TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_NO_SUGGESTIONS);
		consoleHistoryList = new ArrayList<String>();
		consoleHistoryIndex = 0;

		final ViewConfiguration vc = ViewConfiguration.get(this);
        DisplayMetrics dm = getResources().getDisplayMetrics();
        consoleDistance = vc.getScaledPagingTouchSlop() * dm.density;
        consoleVelocity = vc.getScaledMinimumFlingVelocity() / 1000.0f;

		consoleInputBox.setOnTouchListener(new OnTouchListener() {
			private long downTime;
			private float downX;

			public void swipeLeft() {
				if (!consoleHistoryList.isEmpty() && consoleHistoryIndex + 1 < consoleHistoryList.size()) {
					consoleInputBox.setText(consoleHistoryList.get(++consoleHistoryIndex));
				}
			}

			public void swipeRight() {
				if (!consoleHistoryList.isEmpty() && consoleHistoryIndex > 0) {
					consoleInputBox.setText(consoleHistoryList.get(--consoleHistoryIndex));
				}
			}

			public boolean onTouch(View v, MotionEvent event) {
				switch (event.getAction()) {
					case MotionEvent.ACTION_DOWN: {
						// remember down time and position
						downTime = System.currentTimeMillis();
						downX = event.getX();
						return true;
					}
					case MotionEvent.ACTION_UP: {
						long deltaTime = System.currentTimeMillis() - downTime;
						float delta = event.getX() - downX;
						float absDelta = Math.abs(delta);

						if (absDelta > consoleDistance && absDelta > deltaTime * consoleVelocity)
						{
							if (delta < 0)
								this.swipeLeft();
							else
								this.swipeRight();
							return true;
						}
						return false;
					}
				}
				return false;
			}
		});

		// Spinner with Quick Stat Commands
		consoleSpinner = new Spinner(this);
		ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, CONSOLE_SPINNER_ITEMS);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		consoleSpinner.setAdapter(adapter);
		consoleSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override
			public void onItemSelected(AdapterView<?> adapterView, View view, int pos, long id) {
				if (pos > 0)
					consoleInputBox.setText(adapterView.getItemAtPosition(pos).toString());
			}

			@Override
			public void onNothingSelected(AdapterView<?> adapterView) {
				consoleInputBox.setText("");
				consoleSpinner.setSelection(0);
			}
		});

		// Layout for Quick Commands and Console Input
		consoleAlertLayout = new LinearLayout(this);
		consoleAlertLayout.setOrientation(LinearLayout.VERTICAL);
		consoleAlertLayout.addView(consoleSpinner);
		consoleAlertLayout.addView(consoleInputBox);

		builder = new AlertDialog.Builder(this);
		builder.setTitle("Console Window - Enter Command")
		.setMessage("")
		.setView(consoleAlertLayout)
		.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				String message = consoleInputBox.getText().toString().trim();

				// remove it if already in history
				int index = consoleHistoryList.indexOf(message);
				if (index >= 0)
					consoleHistoryList.remove(index);

				// add it to the end
				consoleHistoryList.add(message);

				nativeConsoleCommand(message);
				consoleInputBox.setText(" ");
				consoleSpinner.setSelection(0);
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		})
		.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				consoleInputBox.setText(" ");
				consoleSpinner.setSelection(0);
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		});
		consoleAlert = builder.create();

		virtualKeyboardInputBox = new EditText(this);
		virtualKeyboardInputBox.addTextChangedListener(new TextWatcher() {
			@Override
			public void beforeTextChanged(CharSequence charSequence, int start, int count, int after) {
			}

			@Override
			public void afterTextChanged(Editable s) {
				String message = virtualKeyboardInputBox.getText().toString();
				nativeVirtualKeyboardChanged(message);
			}

			@Override
			public void onTextChanged(CharSequence charSequence, int start, int before, int count) {
			}
		});

		builder = new AlertDialog.Builder(this);
		builder.setTitle("")
		.setView(virtualKeyboardInputBox)
		.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				String message = virtualKeyboardInputBox.getText().toString();
				nativeVirtualKeyboardResult(true, message);
				virtualKeyboardInputBox.setText(" ");
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		})
		.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				nativeVirtualKeyboardChanged(virtualKeyboardPreviousContents);
				nativeVirtualKeyboardResult(false, " ");
				virtualKeyboardInputBox.setText(" ");
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		});
		virtualKeyboardAlert = builder.create();

		GooglePlayLicensing.GoogleLicensing = new GooglePlayLicensing();
		GooglePlayLicensing.GoogleLicensing.Init(this, Log);

		// Build Google Play API Client
		googleClient = new GoogleApiClient.Builder(this)
			.addConnectionCallbacks(this)
			.addOnConnectionFailedListener(this)
			.addApi(Games.API).addScope(Games.SCOPE_GAMES)
			.addApi(Plus.API).addScope(Plus.SCOPE_PLUS_LOGIN)
			.build();

		// Now okay for event handler to be set up on native side
		//	nativeResumeMainInit();
				
		// Try to establish a connection to Google Play
		// AndroidThunkJava_GooglePlayConnect();

		// If we have data in the apk or just loose then carry on init as normal
		/*Log.debug(this.getObbDir().getAbsolutePath());
		String path = this.getObbDir().getAbsolutePath() + "/main.1.com.epicgames.StrategyGame.obb";
		File obb = new File(path);
		Log.debug("=+=+=+=+=+=+=> File exists: " + (obb.exists() ? "True" : "False"));
		*/
		if(PackageDataInsideApkValue == 1 || HasOBBFiles == 0)
		{
			HasAllFiles = true;
		}

		// check for OBB file present if we don't have all the files and don't need to verify
		if (!HasAllFiles && !VerifyOBBOnStartUp)
		{
			HasAllFiles = DownloadShim.expansionFilesDelivered(this);
		}

		containerFrameLayout = new FrameLayout(_activity);
		virtualKeyboardLayout = new LinearLayout(_activity);

		// Need to create our surface view here regardless of if we are going to end up using it
		getWindow().takeSurface(null);
		MySurfaceView = new SurfaceView(this);
		MySurfaceView.setBackgroundColor(Color.TRANSPARENT);
		MySurfaceView.getHolder().addCallback(this);
		containerFrameLayout.addView(MySurfaceView);
		containerFrameLayout.addView(virtualKeyboardLayout);
		setContentView(containerFrameLayout);

		// cache a reference to the main content view and set it so it can be focused on
        mainView = findViewById( android.R.id.content );
        mainView.setFocusable( true );
        mainView.setFocusableInTouchMode( true );

        mainDecorView = getWindow().getDecorView();
		mainDecorViewRect = new Rect();

		createVirtualKeyboardInput();
		
		mainView.getViewTreeObserver().addOnGlobalLayoutListener( new ViewTreeObserver.OnGlobalLayoutListener()
        {
        	@Override
        	public void onGlobalLayout()
        	{
				//Log.debug("VK: onGlobalLayout " + bKeyboardShowing);
        		if( bKeyboardShowing )
        		{
        			Rect visibleRect = new Rect();
        			View visibleView = mainView.getRootView();

        			visibleView.getWindowVisibleDisplayFrame( visibleRect );

					mainDecorView.getDrawingRect( mainDecorViewRect );

					//Log.debug("VK: onGlobalLayout visibleRect:(" +  visibleRect.left + ", " + visibleRect.top +", " + visibleRect.right +", " + visibleRect.bottom +")"+
        			//", mainDecorViewRect:" +  mainDecorViewRect.left + ", " + mainDecorViewRect.top +", " + mainDecorViewRect.right +", " + mainDecorViewRect.bottom + ")" );

        			// determine which side of the screen the keyboard is covering
        			int leftDiff = Math.abs( mainDecorViewRect.left - visibleRect.left );
        			int topDiff = Math.abs( mainDecorViewRect.top - visibleRect.top );
        			int rightDiff = Math.abs( mainDecorViewRect.right - visibleRect.right );
        			int bottomDiff = Math.abs( mainDecorViewRect.bottom - visibleRect.bottom );

        			// Rect covered by the virtual keyboard
        			Rect keyboardRect = new Rect();
    				keyboardRect.left = ( rightDiff > 0 ) ? visibleRect.right : mainDecorViewRect.left; // keyboard is on the right
    				keyboardRect.top = ( bottomDiff > 0 ) ? visibleRect.bottom : mainDecorViewRect.top; // keyboard is on the bottom
    				keyboardRect.right = ( leftDiff > 0 ) ? visibleRect.left : mainDecorViewRect.right; // keyboard is on the left
    				keyboardRect.bottom = ( topDiff > 0 ) ? visibleRect.top : mainDecorViewRect.bottom; // keyboard is on the top

					//keyboard Y coord
					int keyboardYPos = visibleRect.bottom - newVirtualKeyboardInput.getHeight();

					//avoid negative coords if the keyboard is shown on top of the screen
					if(keyboardYPos < 0)
						keyboardYPos = visibleRect.top + newVirtualKeyboardInput.getHeight();

                    int visibleScreenYOffset = Math.max(bottomDiff, topDiff);

        			nativeVirtualKeyboardShown( keyboardRect.left, keyboardRect.top, keyboardRect.right, keyboardRect.bottom );
					//Log.debug("VK: show?" + visibleScreenYOffset + "," + newVirtualKeyboardInput.getY());
                    if(visibleScreenYOffset > 200)
                    {
						//Log.debug("VK: show");
						//newVirtualKeyboardInput.setBackgroundColor(Color.WHITE);
						//newVirtualKeyboardInput.setCursorVisible(true);
                    	newVirtualKeyboardInput.setY(keyboardYPos);
                    	newVirtualKeyboardInput.setVisibility(View.VISIBLE);
						newVirtualKeyboardInput.requestFocus();
                    }
                    else if(newVirtualKeyboardInput.getY() > 0)
                    {
						//Log.debug("VK: hide");
						newVirtualKeyboardInput.setVisibility(View.GONE);
						//set offscreen
        				newVirtualKeyboardInput.setY(-1000); 
                    }
        		}
        	}
        });

//$${gameActivityOnCreateAdditions}$$
		
		Log.debug("==============> GameActive.onCreate complete!");
	}

	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy)
	{
	}
	
	private Handler mRestoreImmersiveModeHandler = new Handler();
	private Runnable restoreImmersiveModeRunnable = new Runnable()
	{
		public void run() 
		{
			restoreTransparentBars();
		}
	};

	public void restoreTranslucentBarsDelayed()
	{
		// we restore it now and after 500 ms!
		restoreTransparentBars();
		mRestoreImmersiveModeHandler.postDelayed(restoreImmersiveModeRunnable, 500);
	}

	public void restoreTransparentBars()
	{
		if(android.os.Build.VERSION.SDK_INT >= 19) {
			try {
				View decorView = getWindow().getDecorView(); 

				Log.debug("=== Restoring Transparent Bars ===");
				// Clear the flag and then restore it
				decorView.setSystemUiVisibility(
								View.SYSTEM_UI_FLAG_LAYOUT_STABLE
							| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
							| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_FULLSCREEN
							/*| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY*/);
				
				decorView.setSystemUiVisibility(
								View.SYSTEM_UI_FLAG_LAYOUT_STABLE
							| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
							| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_FULLSCREEN
							| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
				
			} catch (Exception e) {}
		}
	}

	@Override 
	public boolean onKeyDown(int keyCode, KeyEvent event) 
	{
		if(keyCode == KeyEvent.KEYCODE_BACK ||keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP)
		{
			if (ShouldHideUI)
			{
				Log.debug("=== Restoring Transparent Bars due to KeyCode ===");
				restoreTranslucentBarsDelayed();
			}
		}

		return super.onKeyDown(keyCode, event);
	}
	
	@Override
	public void onResume()
	{
		super.onResume();
		sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME);
		sensorManager.registerListener(this, magnetometer, SensorManager.SENSOR_DELAY_GAME);
		sensorManager.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME);
		// invalidate window cache
		nativeSetWindowInfo(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT, DepthBufferPreference);
		
		// only do this on KitKat and above
		if (ShouldHideUI)
		{ 
			restoreTransparentBars();
			View decorView = getWindow().getDecorView(); 

			decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
				@Override 
				public void onSystemUiVisibilityChange(int visibility) 
				{
					Log.debug("=== Restoring Transparent Bars due to Visibility Change ===");
					restoreTransparentBars();
				}
			});

			decorView.setOnFocusChangeListener(new View.OnFocusChangeListener() {
				@Override
				public void onFocusChange(View v, boolean hasFocus) 
				{
					Log.debug("=== Restoring Transparent Bars due to Focus Change ===");
					restoreTransparentBars();
				}
			});
		}
		
		if(HasAllFiles)
		{
			Log.debug("==============> Resuming main init");
			nativeResumeMainInit();
			InitCompletedOK = true;
		}
		else
		{
			// Post the check activity handler here to run after onResume completes
			Log.debug("==============> Posting request for downloader activity");
			final Handler downloadHandler = new Handler();
			downloadHandler.post(new Runnable() {
				@Override
				public void run() {
					Log.debug("==============> Starting activity to check files and download if required");
					Intent intent = new Intent(_activity, DownloadShim.GetDownloaderType());
					intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
					startActivityForResult(intent, DOWNLOAD_ACTIVITY_ID);
					if (noActionAnimID != -1) {
						overridePendingTransition(noActionAnimID, noActionAnimID);
					}
				}
			});
		}

		LocalNotificationCheckAppOpen();

		// Forcing this to false so the virtual keyboard can be shown again after resuming
		// since calls to showSoftInput are ignored on resume so have to make sure state is reset
		bKeyboardShowing = false;

//$${gameActivityOnResumeAdditions}$$
		Log.debug("==============> GameActive.onResume complete!");
	}

	@Override
	protected void onPause()
	{
		super.onPause();
		sensorManager.unregisterListener(this);

		// hide virtual keyboard before going into the background
		if( bKeyboardShowing )
		{
			AndroidThunkJava_HideVirtualKeyboardInput();
		}

		if(CurrentDialogType != EAlertDialogType.None)
		{
			//	If an AlertDialog is showing when the application is paused, it can cause our main window to be terminated
			//	Hide the dialog here. It will be shown again via AndroidThunkJava_ShowHiddenAlertDialog called from native code
			_activity.runOnUiThread(new Runnable()
			{
				public void run()
				{
					switch(CurrentDialogType)
					{
						// this hides the old alert dialog that was used for input
						case Keyboard:
							virtualKeyboardAlert.hide(); 
							break;
						case Console:
							consoleAlert.hide(); 
							break;
						default:
							Log.debug("ERROR: Unknown EAlertDialogType!");
							break;
					}
				}
			});
		}
//$${gameActivityOnPauseAdditions}$$
		Log.debug("==============> GameActive.onPause complete!");
	}
	
	@Override
	public void onSensorChanged(SensorEvent event)
	{
		float[] current_accelerometer = new float[]{0, 0, 0};
		float[] current_gyroscope = new float[]{0, 0, 0};
		float[] current_magnetometer = new float[]{0, 0, 0};
		int current_accelerometer_sample_count = 0;
		int current_gyroscope_sample_count = 0;
		int current_magnetometer_sample_count = 0;

		if (accelerometer != null && magnetometer != null)
		{
			if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER)
			{
			  System.arraycopy(event.values, 0, current_accelerometer, 0, current_accelerometer.length);
			  ++current_accelerometer_sample_count;
			}
			else if (event.sensor.getType() == Sensor.TYPE_MAGNETIC_FIELD)
			{
			  System.arraycopy(event.values, 0, current_magnetometer, 0, current_magnetometer.length);
			  ++current_magnetometer_sample_count;
			}
			else if (event.sensor.getType() == Sensor.TYPE_GYROSCOPE)
			{
			  System.arraycopy(event.values, 0, current_gyroscope, 0, current_gyroscope.length);
			  ++current_gyroscope_sample_count;
			}

			if (current_accelerometer_sample_count > 0)
			{
				// Do simple average of the samples we just got.
				for(int i = 0; i < current_accelerometer.length; i++)
					current_accelerometer[i] /= (float)current_accelerometer_sample_count;
				last_accelerometer = current_accelerometer;
			}
			else
			{
				current_accelerometer = last_accelerometer;
			}

			if (current_gyroscope_sample_count > 0)
			{
				// Do simple average of the samples we just got.
				for(int i = 0; i < current_gyroscope.length; i++)
					current_gyroscope[i] /= (float)current_gyroscope_sample_count;
			}

			if (current_magnetometer_sample_count > 0)
			{
				// Do simple average of the samples we just got.
				for(int i = 0; i < current_magnetometer.length; i++)
					current_magnetometer[i] /= (float)current_magnetometer_sample_count;
				last_magnetometer = current_magnetometer;
			}
			else
			{
				current_magnetometer = last_magnetometer;
			}

			// If we have motion samples we generate the single event.
			if (current_accelerometer_sample_count > 0 || current_gyroscope_sample_count > 0 ||	current_magnetometer_sample_count > 0)
			{
				// The data we compose the motion event from.
				float[] current_tilt = new float[]{0, 0, 0};
				float[] current_rotation_rate = new float[]{0, 0, 0};
				float[] current_gravity = new float[]{0, 0, 0};
				float[] current_acceleration = new float[]{0, 0, 0};

				

				// We use a low-pass filter to synthesize the gravity
				// vector.
				
				if (!first_acceleration_sample)
				{
					current_gravity[0] = last_gravity[0] * SampleDecayRate + current_accelerometer[0]*(1.0f - SampleDecayRate);
					current_gravity[1] = last_gravity[1] * SampleDecayRate + current_accelerometer[1]*(1.0f - SampleDecayRate);
					current_gravity[2] = last_gravity[2] * SampleDecayRate + current_accelerometer[2]*(1.0f - SampleDecayRate);
				}
				first_acceleration_sample = false;

				// get the rotation matrix value, the convert those to Euler angle rotation values
				updateOrientationAngles(current_accelerometer, current_magnetometer);

				current_tilt = new float[]{orientationAngles[1], orientationAngles[2], orientationAngles[0]};

				// And take out the gravity from the accel to get
				// the linear acceleration.
				for (int i = 0; i < current_acceleration.length; ++i)
					current_acceleration[i] = current_accelerometer[i] - current_gravity[i];

				if (current_gyroscope_sample_count > 0)
				{
					// The rotation rate is the what the gyroscope gives us.
					current_rotation_rate = current_gyroscope;
				}
				else if (null == gyroscope)
				{
					// If we don't have a gyroscope at all we need to calc a rotation
					// rate from our calculated tilt and a delta.
					for (int index = 0; index < current_rotation_rate.length; ++index)
						current_rotation_rate[index] = current_tilt[index] - last_tilt[index];
				}

				// Finally record the motion event with all the data.
				
				nativeHandleSensorEvents(current_tilt, current_rotation_rate, current_gravity, current_acceleration);

				// Update history values.
				last_tilt = current_tilt;
				last_gravity = current_gravity;
			}
		}
	}

	
	public void updateOrientationAngles(float[] accelerometerReading, float[] magnetometerReading)
	{

		sensorManager.getRotationMatrix(rotationMatrix, null, accelerometerReading, magnetometerReading);

		sensorManager.getOrientation(rotationMatrix, orientationAngles);
	}
	
	@Override
	public void onNewIntent(Intent newIntent)
	{
		super.onNewIntent(newIntent);
		setIntent(newIntent);
//$${gameActivityOnNewIntentAdditions}$$
	}

	@Override
	public void onStop()
	{
		super.onStop();

		if (consoleCmdReceiver != null)
		{
			unregisterReceiver(consoleCmdReceiver);
		}

//$${gameActivityOnStopAdditions}$$
		Log.debug("==============> GameActive.onStop complete!");
	}

	@Override
	public void onDestroy()
	{
		super.onDestroy();
		if( IapStoreHelper != null )
		{
			IapStoreHelper.onDestroy();
		}
//$${gameActivityOnDestroyAdditions}$$
		Log.debug("==============> GameActive.onDestroy complete!");
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig)
	{
		super.onConfigurationChanged(newConfig);

		switch (getWindowManager().getDefaultDisplay().getRotation())
		{
			case Surface.ROTATION_0:	DeviceRotation = 0;		break;
			case Surface.ROTATION_90:	DeviceRotation = 90;	break;
			case Surface.ROTATION_180:	DeviceRotation = 180;	break;
			case Surface.ROTATION_270:	DeviceRotation = 270;	break;
		}

		// forward the orientation
		boolean bPortrait = newConfig.orientation == Configuration.ORIENTATION_PORTRAIT;
		nativeOnConfigurationChanged(bPortrait);
	}

	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		if(bUseSurfaceView)
		{
			int newWidth = (DesiredHolderWidth > 0) ? DesiredHolderWidth : width;
			int newHeight = (DesiredHolderHeight > 0) ? DesiredHolderHeight : height;

			super.surfaceChanged(holder, format, newWidth, newHeight);

			holder.setFixedSize(newWidth, newHeight);

			nativeSetSurfaceViewInfo(holder.getSurfaceFrame().width(), holder.getSurfaceFrame().height());
		}
		else
		{
			super.surfaceChanged(holder, format, width, height);
		}
	}

	public void AndroidThunkJava_ShowHiddenAlertDialog()
	{
		if(CurrentDialogType != EAlertDialogType.None)
		{
			Log.debug("==============> [JAVA] AndroidThunkJava_ShowHiddenAlertDialog() - Showing " + CurrentDialogType);
		
			//	If an AlertDialog was showing onPause and we hid it, show it again
			_activity.runOnUiThread(new Runnable()
			{
				public void run()
				{
					switch(CurrentDialogType)
					{
						case Keyboard:
							virtualKeyboardAlert.show(); 
							break;
						case Console:
							consoleAlert.show(); 
							break;
						default:
							Log.debug("ERROR: Unknown EAlertDialogType!");
							break;
					}
				}
			});
		}
	}

	public void AndroidThunkJava_KeepScreenOn(boolean Enable)
	{
		if (Enable)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					_activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			});
		}
		else
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					_activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			});
		}
	}

	private class VibrateRunnable implements Runnable {
		private int duration;
		private Vibrator vibrator;

		VibrateRunnable(final int Duration, final Vibrator vibrator)
		{
			this.duration = Duration;
			this.vibrator = vibrator;
		}
		public void run ()
		{
			if (duration < 1)
			{
				vibrator.cancel();
			} else {
				vibrator.vibrate(duration);
			}
		}
	}

	public void AndroidThunkJava_Vibrate(int Duration)
	{
		Vibrator vibrator = (Vibrator)getSystemService(VIBRATOR_SERVICE);
		if (vibrator != null)
		{
			_activity.runOnUiThread(new VibrateRunnable(Duration, vibrator));
		}
	}

	// Called from event thread in NativeActivity	
	public void AndroidThunkJava_ShowConsoleWindow(String Formats)
	{
		if (consoleAlert.isShowing() == true)
		{
			Log.debug("Console already showing.");
			return;
		}

		// start at end of console history
		consoleHistoryIndex = consoleHistoryList.size();

		consoleAlert.setMessage("[Available texture formats: " + Formats + "]");
		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				if (consoleAlert.isShowing() == false)
				{
					Log.debug("Console not showing yet");
					consoleAlert.show(); 
					CurrentDialogType = EAlertDialogType.Console;
				}
			}
		});
	}

	// old virtual keyboard show/hide functions input dialog
	public void AndroidThunkJava_HideVirtualKeyboardInputDialog()
	{
		if (virtualKeyboardAlert.isShowing() == false)
		{
			Log.debug("Virtual keyboard already hidden.");
			return;
		}

		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				if (virtualKeyboardAlert.isShowing() == true)
				{
					Log.debug("Virtual keyboard hiding");
					virtualKeyboardInputBox.setText(" ");
					virtualKeyboardAlert.dismiss();
					CurrentDialogType = EAlertDialogType.None;
				}
			}
		});
	}

	public void AndroidThunkJava_ShowVirtualKeyboardInputDialog(int inInputType, String inLabel, String inContents)
	{
		if (virtualKeyboardAlert.isShowing() == true)
		{
			Log.debug("Virtual keyboard already showing.");
			return;
		}

		// Capture to pass into ui thread
		final int uiInputType = inInputType;
		final String uiLabel = inLabel;
		final String uiContents = inContents;

		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				// Set label and starting contents
				virtualKeyboardAlert.setTitle(uiLabel);

				// Ensure the input mode of the text box is set before setting the contents.
				// configure for type of input
				virtualKeyboardInputBox.setRawInputType(uiInputType);
				virtualKeyboardInputBox.setTransformationMethod((uiInputType & InputType.TYPE_TEXT_VARIATION_PASSWORD) == 0 ? null : PasswordTransformationMethod.getInstance());

				virtualKeyboardInputBox.setText("");
				virtualKeyboardInputBox.append(uiContents);
				virtualKeyboardPreviousContents = uiContents;

				if (virtualKeyboardAlert.isShowing() == false)
				{
					Log.debug("Virtual keyboard not showing yet");
					virtualKeyboardAlert.show(); 
					CurrentDialogType = EAlertDialogType.Keyboard;
				}
			}
		});
	}

	// new functions to show/hide virtual keyboard
	public void AndroidThunkJava_HideVirtualKeyboardInput()
	{
		//Log.debug("VK: AndroidThunkJava_HideVirtualKeyboardInput");

		//#jira UE-49143 Inconsistent virtual keyboard behavior tapping between controls
		lastVirtualKeyboardCommand = VirtualKeyboardCommand.VK_CMD_HIDE;
		virtualKeyboardHandler.removeCallbacksAndMessages(null) ;
		virtualKeyboardHandler.postDelayed(new Runnable()
		{
			public void run()
			{
				if(lastVirtualKeyboardCommand == VirtualKeyboardCommand.VK_CMD_HIDE)
				{
					processLastVirtualKeyboardCommand();
				}
			}
		}, lastVirtualKeyboardCommandDelay);
	}

	//initial settings for the virtual input
	String virtualKeyboardInputContent;
	int virtualKeyboardInputType;
	public void AndroidThunkJava_ShowVirtualKeyboardInput(int inInputType, String Label, String Contents)
	{
		//Log.debug("VK: AndroidThunkJava_ShowVirtualKeyboardInput");
		virtualKeyboardInputContent = Contents;
		virtualKeyboardInputType = inInputType;
		lastVirtualKeyboardCommand = VirtualKeyboardCommand.VK_CMD_SHOW;
		//#jira UE-49143 Inconsistent virtual keyboard behavior tapping between controls
		virtualKeyboardHandler.removeCallbacksAndMessages(null) ;
		virtualKeyboardHandler.postDelayed(new Runnable()
		{
			public void run()
			{
				if(lastVirtualKeyboardCommand == VirtualKeyboardCommand.VK_CMD_SHOW)
				{
					processLastVirtualKeyboardCommand();
				}
			}
		}, lastVirtualKeyboardCommandDelay);
	}
	
	//jira UE-49141 Virtual keyboard is unresponsive with repeated tapping in control (some devices)
	//#jira UE-49139 Tapping in the same text box doesn't make the virtual keyboard disappear
	public void processLastVirtualKeyboardCommand()
	{
		Log.debug("VK: process last command " + lastVirtualKeyboardCommand);
		synchronized(this) {
			switch(lastVirtualKeyboardCommand)	
			{
				case VK_CMD_SHOW:
				{
					newVirtualKeyboardInput.setVisibility(View.VISIBLE);
				
					//newVirtualKeyboardInput.setBackgroundColor(Color.TRANSPARENT);
					//newVirtualKeyboardInput.setCursorVisible(false);

					//set offscreen
					newVirtualKeyboardInput.setY(-1000);

					//set new content
					newVirtualKeyboardInput.setText(virtualKeyboardInputContent);
				
					int newVirtualKeyboardInputType = virtualKeyboardInputType;

					//commented: as it will disable text prediction for all devices, 
					//	for most of them it will also block the VK in latin/english subtype
					//	disabling chinese or korean subtypes
					//if((virtualKeyboardInputType & InputType.TYPE_TEXT_VARIATION_PASSWORD) == 0)
					//{
						//#jira UE-49117 Chinese and Korean virtual keyboards don't allow native characters
						//#jira UE-49121 Gboard and Swift swipe entry are not supported by Virtual keyboard
						//newVirtualKeyboardInputType |= TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
					//}

					//TYPE: disable text suggestion
					newVirtualKeyboardInputType |= InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
					//TYPE: disable autocorrect
					newVirtualKeyboardInputType &= ~InputType.TYPE_TEXT_FLAG_AUTO_CORRECT;
				
					//TYPE: set input type flags
					newVirtualKeyboardInput.setInputType(newVirtualKeyboardInputType);
					newVirtualKeyboardInput.setRawInputType(newVirtualKeyboardInputType);

					//IME: set Done button for single line input
					int imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI;


					if (ANDROID_BUILD_VERSION >=  11)
					{
						imeOptions |= EditorInfo.IME_FLAG_NO_FULLSCREEN;
					}

					//IME: set single/multi line input type
					if((virtualKeyboardInputType & InputType.TYPE_TEXT_FLAG_MULTI_LINE) != 0)
					{
						//disable enter for multi-line - will be treated by virtualKeyboardInputType in sendKeyEvent
						newVirtualKeyboardInput.setSingleLine(false);
						//#jira UE-49128 Virtual Keyboard text field doesn't appear if there is too much text
						newVirtualKeyboardInput.setMaxLines(5);
						imeOptions |= EditorInfo.IME_FLAG_NO_ENTER_ACTION;
						imeOptions &= ~EditorInfo.IME_ACTION_DONE;
					}
					else
					{
						newVirtualKeyboardInput.setSingleLine(true);
						imeOptions &= ~EditorInfo.IME_FLAG_NO_ENTER_ACTION;
						imeOptions |= EditorInfo.IME_ACTION_DONE;
					}

					//IME: set IME flags
					newVirtualKeyboardInput.setImeOptions(imeOptions);

					//TRANSFORMATION: hide input for passwords
					newVirtualKeyboardInput.setTransformationMethod((
						virtualKeyboardInputType & InputType.TYPE_TEXT_VARIATION_PASSWORD) == 0 ? 
						null : 
						PasswordTransformationMethod.getInstance());

					//SELECTION: move to end
					newVirtualKeyboardInput.setSelection(newVirtualKeyboardInput.getText().length());

					if(newVirtualKeyboardInput.requestFocus())
					{
						//Log.debug("VK: Show newVirtualKeyboardInput");
						InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
						imm.showSoftInput(newVirtualKeyboardInput, 0);

						nativeVirtualKeyboardVisible(true);
						bKeyboardShowing = true;
					}
				}
				break;
				case VK_CMD_HIDE:
				{
						if(bKeyboardShowing)
						{
							//Log.debug("VK: Hide newVirtualKeyboardInput");

							newVirtualKeyboardInput.clearFocus();
							//set offscreen
							newVirtualKeyboardInput.setY(-1000);

							newVirtualKeyboardInput.setVisibility(View.GONE);

							InputMethodManager imm =(InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
							imm.hideSoftInputFromWindow(newVirtualKeyboardInput.getWindowToken(), 0);

							nativeVirtualKeyboardVisible(false);
							bKeyboardShowing = false;
						}
				}
				break;
			}
		}
		lastVirtualKeyboardCommand = VirtualKeyboardCommand.VK_CMD_NONE;
	}

	public void AndroidThunkJava_LaunchURL(String URL)
	{
		Log.debug("[JAVA} AndroidThunkJava_LaunchURL: URL = " + URL);
		if (!URL.contains("://"))
		{
			URL = "http://" + URL;
			Log.debug("[JAVA} AndroidThunkJava_LaunchURL: corrected URL = " + URL);
		}
		try
		{
			Intent BrowserIntent = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(URL));

			// open browser on its own task
			BrowserIntent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY | Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
			BrowserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
			
			// make sure there is a web browser to handle the URL before trying to start activity (or may crash!)
			if (BrowserIntent.resolveActivity(getPackageManager()) != null)
			{
				Log.debug("[JAVA} AndroidThunkJava_LaunchURL: Starting activity");
				startActivity(BrowserIntent);
			}
			else
			{
				Log.debug("[JAVA} AndroidThunkJava_LaunchURL: Could not find an application to receive the URL intent");
			}
		}
		catch (Exception e)
		{
			Log.debug("[JAVA} AndroidThunkJava_LaunchURL: Failed with exception " + e.getMessage());
		}
	}

	public String AndroidThunkJava_GetAndroidId()
	{
		try {
			String androidId = Secure.getString(getApplicationContext().getContentResolver(), Secure.ANDROID_ID);
			return androidId;
		} catch (Exception e) {
			Log.debug("GetAndroidId failed: " + e.getMessage());
		}
		return null;
	}

	public void AndroidThunkJava_ResetAchievements()
	{
		/* Disable so don't need GET_ACCOUNTS - we don't need this
		try
        {
			String accesstoken = GetAccessToken();
			          
			if(!accesstoken.equals(""))
			{
				String ResetURL = "https://www.googleapis.com/games/v1management/achievements/reset?access_token=" + accesstoken;
				Log.debug("AndroidThunkJava_ResetAchievements: using URL " + ResetURL);

				URL url = new URL(ResetURL);
				HttpURLConnection urlConnection = (HttpURLConnection)url.openConnection();

				try
				{
					urlConnection.setRequestMethod("POST");
					int status = urlConnection.getResponseCode();
					Log.debug("AndroidThunkJava_ResetAchievements: HTTP response is " + status);
				}
				finally
				{
					urlConnection.disconnect();
				}
			}
			else
			{
				Log.debug("AndroidThunkJava_ResetAchievements: Access Token not returned.  Possible reason is that android.permission.GET_ACCOUNTS is not granted.  Make sure to add in by going to Project settings > Android > Advanced Project Settings and check the box for \"Request Access Token On Connect\". ");
			}
        }
        catch(Exception e)
        {
            Log.debug("AndroidThunkJava_ResetAchievements failed: " + e.getMessage());
        }
		*/
		Log.debug("AndroidThunkJava_ResetAchievements: disabled");
	}

	// TODO: replace this with non-depreciated method (OK now for up to API-25)
	@TargetApi(23)
	private String GetAccessToken()
	{
		String accesstoken = "";

		try
        {
			if (PermissionHelper.checkPermission("android.permission.GET_ACCOUNTS"))
			{
				String email = Plus.AccountApi.getAccountName(googleClient);
				Log.debug("GetAccessToken: using email " + email);

				accesstoken = GoogleAuthUtil.getToken(this, email, "oauth2:https://www.googleapis.com/auth/games");
			}
		}
		catch(Exception e)
        {
            Log.debug("GetAccessToken failed: " + e.getMessage());
        }

		return accesstoken;
	}

	public void AndroidThunkJava_GoogleClientConnect()
	{
		if (googleClient != null)
		{
			googleClient.connect();
		}
	}

	public void AndroidThunkJava_GoogleClientDisconnect()
	{
		if (googleClient != null)
		{
			googleClient.disconnect();
		}
	}

	// Google Client connected successfully
	@Override
	public void onConnected(Bundle connectionHint)
	{
		if (googleClient != null && googleClient.isConnected())
		{
			new Thread(new Runnable()
			{
				public void run()
				{
					/* Don't do this since deprecated and don't want to request GET_ACCOUNTS permission
					try
					{
						String accesstoken = GetAccessToken();
						if(!accesstoken.equals(""))
						{
							Log.debug("Google Client Connect using Access Token " + accesstoken);
							nativeGoogleClientConnectCompleted(true, accesstoken);
						}
						else
						{
							Log.debug("Google Client Connect succeeded but no access token returned");
							nativeGoogleClientConnectCompleted(true, "");
						}
					}
					catch (Exception e)
					{
						Log.debug("Google Client Connect failed: " + e.getMessage());

						nativeGoogleClientConnectCompleted(false, "");
					}
					*/
					nativeGoogleClientConnectCompleted(true, "NOT_ACQUIRED");
				}
			}).start();
		}
		else
		{
			nativeGoogleClientConnectCompleted(false, "");
		}
	}

	// Google Client connection failed
	@Override
	public void onConnectionFailed(ConnectionResult connectionResult)
	{
		Log.debug("Google Client Connect failed. Error Code: " + connectionResult.getErrorCode() + " Description: " + connectionResult.getErrorMessage());
		nativeGoogleClientConnectCompleted(false, "");
	}

	// Google Client connection suspended
	@Override
	public void onConnectionSuspended(int cause)
	{
		Log.debug("Google Client Connect Suspended: " + cause);
	}

	public AssetManager AndroidThunkJava_GetAssetManager()
	{
		if(AssetManagerReference == null)
		{
			Log.debug("No reference to asset manager found!");
		}

		return AssetManagerReference;
	}

	public static boolean isOBBInAPK()
	{
		Log.debug("Asking if osOBBInAPK? " + (PackageDataInsideApkValue == 1));
		return PackageDataInsideApkValue == 1;
	}

	public void AndroidThunkJava_Minimize()
	{
		Intent startMain = new Intent(Intent.ACTION_MAIN);
		startMain.addCategory(Intent.CATEGORY_HOME);
		startMain.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity(startMain);
	}

	public void AndroidThunkJava_ForceQuit()
	{
		System.exit(0);
		// finish();
	}

	// call back into native code from the Java UI thread, initializing any available VR HMD modules
	public void AndroidThunkJava_InitHMDs()
	{
		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				nativeInitHMDs();
			}
		});
	}

	public static String AndroidThunkJava_GetFontDirectory()
	{
		// Parse and find the first known fonts directory on the device
		String[] fontdirs = { "/system/fonts", "/system/font", "/data/fonts" };

		String targetDir = null;

		for ( String fontdir : fontdirs )
        {
            File dir = new File( fontdir );

			if(dir.exists())
			{
				targetDir = fontdir;
				break;
			}
		}
		
		return targetDir + "/";
	}
	
	public static String getAppPackageName()
	{
		return appPackageName;
	}

	public boolean AndroidThunkJava_IsMusicActive()
	{
		AudioManager audioManager = (AudioManager)this.getSystemService(Context.AUDIO_SERVICE);
		return audioManager.isMusicActive();
	}
	
	// In app purchase functionality
	public void AndroidThunkJava_IapSetupService(String InProductKey)
	{
		if (getPackageManager().checkPermission("com.android.vending.BILLING", getPackageName()) == getPackageManager().PERMISSION_GRANTED)
		{
			IapStoreHelper = new GooglePlayStoreHelper(InProductKey, this, Log);
			if (IapStoreHelper != null)
			{
				Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - Setup started");
			}
			else
			{
				Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - Failed to setup IAP service");
			}
		}
		else
		{
			Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - You do not have the appropriate permission setup.");
			Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - Please ensure com.android.vending.BILLING is added to the manifest.");
		}
	}
	

	private String[] CachedQueryProductIDs;
	public boolean AndroidThunkJava_IapQueryInAppPurchases(String[] ProductIDs)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapQueryInAppPurchases");
		CachedQueryProductIDs = ProductIDs;

		boolean bTriggeredQuery = false;
		if( IapStoreHelper != null )
		{
			bTriggeredQuery = true;

			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					IapStoreHelper.QueryInAppPurchases(CachedQueryProductIDs);
				}
			});
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredQuery;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		if( requestCode == DOWNLOAD_ACTIVITY_ID)
		{
			int errorCode = 0;
			if(resultCode == RESULT_OK)
			{
								
				errorCode = data.getIntExtra(DOWNLOAD_RETURN_NAME, DOWNLOAD_NO_RETURN_CODE);
				
				String logMsg = "DownloadActivity Returned with ";
				switch(errorCode)
				{
				case DOWNLOAD_FILES_PRESENT:
					logMsg += "Download Files Present";
					break;
				case DOWNLOAD_COMPLETED_OK:
					logMsg += "Download Completed OK";
					break;
				case DOWNLOAD_NO_RETURN_CODE:
					logMsg += "Download No Return Code";
					break;
				case DOWNLOAD_USER_QUIT:
					logMsg += "Download User Quit";
					break;
				case DOWNLOAD_FAILED:
					logMsg += "Download Failed";
					break;
				case DOWNLOAD_INVALID:
					logMsg += "Download Invalid";
					break;
				case DOWNLOAD_NO_PLAY_KEY:
					logMsg +="Download No Play Key";
					break;
				default:
					logMsg += "Unknown message!";
					break;
				}
				
				Log.debug(logMsg);
			}
			else
			{
				Log.debug("Download activity cancelled by user.");
				errorCode = DOWNLOAD_USER_QUIT;
			}
			
			HasAllFiles = (errorCode == DOWNLOAD_FILES_PRESENT || errorCode == DOWNLOAD_COMPLETED_OK);
			
			if(errorCode == DOWNLOAD_NO_RETURN_CODE 
			|| errorCode == DOWNLOAD_USER_QUIT 
			|| errorCode == DOWNLOAD_FAILED 
			|| errorCode == DOWNLOAD_INVALID
			|| errorCode == DOWNLOAD_NO_PLAY_KEY)
			{
				finish();
				return;
			}
		}
		else if( IapStoreHelper != null )
		{
			if(!IapStoreHelper.onActivityResult(requestCode, resultCode, data))
			{
				super.onActivityResult(requestCode, resultCode, data);
			}
			else
			{
				Log.debug("[JAVA] - Store Helper handled onActivityResult");
			}
		}
		else
		{
			super.onActivityResult(requestCode, resultCode, data);
		}

//$${gameActivityOnActivityResultAdditions}$$
		
		if(InitCompletedOK)
		{
			nativeOnActivityResult(this, requestCode, resultCode, data);
		}
	}
	
	public boolean AndroidThunkJava_IapBeginPurchase(String ProductId)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapBeginPurchase");
		boolean bTriggeredPurchase = false;
		if( IapStoreHelper != null )
		{
			bTriggeredPurchase = IapStoreHelper.BeginPurchase(ProductId);
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredPurchase;
	}

	public boolean AndroidThunkJava_IapIsAllowedToMakePurchases()
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapIsAllowedToMakePurchases");
		boolean bIsAllowedToMakePurchase = false;
		if( IapStoreHelper != null )
		{
			bIsAllowedToMakePurchase = IapStoreHelper.IsAllowedToMakePurchases();
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bIsAllowedToMakePurchase;
	}

	public boolean AndroidThunkJava_IapConsumePurchase(String purchaseToken)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapConsumePurchase " + purchaseToken);

		if( IapStoreHelper != null )
		{
			IapStoreHelper.ConsumePurchase(purchaseToken);
			return true;
		}

		Log.debug("[JAVA] - Store Helper is invalid");
		return false;
	}

	public boolean AndroidThunkJava_IapQueryExistingPurchases()
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapQueryExistingPurchases");
		boolean bTriggeredQuery = false;
		if( IapStoreHelper != null )
		{
			Log.debug("[JAVA] - AndroidThunkJava_IapQueryExistingPurchases - Kick off logic here!");
			bTriggeredQuery = IapStoreHelper.QueryExistingPurchases();
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredQuery;
	}

	public boolean AndroidThunkJava_IapRestorePurchases(String[] InProductIDs, boolean[] bConsumable)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapRestorePurchases");
		boolean bTriggeredRestore = false;
		if( IapStoreHelper != null )
		{
			Log.debug("[JAVA] - AndroidThunkJava_IapRestorePurchases - Kick off logic here!");
			bTriggeredRestore = IapStoreHelper.RestorePurchases(InProductIDs, bConsumable);
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredRestore;
	}

	public void AndroidThunkJava_DismissSplashScreen()
	{
		if (mSplashDialog != null)
		{
			mSplashDialog.dismiss();
			mSplashDialog = null;
		}
	}

	private static class DeviceInfoData {
		public final int vendorId;
		public final int productId;
		public final String name;

		DeviceInfoData(int vid, int pid, String inName)
		{
			vendorId = vid;
			productId = pid;
			name = inName;
		}

		boolean IsMatch(int vid, int pid)
		{
			return (vendorId == vid && productId == pid);
		}
	}

	private void LocalNotificationCheckAppOpen()
	{
		Intent launchIntent = getIntent();
		if (launchIntent != null)
		{	
			Bundle extrasBundle = launchIntent.getExtras();

			localNotificationAppLaunched = launchIntent.getBooleanExtra("localNotificationAppLaunched", false);

			if(localNotificationAppLaunched)
			{
				localNotificationLaunchActivationEvent = extrasBundle.get("localNotificationLaunchActivationEvent").toString();
				int notificationID = extrasBundle.getInt("localNotificationID");

				LocalNotificationRemoveID(notificationID);

				// TODO
				localNotificationLaunchFireDate = 0; 
			}
		}
		else
		{
			localNotificationAppLaunched = false;
			localNotificationLaunchActivationEvent = "";
			localNotificationLaunchFireDate = 0;
		}
	}

	private int LocalNotificationGetID()
	{
		SharedPreferences preferences = getApplicationContext().getSharedPreferences("LocalNotificationPreferences", MODE_PRIVATE);
		SharedPreferences.Editor editor = preferences.edit();
		String notificationIDs = preferences.getString("notificationIDs", "");

		int idToReturn = 1;
		if(notificationIDs.length() == 0)
		{
			editor.putString("notificationIDs", Integer.toString(idToReturn));
		}
		else
		{
			String[] parts = notificationIDs.split("-");
			ArrayList<Integer> iParts = new ArrayList<Integer>();
			for(String part : parts)
			{
				if(part.length() > 0)
				{
					iParts.add(Integer.parseInt(part));
				}
			}
			while(true)
			{
				if(!iParts.contains(idToReturn))
				{
					break;
				}
				idToReturn++;
			}
			editor.putString("notificationIDs", notificationIDs + "-" + idToReturn);
		}

		notificationIDs = preferences.getString("notificationIDs", "");

		editor.commit();

		return idToReturn;
	}

	private ArrayList<Integer> LocalNotificationGetIDList()
	{
		SharedPreferences preferences = getApplicationContext().getSharedPreferences("LocalNotificationPreferences", MODE_PRIVATE);
		SharedPreferences.Editor editor = preferences.edit();
		String notificationIDs = preferences.getString("notificationIDs", "");
		ArrayList<Integer> iParts = new ArrayList<Integer>();

		String[] parts = notificationIDs.split("-");
		for(String part : parts)
		{
			if(part.length() > 0)
			{
				iParts.add(Integer.parseInt(part));
			}
		}

		return iParts;
	}

	private void LocalNotificationRemoveID(int notificationID)
	{
		SharedPreferences preferences = getApplicationContext().getSharedPreferences("LocalNotificationPreferences", MODE_PRIVATE);
		SharedPreferences.Editor editor = preferences.edit();
		String notificationIDs = preferences.getString("notificationIDs", null);

		ArrayList<String> iParts = new ArrayList<String>();

		if(notificationIDs.length() == 0)
		{
			return;
		}
		else
		{
			String[] parts = notificationIDs.split("-");
			for(String part : parts)
			{
				if(part.length() > 0)
				{
					iParts.add(part);
				}
			}
			iParts.remove(Integer.toString(notificationID));
		}

		String newNotificationIDs = "";
		for(String notifID : iParts)
		{
			if(newNotificationIDs.length() == 0)
			{
				newNotificationIDs = notifID;
			}
			else
			{
				newNotificationIDs += "-" + notifID;
			}
		}

		editor.putString("notificationIDs", newNotificationIDs);
		editor.commit();
	}

	public void AndroidThunkJava_LocalNotificationScheduleAtTime(String targetDateTime, boolean localTime, String title, String body, String action, String activationEvent) 
	{
		int notificationID = LocalNotificationGetID();

		// Create callback for PendingIntent
		Intent notificationIntent = new Intent(this, LocalNotificationReceiver.class); 

		// Add user-provided data
		notificationIntent.putExtra("local-notification-ID", notificationID);
		notificationIntent.putExtra("local-notification-title", title);
		notificationIntent.putExtra("local-notification-body", body);
		notificationIntent.putExtra("local-notification-action", action);
		notificationIntent.putExtra("local-notification-activationEvent", activationEvent);
		
		// Designate the callback as a PendingIntent
		PendingIntent pendingIntent = PendingIntent.getBroadcast(this, notificationID, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

		TimeZone targetTimeZone = TimeZone.getTimeZone("UTC");

		if(localTime) 
		{
			targetTimeZone = TimeZone.getDefault();
		}

		DateFormat targetDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
		targetDateFormat.setTimeZone(targetTimeZone);

		Date targetDate = new Date();

		try 
		{
			targetDate = targetDateFormat.parse(targetDateTime);
		} 

		catch (ParseException e) 
		{
			e.printStackTrace();
			return;
		}

		Date currentDate = new Date();

		long msDiff = targetDate.getTime() - currentDate.getTime();

		if(msDiff < 0)
		{
			return;
		}

		long futureTimeInMillis = SystemClock.elapsedRealtime() + msDiff;//Calculate the time to run the callback
		AlarmManager alarmManager = (AlarmManager)getSystemService(Context.ALARM_SERVICE);

		//Schedule the operation by using AlarmService
		alarmManager.set(AlarmManager.ELAPSED_REALTIME_WAKEUP, futureTimeInMillis, pendingIntent);
	}

	public class LaunchNotification {
		public boolean	used;
		public String	event;
		public int		fireDate;

		LaunchNotification(boolean inUsed, String inEvent, int inFireDate)
		{
			used = inUsed;
			event = inEvent;
			fireDate = inFireDate;
		}
	}

	public LaunchNotification AndroidThunkJava_LocalNotificationGetLaunchNotification()
	{
		return new LaunchNotification(localNotificationAppLaunched, localNotificationLaunchActivationEvent, localNotificationLaunchFireDate);
	}

	public void AndroidThunkJava_LocalNotificationClearAll()
	{
		ArrayList<Integer> idList = LocalNotificationGetIDList(); 

		for(int curID : idList)
		{
			AlarmManager alarmManager = (AlarmManager)getSystemService(Context.ALARM_SERVICE);
			PendingIntent pendingIntent = PendingIntent.getBroadcast(this, curID, new Intent(this, LocalNotificationReceiver.class), PendingIntent.FLAG_UPDATE_CURRENT);
			pendingIntent.cancel();
			alarmManager.cancel(pendingIntent);
		}
	}

	/*
	// Returns true only if the scheduled notification exists and gets destoyed successfully
	public boolean AndroidThunkJava_LocalNotificationDestroyIfExists(int notificationId)
	{
		if (AndroidThunkJava_ScheduledNotificationExists(notificationId))
		{
			//Cancel the intent itself as well as from the alarm manager
			AlarmManager alarmManager = (AlarmManager)getSystemService(Context.ALARM_SERVICE);
			PendingIntent pendingIntent = PendingIntent.getBroadcast(this, notificationId, new Intent(this, ScheduledNotificationReceiver.class), PendingIntent.FLAG_UPDATE_CURRENT);
			pendingIntent.cancel();
			alarmManager.cancel(pendingIntent);
			return true;
		}
		return false;
	}
	*/

	// List of vendor/product ids
	private static final DeviceInfoData[] DeviceInfoList = {
		new DeviceInfoData(0x04e8, 0xa000, "Samsung Game Pad EI-GP20"),
		new DeviceInfoData(0x0955, 0x7203, "NVIDIA Corporation NVIDIA Controller v01.01"),
		new DeviceInfoData(0x0955, 0x7210, "NVIDIA Corporation NVIDIA Controller v01.03"),
		new DeviceInfoData(0x1949, 0x0404, "Amazon Fire TV Remote"),
		new DeviceInfoData(0x1949, 0x0406, "Amazon Fire Game Controller"),
		new DeviceInfoData(0x0738, 0x5263, "Mad Catz C.T.R.L.R (Smart)"),
		new DeviceInfoData(0x0738, 0x5266, "Mad Catz C.T.R.L.R"),
		new DeviceInfoData(0x045e, 0x02e0, "Xbox Wireless Controller"),
		new DeviceInfoData(0x0111, 0x1419, "SteelSeries Stratus XL"), 
		new DeviceInfoData(0x054c, 0x05c4, "PS4 Wireless Controller")
	};

	public class InputDeviceInfo {
		public int deviceId;
		public int vendorId;
		public int productId;
		public int controllerId;
		public String name;
		public String descriptor;

		InputDeviceInfo(int did, int vid, int pid, int cid, String inName, String inDescriptor)
		{
			deviceId = did;
			vendorId = vid;
			productId = pid;
			controllerId = cid;
			name = inName;
			descriptor = inDescriptor;
		}
	}

	public InputDeviceInfo AndroidThunkJava_GetInputDeviceInfo(int deviceId)
	{
		int[] deviceIds = InputDevice.getDeviceIds();
		for (int deviceIndex=0; deviceIndex < deviceIds.length; deviceIndex++)
		{
			InputDevice inputDevice = InputDevice.getDevice(deviceIds[deviceIndex]);
			if (inputDevice.getId() == deviceId)
			{
				int vendorId = 0;
				int productId = 0;
				int controllerNumber = 0;

				// requires 4.1 (Jellybean)
				String descriptor = ANDROID_BUILD_VERSION >= 16 ? inputDevice.getDescriptor() : Integer.toString(deviceId);

				// supported on 4.4 (KitKat) onward
				if (ANDROID_BUILD_VERSION >= 19)
				{
					vendorId = inputDevice.getVendorId();
					productId = inputDevice.getProductId();
					controllerNumber = inputDevice.getControllerNumber();

					// note: inputDevice.getName may not return a proper name so check vendor and product id first
					for (DeviceInfoData deviceInfo : DeviceInfoList)
					{
						if (deviceInfo.IsMatch(vendorId, productId))
						{
							return new InputDeviceInfo(deviceId, vendorId, productId, controllerNumber, deviceInfo.name, descriptor);
						}
					}
				}

				// use device name as fallback (may be generic like "Bluetooth HID" so not always useful)
				return new InputDeviceInfo(deviceId, vendorId, productId, controllerNumber, inputDevice.getName(), descriptor);
			}
		}
		return new InputDeviceInfo(deviceId, 0, 0, -1, "Unknown", "Unknown");
	}

	public void AndroidThunkJava_UseSurfaceViewWorkaround()
	{
		// We only need apply a change to the SurfaceHolder on the first call
		// Once bUseSurfaceView is true, it is never changed back
		if(bUseSurfaceView)
		{
			return;
		}

		bUseSurfaceView = true;
		Log.debug("[JAVA] Using SurfaceView sizing workaround for this device");

		if(DesiredHolderWidth > 0 && 
			DesiredHolderHeight > 0 &&
			MySurfaceView != null)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					MySurfaceView.getHolder().setFixedSize(DesiredHolderWidth, DesiredHolderHeight);
				}
			});
		}
	}

	public void AndroidThunkJava_SetDesiredViewSize(int width, int height)
	{
		if (width == DesiredHolderWidth && height == DesiredHolderHeight)
		{
			return;
		}

		Log.debug("[JAVA] - SetDesiredViewSize width=" + width + " and height=" + height);
		DesiredHolderWidth = width;
		DesiredHolderHeight = height;

		if(bUseSurfaceView && MySurfaceView != null)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					MySurfaceView.getHolder().setFixedSize(DesiredHolderWidth, DesiredHolderHeight);
				}
			});
		}
	}

	public boolean AndroidThunkJava_IsGamepadAttached()
	{
		int[] deviceIds = InputDevice.getDeviceIds();
		for (int deviceIndex=0; deviceIndex < deviceIds.length; deviceIndex++)
		{
			InputDevice inputDevice = InputDevice.getDevice(deviceIds[deviceIndex]);
			// is it a joystick, dpad, or gamepad?
			if (((inputDevice.getSources() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
                || ((inputDevice.getSources() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK)
				|| ((inputDevice.getSources() & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD))
			{
				return true;
			}
		}

		return false;
	}

	public boolean AndroidThunkJava_HasActiveWiFiConnection()
	{
		ConnectivityManager cm = (ConnectivityManager)this.getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
		
		boolean isConnected = (activeNetwork != null && activeNetwork.isConnectedOrConnecting());
		if (isConnected)
		{
			return (activeNetwork.getType() == ConnectivityManager.TYPE_WIFI);
		}
		return false;
	}
	
	public boolean AndroidThunkJava_HasMetaDataKey(String key)
	{
		if (_bundle == null || key == null)
		{
			return false;
		}
		return _bundle.containsKey(key);
	}

	public boolean AndroidThunkJava_GetMetaDataBoolean(String key)
	{
		if (_bundle == null || key == null)
		{
			return false;
		}
		return _bundle.getBoolean(key);
	}

	public int AndroidThunkJava_GetMetaDataInt(String key)
	{
		if (key.equals("android.hardware.vulkan.version"))
		{
			return VulkanVersion;
		}
		else
		if (key.equals("android.hardware.vulkan.level"))
		{
			return VulkanLevel;
		}
		else
		if (key.equals("audiomanager.framesPerBuffer"))
		{
			AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
			String framesPerBuffer = am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
			int framesPerBufferInt = Integer.parseInt(framesPerBuffer);
			if (framesPerBufferInt == 0) framesPerBufferInt = 256; // Use default
			Log.debug("[JAVA] audiomanager.framesPerBuffer = " + framesPerBufferInt);
			return framesPerBufferInt;
		}
		else
		if (key.equals("audiomanager.optimalSampleRate"))
		{
			AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
			String sampleRateStr = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
			int sampleRateInt = Integer.parseInt(sampleRateStr);
			if (sampleRateInt == 0) sampleRateInt = 44100; // Use a default value if property not found
			Log.debug("[JAVA] audiomanager.optimalSampleRate = " + sampleRateInt);
			return sampleRateInt;
		}
		if (_bundle == null || key == null)
		{
			return 0;
		}
		return _bundle.getInt(key);
	}

	public String AndroidThunkJava_GetMetaDataString(String key)
	{
		if (key.equals("ue4.displaymetrics.dpi"))
		{
			DisplayMetrics metrics = new DisplayMetrics();
			if (android.os.Build.VERSION.SDK_INT >= 17)
			{
				getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
			} else {
				// note: not available so get what we can
				getWindowManager().getDefaultDisplay().getMetrics(metrics);
			}
			String screenDPI = new DecimalFormat("###.##").format(metrics.xdpi) + "," + new DecimalFormat("###.##").format(metrics.ydpi) + "," + new DecimalFormat("###.##").format(metrics.densityDpi);
			return screenDPI;
		}
		else
		if (_bundle == null || key == null)
		{
			return null;
		}
		return _bundle.getString(key);
	}


	public void AndroidThunkJava_SetSustainedPerformanceMode(final boolean bEnable)
	{
		if (ANDROID_BUILD_VERSION >= 24)
		{
			Log.debug("==================================> SetSustainedPerformanceMode:"+bEnable);
			_activity.runOnUiThread(new Runnable()
			{
				public void run()
				{
					try
					{
						android.view.Window ActivityWindow = _activity.getWindow();
						Method m = android.view.Window.class.getMethod("setSustainedPerformanceMode",Boolean.TYPE);
						m.invoke(ActivityWindow, bEnable);
					}
					catch (Exception e)
					{
						Log.debug("SetSustainedPerformanceMode: failed "+ e.getMessage());
					}
				}
			});
		}
		else
		{
			Log.debug("==================================> API<24, cannot use SetSustainedPerformanceMode");
		}
	}
				
	//virtual keyboard input class - custom EditText
	public class VirtualKeyboardInput extends EditText 
	{
		public VirtualKeyboardInput(Context context, AttributeSet attrs, int defStyle) 
		{
			super(context, attrs, defStyle);
	        init();
		}

		public VirtualKeyboardInput(Context context, AttributeSet attrs) 
		{
			super(context, attrs);
	        init();
		}

		public VirtualKeyboardInput(Context context) 
		{
			super(context);
	        init();
		}

		private void init() 
		{
			if (emojiExcludeFilter == null)
			{
				emojiExcludeFilter = new EmojiExcludeFilter();
			}
			setFilters(new InputFilter[]{emojiExcludeFilter});
		}

		@Override
		public void setFilters(InputFilter[] filters) 
		{
			if (filters.length != 0 && emojiExcludeFilter != null) 
			{ //if length == 0 it will here return when init() is called
				boolean add = true;
				for (InputFilter inputFilter : filters) 
				{
					if (inputFilter == emojiExcludeFilter) 
					{
						add = false;
						break;
					}
				}
				if (add) {
					filters = Arrays.copyOf(filters, filters.length + 1);
					filters[filters.length - 1] = emojiExcludeFilter;
				}
			}
			super.setFilters(filters);
		}
		    
		private EmojiExcludeFilter emojiExcludeFilter;

	    private class EmojiExcludeFilter implements InputFilter 
	    {
	        @Override
	        public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
	            for (int i = start; i < end; i++) {
	                int type = Character.getType(source.charAt(i));
	                if (type == Character.SURROGATE || type == Character.OTHER_SYMBOL) {
	                    return "";
	                }
	            }
	            return null;
	        }
	    }

		//Override BACK key to hide the virtual keyboard
		@Override 
		public boolean onKeyPreIme(int keyCode, KeyEvent event) 
		{
		    if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_DOWN) 
			{
		    	AndroidThunkJava_HideVirtualKeyboardInput();
				nativeVirtualKeyboardSendKey(KeyEvent.KEYCODE_BACK);
		    }
		    return super.dispatchKeyEvent(event);
		}

		//extend associated InputConnection to handle special keys 
		@Override
		public InputConnection onCreateInputConnection(EditorInfo outAttrs) 
		{
			return new VirtualKeyboardInputConnection(super.onCreateInputConnection(outAttrs), true, this);
		}

		private class VirtualKeyboardInputConnection extends InputConnectionWrapper 
		{
			VirtualKeyboardInput owner;
			public VirtualKeyboardInputConnection(InputConnection target, boolean mutable, VirtualKeyboardInput editText) 
			{
				super(target, mutable);
				owner = editText;
			}

			private void replaceSubstring(String newString)
			{
				StringBuffer text = new StringBuffer(owner.getText().toString());
				int selStart, selEnd;
 
				int a = owner.getSelectionStart();
				int b = owner.getSelectionEnd();
 
				selStart = Math.min(a, b);
				selEnd = Math.max(a, b);
				//Log.debug("VK: replaceSubstring selStart=" + selStart + " selEnd="+selEnd + " text="+text);

				if (selStart != selEnd) 
				{
					//replace
					text.replace(selStart, selEnd, newString);
				} 
				else if(newString.length() > 0)
				{ 
					//insert
					text.insert(selStart, newString);
				} 
				else if(selStart > 0) 
				{ 
					//delete last character
					selStart--;
					text.replace(selStart, selStart + 1, "");
				} 

				if(newString.length() == 0)
				{
					//#jira UE-48948 Crash when pressing backspace on empty line 
					selStart--;
				}
				//#jira UE-49120 Virtual keyboard number pad "kicks" user back to regular keyboard
				owner.getText().clear();
				owner.append(text.toString());
				owner.setSelection(selStart + 1);
			}


			//implement virtual keyboard's NUMERIC, BACKSPACE and ENTER keys
			@Override
			public boolean sendKeyEvent(KeyEvent event) 
			{
				////Log.debug("VK: sendKeyEvent " + event.getKeyCode() );
				if (event.getAction() == KeyEvent.ACTION_DOWN )
				{ 
					if(event.getKeyCode() >= KeyEvent.KEYCODE_0 && event.getKeyCode() <= KeyEvent.KEYCODE_9)
					{
						char numChar = (char)('0' + (event.getKeyCode() - KeyEvent.KEYCODE_0));
						replaceSubstring(String.valueOf(numChar));
					}
					else if(event.getKeyCode() >= KeyEvent.KEYCODE_NUMPAD_0 && event.getKeyCode() <= KeyEvent.KEYCODE_NUMPAD_9)
					{
						char numChar = (char)('0' + (event.getKeyCode() - KeyEvent.KEYCODE_NUMPAD_0));
						replaceSubstring(String.valueOf(numChar));
					}
					else if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) 
					{
						//delete selected text / previous character
						replaceSubstring("");
					}
					else if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER)
					{

						if (0 != (getInputType() & InputType.TYPE_TEXT_FLAG_MULTI_LINE))
						{
							//add new line
							replaceSubstring("\n");
						}
						else
						{
							AndroidThunkJava_HideVirtualKeyboardInput();
							nativeVirtualKeyboardSendKey(KeyEvent.KEYCODE_ENTER);
						}
					}
				}
				return true;
			}

			//in latest Android, deleteSurroundingText(1, 0) will be called for BACKSPACE
			@Override
			public boolean deleteSurroundingText(int beforeLength, int afterLength) 
			{       
				////Log.debug("VK: deleteSurroundingText");
				if (beforeLength == 1 && afterLength == 0) 
				{
					// backspace
					return sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL))
						&& sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));
				}

				return super.deleteSurroundingText(beforeLength, afterLength);
			}

		}
	}

	//create the virtual keyboard input 
	private void createVirtualKeyboardInput()
	{
		getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
		newVirtualKeyboardInput = new VirtualKeyboardInput(this);
		newVirtualKeyboardInput.setSingleLine(false);
		newVirtualKeyboardInput.setBackgroundColor(Color.WHITE);
		newVirtualKeyboardInput.setLayoutParams(new RelativeLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
		newVirtualKeyboardInput.setVisibility(View.GONE);
		if (ANDROID_BUILD_VERSION < 11)
		{
			newVirtualKeyboardInput.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);
		}
		else
		{
			newVirtualKeyboardInput.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN);
		}

		newVirtualKeyboardInput.setOnEditorActionListener(new OnEditorActionListener() 
		{
			@Override
			public boolean onEditorAction(TextView view, int actionId, KeyEvent event) 
			{
				////Log.debug("VK: onEditorAction");
				int result = actionId & EditorInfo.IME_MASK_ACTION;
				switch(result) 
				{
				case EditorInfo.IME_ACTION_DONE:
					AndroidThunkJava_HideVirtualKeyboardInput();
					nativeVirtualKeyboardSendKey(KeyEvent.KEYCODE_ENTER);
	                return true;
				}
	            return false;
		}
		});

		newVirtualKeyboardInput.addTextChangedListener(new TextWatcher() 
		{
			@Override
			public void beforeTextChanged(CharSequence charSequence, int start, int count, int after) 
			{
			}

			@Override
			public void afterTextChanged(Editable s) 
			{
			}

			@Override
			public void onTextChanged(CharSequence charSequence, int start, int before, int count) 
			{
				//send to the associated Slate control
				//Log.debug("VK onTextChanged " + charSequence);

				//#jira UE-49143 Inconsistent virtual keyboard behavior tapping between controls
				if(newVirtualKeyboardInput.getY() > 0)
				{
					//try to avoid "false empty string" events
					//delay the "set empty string" event and wait for a second call
					if(charSequence.length() == 0)
					{
						virtualKeyboardHandler.postDelayed(new Runnable()
						{
							public void run()
							{
								String message = newVirtualKeyboardInput.getText().toString();
								nativeVirtualKeyboardChanged(message);
							}
						}, 100);
					}
					else
					{
						String message = newVirtualKeyboardInput.getText().toString();
						nativeVirtualKeyboardChanged(message);
					}
				}
			}
		});
		
		virtualKeyboardLayout.addView (newVirtualKeyboardInput);

		virtualKeyboardHandler = new Handler(android.os.Looper.getMainLooper());

	}

	//check if the new virtual keyboard input has received a MOUSE_DOWN event
	// or the keyboard animation is playing
	public boolean AndroidThunkJava_VirtualInputIgnoreClick(int x, int y) 
	{
		View view = getCurrentFocus();
		if (view == newVirtualKeyboardInput) 
		{
			Rect r = new Rect();
			view.getGlobalVisibleRect(r);
			if (r.contains(x, y) || newVirtualKeyboardInput.getY() < 0) 
			{
				//Log.debug("VK: AndroidThunkJava_VirtualInputIgnoreClick true");
				return true;
			}
		}
		//Log.debug("VK: AndroidThunkJava_VirtualInputIgnoreClick false");
		return false;
	}

	public native boolean nativeIsShippingBuild();
	public native void nativeSetGlobalActivity(boolean bUseExternalFilesDir, boolean bOBBInAPK, String APKPath);
	public native void nativeSetWindowInfo(boolean bIsPortrait, int DepthBufferPreference);
	public native void nativeSetObbInfo(String ProjectName, String PackageName, int Version, int PatchVersion);
	public native void nativeSetAndroidVersionInformation( String AndroidVersion, String PhoneMake, String PhoneModel, String OSLanguage );

	public native void nativeSetSurfaceViewInfo(int width, int height);

	public native void nativeConsoleCommand(String commandString);
	public native void nativeVirtualKeyboardChanged(String contents);
	public native void nativeVirtualKeyboardResult(boolean update, String contents);
	public native void nativeVirtualKeyboardSendKey(int keyCode);

	public native void nativeInitHMDs();

	public native void nativeResumeMainInit();

	public native void nativeOnActivityResult(GameActivity activity, int requestCode, int resultCode, Intent data);

	public native void nativeGoogleClientConnectCompleted(boolean bSuccess, String accessToken);

	public native void nativeVirtualKeyboardShown(int left, int top, int right, int bottom);
	public native void nativeVirtualKeyboardVisible(boolean bShown);

	public native void nativeOnConfigurationChanged(boolean bPortrait);
		
	static
	{
		System.loadLibrary("gnustl_shared");
//$${soLoadLibrary}$$
		System.loadLibrary("UE4");
	}

	public native void nativeHandleSensorEvents(float[] tilt, float[] rotation_rate, float[] gravity, float[] acceleration);
}

