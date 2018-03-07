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

import android.util.Log;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.media.AudioManager;

class VolumeReceiver extends BroadcastReceiver 
{
	private static native void volumeChanged( int state );

	private static IntentFilter filter;
	private static VolumeReceiver receiver;

	private static String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
	private static String STREAM_TYPE = "android.media.EXTRA_VOLUME_STREAM_TYPE";
	private static String STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";

	public static void startReceiver( Activity activity )
	{
		GameActivity.Log.debug( "Registering volume receiver" );
		if ( filter == null ) 
		{
			filter = new IntentFilter();
			filter.addAction( VOLUME_CHANGED_ACTION );
		}
		if ( receiver == null ) 
		{
			receiver = new VolumeReceiver();
		}

		activity.registerReceiver( receiver, filter );

		AudioManager audio = (AudioManager)activity.getSystemService( Context.AUDIO_SERVICE );
		int volume = audio.getStreamVolume( AudioManager.STREAM_MUSIC );
		GameActivity.Log.debug( "startVolumeReceiver: " + volume );

		// initialize with the current volume state
		volumeChanged( volume );
	}

	public static void stopReceiver( Activity activity )
	{
		GameActivity.Log.debug( "Unregistering volume receiver" );
		activity.unregisterReceiver( receiver );
	}

	@Override
	public void onReceive( final Context context, final Intent intent ) 
	{
		GameActivity.Log.debug( "OnReceive VOLUME_CHANGED_ACTION" );
		int stream = ( Integer )intent.getExtras().get( STREAM_TYPE );
		int volume = ( Integer )intent.getExtras().get( STREAM_VALUE );
		if ( stream == AudioManager.STREAM_MUSIC )
		{
			volumeChanged( volume );
		}
		else
		{
			GameActivity.Log.debug( "skipping volume change from stream " + stream );
		}
	}
}
