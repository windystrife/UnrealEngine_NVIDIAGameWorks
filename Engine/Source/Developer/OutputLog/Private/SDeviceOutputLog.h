// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SOutputLog.h"
#include "TargetDeviceId.h"
#include "Interfaces/ITargetDevice.h"

class SComboButton;

struct FTargetDeviceEntry
{
	FTargetDeviceId			DeviceId;
	FString					DeviceName;
	const FSlateBrush*		DeviceIconBrush;
	ITargetDeviceWeakPtr	DeviceWeakPtr;
};

typedef TSharedPtr<FTargetDeviceEntry> FTargetDeviceEntryPtr;

class SDeviceOutputLog : public SOutputLog
{
public:
	SLATE_BEGIN_ARGS( SDeviceOutputLog )
	{}
	SLATE_END_ARGS()

	/** Destructor for output log, so we can unregister from notifications */
	~SDeviceOutputLog();

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs );

protected:
	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	// FOutputDevice interface
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
	virtual bool CanBeUsedOnAnyThread() const;

	void ExecuteConsoleCommand(const FString& ExecCommand);

	/** Callback for lost target devices. */
	void HandleTargetPlatformDeviceLost(ITargetDeviceRef LostDevice);
	/** Callback for discovered target devices. */
	void HandleTargetPlatformDeviceDiscovered(ITargetDeviceRef DiscoveredDevice);

	void AddDeviceEntry(ITargetDeviceRef TargetDevice);

	void OnDeviceSelectionChanged(FTargetDeviceEntryPtr DeviceEntry);
	TSharedRef<SWidget> MakeDeviceComboButtonMenu();
	TSharedRef<SWidget> GenerateWidgetForDeviceComboBox(const FTargetDeviceEntryPtr& DeviceEntry) const;

	FText GetTargetDeviceText(FTargetDeviceEntryPtr DeviceEntry) const;
	FText GetSelectedTargetDeviceText() const;
	
	const FSlateBrush* GetTargetDeviceBrush(FTargetDeviceEntryPtr DeviceEntry) const;
	const FSlateBrush* GetSelectedTargetDeviceBrush() const;
		
private:
	TArray<FTargetDeviceEntryPtr>			DeviceList;
	FTargetDeviceEntryPtr					CurrentDevicePtr;
	ITargetDeviceOutputPtr					CurrentDeviceOutputPtr;

	TSharedPtr<SComboButton> TargetDeviceComboButton;
		
	/** Synchronization object for access to buffered lines */
	FCriticalSection		BufferedLinesSynch;
	TArray<FBufferedLine>	BufferedLines;
};
