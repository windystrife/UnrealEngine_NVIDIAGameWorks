// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SDeviceOutputLog.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"

static bool IsSupportedPlatform(ITargetPlatform* Platform)
{
	static const FName AndroidPlaftomName("Android"); // TODO: currently implemented only for Android 
	
	check(Platform);
	const auto& PlatfromInfo = Platform->GetPlatformInfo();
	return PlatfromInfo.IsVanilla() && PlatfromInfo.VanillaPlatformName == AndroidPlaftomName;
}


void SDeviceOutputLog::Construct( const FArguments& InArgs )
{
	MessagesTextMarshaller = FOutputLogTextLayoutMarshaller::Create(TArray<TSharedPtr<FLogMessage>>(), &Filter);

	MessagesTextBox = SNew(SMultiLineEditableTextBox)
		.Style(FEditorStyle::Get(), "Log.TextBox")
		.TextStyle(FEditorStyle::Get(), "Log.Normal")
		.ForegroundColor(FLinearColor::Gray)
		.Marshaller(MessagesTextMarshaller)
		.IsReadOnly(true)
		.AlwaysShowScrollbars(true)
		.OnVScrollBarUserScrolled(this, &SDeviceOutputLog::OnUserScrolled)
		.ContextMenuExtender(this, &SDeviceOutputLog::ExtendTextBoxMenu);

	ChildSlot
	[
		SNew(SVerticalBox)

			// Output log area
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				MessagesTextBox.ToSharedRef()
			]
			// The console input box
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(TargetDeviceComboButton, SComboButton)
					.ComboButtonStyle(FEditorStyle::Get(), "GenericFilters.ComboButtonStyle")
					.ForegroundColor(FLinearColor::White)
					.OnGetMenuContent(this, &SDeviceOutputLog::MakeDeviceComboButtonMenu)
					.ContentPadding(FMargin(4.0f, 0.0f))
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(16)
							.HeightOverride(16)
							[
								SNew(SImage).Image(this, &SDeviceOutputLog::GetSelectedTargetDeviceBrush)
							]
						]
			
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
							.Text(this, &SDeviceOutputLog::GetSelectedTargetDeviceText)
						]
					]
				]

				+SHorizontalBox::Slot()
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.FillWidth(1)
				.VAlign(VAlign_Center)
				[
					SNew(SConsoleInputBox)
					.ConsoleCommandCustomExec(this, &SDeviceOutputLog::ExecuteConsoleCommand)
					.OnConsoleCommandExecuted(this, &SDeviceOutputLog::OnConsoleCommandExecuted)
					// Always place suggestions above the input line for the output log widget
					.SuggestionListPlacement( MenuPlacement_AboveAnchor )
				]
			]
	];

	bIsUserScrolled = false;
	RequestForceScroll();
	
	//
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	for (ITargetPlatform* Platform : Platforms)
	{
		if (IsSupportedPlatform(Platform))
		{
			Platform->OnDeviceDiscovered().AddRaw(this, &SDeviceOutputLog::HandleTargetPlatformDeviceDiscovered);
			Platform->OnDeviceLost().AddRaw(this, &SDeviceOutputLog::HandleTargetPlatformDeviceLost);
		}
	}
		
	// Get list of available devices
	for (ITargetPlatform* Platform : Platforms)
	{
		if (IsSupportedPlatform(Platform))
		{
			TArray<ITargetDevicePtr> TargetDevices;
			Platform->GetAllDevices(TargetDevices);

			for (const ITargetDevicePtr& Device : TargetDevices)
			{
				if (Device.IsValid())
				{
					AddDeviceEntry(Device.ToSharedRef());
				}
			}
		}
	}
}

SDeviceOutputLog::~SDeviceOutputLog()
{
	ITargetPlatformManagerModule* Module = FModuleManager::GetModulePtr<ITargetPlatformManagerModule>("TargetPlatform");
	if (Module)
	{
		TArray<ITargetPlatform*> Platforms = Module->GetTargetPlatforms();
		for (ITargetPlatform* Platform : Platforms)
		{
			Platform->OnDeviceDiscovered().RemoveAll(this);
			Platform->OnDeviceLost().RemoveAll(this);
		}
	}
}

void SDeviceOutputLog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FScopeLock ScopeLock(&BufferedLinesSynch);
	if (BufferedLines.Num() > 0)
	{
		for (const FBufferedLine& Line : BufferedLines)
		{
			MessagesTextMarshaller->AppendMessage(*Line.Data, Line.Verbosity, Line.Category);
		}

		// Don't scroll to the bottom automatically when the user is scrolling the view or has scrolled it away from the bottom.
		if (!bIsUserScrolled)
		{
			MessagesTextBox->ScrollTo(FTextLocation(MessagesTextMarshaller->GetNumMessages() - 1));
		}

		BufferedLines.Empty(32);
	}
}

void SDeviceOutputLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FScopeLock ScopeLock(&BufferedLinesSynch);
	BufferedLines.Add(FBufferedLine(V, Category, Verbosity));
}

bool SDeviceOutputLog::CanBeUsedOnAnyThread() const
{
	return true;
}

void SDeviceOutputLog::ExecuteConsoleCommand(const FString& ExecCommand)
{
	if (CurrentDevicePtr.IsValid())
	{
		ITargetDevicePtr PinnedPtr = CurrentDevicePtr->DeviceWeakPtr.Pin();
		if (PinnedPtr.IsValid())
		{
			PinnedPtr->ExecuteConsoleCommand(ExecCommand);
		}
	}
}

void SDeviceOutputLog::HandleTargetPlatformDeviceLost(ITargetDeviceRef LostDevice)
{
	FTargetDeviceId LostDeviceId = LostDevice->GetId();
	
	if (CurrentDevicePtr.IsValid() && CurrentDevicePtr->DeviceId == LostDeviceId)
	{
		// Kill device output object, but do not clean up output in the window
		CurrentDeviceOutputPtr.Reset();
	}
	
	// Should not do it, but what if someone somewhere holds strong reference to a lost device?
	for (const TSharedPtr<FTargetDeviceEntry>& EntryPtr : DeviceList)
	{
		if (EntryPtr->DeviceId == LostDeviceId)
		{
			EntryPtr->DeviceWeakPtr = nullptr;
		}
	}
}

void SDeviceOutputLog::HandleTargetPlatformDeviceDiscovered(ITargetDeviceRef DiscoveredDevice)
{
	FTargetDeviceId DiscoveredDeviceId = DiscoveredDevice->GetId();

	int32 ExistingEntryIdx = DeviceList.IndexOfByPredicate([&](const TSharedPtr<FTargetDeviceEntry>& Other) {
		return (Other->DeviceId == DiscoveredDeviceId);
	});

	if (DeviceList.IsValidIndex(ExistingEntryIdx))
	{
		DeviceList[ExistingEntryIdx]->DeviceWeakPtr = DiscoveredDevice;
		
		if (CurrentDevicePtr.IsValid() && CurrentDevicePtr->DeviceId == DeviceList[ExistingEntryIdx]->DeviceId)
		{
			CurrentDeviceOutputPtr = DiscoveredDevice->CreateDeviceOutputRouter(this);
		}
	}
	else
	{
		AddDeviceEntry(DiscoveredDevice);
	}
}

void SDeviceOutputLog::AddDeviceEntry(ITargetDeviceRef TargetDevice)
{
	using namespace PlatformInfo;
	FName DeviceIconStyleName = TargetDevice->GetTargetPlatform().GetPlatformInfo().GetIconStyleName(EPlatformIconSize::Normal);
	
	TSharedPtr<FTargetDeviceEntry> DeviceEntry = MakeShareable(new FTargetDeviceEntry());
	
	DeviceEntry->DeviceId = TargetDevice->GetId();
	DeviceEntry->DeviceName = TargetDevice->GetName();
	DeviceEntry->DeviceIconBrush = FEditorStyle::GetBrush(DeviceIconStyleName);
	DeviceEntry->DeviceWeakPtr = TargetDevice;
	
	DeviceList.Add(DeviceEntry);
}

void SDeviceOutputLog::OnDeviceSelectionChanged(FTargetDeviceEntryPtr DeviceEntry)
{
	CurrentDeviceOutputPtr.Reset();
	OnClearLog();
	CurrentDevicePtr = DeviceEntry;
	
	if (DeviceEntry.IsValid())
	{
		ITargetDevicePtr PinnedPtr = DeviceEntry->DeviceWeakPtr.Pin();
		if (PinnedPtr.IsValid() && PinnedPtr->IsConnected())
		{
			CurrentDeviceOutputPtr = PinnedPtr->CreateDeviceOutputRouter(this);
		}
	}
}

TSharedRef<SWidget> SDeviceOutputLog::MakeDeviceComboButtonMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	for (const FTargetDeviceEntryPtr& TargetDeviceEntryPtr : DeviceList)
	{
		TSharedRef<SWidget> MenuEntryWidget = GenerateWidgetForDeviceComboBox(TargetDeviceEntryPtr);
				
		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction::CreateSP(this, &SDeviceOutputLog::OnDeviceSelectionChanged, TargetDeviceEntryPtr)), 
			MenuEntryWidget
			);
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SDeviceOutputLog::GenerateWidgetForDeviceComboBox(const FTargetDeviceEntryPtr& DeviceEntry) const
{
	return 
		SNew(SBox)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(24)
				.HeightOverride(24)
				[
					SNew(SImage).Image(GetTargetDeviceBrush(DeviceEntry))
				]
			]
			
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.0f, 0.0f))
			[
				SNew(STextBlock).Text(this, &SDeviceOutputLog::GetTargetDeviceText, DeviceEntry)
			]
		];
}

const FSlateBrush* SDeviceOutputLog::GetTargetDeviceBrush(FTargetDeviceEntryPtr DeviceEntry) const
{
	if (DeviceEntry.IsValid())
	{
		return DeviceEntry->DeviceIconBrush;
	}
	else
	{
		return FEditorStyle::GetBrush("Launcher.Instance_Unknown");
	}
}

const FSlateBrush* SDeviceOutputLog::GetSelectedTargetDeviceBrush() const
{
	return GetTargetDeviceBrush(CurrentDevicePtr);
}

FText SDeviceOutputLog::GetTargetDeviceText(FTargetDeviceEntryPtr DeviceEntry) const
{
	if (DeviceEntry.IsValid())
	{
		ITargetDevicePtr PinnedPtr = DeviceEntry->DeviceWeakPtr.Pin();
		if (PinnedPtr.IsValid() && PinnedPtr->IsConnected())
		{
			return FText::FromString(DeviceEntry->DeviceName);
		}
		else
		{
			return FText::Format(NSLOCTEXT("OutputLog", "TargetDeviceOffline", "{0} (Offline)"), FText::FromString(DeviceEntry->DeviceName));
		}
	}
	else
	{
		return NSLOCTEXT("OutputLog", "UnknownTargetDevice", "<Unknown device>");
	}
}

FText SDeviceOutputLog::GetSelectedTargetDeviceText() const
{
	return GetTargetDeviceText(CurrentDevicePtr);
}
