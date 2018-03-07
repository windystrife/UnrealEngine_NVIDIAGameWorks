// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Application/IPlatformTextField.h"

class IVirtualKeyboardEntry;

class FGenericPlatformTextField : public IPlatformTextField
{
public:
	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override {};

};

typedef FGenericPlatformTextField FPlatformTextField;
