//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.view.View;

public class SplashActivity extends Activity
{
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		boolean ShouldHideUI = false;
		try {
			ApplicationInfo ai = getPackageManager().getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;

			if(bundle.containsKey("com.epicgames.ue4.GameActivity.bShouldHideUI"))
			{
				ShouldHideUI = bundle.getBoolean("com.epicgames.ue4.GameActivity.bShouldHideUI");
			}
		}
		catch (NameNotFoundException e)
		{
		}
		catch (NullPointerException e)
		{
		}

		if (ShouldHideUI)
		{ 
			View decorView = getWindow().getDecorView(); 
			// only do this on KitKat and above
			if(android.os.Build.VERSION.SDK_INT >= 19) {
				decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
											| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
											| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_FULLSCREEN
											| View.SYSTEM_UI_FLAG_IMMERSIVE);  // NOT sticky (will be set later in MainActivity)
			}
		}

		Intent intent = new Intent(this, GameActivity.class);
		intent.putExtras(getIntent());
		intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		intent.putExtra("UseSplashScreen", "true");
		if (ShouldHideUI)
		{
			intent.putExtra("ShouldHideUI", "true");
		}

		//pass down any extras added to this Activity's intent to the GameActivity intent (GCM data, for example)
		Intent intentFromActivity = getIntent();
		Bundle intentBundle = intentFromActivity.getExtras();
		if(intentBundle != null)
		{
			intent.putExtras(intentBundle);
		}
		
		// pass the action if available
		String intentAction = intentFromActivity.getAction();
		if (intentAction != null)
		{
			intent.setAction(intentAction);
		}

		startActivity(intent);
		finish();
		overridePendingTransition(0, 0);
	}

	@Override
	protected void onPause()
	{
		super.onPause();
		finish();
		overridePendingTransition(0, 0);
	}

}