// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class FString;
class ITargetDeviceServiceManager;
class SButton;
class SEditableTextBox;
class SOverlay;


/**
 * Implements a widget for manually locating target devices.
 */
class SDeviceBrowserDeviceAdder
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceBrowserDeviceAdder) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InDeviceManager The target device manager to use.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<ITargetDeviceServiceManager>& InDeviceServiceManager);

protected:

	/** Determine the visibility of the AddUnlistedButton. */
	void DetermineAddUnlistedButtonVisibility();

	/** Refreshes the list of known platforms. */
	void RefreshPlatformList();

private:

	/** The button for adding an unlisted device. */
	TSharedPtr<SButton> AddButton;

	/** The device identifier text box.  */
	TSharedPtr<SEditableTextBox> DeviceIdTextBox;

	/** Holds a pointer to the target device service manager. */
	TSharedPtr<ITargetDeviceServiceManager> DeviceServiceManager;

	/** The device name text box. */
	TSharedPtr<SEditableTextBox> DeviceNameTextBox;

	/** The user name text box. */
	TSharedPtr<SEditableTextBox> UserNameTextBox;

	/** The user password text box. */
	TSharedPtr<SEditableTextBox> UserPasswordTextBox;

	/** The turnable overlay with user data. */
	TSharedPtr<SOverlay> UserDataOverlay;

	/** The platforms combo box. */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PlatformComboBox;

	/** The list of known platforms. */
	TArray<TSharedPtr<FString>> PlatformList;
};
