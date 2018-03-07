// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Editor.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SComboBox.h"
#include "Runtime/Engine/Classes/Sound/AudioSettings.h"


#define LOCTEXT_NAMESPACE "FLevelEditorPlaySettingsCustomization"

class SScreenPositionCustomization
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SScreenPositionCustomization) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param LayoutBuilder The layout builder to use for generating property widgets.
	 * @param InWindowPositionProperty The handle to the window position property.
	 * @param InCenterWindowProperty The handle to the center window property.
	 */
	void Construct( const FArguments& InArgs, IDetailLayoutBuilder* LayoutBuilder, const TSharedRef<IPropertyHandle>& InWindowPositionProperty, const TSharedRef<IPropertyHandle>& InCenterWindowProperty )
	{
		check(LayoutBuilder != NULL);

		CenterWindowProperty = InCenterWindowProperty;

		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
						.IsEnabled(this, &SScreenPositionCustomization::HandleNewWindowPositionPropertyIsEnabled)

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							InWindowPositionProperty->CreatePropertyNameWidget(LOCTEXT("WindowPosXLabel", "Left Position"))
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							InWindowPositionProperty->GetChildHandle(0)->CreatePropertyValueWidget()
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
						.IsEnabled(this, &SScreenPositionCustomization::HandleNewWindowPositionPropertyIsEnabled)

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							InWindowPositionProperty->CreatePropertyNameWidget(LOCTEXT("TopPositionLabel", "Top Position"))
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							InWindowPositionProperty->GetChildHandle(1)->CreatePropertyValueWidget()
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Bottom)
				[
					InCenterWindowProperty->CreatePropertyValueWidget()
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					InWindowPositionProperty->CreatePropertyNameWidget(LOCTEXT("CenterWindowLabel", "Always center window to screen"))
				]
		];
	}

private:

	// Callback for checking whether the window position properties are enabled.
	bool HandleNewWindowPositionPropertyIsEnabled( ) const
	{
		bool CenterNewWindow;
		CenterWindowProperty->GetValue(CenterNewWindow);

		return !CenterNewWindow;

	}

private:

	// Holds the 'Center window' property
	TSharedPtr<IPropertyHandle> CenterWindowProperty;
};


/**
 * Implements a screen resolution picker widget.
 */
class SScreenResolutionCustomization
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SScreenResolutionCustomization) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param LayoutBuilder The layout builder to use for generating property widgets.
	 * @param InWindowHeightProperty The handle to the window height property.
	 * @param InWindowWidthProperty The handle to the window width property.
	 */
	void Construct( const FArguments& InArgs, IDetailLayoutBuilder* LayoutBuilder, const TSharedRef<IPropertyHandle>& InWindowHeightProperty, const TSharedRef<IPropertyHandle>& InWindowWidthProperty )
	{
		check(LayoutBuilder != NULL);

		WindowHeightProperty = InWindowHeightProperty;
		WindowWidthProperty = InWindowWidthProperty;

		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							WindowWidthProperty->CreatePropertyNameWidget(LOCTEXT("WindowWidthLabel", "Window Width"))
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							WindowWidthProperty->CreatePropertyValueWidget()
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							WindowHeightProperty->CreatePropertyNameWidget(LOCTEXT("WindowHeightLabel", "Window Height"))
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							WindowHeightProperty->CreatePropertyValueWidget()
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew(SComboButton)
						.ButtonContent()
						[
							SNew(STextBlock)
								.Font(LayoutBuilder->GetDetailFont())
								.Text(LOCTEXT("CommonResolutionsButtonText", "Common Window Sizes"))
						]
						.ContentPadding(FMargin(6.0f, 1.0f))
						.MenuContent()
						[
							MakeCommonResolutionsMenu()
						]
						.ToolTipText(LOCTEXT("CommonResolutionsButtonTooltip", "Pick from a list of common screen resolutions"))
				]		
		];
	}

protected:

	/**
	 * Adds a menu entry to the common screen resolutions menu.
	 */
	void AddCommonResolutionEntry( FMenuBuilder& MenuBuilder, int32 Width, int32 Height, const FString& AspectRatio, const FText& Description )
	{
	}

	/**
	 * Adds a section to the screen resolution menu.
	 *
	 * @param MenuBuilder The menu builder to add the section to.
	 * @param Resolutions The collection of screen resolutions to add.
	 * @param SectionName The name of the section to add.
	 */
	void AddScreenResolutionSection( FMenuBuilder& MenuBuilder, const TArray<FPlayScreenResolution>& Resolutions, const FText& SectionName )
	{
		MenuBuilder.BeginSection(NAME_None, SectionName);
		{
			for (auto Iter = Resolutions.CreateConstIterator(); Iter; ++Iter)
			{
				FUIAction Action(FExecuteAction::CreateRaw(this, &SScreenResolutionCustomization::HandleCommonResolutionSelected, Iter->Width, Iter->Height));

				FInternationalization& I18N = FInternationalization::Get();

				FFormatNamedArguments Args;
				Args.Add(TEXT("Width"), FText::AsNumber(Iter->Width, NULL, I18N.GetInvariantCulture()));
				Args.Add(TEXT("Height"), FText::AsNumber(Iter->Height, NULL, I18N.GetInvariantCulture()));
				Args.Add(TEXT("AspectRatio"), FText::FromString(Iter->AspectRatio));

				MenuBuilder.AddMenuEntry(FText::FromString(Iter->Description), FText::Format(LOCTEXT("CommonResolutionFormat", "{Width} x {Height} ({AspectRatio})"), Args), FSlateIcon(), Action);
			}
		}
		MenuBuilder.EndSection();
	}

	/**
	 * Creates a widget for the resolution picker.
	 *
	 * @return The widget.
	 */
	TSharedRef<SWidget> MakeCommonResolutionsMenu( )
	{
		const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
		FMenuBuilder MenuBuilder(true, NULL);

		AddScreenResolutionSection(MenuBuilder, PlaySettings->PhoneScreenResolutions, LOCTEXT("CommonPhonesSectionHeader", "Phones"));
		AddScreenResolutionSection(MenuBuilder, PlaySettings->TabletScreenResolutions, LOCTEXT("CommonTabletsSectionHeader", "Tablets"));
		AddScreenResolutionSection(MenuBuilder, PlaySettings->LaptopScreenResolutions, LOCTEXT("CommonLaptopsSectionHeader", "Laptops"));
		AddScreenResolutionSection(MenuBuilder, PlaySettings->MonitorScreenResolutions, LOCTEXT("CommoMonitorsSectionHeader", "Monitors"));
		AddScreenResolutionSection(MenuBuilder, PlaySettings->TelevisionScreenResolutions, LOCTEXT("CommonTelevesionsSectionHeader", "Televisions"));

		return MenuBuilder.MakeWidget();
	}

private:

	// Handles selecting a common screen resolution.
	void HandleCommonResolutionSelected( int32 Width, int32 Height )
	{
		WindowHeightProperty->SetValue(Height);
		WindowWidthProperty->SetValue(Width);
	}

private:

	// Holds the handle to the window height property.
	TSharedPtr<IPropertyHandle> WindowHeightProperty;

	// Holds the handle to the window width property.
	TSharedPtr<IPropertyHandle> WindowWidthProperty;
};


/**
 * Implements a details view customization for ULevelEditorPlaySettings objects.
 */
class FLevelEditorPlaySettingsCustomization
	: public IDetailCustomization
{
public:

	/** Virtual destructor. */
	virtual ~FLevelEditorPlaySettingsCustomization( ) { }

public:

	// IDetailCustomization interface

	virtual void CustomizeDetails( IDetailLayoutBuilder& LayoutBuilder ) override
	{
		const float MaxPropertyWidth = 400.0f;

		// play in editor settings
		IDetailCategoryBuilder& PlayInEditorCategory = LayoutBuilder.EditCategory("PlayInEditor");
		{
			TArray<TSharedRef<IPropertyHandle>> PIECategoryProperties;
			PlayInEditorCategory.GetDefaultProperties(PIECategoryProperties, true, false);

			TSharedPtr<IPropertyHandle> PIEEnableSoundHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, EnableGameSound));
			PIESoundQualityLevelHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, PlayInEditorSoundQualityLevel));
			PIESoundQualityLevelHandle->MarkHiddenByCustomization();

			for (TSharedRef<IPropertyHandle>& PropertyHandle : PIECategoryProperties)
			{
				if (PropertyHandle->GetProperty() != PIESoundQualityLevelHandle->GetProperty())
				{
					PlayInEditorCategory.AddProperty(PropertyHandle);
				}

				if (PropertyHandle->GetProperty() == PIEEnableSoundHandle->GetProperty())
				{
					PlayInEditorCategory.AddCustomRow(PIESoundQualityLevelHandle->GetPropertyDisplayName(), false)
						.NameContent()
						[
							PIESoundQualityLevelHandle->CreatePropertyNameWidget()
						]
						.ValueContent()
						.MaxDesiredWidth(MaxPropertyWidth)
						[
							SAssignNew(QualityLevelComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&AvailableQualityLevels)
							.OnComboBoxOpening(this, &FLevelEditorPlaySettingsCustomization::HandleQualityLevelComboBoxOpening)
							.OnGenerateWidget(this, &FLevelEditorPlaySettingsCustomization::HandleQualityLevelComboBoxGenerateWidget)
							.OnSelectionChanged(this, &FLevelEditorPlaySettingsCustomization::HandleQualityLevelSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &FLevelEditorPlaySettingsCustomization::GetSelectedQualityLevelName)
							]
						];
				}
			}


		}

		// play in new window settings
		IDetailCategoryBuilder& PlayInNewWindowCategory = LayoutBuilder.EditCategory("PlayInNewWindow");
		{
		// Mac does not support parenting, do not show
#if PLATFORM_MAC
			PlayInNewWindowCategory.AddProperty("PIEAlwaysOnTop")
				.DisplayName(LOCTEXT("PIEAlwaysOnTop", "Always On Top"))
				.IsEnabled(false);
#else
			PlayInNewWindowCategory.AddProperty("PIEAlwaysOnTop")
				.DisplayName(LOCTEXT("PIEAlwaysOnTop", "Always On Top"));
#endif

			// new window size
			TSharedRef<IPropertyHandle> WindowHeightHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, NewWindowHeight));
			TSharedRef<IPropertyHandle> WindowWidthHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, NewWindowWidth));
			TSharedRef<IPropertyHandle> WindowPositionHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, NewWindowPosition));
			TSharedRef<IPropertyHandle> CenterNewWindowHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, CenterNewWindow));

			WindowHeightHandle->MarkHiddenByCustomization();
			WindowWidthHandle->MarkHiddenByCustomization();
			WindowPositionHandle->MarkHiddenByCustomization();
			CenterNewWindowHandle->MarkHiddenByCustomization();

			PlayInNewWindowCategory.AddCustomRow(LOCTEXT("NewWindowSizeRow", "New Window Size"), false)
				.NameContent()
				[
					SNew(STextBlock)
						.Font(LayoutBuilder.GetDetailFont())
						.Text(LOCTEXT("NewWindowSizeName", "New Window Size"))
						.ToolTipText(LOCTEXT("NewWindowSizeTooltip", "Sets the width and height of floating PIE windows (in pixels)"))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(SScreenResolutionCustomization, &LayoutBuilder, WindowHeightHandle, WindowWidthHandle)
				];

			PlayInNewWindowCategory.AddCustomRow(LOCTEXT("NewWindowPositionRow", "New Window Position"), false)
				.NameContent()
				[
					SNew(STextBlock)
						.Font(LayoutBuilder.GetDetailFont())
						.Text(LOCTEXT("NewWindowPositionName", "New Window Position"))
						.ToolTipText(LOCTEXT("NewWindowPositionTooltip", "Sets the screen coordinates for the top-left corner of floating PIE windows (in pixels)"))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(SScreenPositionCustomization, &LayoutBuilder, WindowPositionHandle, CenterNewWindowHandle)
				];
		}

		// play in standalone game settings
		IDetailCategoryBuilder& PlayInStandaloneCategory = LayoutBuilder.EditCategory("PlayInStandaloneGame");
		{
			// standalone window size
			TSharedRef<IPropertyHandle> WindowHeightHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, StandaloneWindowHeight));
			TSharedRef<IPropertyHandle> WindowWidthHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, StandaloneWindowWidth));
			TSharedRef<IPropertyHandle> WindowPositionHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, StandaloneWindowPosition));
			TSharedRef<IPropertyHandle> CenterNewWindowHandle = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, CenterStandaloneWindow));

			WindowHeightHandle->MarkHiddenByCustomization();
			WindowWidthHandle->MarkHiddenByCustomization();
			WindowPositionHandle->MarkHiddenByCustomization();
			CenterNewWindowHandle->MarkHiddenByCustomization();

			PlayInStandaloneCategory.AddCustomRow(LOCTEXT("StandaloneWindowSizeName", "Standalone Window Size"), false)
				.NameContent()
				[
					SNew(STextBlock)
						.Font(LayoutBuilder.GetDetailFont())
						.Text(LOCTEXT("StandaloneWindowSizeName", "Standalone Window Size"))
						.ToolTipText(LOCTEXT("StandaloneWindowSizeTooltip", "Sets the width and height of standalone game windows (in pixels)"))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(SScreenResolutionCustomization, &LayoutBuilder, WindowHeightHandle, WindowWidthHandle)
				];

			PlayInStandaloneCategory.AddCustomRow(LOCTEXT("StandaloneWindowPosName", "Standalone Window Position"), false)
				.NameContent()
				[
					SNew(STextBlock)
						.Font(LayoutBuilder.GetDetailFont())
						.Text(LOCTEXT("StandaloneWindowPosName", "Standalone Window Position"))
						.ToolTipText(LOCTEXT("StandaloneWindowSizeTooltip", "Sets the width and height of standalone game windows (in pixels)"))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(SScreenPositionCustomization, &LayoutBuilder, WindowPositionHandle, CenterNewWindowHandle)
				];


			// command line options
			TSharedPtr<IPropertyHandle> DisableStandaloneSoundProperty = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelEditorPlaySettings, DisableStandaloneSound));

			DisableStandaloneSoundProperty->MarkHiddenByCustomization();

			PlayInStandaloneCategory.AddCustomRow(LOCTEXT("AdditionalStandaloneDetails", "Additional Options"), true)
				.NameContent()
				[
					SNew(STextBlock)
						.Font(LayoutBuilder.GetDetailFont())
						.Text(LOCTEXT("ClientCmdLineName", "Command Line Options"))
						.ToolTipText(LOCTEXT("ClientCmdLineTooltip", "Generates a command line for additional settings that will be passed to the game clients."))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							DisableStandaloneSoundProperty->CreatePropertyValueWidget()
						]

					+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.5f)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							DisableStandaloneSoundProperty->CreatePropertyNameWidget(LOCTEXT("DisableStandaloneSoundLabel", "Disable Sound (-nosound)"))
						]
		
				];
		}

		// multi-player options
		IDetailCategoryBuilder& NetworkCategory = LayoutBuilder.EditCategory("MultiplayerOptions");
		{
			// Number of players
			NetworkCategory.AddProperty("PlayNumberOfClients")
				.DisplayName(LOCTEXT("NumberOfPlayersLabel", "Number of Players"))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandlePlayNumberOfClientsIsEnabled)));

			NetworkCategory.AddProperty("ServerPort")
				.DisplayName(LOCTEXT("ServerPortLabel", "Server Port"))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleServerPortIsEnabled)));

			NetworkCategory.AddProperty("AdditionalServerGameOptions")
				.DisplayName(LOCTEXT("ServerGameOptionsLabel", "Server Game Options"))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleGameOptionsIsEnabled)));

			NetworkCategory.AddProperty("PlayNetDedicated")
				.DisplayName(LOCTEXT("RunDedicatedServerLabel", "Run Dedicated Server"))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandlePlayNetDedicatedPropertyIsEnabled)));

			// client window size
			TSharedRef<IPropertyHandle> WindowHeightHandle = LayoutBuilder.GetProperty("ClientWindowHeight");
			TSharedRef<IPropertyHandle> WindowWidthHandle = LayoutBuilder.GetProperty("ClientWindowWidth");

			WindowHeightHandle->MarkHiddenByCustomization();
			WindowWidthHandle->MarkHiddenByCustomization();

			NetworkCategory.AddProperty("AutoConnectToServer")
				.DisplayName(LOCTEXT("AutoConnectToServerLabel", "Auto Connect To Server"))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleAutoConnectToServerEnabled)))
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleAutoConnectToServerVisibility)));

			NetworkCategory.AddProperty("RouteGamepadToSecondWindow")
				.DisplayName(LOCTEXT("RouteGamepadToSecondWindowLabel", "Route 1st Gamepad to 2nd Client"))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleRerouteInputToSecondWindowEnabled)))
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleRerouteInputToSecondWindowVisibility)));
			
			// Run under one instance
			if (GEditor && GEditor->bAllowMultiplePIEWorlds)
			{
				NetworkCategory.AddProperty("RunUnderOneProcess")
					.DisplayName(LOCTEXT("RunUnderOneProcessEnabledLabel", "Use Single Process"));
			}
			else
			{
				NetworkCategory.AddProperty("RunUnderOneProcess")
					.DisplayName( LOCTEXT("RunUnderOneProcessDisabledLabel", "Run Under One Process is disabled.") )
					.Visibility( EVisibility::Collapsed )
					.IsEnabled( false );
			}

			// Net Mode
			NetworkCategory.AddProperty("PlayNetMode")
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandlePlayNetModeVisibility)))
				.DisplayName(LOCTEXT("PlayNetModeLabel", "Editor Multiplayer Mode"));

			NetworkCategory.AddProperty("AdditionalLaunchOptions")
				.DisplayName(LOCTEXT("AdditionalLaunchOptionsLabel", "Command Line Arguments"))
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleCmdLineVisibility)));

			NetworkCategory.AddCustomRow(LOCTEXT("PlayInNetworkWindowDetails", "Multiplayer Window Size"), false)
				.NameContent()
				[
					WindowHeightHandle->CreatePropertyNameWidget(LOCTEXT("ClientWindowSizeName", "Multiplayer Window Size (in pixels)"), LOCTEXT("ClientWindowSizeTooltip", "Width and Height to use when spawning additional windows."))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(SScreenResolutionCustomization, &LayoutBuilder, WindowHeightHandle, WindowWidthHandle)
				]
				.IsEnabled(TAttribute<bool>(this, &FLevelEditorPlaySettingsCustomization::HandleClientWindowSizePropertyIsEnabled))
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FLevelEditorPlaySettingsCustomization::HandleClientWindowSizePropertyVisibility)));
				
			NetworkCategory.AddCustomRow(LOCTEXT("AdditionalMultiplayerDetails", "Additional Options"), true)
				.NameContent()
				[
					SNew(STextBlock)
					.Font(LayoutBuilder.GetDetailFont())
					.Text(LOCTEXT("PlainTextName", "Play In Editor Description"))
					.ToolTipText(LOCTEXT("PlainTextToolTip", "A brief description of the multiplayer settings and what to expect if you play with them in the editor."))
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SNew(STextBlock)
					.Font(LayoutBuilder.GetDetailFont())
					.Text(this, &FLevelEditorPlaySettingsCustomization::HandleMultiplayerOptionsDescription)
					.WrapTextAt(MaxPropertyWidth)
				];
		}
	}

	// End IDetailCustomization interface

public:

	/**
	 * Creates a new instance.
	 *
	 * @return A new struct customization for play-in settings.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance( )
	{
		return MakeShareable(new FLevelEditorPlaySettingsCustomization());
	}

private:

	// Callback for getting the description of the settings
	FText HandleMultiplayerOptionsDescription( ) const
	{
		const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
		const bool CanRunUnderOneProcess = [&PlayInSettings]{ bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }();
		const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();
		const int32 PlayNumberOfClients = [&PlayInSettings]{ int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();
		const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
		FString Desc;
		if (CanRunUnderOneProcess)
		{
			Desc += LOCTEXT("MultiplayerDescription_OneProcess", "The following will all run under one UE4 instance:\n").ToString();
			if (CanPlayNetDedicated)
			{
				Desc += LOCTEXT("MultiplayerDescription_DedicatedServer", "A dedicated server will open in a new window. ").ToString();
				if (PlayNumberOfClients == 1)
				{
					Desc += LOCTEXT("MultiplayerDescription_EditorClient", "The editor will connect as a client. ").ToString();
				}
				else
				{
					Desc += FText::Format(LOCTEXT("MultiplayerDescription_EditorAndClients", "The editor will connect as a client and {0} additional client window(s) will also connect. "), FText::AsNumber(PlayNumberOfClients-1)).ToString();
				}
			}
			else
			{
				if (PlayNumberOfClients == 1)
				{
					Desc += LOCTEXT("MultiplayerDescription_EditorListenServer", "The editor will run as a listen server. ").ToString();
				}
				else
				{
					Desc += FText::Format(LOCTEXT("MultiplayerDescription_EditorListenServerAndClients", "The editor will run as a listen server and {0} additional client window(s) will also connect to it. "), FText::AsNumber(PlayNumberOfClients-1)).ToString();
				}
			}
		}
		else
		{
			Desc += LOCTEXT("MultiplayerDescription_MultiProcess", "The following will run with multiple UE4 instances:\n").ToString();
			if (PlayNetMode == PIE_Standalone)
			{
				Desc += LOCTEXT("MultiplayerDescription_EditorOffline", "The editor will run offline. ").ToString();
			}
			else if (PlayNetMode == PIE_ListenServer)
			{
				if (PlayNumberOfClients == 1)
				{
					Desc += LOCTEXT("MultiplayerDescription_EditorListenServer", "The editor will run as a listen server. ").ToString();
				}
				else
				{
					Desc += FText::Format(LOCTEXT("MultiplayerDescription_EditorListenServerAndClients", "The editor will run as a listen server and {0} additional client window(s) will also connect to it. "), FText::AsNumber(PlayNumberOfClients-1)).ToString();
				}	
			}
			else
			{
				if (CanPlayNetDedicated)
				{
					Desc += LOCTEXT("MultiplayerDescription_DedicatedServer", "A dedicated server will open in a new window. ").ToString();
					if (PlayNumberOfClients == 1)
					{
						Desc += LOCTEXT("MultiplayerDescription_EditorClient", "The editor will connect as a client. ").ToString();
					}
					else
					{
						Desc += FText::Format(LOCTEXT("MultiplayerDescription_EditorAndClients", "The editor will connect as a client and {0} additional client window(s) will also connect. "), FText::AsNumber(PlayNumberOfClients-1)).ToString();
					}
				}
				else
				{
					if (PlayNumberOfClients <= 2)
					{
						Desc += LOCTEXT("MultiplayerDescription_EditorClientAndListenServer", "A listen server will open in a new window and the editor will connect to it. ").ToString();
					}
					else
					{
						Desc += FText::Format(LOCTEXT("MultiplayerDescription_EditorClientAndListenServerClients", "A listen server will open in a new window and the editor will connect as a client and {0} additional client window(s) will also connect to it. "), FText::AsNumber(FMath::Max(0, PlayNumberOfClients-2))).ToString(); 
					}
				}
			}
		}
		return FText::FromString(Desc);
	}

	// Callback for checking whether the ClientWindowHeight and ClientWindowWidth properties are enabled.
	bool HandleClientWindowSizePropertyIsEnabled( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->IsClientWindowSizeActive();
	}

	// Callback for getting the visibility of the ClientWindowHeight and ClientWindowWidth properties.
	EVisibility HandleClientWindowSizePropertyVisibility() const
	{
		return GetDefault<ULevelEditorPlaySettings>()->GetClientWindowSizeVisibility();
	}

	// Callback for checking whether the PlayNetDedicated is enabled.
	bool HandlePlayNetDedicatedPropertyIsEnabled( ) const
	{		
		return GetDefault<ULevelEditorPlaySettings>()->IsPlayNetDedicatedActive();
	}

	// Callback for checking whether the PlayNumberOfClients is enabled.
	bool HandlePlayNumberOfClientsIsEnabled( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->IsPlayNumberOfClientsActive();
	}

	// Callback for checking whether the ServerPort is enabled.
	bool HandleServerPortIsEnabled() const
	{
		return GetDefault<ULevelEditorPlaySettings>()->IsServerPortActive();
	}

	// Callback for checking whether the AdditionalServerGameOptions is enabled.
	bool HandleGameOptionsIsEnabled( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->IsAdditionalServerGameOptionsActive();
	}

	// Callback for getting the enabled state of the AutoConnectToServer property.
	bool HandleAutoConnectToServerEnabled() const
	{
		return GetDefault<ULevelEditorPlaySettings>()->IsAutoConnectToServerActive();
	}

	// Callback for getting the visibility of the RerouteInputToSecondWindow property.
	EVisibility HandleAutoConnectToServerVisibility() const
	{
		return GetDefault<ULevelEditorPlaySettings>()->GetAutoConnectToServerVisibility();
	}

	// Callback for getting the enabled state of the RerouteInputToSecondWindow property.
	bool HandleRerouteInputToSecondWindowEnabled( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->IsRouteGamepadToSecondWindowActive();
	}
	
	// Callback for getting the visibility of the RerouteInputToSecondWindow property.
	EVisibility HandleRerouteInputToSecondWindowVisibility( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->GetRouteGamepadToSecondWindowVisibility();
	}

	// Callback for getting the visibility of the PlayNetMode property.
	EVisibility HandlePlayNetModeVisibility( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->GetPlayNetModeVisibility();
	}

	// Callback for getting the visibility of the AdditionalLaunchOptions property.
	EVisibility HandleCmdLineVisibility( ) const
	{
		return GetDefault<ULevelEditorPlaySettings>()->GetAdditionalLaunchOptionsVisibility();
	}

	void HandleQualityLevelComboBoxOpening()
	{
		const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
		AvailableQualityLevels.Empty(AudioSettings->QualityLevels.Num());
		for (const FAudioQualitySettings& AQSettings : AudioSettings->QualityLevels)
		{
			AvailableQualityLevels.Add(MakeShareable(new FString(AQSettings.DisplayName.ToString())));
		}
		QualityLevelComboBox->RefreshOptions();
	}

	TSharedRef<SWidget> HandleQualityLevelComboBoxGenerateWidget(TSharedPtr<FString> InItem)
	{
		return SNew(STextBlock)
				.Text(FText::FromString(*InItem));
	}

	void HandleQualityLevelSelectionChanged(TSharedPtr<FString> InSelection, ESelectInfo::Type SelectInfo)
	{
		if (InSelection.IsValid())
		{
			const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
			for (int32 QualityLevel = 0; QualityLevel < AudioSettings->QualityLevels.Num(); ++QualityLevel)
			{
				if (AudioSettings->QualityLevels[QualityLevel].DisplayName.ToString() == *InSelection)
				{
					PIESoundQualityLevelHandle->SetValue(QualityLevel);
					break;
				}
			}
		}
	}

	FText GetSelectedQualityLevelName() const
	{
		int32 QualityLevel = 0;
		PIESoundQualityLevelHandle->GetValue(QualityLevel);
		const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
		return (QualityLevel >= 0 && QualityLevel < AudioSettings->QualityLevels.Num() ? AudioSettings->QualityLevels[QualityLevel].DisplayName : FText::GetEmpty());
	}

private:

	/** Collection of possible quality levels we can use as a parent for this profile */
	TArray<TSharedPtr<FString>> AvailableQualityLevels;
	TSharedPtr<IPropertyHandle> PIESoundQualityLevelHandle;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> QualityLevelComboBox;

};


#undef LOCTEXT_NAMESPACE
