package com.epicgames.ue4;

import android.util.Log;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.content.res.Resources;
import android.os.Bundle;
import android.util.Base64;

import com.google.android.gms.auth.api.Auth;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.auth.api.signin.GoogleSignInResult;
import com.google.android.gms.auth.api.signin.GoogleSignInStatusCodes;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.OptionalPendingResult;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class GoogleLogin implements
		GoogleApiClient.OnConnectionFailedListener
{
	/** Responses supported by this class */
	public static final int GOOGLE_RESPONSE_OK = 0;
	public static final int GOOGLE_RESPONSE_CANCELED = 1;
	public static final int GOOGLE_RESPONSE_ERROR = 2;

	/** Debug output tag */
	private static final String TAG = "UE4-GOOGLE";

	/** Key to save in bundle when restoring state */
	private static final String STATE_RESOLVING_ERROR = "resolving_error";
	// Request code to use when launching the resolution activity
	private static final int REQUEST_RESOLVE_ERROR = 1001;
	// Bool to track whether the app is already resolving an error
	private boolean bResolvingError = false;

	// Output device for log messages.
	private Logger GoogleLog;
	private Logger ActivityLog;

	/**
	 * Activity needed here to send the signal back when user successfully logged in.
	 */
	private GameActivity activity;

	/** Android key from Google API dashboard */
	private String clientId;
	/** Backend server key from Google API dashboard */
	private String serverClientId;
	/** Unique request id when using sign in activity */
	private static final int REQUEST_SIGN_IN = 9001;
	/** Google API client needed for actual sign in */
	private GoogleApiClient mGoogleApiClient;
	/** Callbacks for handling Google sign in behavior */
	private GoogleApiClient.ConnectionCallbacks connectionCallbacks;
	/** Connection status */
	private boolean bConnected = false;

	public GoogleLogin(GameActivity activity, final Logger InLog) 
	{
		this.activity = activity;

		GoogleLog = new Logger(TAG);
		ActivityLog = InLog;
	} 

	public boolean init(String inPackageName, String BuildConfiguration, String inClientId, String inServerClientId, Bundle savedInstanceState)
	{
		boolean bInitSuccess = false;
		boolean bShippingBuild = BuildConfiguration.equals("Shipping");

		if (bShippingBuild)
		{
			GoogleLog.SuppressLogs();
		}

		boolean bClientIdValid = (inClientId != null && !inClientId.isEmpty());
		boolean bServerIdValid = (inServerClientId != null && !inServerClientId.isEmpty());
		if (bClientIdValid && bServerIdValid)
		{
			GoogleLog.debug("init");

			boolean bIsAvailable = isGooglePlayServicesAvailable();
			GoogleLog.debug("Is Google Play Services Available:" + bIsAvailable);
			if (bIsAvailable)
			{
				PrintGoogleServiceSettings();

				bResolvingError = savedInstanceState != null &&
					savedInstanceState.getBoolean(STATE_RESOLVING_ERROR, false);

				GoogleLog.debug("inPackageName: " + inPackageName);
				clientId = inClientId;
				GoogleLog.debug("GoogleSignIn clientId:" + clientId);
				serverClientId = inServerClientId;
				GoogleLog.debug("GoogleSignIn serverClientId:" + serverClientId);

				connectionCallbacks = getConnectionCallbacks();

				// Configure sign-in to request the user's ID, email address, and basic
				// profile. ID and basic profile are included in DEFAULT_SIGN_IN.
				GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
						.requestIdToken(serverClientId)
						.requestProfile()
						//.requestServerAuthCode(serverClientId)
						.requestEmail()
						.build();

				// Build a GoogleApiClient with access to the Google Sign-In API and the
				// options specified by gso.
				mGoogleApiClient = new GoogleApiClient.Builder(activity)
						//.enableAutoManage(activity /* FragmentActivity */, this /* OnConnectionFailedListener */)
						.addConnectionCallbacks(connectionCallbacks)
						.addOnConnectionFailedListener(this)
						.addApi(Auth.GOOGLE_SIGN_IN_API, gso)
						.build();

				PrintGoogleAPIState();
				PrintKeyHash(inPackageName);
				bInitSuccess = true;
			}
		}
		else
		{
			GoogleLog.debug("clientId: " + inClientId + " or serverClientId: " + inServerClientId + " is invalid");
		}

		return bInitSuccess;
	}

	public void onStart()
	{
		GoogleLog.debug("onStart");
		mGoogleApiClient.connect();
		PrintGoogleAPIState();

		OptionalPendingResult<GoogleSignInResult> opr = Auth.GoogleSignInApi.silentSignIn(mGoogleApiClient);
		if (opr.isDone()) {
			// If the user's cached credentials are valid, the OptionalPendingResult will be "done"
			// and the GoogleSignInResult will be available instantly.
			GoogleLog.debug("Got cached sign-in");
			GoogleSignInResult result = opr.get();
			handleSignInResult(result);
		} else {
			// If the user has not previously signed in on this device or the sign-in has expired,
			// this asynchronous branch will attempt to sign in the user silently.  Cross-device
			// single sign-on will occur in this branch.
			opr.setResultCallback(new ResultCallback<GoogleSignInResult>() {
				@Override
				public void onResult(GoogleSignInResult googleSignInResult) {
					GoogleLog.debug("Sign-in callback");
					handleSignInResult(googleSignInResult);
				}
			});
		}

		PrintGoogleAPIState();
	}

	public void onStop()
	{
		GoogleLog.debug("onStop");
		mGoogleApiClient.disconnect();
	}

	public void onSaveInstanceState(Bundle outState)
	{
		GoogleLog.debug("onSaveInstanceState");
		outState.putBoolean(STATE_RESOLVING_ERROR, bResolvingError);
	}

	public void onDestroy()
	{
		GoogleLog.debug("onDestroy");
		//mGoogleApiClient.unregisterConnectionCallbacks(connectionCallbacks);
	}

	private GoogleApiClient.ConnectionCallbacks getConnectionCallbacks()
	{
		return new GoogleApiClient.ConnectionCallbacks()
		{
			@Override
			public void onConnected(Bundle connectionHint)
			{
				bConnected = true;
				if (connectionHint != null) {
					GoogleLog.debug("onConnected " + connectionHint.toString());
				}
				else
				{
					GoogleLog.debug("onConnected null bundle");
				}
			}
			@Override
			public void onConnectionSuspended(int cause)
			{
				// CAUSE_SERVICE_DISCONNECTED
				// CAUSE_NETWORK_LOST;
				GoogleLog.debug("onConnectionSuspended " + cause);
				bConnected = false;
			}
		};
	}

	public int login(String[] ScopeFields)
	{
		GoogleLog.debug("login:" + ScopeFields.toString());
		PrintGoogleAPIState();

		int resultCode = GOOGLE_RESPONSE_ERROR;
		if (!mGoogleApiClient.isConnecting()) 
		{
			GoogleLog.debug("login intent:");
			Intent signInIntent = Auth.GoogleSignInApi.getSignInIntent(mGoogleApiClient);
			if (signInIntent != null)
			{
				GoogleLog.debug("login start activity:");
				activity.startActivityForResult(signInIntent, REQUEST_SIGN_IN);
				resultCode = GOOGLE_RESPONSE_OK;
			} 
			else 
			{
				GoogleLog.debug("getSignInIntent failure:");
				nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
			}
		}
		else
		{
			GoogleLog.debug("onSignIn: still connecting");
			nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
		}

		return resultCode;
	}

	public int logout()
	{
		GoogleLog.debug("logout");
		PrintGoogleAPIState();

		int resultCode = GOOGLE_RESPONSE_ERROR;
		if (!mGoogleApiClient.isConnecting()) 
		{
			GoogleLog.debug("calling signout");
			Auth.GoogleSignInApi.signOut(mGoogleApiClient).setResultCallback(
				new ResultCallback<Status>() 
				{
					@Override
					public void onResult(Status status) 
					{
						GoogleLog.debug("onSignOut Complete" + status + " " + status.getStatusMessage());
						nativeLogoutComplete(GOOGLE_RESPONSE_OK);
					}
				});
			resultCode = GOOGLE_RESPONSE_OK;
		}
		else
		{
			GoogleLog.debug("onSignOut: still connecting");
			nativeLogoutComplete(GOOGLE_RESPONSE_ERROR);
		}

		return resultCode;
	}

	public void onActivityResult(int requestCode, int resultCode, Intent data) 
	{
		GoogleLog.debug("onActivityResult: " + requestCode + " result: " + resultCode);
		// Result returned from launching the Intent from GoogleSignInApi.getSignInIntent(...);
		if (requestCode == REQUEST_SIGN_IN) 
		{
			GoogleLog.debug("onActivityResult REQUEST_SIGN_IN");
			if (resultCode == Activity.RESULT_OK)
			{
				GoogleLog.debug("signing in");
			}

			GoogleLog.debug("data: " + ((data != null) ? data.toString() : "null"));

			GoogleSignInResult result = Auth.GoogleSignInApi.getSignInResultFromIntent(data);
			if (result != null)
			{
				handleSignInResult(result);

				GoogleLog.debug("onActivityResult result:" + result.isSuccess());
				GoogleLog.debug("result:" + result.toString());
				if (result.isSuccess()) 
				{
					// Signed in successfully...
					GoogleSignInAccount acct = result.getSignInAccount();
					nativeLoginComplete(GOOGLE_RESPONSE_OK, getLoginJsonStr(acct));
				} 
				else 
				{
					// Signed out
					Status myStatus = result.getStatus();
					GoogleLog.debug("Code:" + GoogleSignInStatusCodes.getStatusCodeString(myStatus.getStatusCode()));
					GoogleLog.debug("Message:" + myStatus.getStatusMessage());
					nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
				}
			}
			else
			{
				GoogleLog.debug("onActivityResult result is null");
				nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
			}
			GoogleLog.debug("onActivityResult end");
		}

		if (requestCode == REQUEST_RESOLVE_ERROR) 
		{
			bResolvingError = false;
			if (resultCode == Activity.RESULT_OK)
			{
				// Make sure the app is not already connected or attempting to connect
				if (!mGoogleApiClient.isConnecting() && !mGoogleApiClient.isConnected())
				{
					mGoogleApiClient.connect();
				}
			}
		}
	}

	public void onConnectionFailed(ConnectionResult connectionResult) 
	{
		// An unresolvable error has occurred and Google APIs (including Sign-In) will not
		// be available.
		GoogleLog.debug("onConnectionFailed:" + connectionResult);
		//PendingIntent signInIntent = connectionResult.getResolution();

		if (bResolvingError)
		{
			// Already attempting to resolve an error.
			GoogleLog.debug("already resolving");
			return;
		} 
		else if (connectionResult.hasResolution())
		{
			try 
			{
				GoogleLog.debug("start resolving");
				bResolvingError = true;
				connectionResult.startResolutionForResult(activity, REQUEST_RESOLVE_ERROR);
			} 
			catch (IntentSender.SendIntentException e)
			{
				// There was an error with the resolution intent. Try again.
				GoogleLog.debug("error resolving");
				mGoogleApiClient.connect();
			}
		} 
		else 
		{
			// Show dialog using GoogleApiAvailability.getErrorDialog()
			//showErrorDialog(connectionResult.getErrorCode());
			GoogleLog.debug("no resolution");
			bResolvingError = true;
		}
	}

	private void handleSignInResult(GoogleSignInResult result) 
	{
		if (result != null)
		{
			GoogleLog.debug("handleSignInResult:" + result.isSuccess());
			if (result.isSuccess()) 
			{
				// Signed in successfully, show authenticated UI.
				GoogleSignInAccount acct = result.getSignInAccount();
				PrintUserAccountInfo(acct);
				//nativeLoginComplete(GOOGLE_RESPONSE_OK, getLoginJsonStr(acct));
			} 
			else 
			{
				// Signed out
				Status myStatus = result.getStatus();
				GoogleLog.debug("Code:" + GoogleSignInStatusCodes.getStatusCodeString(myStatus.getStatusCode()));
				GoogleLog.debug("Message:" + myStatus.getStatusMessage());
				//nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
			}
		}
		else
		{
			GoogleLog.debug("handleSignInResult: result is null");
		}
	}

	private String getLoginJsonStr(GoogleSignInAccount acct)
	{
		if (acct != null)
		{
			return "{\"user_data\":" + getUserJsonStr(acct) + "," +
					"\"auth_data\":" + getAuthTokenJsonStr(acct) + "}";
		}

		return "";
	}

	private String getUserJsonStr(GoogleSignInAccount acct)
	{
		if (acct != null)
		{
			return "{\"sub\":\""+ acct.getId() + "\"," +
					"\"given_name\":\"" + acct.getGivenName()  + "\"," +
					"\"family_name\":\"" + acct.getFamilyName() + "\"," +
					"\"name\":\"" + acct.getDisplayName() + "\"," +
					"\"picture\":\"" + acct.getPhotoUrl() + "\"" + "}";
		}
		return "";
	}

	private String getAuthTokenJsonStr(GoogleSignInAccount acct)
	{
		if (acct != null)
		{
			return "{\"access_token\":\"androidInternal\"," +
					"\"refresh_token\":\"androidInternal\"," +
					"\"id_token\":\""+ acct.getIdToken() + "\"}";
		}
		return "";
	}

	public void PrintGoogleAPIState()
	{
		boolean bEnabled = false;
		if (bEnabled)
		{
			PrintWriter printWriter = new PrintWriter(System.out, true);
			GoogleLog.debug("--------------------------------------");
			mGoogleApiClient.dumpAll(TAG, FileDescriptor.out, printWriter, null);
			GoogleLog.debug("Connected: " + mGoogleApiClient.isConnected() + " Connecting: " + mGoogleApiClient.isConnecting());
			GoogleLog.debug("--------------------------------------");
		}
	}

	private String GetResourceById(String resourceName, Resources resources, String packageName)
	{
		try
		{
			int resourceId = resources.getIdentifier(resourceName, "string", packageName);
			return activity.getString(resourceId);
		}
		catch (Exception e) {
			GoogleLog.debug("Resource: " + resourceName + " is not found!");
			return "";
		} 
	}

	public void PrintGoogleServiceSettings()
	{
		String packageName = activity.getPackageName();
		Resources resources = activity.getResources();

		GoogleLog.debug("--------------------------------------");
		GoogleLog.debug("default_web_client_id:" + GetResourceById("default_web_client_id", resources, packageName));
		GoogleLog.debug("gcm_defaultSenderId:" + GetResourceById("gcm_defaultSenderId", resources, packageName));
		GoogleLog.debug("google_api_key:" + GetResourceById("google_api_key", resources, packageName));
		GoogleLog.debug("google_app_id:" + GetResourceById("google_app_id", resources, packageName));
		GoogleLog.debug("google_crash_reporting_api_key:" + GetResourceById("google_crash_reporting_api_key", resources, packageName));
		GoogleLog.debug("--------------------------------------");
	}

	public void PrintUserAccountInfo(GoogleSignInAccount acct)
	{
		GoogleLog.debug("User Details:");
		GoogleLog.debug("    DisplayName:" + acct.getDisplayName());
		GoogleLog.debug("    Id:" + acct.getId());
		GoogleLog.debug("    Email:" + acct.getEmail());
		// 10.2.0 GoogleLog.debug("    Account:" + acct.getAccount().toString());

		GoogleLog.debug("    Scopes:" + acct.getGrantedScopes());
		GoogleLog.debug("    IdToken:" + acct.getIdToken());
		GoogleLog.debug("    ServerAuthCode:" + acct.getServerAuthCode());
	}

	private boolean isGooglePlayServicesAvailable() 
	{
		GoogleApiAvailability apiAvail = GoogleApiAvailability.getInstance();
		int status = apiAvail.isGooglePlayServicesAvailable(activity);
		GoogleLog.debug("isGooglePlayServicesAvailable statusCode: " + status);
		if (status == ConnectionResult.SUCCESS) 
		{
			return true;
		} 
		else 
		{
			return false;
		}
	}

	public void PrintKeyHash(String packageName) 
	{
		try 
		{
			PackageInfo info = activity.getPackageManager().getPackageInfo(
					packageName,
					PackageManager.GET_SIGNATURES);
			for (Signature signature : info.signatures) 
			{
				MessageDigest md = MessageDigest.getInstance("SHA");
				md.update(signature.toByteArray());
				GoogleLog.debug(Base64.encodeToString(md.digest(), Base64.DEFAULT));
			}
		} 
		catch (PackageManager.NameNotFoundException e) 
		{
			GoogleLog.debug("NameNotFoundException:" + e);
		} 
		catch (NoSuchAlgorithmException e) 
		{
			GoogleLog.debug("NoSuchAlgorithmException:" + e);
		}
	}

	// Callback that notify the C++ implementation that a task has completed
	public native void nativeLoginComplete(int responseCode, String javaData);
	public native void nativeLogoutComplete(int responseCode);
}
