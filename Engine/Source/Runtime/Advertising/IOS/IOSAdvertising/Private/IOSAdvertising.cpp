// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSAdvertising.h"
#include "IOSAppDelegate.h"
#include "Modules/ModuleManager.h"
#include "IOS/IOSAsyncTask.h"
#include "IOS/IOSAppDelegate.h"

#import <iAd/ADBannerView.h>

DEFINE_LOG_CATEGORY_STATIC( LogAdvertising, Display, All );

IMPLEMENT_MODULE( FIOSAdvertisingProvider, IOSAdvertising );

@interface IOSAdvertising : UIResponder <ADBannerViewDelegate>
	/** iAd banner view, if open */
	@property(retain) ADBannerView* BannerView;
@end

@implementation IOSAdvertising

@synthesize BannerView;

/** TRUE if the iAd banner should be on the bottom of the screen */
static BOOL bDrawOnBottom;

/** true if the user wants the banner to be displayed */
static bool bWantVisibleBanner = false;

+ (IOSAdvertising*)GetDelegate
{
	static IOSAdvertising * Singleton = [[IOSAdvertising alloc] init];
	return Singleton;
}

-(void)dealloc
{
	[BannerView release];
	[super dealloc];
}

/**
* Will show an iAd on the top or bottom of screen, on top of the GL view (doesn't resize
* the view)
*
* @param bShowOnBottomOfScreen If true, the iAd will be shown at the bottom of the screen, top otherwise
*/
-(void)ShowAdBanner:(NSNumber*)bShowOnBottomOfScreen
{
	bDrawOnBottom = [bShowOnBottomOfScreen boolValue];
	bWantVisibleBanner = true;

	bool bNeedsToAddSubview = false;
	if (self.BannerView == nil)
	{
		self.BannerView = [[ADBannerView alloc] initWithAdType:ADAdTypeBanner];
		self.BannerView.delegate = self;
		bNeedsToAddSubview = true;
	}
	CGRect BannerFrame = CGRectZero;
	BannerFrame.size = [self.BannerView sizeThatFits : [IOSAppDelegate GetDelegate].RootView.bounds.size];

	if (bDrawOnBottom)
	{
		// move to off the bottom
		BannerFrame.origin.y = [IOSAppDelegate GetDelegate].RootView.bounds.size.height - BannerFrame.size.height;
	}

	self.BannerView.frame = BannerFrame;

	// start out hidden, will fade in when ad loads
	self.BannerView.hidden = YES;
	self.BannerView.alpha = 0.0f;

	if (bNeedsToAddSubview)
	{
		[[IOSAppDelegate GetDelegate].RootView addSubview : self.BannerView];
	}
	else if (self.BannerView.bannerLoaded)
	{
		[self bannerViewDidLoadAd : self.BannerView];
	}
}

-(void)bannerViewDidLoadAd:(ADBannerView*)Banner
{
#if !NO_LOGGING
	NSLog(@"Ad loaded!");
#endif

	if (self.BannerView.hidden && bWantVisibleBanner)
	{
		self.BannerView.hidden = NO;
		[UIView animateWithDuration:0.4f
animations:^
		{
			self.BannerView.alpha = 1.0f;
		}
		];
	}
}

-(void)bannerView:(ADBannerView *)Banner didFailToReceiveAdWithError : (NSError *)Error
{
#if !NO_LOGGING
	NSLog(@"Ad failed to load: '%@'", Error);
#endif

	// if we get an error, hide the banner 
	[self HideAdBanner];
}

/**
* Callback when the user clicks on an ad
*/
-(BOOL)bannerViewActionShouldBegin:(ADBannerView*)Banner willLeaveApplication : (BOOL)bWillLeave
{
	// if we aren't about to swap out the app, tell the game to pause (or whatever)
	if (!bWillLeave)
	{
		FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ bool(void)
		{
			// tell the ad manager the user clicked on the banner
			//@TODO: IAD:			UPlatformInterfaceBase::GetInGameAdManagerSingleton()->OnUserClickedBanner();

			return true;
		};
		[AsyncTask FinishedTask];
	}

	return YES;
}

/**
* Callback when an ad is closed
*/
-(void)bannerViewActionDidFinish:(ADBannerView*)Banner
{
	FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
	AsyncTask.GameThreadCallback = ^ bool(void)
	{
		// tell ad singleton we closed the ad
		//@TODO: IAD:		UPlatformInterfaceBase::GetInGameAdManagerSingleton()->OnUserClosedAd();

		return true;
	};
	[AsyncTask FinishedTask];
}

/**
* Hides the iAd banner shows with ShowAdBanner. Will force close the ad if it's open
*/
-(void)HideAdBanner
{
	// fade it out
	if (!self.BannerView.hidden)
	{
		[UIView animateWithDuration:0.4f
animations:^
		{
			self.BannerView.alpha = 0.0f;
		}
completion:^(BOOL finished) 
		   {
			   self.BannerView.hidden = YES;
		   }
		];
	}
}

-(void)UserHideAdBanner
{
	bWantVisibleBanner = false;
	[self HideAdBanner];
}

/**
* Forces closed any displayed ad. Can lead to loss of revenue
*/
-(void)CloseAd
{
	// boot user out of the ad
	bWantVisibleBanner = false;
	[self.BannerView cancelBannerViewAction];
}
@end

void FIOSAdvertisingProvider::ShowAdBanner( bool bShowOnBottomOfScreen, int32 /*AdID*/ ) 
{
	[[IOSAdvertising GetDelegate] performSelectorOnMainThread:@selector(ShowAdBanner:) withObject:[NSNumber numberWithBool : bShowOnBottomOfScreen] waitUntilDone : NO];
}

void FIOSAdvertisingProvider::HideAdBanner() 
{
	[[IOSAdvertising GetDelegate] performSelectorOnMainThread:@selector(UserHideAdBanner) withObject:nil waitUntilDone : NO];
}

void FIOSAdvertisingProvider::CloseAdBanner() 
{
	[[IOSAdvertising GetDelegate] performSelectorOnMainThread:@selector(CloseAd) withObject:nil waitUntilDone : NO];
}

int32 FIOSAdvertisingProvider::GetAdIDCount()
{
	return 1;
}
