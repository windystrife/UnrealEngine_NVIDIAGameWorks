// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
package com.epicgames.ue4;

import java.io.IOException;
import android.app.IntentService;
import android.content.Intent;
import com.google.android.gms.iid.InstanceID;
import com.google.android.gms.gcm.GoogleCloudMessaging;
import com.google.android.gms.gcm.GcmPubSub;

public class RemoteNotificationsRegistrationIntentService extends IntentService
{
  public RemoteNotificationsRegistrationIntentService()
  {
    super( "com.epicgames.ue4.RemoteNotificationsRegistrationIntentService" );
  }

  public RemoteNotificationsRegistrationIntentService( String name )
  {
    super( name );
  }
  
  @Override
  protected void onHandleIntent( Intent intent )
  {
    try
    {
      InstanceID instanceID = InstanceID.getInstance( GameActivity._activity.getApplicationContext() );
      String Token = instanceID.getToken( GameActivity._activity.GCMSenderId, GoogleCloudMessaging.INSTANCE_ID_SCOPE, null );

      // subscribe to default topic: /topics/global
      subscribeTopics( Token );

      GameActivity._activity.nativeGCMRegisteredForRemoteNotifications( Token );
    }
    catch( Exception e )
    {
      GameActivity._activity.nativeGCMFailedToRegisterForRemoteNotifications( "Failed to complete token refresh" );
    }
  }

  private void subscribeTopics( String token ) throws IOException {
    GcmPubSub pubSub = GcmPubSub.getInstance( GameActivity._activity.getApplicationContext() );
    pubSub.subscribe( token, "/topics/global", null );
  }
}
