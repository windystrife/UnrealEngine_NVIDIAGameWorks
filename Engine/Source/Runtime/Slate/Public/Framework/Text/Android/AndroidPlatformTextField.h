// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPlatformTextField.h"

class IVirtualKeyboardEntry;

class FAndroidPlatformTextField : public IPlatformTextField
{
public:
	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;
	virtual bool AllowMoveCursor() override;
private:
	bool EnableNewKeyboardConfig() const;
	//	SlateTextField* TextField;
};

typedef FAndroidPlatformTextField FPlatformTextField;

