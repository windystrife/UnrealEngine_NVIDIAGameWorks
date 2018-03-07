// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
package com.epicgames.ue4;

import android.content.Intent;
import com.google.android.gms.iid.InstanceID;
import com.google.android.gms.iid.InstanceIDListenerService;

public class RemoteNotificationsInstanceIDListener extends InstanceIDListenerService
{
	@Override
	public void onTokenRefresh()
	{
		Intent intent = new Intent( GameActivity._activity.getApplicationContext(), RemoteNotificationsRegistrationIntentService.class );
		startService( intent );
	}
}
