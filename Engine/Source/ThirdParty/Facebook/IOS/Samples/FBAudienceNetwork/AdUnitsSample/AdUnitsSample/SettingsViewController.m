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

#import "SettingsViewController.h"



@interface SettingsViewController () <UITextFieldDelegate>

@property (weak, nonatomic) IBOutlet UIStepper *debugLevelStepper;
@property (weak, nonatomic) IBOutlet UILabel *debugLevelLabel;
@property (weak, nonatomic) IBOutlet UITextField *sandboxTextField;
@property (weak, nonatomic) IBOutlet UISwitch *testModeSwitch;
@property (weak, nonatomic) IBOutlet UILabel *versionLabel;


@end

@implementation SettingsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.sandboxTextField.delegate = self;

  UIImage *whiteImage = [self imageWithSolidColor:[UIColor whiteColor]];
  UIImage *greyImage = [self imageWithSolidColor:[UIColor lightGrayColor]];
  [self.debugLevelStepper setBackgroundImage:whiteImage forState:UIControlStateNormal];
  [self.debugLevelStepper setBackgroundImage:greyImage forState:UIControlStateHighlighted];
  [self.debugLevelStepper setBackgroundImage:whiteImage forState:UIControlStateDisabled];
  [self.testModeSwitch addTarget:self action:@selector(stateChanged:) forControlEvents:UIControlEventValueChanged];
  [self.testModeSwitch setOn:[FBAdSettings isTestMode] animated:NO];


}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];

  self.sandboxTextField.text = [FBAdSettings urlPrefix];
  self.debugLevelLabel.text = [self logLevelToString:[FBAdSettings getLogLevel]];
  self.debugLevelStepper.value = [FBAdSettings getLogLevel];
  self.versionLabel.text = FB_AD_SDK_VERSION;
}

- (UIImage *)imageWithSolidColor:(UIColor *)color {
  CGRect rect = CGRectMake(0.0f, 0.0f, 1.0f, 1.0f);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();

  CGContextSetFillColorWithColor(context, [color CGColor]);
  CGContextFillRect(context, rect);

  UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return image;
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
}

- (void)stateChanged:(id)sender
{
  if(sender == self.testModeSwitch) {
    if ([sender isOn]) {
      [FBAdSettings addTestDevice:[FBAdSettings testDeviceHash]];
    } else {
      [FBAdSettings clearTestDevice:[FBAdSettings testDeviceHash]];
    }
  }
}

- (IBAction)debugLevelStepperDidChange:(UIStepper *)sender
{
  if (sender == self.debugLevelStepper) {
    [FBAdSettings setLogLevel:sender.value];
    self.debugLevelLabel.text = [self logLevelToString:[FBAdSettings getLogLevel]];
  }
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
  if (textField == self.sandboxTextField) {
    [FBAdSettings setUrlPrefix:textField.text];
  }

  [textField resignFirstResponder];

  return YES;
}

- (NSString *)logLevelToString:(FBAdLogLevel)logLevel
{
  NSString *logLevelString = nil;
  switch (logLevel) {
    case FBAdLogLevelNone:
      logLevelString = @"None";
      break;
    case FBAdLogLevelNotification:
      logLevelString = @"Notification";
      break;
    case FBAdLogLevelError:
      logLevelString = @"Error";
      break;
    case FBAdLogLevelWarning:
      logLevelString = @"Warning";
      break;
    case FBAdLogLevelLog:
      logLevelString = @"Log";
      break;
    case FBAdLogLevelDebug:
      logLevelString = @"Debug";
      break;
    case FBAdLogLevelVerbose:
      logLevelString = @"Verbose";
      break;
    default:
      logLevelString = @"Unknown";
      break;
  }

  return logLevelString;
}

@end
