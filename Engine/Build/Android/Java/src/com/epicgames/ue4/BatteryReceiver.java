/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.epicgames.ue4;

import android.app.Activity;
import android.util.Log;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;

class BatteryReceiver extends BroadcastReceiver 
{
	public static BatteryReceiver receiver = null;
	public static IntentFilter filter = null;

	private int batteryLevel = 0;
	private int batteryStatus = 0;
	private int batteryTemperature = 0;

	private static native void dispatchEvent(int status, int level, int temperature);

	private void processIntent(final Intent intent) 
	{
		// TODO: The following functions should use the proper extra strings, ie BatteryManager.EXTRA_SCALE
		boolean isPresent = intent.getBooleanExtra("present", false);
		if (isPresent) 
		{
			int status = intent.getIntExtra("status", 0);
			int rawlevel = intent.getIntExtra("level", -1);
			int scale = intent.getIntExtra("scale", -1);
			// temperature is in tenths of a degree centigrade
			int temperature = intent.getIntExtra("temperature", 0);
			// always tell native code what the battery level is
			int level = 0;
			if ( rawlevel >= 0 && scale > 0) 
			{
				level = (rawlevel * 100) / scale;
			}
			if ( status != batteryStatus ||
				 level != batteryLevel ||
				 temperature != batteryTemperature) 
			{
				GameActivity.Log.debug( "Battery: status = " + status + ", rawlevel = " + rawlevel + ", scale = " + scale );

				batteryStatus = status;
				batteryLevel = level;
				batteryTemperature = temperature;
				dispatchEvent(status,level,temperature);
			}
		}
	}

	public static void startReceiver( Activity activity )
	{
		GameActivity.Log.debug("Registering battery receiver");

		if ( filter == null ) 
		{
			filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
		}

		if ( receiver == null ) 
		{
			receiver = new BatteryReceiver();
		}

		activity.registerReceiver(receiver, filter);

		// initialize with the current battery state
		receiver.processIntent( activity.getIntent() );
	}

	public static void stopReceiver( Activity activity )
	{
		GameActivity.Log.debug("Unregistering battery receiver");
		activity.unregisterReceiver( receiver );
	}

	@Override
	public void onReceive( final Context context, final Intent intent ) 
	{
		//GameActivity.Log.debug("OnReceive BATTERY_ACTION_CHANGED");
		processIntent( intent );
	}
}
