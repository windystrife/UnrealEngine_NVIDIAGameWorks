// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPlatformTextField.h"
#include "Internationalization/Text.h"

#import <UIKit/UIKit.h>

class IVirtualKeyboardEntry;

@class SlateTextField;

class FIOSPlatformTextField : public IPlatformTextField
{
public:
	FIOSPlatformTextField();
	virtual ~FIOSPlatformTextField();

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;

private:
	SlateTextField* TextField;
};

typedef FIOSPlatformTextField FPlatformTextField;

#if !PLATFORM_TVOS
@interface SlateTextField : NSObject<UIAlertViewDelegate>
{
	TSharedPtr<IVirtualKeyboardEntry> TextWidget;
	FText TextEntry;
    
#ifdef __IPHONE_8_0
    UIAlertController* AlertController;
#endif
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
    UIAlertView* AlertView;
#endif
}

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget;
-(void)hide;

@end
#endif
