package com.epicgames.ue4;

import android.util.Log;

public class Logger
{
	private String mTag;
	
	private static boolean bAllowLogging			= true;
	private static boolean bAllowExceptionLogging	= true;
	public static void SuppressLogs ()
	{
		bAllowLogging = bAllowExceptionLogging = false;
	}

	public Logger(String Tag)
	{
		mTag = Tag;
	}
	public void debug(String Message)
	{
		if (bAllowLogging)
		{
			Log.d(mTag, Message);
		}
	}
	
	public void warn(String Message)
	{
		if (bAllowLogging)
		{
			Log.w(mTag, Message);
		}
	}
	
	public void error(String Message)
	{
		if (bAllowLogging)
		{
			Log.e(mTag, Message);
		}
	}
}