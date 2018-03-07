// Copyright (c) 2014-present, Facebook, Inc. All rights reserved.
//
// You are hereby granted a non-exclusive, worldwide, royalty-free license to use,
// copy, modify, and distribute this software in source code or binary form for use
// in connection with the web services and APIs provided by Facebook.
//
// As with any software that integrates with the Facebook platform, your use of
// this software is subject to the Facebook Developer Principles and Policies
// [http://developers.facebook.com/policy/]. This copyright notice shall be
// included in all copies or substantial portions of the software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#import "RewardedVideoViewController.h"

@interface RewardedVideoViewController ()

@property (weak, nonatomic) IBOutlet UILabel *adStatusLabel;
@property (nonatomic, strong) FBRewardedVideoAd *rewardedVideoAd;

@end

@implementation RewardedVideoViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  // Dispose of any resources that can be recreated.
}

- (IBAction)loadRewardedVideoTapped:(id)sender
{
  self.adStatusLabel.text = @"Loading rewarded video ad...";

  // Create the rewarded video unit with a placement ID (generate your own on the Facebook app settings).
  // Use different ID for each ad placement in your app.
  self.rewardedVideoAd = [[FBRewardedVideoAd alloc] initWithPlacementID:@"YOUR_PLACEMENT_ID" withUserID:@"user123"  withCurrency:@"gold" withAmount:1];
  // Set a delegate to get notified on changes or when the user interact with the ad.
  self.rewardedVideoAd.delegate = self;

  // Initiate the request to load the ad.
  [self.rewardedVideoAd loadAd];
}

- (IBAction)showRewardedVideoTapped:(id)sender
{
  if (!self.rewardedVideoAd || !self.rewardedVideoAd.isAdValid)
  {
    // Ad not ready to present.
    self.adStatusLabel.text = @"Ad not loaded. Click load to request an ad.";
  } else {
    self.adStatusLabel.text = nil;

    // Ad is ready, present it!
    [self.rewardedVideoAd showAdFromRootViewController:self animated:NO];
  }
}

#pragma mark - FBRewardedVideoAdDelegate implementation

- (void)rewardedVideoAdDidLoad:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video ad was loaded. Can present now.");
  self.adStatusLabel.text = @"Ad loaded. Click show to present!";
}

- (void)rewardedVideoAd:(FBRewardedVideoAd *)rewardedVideoAd didFailWithError:(NSError *)error
{
  NSLog(@"Rewarded video failed to load with error: %@", error.description);
  self.adStatusLabel.text = [NSString stringWithFormat:@"Rewarded Video ad failed to load. %@", error.localizedDescription];
}

- (void)rewardedVideoAdDidClick:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video was clicked.");
}

- (void)rewardedVideoAdDidClose:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video closed.");

  // Optional, Cleaning up.
  self.rewardedVideoAd = nil;
}

- (void)rewardedVideoAdWillClose:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video will close.");
}

- (void)rewardedVideoAdWillLogImpression:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video impression is being captured.");
}

- (void)rewardedVideoAdComplete:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video was completed successfully.");
}

- (void)rewardedVideoAdServerSuccess:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video server side reward succeeded.");
}

- (void)rewardedVideoAdServerFailed:(FBRewardedVideoAd *)rewardedVideoAd
{
  NSLog(@"Rewarded video server side reward failed.");
}

@end
