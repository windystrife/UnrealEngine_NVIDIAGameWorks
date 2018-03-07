/*
 * Copyright (C) 2015-2017 Epic Games, Inc. All Rights Reserved.
 */
package com.epicgames.replayserver;

import java.util.TimerTask;

public class CleanupTask 
{
	public static class QuickTask extends TimerTask 
	{
		@Override
		public void run() 
		{		
			ReplayDB.quickCleanup();
		}
	}

	public static class LongTask extends TimerTask 
	{
		@Override
		public void run() 
		{		
			ReplayDB.longCleanup();
		}
	}	
}
