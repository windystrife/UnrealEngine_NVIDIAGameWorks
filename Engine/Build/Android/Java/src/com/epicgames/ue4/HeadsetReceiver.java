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

class HeadsetReceiver extends BroadcastReceiver 
{
	public static HeadsetReceiver receiver = null;
	public static IntentFilter filter = null;

	private static native void stateChanged(int state);

	public static void startReceiver( Activity activity )
	{
		GameActivity.Log.debug("Registering headset receiver");
		if ( filter == null ) 
		{
			filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
		}
		if ( receiver == null ) 
		{
			receiver = new HeadsetReceiver();
		}

		activity.registerReceiver(receiver, filter);

		// initialize with the current headset state
		int state = activity.getIntent().getIntExtra("state", 0);
		GameActivity.Log.debug("startHeadsetReceiver: " + state);
		stateChanged( state );
	}

	public static void stopReceiver( Activity activity )
	{
		GameActivity.Log.debug("Unregistering headset receiver");
		activity.unregisterReceiver( receiver );
	}

	@Override
	public void onReceive( final Context context, final Intent intent ) 
	{
		GameActivity.Log.debug("headsetReceiver::onReceive");
		if (intent.hasExtra("state"))
		{
			int state = intent.getIntExtra("state", 0);
			stateChanged( state );
		}
	}
}
