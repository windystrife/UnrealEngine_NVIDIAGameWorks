// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/IOS/IOSPlatformTextField.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "IOS/IOSAsyncTask.h"

namespace
{
	void GetKeyboardConfig(EKeyboardType TargetKeyboardType, FKeyboardConfig& KeyboardConfig)
	{
		KeyboardConfig.KeyboardType = UIKeyboardTypeDefault;
		KeyboardConfig.bSecureTextEntry = NO;
		
		switch (TargetKeyboardType)
		{
		case EKeyboardType::Keyboard_Email:
			KeyboardConfig.KeyboardType = UIKeyboardTypeEmailAddress;
			break;
		case EKeyboardType::Keyboard_Number:
			KeyboardConfig.KeyboardType = UIKeyboardTypeDecimalPad;
			break;
		case EKeyboardType::Keyboard_Web:
			KeyboardConfig.KeyboardType = UIKeyboardTypeURL;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
			KeyboardConfig.KeyboardType = UIKeyboardTypeASCIICapable;
			break;
		case EKeyboardType::Keyboard_Password:
			KeyboardConfig.bSecureTextEntry = YES;
		case EKeyboardType::Keyboard_Default:
		default:
			KeyboardConfig.KeyboardType = UIKeyboardTypeDefault;
			break;
		}
	}
}

FIOSPlatformTextField::FIOSPlatformTextField()
	: TextField( nullptr )
{
}

FIOSPlatformTextField::~FIOSPlatformTextField()
{
	if(TextField != nullptr)
	{
		[TextField release];
		TextField = nullptr;
	}
}

void FIOSPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
#if !PLATFORM_TVOS
	
	FIOSView* View = [IOSAppDelegate GetDelegate].IOSView;
	if (View->bIsUsingIntegratedKeyboard)
	{
		if (bShow)
		{
			EKeyboardType TargetKeyboardType = (TextEntryWidget.IsValid()) ? TextEntryWidget->GetVirtualKeyboardType() : Keyboard_Default;
			
			FKeyboardConfig KeyboardConfig;
			GetKeyboardConfig(TargetKeyboardType, KeyboardConfig);
			
			[View ActivateKeyboard:false keyboardConfig:KeyboardConfig];
		}
		else
		{
			[View DeactivateKeyboard];
		}
	}
	else
	{
		if(TextField == nullptr)
		{
			TextField = [SlateTextField alloc];
		}
		
		if(bShow)
		{
			// these functions must be run on the main thread
			dispatch_async(dispatch_get_main_queue(),^ {
				[TextField show: TextEntryWidget];
			});
		}
        else
        {
            [TextField hide];
        }
	}
#endif
}

#if !PLATFORM_TVOS

@implementation SlateTextField

-(void)hide
{
    if(!TextWidget.IsValid())
    {
        return;
    }
    
#ifdef __IPHONE_8_0
    if([UIAlertController class] && AlertController != nil)
    {
        [AlertController dismissViewControllerAnimated: YES completion: nil];
    }
    else
#endif
    {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
		if (AlertView != nil)
        {
            [AlertView dismissWithClickedButtonIndex: 0 animated: YES]; //0 is the cancel button
        }
#endif
    }
    
    TextWidget = nullptr;
}

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget
{
	TextWidget = InTextWidget;
	TextEntry = FText::FromString(TEXT(""));

#ifdef __IPHONE_8_0
	if ([UIAlertController class])
	{
		AlertController = [UIAlertController alertControllerWithTitle : @"" message:@"" preferredStyle:UIAlertControllerStyleAlert];
		UIAlertAction* okAction = [UIAlertAction 
										actionWithTitle:NSLocalizedString(@"OK", nil)
										style:UIAlertActionStyleDefault
										handler:^(UIAlertAction* action)
										{
											[AlertController dismissViewControllerAnimated : YES completion : nil];

											UITextField* AlertTextField = AlertController.textFields.firstObject;
											TextEntry = FText::FromString(AlertTextField.text);

											FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
											AsyncTask.GameThreadCallback = ^ bool(void)
											{
                                                if(TextWidget.IsValid())
                                                {
                                                    TextWidget->SetTextFromVirtualKeyboard(TextEntry, ETextEntryType::TextEntryAccepted);
                                                }

												// clear the TextWidget
												TextWidget = nullptr;
												return true;
											};
											[AsyncTask FinishedTask];
										}
		];
		UIAlertAction* cancelAction = [UIAlertAction
										actionWithTitle: NSLocalizedString(@"Cancel", nil)
										style:UIAlertActionStyleDefault
										handler:^(UIAlertAction* action)
										{
											[AlertController dismissViewControllerAnimated : YES completion : nil];

											FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
											AsyncTask.GameThreadCallback = ^ bool(void)
											{
												// clear the TextWidget
												TextWidget = nullptr;
												return true;
											};
											[AsyncTask FinishedTask];
										}
		];

		[AlertController addAction: okAction];
		[AlertController addAction: cancelAction];
		[AlertController
						addTextFieldWithConfigurationHandler:^(UITextField* AlertTextField)
						{
							AlertTextField.clearsOnBeginEditing = NO;
							AlertTextField.clearsOnInsertion = NO;
							AlertTextField.text = [NSString stringWithFString : TextWidget->GetText().ToString()];
							AlertTextField.placeholder = [NSString stringWithFString : TextWidget->GetHintText().ToString()];
		 
							EKeyboardType TargetKeyboardType = (TextWidget.IsValid()) ? TextWidget->GetVirtualKeyboardType() : Keyboard_Default;
							FKeyboardConfig KeyboardConfig;
							GetKeyboardConfig(TargetKeyboardType, KeyboardConfig);

							AlertTextField.keyboardType = KeyboardConfig.KeyboardType;
							AlertTextField.autocorrectionType = KeyboardConfig.AutocorrectionType;
							AlertTextField.autocapitalizationType = KeyboardConfig.AutocapitalizationType;
							AlertTextField.secureTextEntry = KeyboardConfig.bSecureTextEntry;
						}
		];
		[[IOSAppDelegate GetDelegate].IOSController presentViewController : AlertController animated : YES completion : nil];
	}
	else
#endif
	{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
		AlertView = [[UIAlertView alloc] initWithTitle:@""
									message:@""
									delegate:self
									cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
									otherButtonTitles:NSLocalizedString(@"OK", nil), nil];

		// give the UIAlertView a style so a UITextField is created
		switch (TextWidget->GetVirtualKeyboardType())
		{
		case EKeyboardType::Keyboard_Password:
			AlertView.alertViewStyle = UIAlertViewStyleSecureTextInput;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
		case EKeyboardType::Keyboard_Default:
		case EKeyboardType::Keyboard_Email:
		case EKeyboardType::Keyboard_Number:
		case EKeyboardType::Keyboard_Web:
		default:
			AlertView.alertViewStyle = UIAlertViewStylePlainTextInput;
			break;
		}

		UITextField* AlertTextField = [AlertView textFieldAtIndex : 0];
		AlertTextField.clearsOnBeginEditing = NO;
		AlertTextField.clearsOnInsertion = NO;
		AlertTextField.autocorrectionType = UITextAutocorrectionTypeNo;
		AlertTextField.autocapitalizationType = UITextAutocapitalizationTypeNone;
		AlertTextField.text = [NSString stringWithFString : TextWidget->GetText().ToString()];
		AlertTextField.placeholder = [NSString stringWithFString : TextWidget->GetHintText().ToString()];

		// set up the keyboard styles not supported in the AlertViewStyle styles
		switch (TextWidget->GetVirtualKeyboardType())
		{
		case EKeyboardType::Keyboard_Email:
			AlertTextField.keyboardType = UIKeyboardTypeEmailAddress;
			break;
		case EKeyboardType::Keyboard_Number:
			AlertTextField.keyboardType = UIKeyboardTypeDecimalPad;
			break;
		case EKeyboardType::Keyboard_Web:
			AlertTextField.keyboardType = UIKeyboardTypeURL;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
			AlertTextField.keyboardType = UIKeyboardTypeASCIICapable;
			break;
		case EKeyboardType::Keyboard_Default:
		case EKeyboardType::Keyboard_Password:
		default:
			// nothing to do, UIAlertView style handles these keyboard types
			break;
		}

		[AlertView show];

		[AlertView release];
#endif
	}
}

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	UITextField* AlertTextField = [alertView textFieldAtIndex : 0];
	TextEntry = FText::FromString(AlertTextField.text);

	FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
    AsyncTask.GameThreadCallback = ^ bool(void)
    {
		// index 1 is the OK button
		if(buttonIndex == 1 && TextWidget.IsValid())
		{
			TextWidget->SetTextFromVirtualKeyboard(TextEntry, ETextEntryType::TextEntryAccepted);
		}
    
        // clear the TextWidget
        TextWidget = nullptr;
        return true;
    };
    [AsyncTask FinishedTask];
}
#endif

@end

#endif

