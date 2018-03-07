// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

package com.epicgames.ue4;

import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import java.util.ArrayList;
import java.util.HashMap;
import android.view.View;
import android.view.Gravity;

/*
	Utility class to manage displaying a message box
	with any number of buttons.
*/
class MessageBox01
	implements View.OnClickListener
{
	public MessageBox01()
	{
	}
	
	public void setCaption(String text)
	{
		Caption = text;
	}
	
	public void setText(String text)
	{
		Text = text;
	}
	
	public void addButton(String text)
	{
		ButtonText.add(text);
	}
	
	public void clear()
	{
		Caption = "";
		Text = "";
		ButtonText.clear();
		ButtonPressed = -1;
	}
	
	public int show()
	{
		ButtonPressed = -1;
		createAlert();
		if (null != Alert)
		{
			GameActivity.Get().runOnUiThread(new Runnable()
				{
					public void run()
					{
						Alert.show();
					}
				});
			while (ButtonPressed == -1 && Alert != null)
			{
				try
				{
					java.lang.Thread.sleep(100);
				}
				catch (java.lang.InterruptedException e)
				{
				}
			}
		}
		return ButtonPressed;
	}
	
	protected void createAlert()
	{
		dismissAlert();
		ButtonIdToIndex.clear();
		Builder = new AlertDialog.Builder(GameActivity.Get());
		Builder.setTitle(Caption);
		Builder.setMessage(Text);
		if (ButtonText.size() >= 1)
		{
			Builder.setPositiveButton(ButtonText.get(0), null);
		}
		if (ButtonText.size() >= 2)
		{
			Builder.setNeutralButton(ButtonText.get(1), null);
		}
		Alert = null;
		GameActivity.Get().runOnUiThread(new Runnable()
			{
				public void run()
				{
					Alert = Builder.create();
				}
			});
		while (null == Alert)
		{
			try
			{
				java.lang.Thread.sleep(100);
			}
			catch (java.lang.InterruptedException e)
			{
			}
		}
		Builder = null;
		Alert.setCancelable(false);
		Alert.setCanceledOnTouchOutside(false);
		Alert.setOnShowListener(new DialogInterface.OnShowListener() {
			public void onShow(DialogInterface dialog)
			{
				android.widget.Button button = null;
				if (ButtonText.size() >= 1)
				{
					button = Alert.getButton(
						android.content.DialogInterface.BUTTON_POSITIVE);
					registerButton(button, 0);
				}
				if (ButtonText.size() >= 2)
				{
					button = Alert.getButton(
						android.content.DialogInterface.BUTTON_NEUTRAL);
					registerButton(button, 1);
				}
				if (button != null)
				{
					android.view.ViewGroup parent = (android.view.ViewGroup)button.getParent();
					for (int i = 2; i < ButtonText.size(); ++i)
					{
						button = new android.widget.Button(GameActivity.Get());
						button.setText(ButtonText.get(i));
						parent.addView(button);
						registerButton(button, i);
					}
				}
			}
			});
	}
	
	protected void registerButton(android.widget.Button button, int button_index)
	{
		int new_id = button.getId();
		if (new_id == View.NO_ID)
		{
			new_id = View.generateViewId();
		}
		button.setId(new_id);
		ButtonIdToIndex.put(new_id, button_index);
		button.setOnClickListener(this);
		if (ButtonText.size() > 1)
		{
			android.widget.LinearLayout.LayoutParams params
				= (android.widget.LinearLayout.LayoutParams) button.getLayoutParams();
			params.gravity = Gravity.FILL;
			params.weight = 1.0f;
			params.width = android.widget.LinearLayout.LayoutParams.FILL_PARENT;
			button.setLayoutParams(params);
		}
	}
	
	public void onClick(android.view.View button)
	{
		ButtonPressed = ButtonIdToIndex.get(button.getId());
		dismissAlert();
	}
	
	protected void dismissAlert()
	{
		if (null != Alert)
		{
			Alert.dismiss();
			Alert = null;
		}
	}
	
	protected String Caption = "";
	protected String Text = "";
	protected ArrayList<String> ButtonText
		= new ArrayList<String>();
	protected int ButtonPressed = -1;
	protected AlertDialog Alert = null;
	protected HashMap<Integer,Integer> ButtonIdToIndex = new HashMap<Integer,Integer>();
	protected AlertDialog.Builder Builder = null;
}
