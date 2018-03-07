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

#import "InstreamAdRendererController.h"

#import "VASTAdRequest.h"
#import "VASTAdResponse.h"

@interface InstreamAdRendererController () <FBInstreamAdRendererViewDelegate,VASTAdRequestDelegate,UITextFieldDelegate>

@property (nonatomic, strong) VASTAdRequest *adRequest;
@property (weak, nonatomic) IBOutlet UILabel *adStatusLabel;
@property (nonatomic, strong) FBInstreamAdRendererView *rendererView;
@property (weak, nonatomic) IBOutlet UITextField *vastUrlField;

@end

@implementation InstreamAdRendererController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.vastUrlField.delegate = self;
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
    if (self.tabBarController.selectedViewController != self) {
        [self.rendererView removeFromSuperview];
        self.rendererView = nil;
        self.adStatusLabel.text = @"";
    }
}

- (IBAction)showAdTapped:(id)sender
{
    [self makeAdRequest];
}

- (void)resizeInstreamAdRendererViewToSize:(CGSize)size
{
    if (self.rendererView) {
        // Resize with 16:9 ratio
        CGFloat width = size.width;
        CGFloat height = width * (9.0 / 16.0);
        self.rendererView.frame = CGRectMake(0.0, 80.0, width, height);
    }
}

- (void)makeAdRequest
{
    if (self.adRequest) {
        return;
    }
    NSURL *vastUrl = [NSURL URLWithString:self.vastUrlField.text];
    if (!vastUrl) {
        self.adStatusLabel.text = @"Invalid VAST request URL";
        return;
    }
    self.adStatusLabel.text = @"Loading ad from VAST...";
    self.adRequest = [[VASTAdRequest alloc] initWithURL:vastUrl];
    self.adRequest.delegate = self;
    [self.adRequest makeRequest];
}

- (void)renderAdWithAdParameters:(NSString *)adParameters
{
    if (self.rendererView) {
        [self.rendererView removeFromSuperview];
    }
    self.rendererView = [FBInstreamAdRendererView new];
    self.rendererView.delegate = self;
    self.rendererView.backgroundColor = [UIColor grayColor];
    [self resizeInstreamAdRendererViewToSize:self.view.bounds.size];
    [self.view addSubview:self.rendererView];
    self.adStatusLabel.text = @"Loading ad...";
    [self.rendererView loadAdFromAdParameters:adParameters];
}

- (void)removeAdRendererView
{
    [self.rendererView removeFromSuperview];
    self.rendererView = nil;
}

#pragma mark - VASTAdRequestDelete implementation

- (void)adRequest:(VASTAdRequest *)adRequest didCompleteWithAdResponse:(VASTAdResponse *)adResponse
{
    self.adRequest = nil;
    if (adResponse.adParameters) {
        [self renderAdWithAdParameters:adResponse.adParameters];
    } else {
        self.adStatusLabel.text = @"VAST AdParameters missing";
    }
}

- (void)adRequest:(VASTAdRequest *)adRequest didFailWithError:(NSError *)error
{
    self.adRequest =  nil;
    self.adStatusLabel.text = [NSString stringWithFormat:@"VAST request failed: %@", error.localizedDescription];
}

#pragma mark - FBInstreamAdRendererViewDelegate implementation

- (void)adRendererViewDidClick:(FBInstreamAdRendererView *)adRendererView
{
    self.adStatusLabel.text = @"Ad clicked";
}

- (void)adRendererViewDidFinishHandlingClick:(FBInstreamAdRendererView *)adRendererView
{
    self.adStatusLabel.text = @"Ad finished handling click";
}

- (void)adRendererViewDidLoad:(FBInstreamAdRendererView *)adRendererView
{
    self.adStatusLabel.text = @"Ad loaded";
    [self.rendererView showAdFromRootViewController:self];
}

- (void)adRendererViewDidEnd:(FBInstreamAdRendererView *)adRendererView
{
    self.adStatusLabel.text = @"Ad ended";
    [self removeAdRendererView];
}

- (void)adRendererView:(FBInstreamAdRendererView *)adRendererView didFailWithError:(NSError *)error
{
    self.adStatusLabel.text = [NSString stringWithFormat:@"Ad failed: %@", error.localizedDescription];
    [self removeAdRendererView];
}

- (void)adRendererViewWillLogImpression:(FBInstreamAdRendererView *)adRendererView
{
    self.adStatusLabel.text = @"Ad impression";
}

#pragma mark - UITextFieldDelegate

-(BOOL)textFieldShouldReturn:(UITextField *)textField
{
    [textField resignFirstResponder];
    textField.text = [textField.text stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    return YES;
}

@end
