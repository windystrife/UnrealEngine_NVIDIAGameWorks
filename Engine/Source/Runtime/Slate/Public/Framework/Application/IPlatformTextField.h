// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IVirtualKeyboardEntry;

class IPlatformTextField
{
public:
	virtual ~IPlatformTextField() {};

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) = 0;
	virtual bool AllowMoveCursor() { return true; }

private:

};
