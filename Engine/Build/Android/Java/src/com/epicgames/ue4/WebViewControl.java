// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
package com.epicgames.ue4;

import android.os.Build;
import android.os.Bundle;
import android.content.Context;

import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceResponse;
import android.webkit.JsResult;
import android.webkit.JsPromptResult;
import android.graphics.Bitmap;
// @HSL_BEGIN - Josh.May - 3/08/2017 - Added background transparency support for AndroidWebBrowserWidget.
import android.graphics.Color;
// @HSL_END - Josh.May - 3/08/2017
import android.webkit.WebBackForwardList;
import java.io.ByteArrayInputStream;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Semaphore;
	
// Simple layout to apply absolute positioning for the WebView
class WebViewPositionLayout extends ViewGroup
{
	public WebViewPositionLayout(Context context, WebViewControl inWebViewControl)
	{
        super(context);
		webViewControl = inWebViewControl;
	}

	@Override
	protected void onLayout(boolean changed, int left, int top, int right, int bottom)
	{
		webViewControl.webView.layout(webViewControl.curX,webViewControl.curY,webViewControl.curX+webViewControl.curW,webViewControl.curY+webViewControl.curH);
	}

	private WebViewControl webViewControl;
}


// Wrapper for the layout and WebView for the C++ to call
class WebViewControl
{
	// @HSL_BEGIN - Josh.May - 3/08/2017 - Added background transparency support for AndroidWebBrowserWidget.
	public WebViewControl(long inNativePtr, final boolean bEnableRemoteDebugging, final boolean bUseTransparency)
	// @HSL_END - Josh.May - 3/08/2017
	{
		final WebViewControl w = this;

		nativePtr = inNativePtr;
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				// enable remote debugging if requested and supported by the current platform
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT && bEnableRemoteDebugging)
				{
					WebView.setWebContentsDebuggingEnabled(true);
				}

				// create the WebView
				webView = new WebView(GameActivity._activity);
				webView.setWebViewClient(new ViewClient());
				webView.setWebChromeClient(new ChromeClient());
				webView.getSettings().setJavaScriptEnabled(true);
				webView.getSettings().setLightTouchEnabled(true);
				webView.setFocusableInTouchMode(true);

				// @HSL_BEGIN - Josh.May - 3/08/2017 - Added background transparency support for AndroidWebBrowserWidget.
				if (bUseTransparency)
				{
					webView.setBackgroundColor(Color.TRANSPARENT);
				}
				// @HSL_END - Josh.May - 3/08/2017

				// Wrap the webview in a layout that will do absolute positioning for us
				positionLayout = new WebViewPositionLayout(GameActivity._activity, w);
				positionLayout.addView(webView);								

				bShown = false;
				NextURL = null;
				NextContent = null;
				curX = curY = curW = curH = 0;
			}
		});
	}

	public void ExecuteJavascript(final String script)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if(webView != null)
				{
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
					{
						webView.evaluateJavascript(script, null);
					}
					else
					{
						// This is executed directly here instead of setting NextURL as otherwise calling ExecuteJavascript would only be possible once per tick.
						webView.loadUrl("javascript:"+script);
					}
				}
			}
		});
	}
	public void LoadURL(final String url)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				NextURL = url;
				NextContent = null;
			}
		});
	}

	public void LoadString(final String contents, final String url)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				NextURL = url;
				NextContent = contents;
			}
		});
	}

	public void StopLoad()
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.stopLoading();
			}
		});
	}

	public void Reload()
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.reload();
			}
		});
	}

	public void GoBackOrForward(final int Steps)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.goBackOrForward(Steps);
			}
		});
	}

	public boolean CanGoBackOrForward(int Steps)
	{
		return webView.canGoBackOrForward(Steps);
	}

	// called from C++ paint event
	public void Update(final int x, final int y, final int width, final int height)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (bClosed)
				{
					// queued up run()s can occur after Close() called; don't want to show it again
					return;
				}
				if (!bShown)
				{
					bShown = true;
				
					// add to the activitiy, on top of the SurfaceView
					ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.FILL_PARENT);			
					GameActivity._activity.addContentView(positionLayout, params);
					webView.requestFocus();
				}
				else
				{
					if(webView != null)
					{
						if(NextContent != null)
						{
							webView.loadDataWithBaseURL(NextURL, NextContent, "text/html", "UTF-8", null);
							NextURL = null;
							NextContent = null;
						}
						else
						if(NextURL != null)
						{
							webView.loadUrl(NextURL);
							NextURL = null;
						}
					}		
				}
				if(x != curX || y != curY || width != curW || height != curH)
				{
					curX = x;
					curY = y;
					curW = width;
					curH = height;
					positionLayout.requestLayout();
				}
			}
		});
	}

	public void Close()
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (bShown)
				{
					ViewGroup parent = (ViewGroup)webView.getParent();
					if (parent != null)
					{
						parent.removeView(webView);
					}
					parent = (ViewGroup)positionLayout.getParent();
					if (parent != null)
					{
						parent.removeView(positionLayout);
					}
					bShown = false;
				}
				bClosed = true;
			}
		});
	}

	private class ViewClient
		extends WebViewClient
	{
		@Override
		public WebResourceResponse shouldInterceptRequest(WebView View, String Url)
		{
			byte[] Result = shouldInterceptRequestImpl(Url);
			if (Result != null)
			{
				return new WebResourceResponse("text/html", "utf8", new ByteArrayInputStream(Result));
			}
			else
			{
				return null;
			}
		}

		@Override
		public native boolean shouldOverrideUrlLoading(WebView View, String Url);

		@Override
		public void onPageStarted(WebView View, String Url, Bitmap Favicon)
		{
			WebBackForwardList History = View.copyBackForwardList();
			onPageLoad(Url, true, History.getSize(), History.getCurrentIndex());
		}

		@Override
		public void onPageFinished(WebView View, String Url)
		{
			WebBackForwardList History = View.copyBackForwardList();
			onPageLoad(Url, false, History.getSize(), History.getCurrentIndex());
		}

		@Override
		public native void onReceivedError(WebView View, int ErrorCode, String Description, String Url);

		public native void onPageLoad(String Url, boolean bIsLoading, int HistorySize, int HistoryPosition);
		private native byte[] shouldInterceptRequestImpl(String Url);

		public long GetNativePtr()
		{
			return WebViewControl.this.nativePtr;
		}
	}

	private class ChromeClient
		extends WebChromeClient
	{
		public native boolean onJsAlert(WebView View, String Url, String Message, JsResult Result);
		public native boolean onJsBeforeUnload(WebView View, String Url, String Message, JsResult Result);
		public native boolean onJsConfirm(WebView View, String Url, String Message, JsResult Result);
		public native boolean onJsPrompt(WebView View, String Url, String Message, String DefaultValue, JsPromptResult Result);
		public native void onReceivedTitle(WebView View, String Title);

		public long GetNativePtr()
		{
			return WebViewControl.this.nativePtr;
		}
	}
	public long GetNativePtr()
	{
		return nativePtr;
	}
		
	public WebView webView;
	private WebViewPositionLayout positionLayout;
	public int curX, curY, curW, curH;
	private boolean bShown;
	private boolean bClosed;
	private String NextURL;
	private String NextContent;
	
	// Address of the native SAndroidWebBrowserWidget object
	private long nativePtr;
}
