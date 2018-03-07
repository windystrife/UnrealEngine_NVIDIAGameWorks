// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "STransformViewportToolbar.h"
#include "EngineDefines.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorViewportClient.h"
#include "UnrealEdGlobals.h"
#include "SEditorViewport.h"
#include "EditorViewportCommands.h"
#include "SViewportToolBarIconMenu.h"
#include "SViewportToolBarComboMenu.h"
#include "ISettingsModule.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Settings/EditorProjectSettings.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"

#define LOCTEXT_NAMESPACE "TransformToolBar"

namespace TransformViewportToolbarDefs
{
	/** Size of the arrow shown on SGridSnapSettings Menu button */
	const float DownArrowSize = 4.0f;

	/** Size of the icon displayed on the toggle button of SGridSnapSettings */
	const float ToggleImageScale = 16.0f;
}

void STransformViewportToolBar::Construct( const FArguments& InArgs )
{
	Viewport = InArgs._Viewport;
	CommandList = InArgs._CommandList;

	ChildSlot
	[
		MakeTransformToolBar(InArgs._Extenders)
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef< SWidget > STransformViewportToolBar::MakeSurfaceSnappingButton( FName ToolBarStyle )
{
	auto IsSnappingEnabled = []{
		return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled;
	};

	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder( bCloseAfterSelection, CommandList );

	MenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().SurfaceSnapping);

	MenuBuilder.BeginSection("SurfaceSnappingSettings", LOCTEXT("SnapToSurfaceSettings", "Settings"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SnapToSurfaceSettings_Rotation", "Rotate to Surface Normal"),
			LOCTEXT("SnapToSurfaceSettings_RotationTip", "When checked, snapping an object to a surface will also rotate the object to align to the surface normal"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic( []{
					auto& Settings = GetMutableDefault<ULevelEditorViewportSettings>()->SnapToSurface;
					Settings.bSnapRotation = !Settings.bSnapRotation;
				}),
				FCanExecuteAction::CreateStatic( IsSnappingEnabled ),
				FIsActionChecked::CreateStatic( []{
					const auto& Settings = GetDefault<ULevelEditorViewportSettings>()->SnapToSurface;
					return Settings.bSnapRotation;
				})
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton);

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(IsSnappingEnabled)))

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
			[
				SNew( STextBlock )
				.Text( LOCTEXT("SnapToSurfaceSettings_Offset", "Surface Offset") )
			]
			+SHorizontalBox::Slot()
			.VAlign( VAlign_Bottom )
			.FillWidth( 1.f )
			[
				SNew( SNumericEntryBox<float> )
				.Value( 
					TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateStatic([]{
						const auto& Settings = GetDefault<ULevelEditorViewportSettings>()->SnapToSurface;
						return TOptional<float>(Settings.SnapOffsetExtent);
					}))
				)
				.OnValueChanged(
					SNumericEntryBox<float>::FOnValueChanged::CreateStatic([](float Val){
						GetMutableDefault<ULevelEditorViewportSettings>()->SnapToSurface.SnapOffsetExtent = Val;
					})
				)
				.MinValue(0.f)
				.MaxValue(HALF_WORLD_MAX)
				.MaxSliderValue(1000.f) // 'Sensible' range for the slider (10m)
				.AllowSpin(true)
			],
			FText::GetEmpty()
		);

	}
	MenuBuilder.EndSection();

	// Have to use a custom widget here to make the checkbox work with the subsequent widget :(
	return 
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Cursor( EMouseCursor::Default )
			.Style( FEditorStyle::Get(), EMultiBlockLocation::ToName(FEditorStyle::Join(ToolBarStyle, ".ToggleButton"), EMultiBlockLocation::Start) )
			.Padding( 0 )
			.ToolTipText( LOCTEXT("SurfaceSnappingCheckboxDescription", "Open editor surface snapping options") )
			.IsChecked_Static( []{ return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.Content()
			[
				SNew( SComboButton )
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.HasDownArrow( false )
				.ContentPadding( 0 )
				.ButtonContent()
				[
					SNew( SVerticalBox )

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(5.f, 2.f, 3.f, 0.f))
					[
						SNew( SBox )
						.WidthOverride( TransformViewportToolbarDefs::ToggleImageScale )
						.HeightOverride( TransformViewportToolbarDefs::ToggleImageScale )
						.HAlign( HAlign_Center )
						.VAlign( VAlign_Center )
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush("EditorViewport.ToggleSurfaceSnapping") )
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign( HAlign_Center )
					.Padding(FMargin(0.f, 0.f, 0.f, 3.f))
					[
						SNew( SBox )
						.WidthOverride( TransformViewportToolbarDefs::DownArrowSize )
						.HeightOverride( TransformViewportToolbarDefs::DownArrowSize )
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
				.MenuContent()
				[
					MenuBuilder.MakeWidget()
				]
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(FMargin(1.0f, 0.0f, 0.0f, 0.0f))
			.BorderImage(FEditorStyle::GetDefaultBrush())
			.BorderBackgroundColor(FLinearColor::Black)
		];
}

TSharedRef< SWidget > STransformViewportToolBar::MakeTransformToolBar( const TSharedPtr< FExtender > InExtenders )
{
	FToolBarBuilder ToolbarBuilder( CommandList, FMultiBoxCustomization::None, InExtenders );

	// Use a custom style
	FName ToolBarStyle = "ViewportMenu";
	ToolbarBuilder.SetStyle(&FEditorStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls cannot be focusable as it fights with the press space to change transform mode feature
	ToolbarBuilder.SetIsFocusable( false );

	ToolbarBuilder.BeginSection("Transform");
	ToolbarBuilder.BeginBlockGroup();
	{
		// Move Mode
		static FName TranslateModeName = FName(TEXT("TranslateMode"));
		ToolbarBuilder.AddToolBarButton( FEditorViewportCommands::Get().TranslateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateModeName );
		
		// TranslateRotate Mode
		static FName TranslateRotateModeName = FName(TEXT("TranslateRotateMode"));
		ToolbarBuilder.AddToolBarButton( FEditorViewportCommands::Get().TranslateRotateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateRotateModeName );

		// 2D Mode
		static FName TranslateRotate2DModeName = FName(TEXT("TranslateRotate2DMode"));
		ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().TranslateRotate2DMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateRotate2DModeName);

		// Rotate Mode
		static FName RotateModeName = FName(TEXT("RotateMode"));
		ToolbarBuilder.AddToolBarButton( FEditorViewportCommands::Get().RotateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), RotateModeName );

		// Scale Mode
		static FName ScaleModeName = FName(TEXT("ScaleMode"));
		ToolbarBuilder.AddToolBarButton( FEditorViewportCommands::Get().ScaleMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ScaleModeName );

	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();
	
	ToolbarBuilder.SetIsFocusable( true );
	
	ToolbarBuilder.BeginSection("LocalToWorld");
	ToolbarBuilder.BeginBlockGroup();
	{
		// Move Mode
		ToolbarBuilder.AddToolBarButton( FEditorViewportCommands::Get().CycleTransformGizmoCoordSystem,
										NAME_None,
										TAttribute<FText>(),
										TAttribute<FText>(),
										TAttribute<FSlateIcon>(this, &STransformViewportToolBar::GetLocalToWorldIcon),
										FName(TEXT("CycleTransformGizmoCoordSystem"))
										);
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();
	
	ToolbarBuilder.BeginSection("LocationGridSnap");
	{
		static FName SurfaceSnapName = FName(TEXT("SurfaceSnap"));
		ToolbarBuilder.AddWidget( MakeSurfaceSnappingButton( ToolBarStyle ), SurfaceSnapName );

		// Grab the existing UICommand 
		FUICommandInfo* Command = FEditorViewportCommands::Get().LocationGridSnap.Get();

		static FName PositionSnapName = FName(TEXT("PositionSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(	SNew(SViewportToolBarComboMenu)
									.Style(ToolBarStyle)
									.BlockLocation(EMultiBlockLocation::Middle)
									.Cursor( EMouseCursor::Default )
									.IsChecked(this, &STransformViewportToolBar::IsLocationGridSnapChecked)
									.OnCheckStateChanged(this, &STransformViewportToolBar::HandleToggleLocationGridSnap)
									.Label(this, &STransformViewportToolBar::GetLocationGridLabel)
									.OnGetMenuContent(this, &STransformViewportToolBar::FillLocationGridSnapMenu)
									.ToggleButtonToolTip(Command->GetDescription())
									.MenuButtonToolTip(LOCTEXT("LocationGridSnap_ToolTip", "Set the Position Grid Snap value"))
									.Icon(Command->GetIcon())
									.ParentToolBar(SharedThis(this))
									, PositionSnapName );
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("RotationGridSnap");
	{
		// Grab the existing UICommand 
		FUICommandInfo* Command = FEditorViewportCommands::Get().RotationGridSnap.Get();

		static FName RotationSnapName = FName(TEXT("RotationSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(	SNew(SViewportToolBarComboMenu)
									.Cursor( EMouseCursor::Default )
									.Style(ToolBarStyle)
									.IsChecked(this, &STransformViewportToolBar::IsRotationGridSnapChecked)
									.OnCheckStateChanged(this, &STransformViewportToolBar::HandleToggleRotationGridSnap)
									.Label(this, &STransformViewportToolBar::GetRotationGridLabel)
									.OnGetMenuContent(this, &STransformViewportToolBar::FillRotationGridSnapMenu)
									.ToggleButtonToolTip(Command->GetDescription())
									.MenuButtonToolTip(LOCTEXT("RotationGridSnap_ToolTip", "Set the Rotation Grid Snap value"))
									.Icon(Command->GetIcon())
									.ParentToolBar(SharedThis(this))
									, RotationSnapName );
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Layer2DSnap");
	{
		// Grab the existing UICommand 
		FUICommandInfo* Command = FEditorViewportCommands::Get().Layer2DSnap.Get();

		static FName Layer2DSnapName = FName(TEXT("Layer2DSnap"));

		TSharedRef<SWidget> SnapLayerPickerWidget =
			SNew(SViewportToolBarComboMenu)
			.Cursor(EMouseCursor::Default)
			.Style(ToolBarStyle)
			.Visibility(this, &STransformViewportToolBar::IsLayer2DSnapVisible)
			.IsChecked(this, &STransformViewportToolBar::IsLayer2DSnapChecked)
			.OnCheckStateChanged(this, &STransformViewportToolBar::HandleToggleLayer2DSnap)
			.Label(this, &STransformViewportToolBar::GetLayer2DLabel)
			.OnGetMenuContent(this, &STransformViewportToolBar::FillLayer2DSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("Layer2DSnap_ToolTip", "Set the 2d layer snap value"))
			.Icon(Command->GetIcon())
			.ParentToolBar(SharedThis(this))
			.MinDesiredButtonWidth(88.0f);

		ToolbarBuilder.AddWidget(SnapLayerPickerWidget, Layer2DSnapName);
	}
	ToolbarBuilder.EndSection();



	ToolbarBuilder.BeginSection("ScaleGridSnap");
	{
		// Grab the existing UICommand 
		FUICommandInfo* Command = FEditorViewportCommands::Get().ScaleGridSnap.Get();

		static FName ScaleSnapName = FName(TEXT("ScaleSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(	SNew(SViewportToolBarComboMenu)
									.Cursor( EMouseCursor::Default )
									.Style(ToolBarStyle)
									.IsChecked(this,&STransformViewportToolBar::IsScaleGridSnapChecked)
									.OnCheckStateChanged(this, &STransformViewportToolBar::HandleToggleScaleGridSnap)
									.Label(this ,&STransformViewportToolBar::GetScaleGridLabel)
									.OnGetMenuContent(this, &STransformViewportToolBar::FillScaleGridSnapMenu)
									.ToggleButtonToolTip(Command->GetDescription())
									.MenuButtonToolTip(LOCTEXT("ScaleGridSnap_ToolTip", "Set scaling options"))
									.Icon(Command->GetIcon())
									.ParentToolBar(SharedThis(this))
									, ScaleSnapName );
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("CameraSpeed");
	ToolbarBuilder.BeginBlockGroup();
	{
		// Camera speed 
		ToolbarBuilder.AddWidget(	SNew(SViewportToolBarIconMenu)
									.Cursor(EMouseCursor::Default)
									.Style(ToolBarStyle)
									.Label(this, &STransformViewportToolBar::GetCameraSpeedLabel)
									.OnGetMenuContent(this, &STransformViewportToolBar::FillCameraSpeedMenu)
									.ToolTipText(LOCTEXT("CameraSpeed_ToolTip","Camera Speed"))
									.Icon(FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorViewport.CamSpeedSetting"))
									.ParentToolBar(SharedThis(this))
									.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("CameraSpeedButton")))
									);
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}


TSharedRef<SWidget> STransformViewportToolBar::FillCameraSpeedMenu()
{
	TSharedRef<SWidget> ReturnWidget = SNew(SBorder)
	.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin(8.0f, 2.0f, 60.0f, 2.0f) )
		.HAlign( HAlign_Left )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("MouseSettingsCamSpeed", "Camera Speed")  )
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin(8.0f, 4.0f) )
		[	
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.Padding( FMargin(0.0f, 2.0f) )
			[
				SAssignNew(CamSpeedSlider, SSlider)
				.Value(this, &STransformViewportToolBar::GetCamSpeedSliderPosition)
				.OnValueChanged(this, &STransformViewportToolBar::OnSetCamSpeed)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 8.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew( STextBlock )
				.Text(this, &STransformViewportToolBar::GetCameraSpeedLabel )
				.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
			]
		]
	];

	return ReturnWidget;
}

FSlateIcon STransformViewportToolBar::GetLocalToWorldIcon() const
{
	if( Viewport.IsValid() && Viewport.Pin()->IsCoordSystemActive(COORD_World) )
	{
		static FName WorldIcon("EditorViewport.RelativeCoordinateSystem_World");
		return FSlateIcon(FEditorStyle::GetStyleSetName(), WorldIcon);
	}

	static FName LocalIcon("EditorViewport.RelativeCoordinateSystem_Local");
	return FSlateIcon(FEditorStyle::GetStyleSetName(), LocalIcon);
}

FText STransformViewportToolBar::GetLocationGridLabel() const
{
	return FText::AsNumber( GEditor->GetGridSize() );
}

FText STransformViewportToolBar::GetRotationGridLabel() const
{
	return FText::Format(LOCTEXT("GridRotation - Number - DegreeSymbol", "{0}\u00b0"), FText::AsNumber(GEditor->GetRotGridSize().Pitch));
}

FText STransformViewportToolBar::GetLayer2DLabel() const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	if (Settings2D->SnapLayers.IsValidIndex(ViewportSettings->ActiveSnapLayerIndex))
	{
		return FText::FromString(Settings2D->SnapLayers[ViewportSettings->ActiveSnapLayerIndex].Name);
	}
	
	return FText();
}

FText STransformViewportToolBar::GetScaleGridLabel() const
{
	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 5;

	const float CurGridAmount = GEditor->GetScaleGridSize();
	return (GEditor->UsePercentageBasedScaling()) 
		? FText::AsPercent(CurGridAmount / 100.0f, &NumberFormattingOptions) 
		: FText::AsNumber(CurGridAmount, &NumberFormattingOptions);
}

FText STransformViewportToolBar::GetCameraSpeedLabel() const
{
	auto ViewportPin = Viewport.Pin();
	if (ViewportPin.IsValid() && ViewportPin->GetViewportClient().IsValid())
	{
		return FText::AsNumber( ViewportPin->GetViewportClient()->GetCameraSpeedSetting() );
	}

	return FText();
}	

float STransformViewportToolBar::GetCamSpeedSliderPosition() const
{
	float SliderPos = 0.f;

	auto ViewportPin = Viewport.Pin();
	if (ViewportPin.IsValid() && ViewportPin->GetViewportClient().IsValid())
	{
		SliderPos = (ViewportPin->GetViewportClient()->GetCameraSpeedSetting() - 1) / ((float)FEditorViewportClient::MaxCameraSpeeds - 1);
	}

	return SliderPos;
}

void STransformViewportToolBar::OnSetCamSpeed(float NewValue)
{
	auto ViewportPin = Viewport.Pin();
	if (ViewportPin.IsValid() && ViewportPin->GetViewportClient().IsValid())
	{
		const int32 SpeedSetting = NewValue * ((float)FEditorViewportClient::MaxCameraSpeeds - 1) + 1;
		ViewportPin->GetViewportClient()->SetCameraSpeedSetting(SpeedSetting);
	}
}

/**
	* Sets our grid size based on what the user selected in the UI
	*
	* @param	InIndex		The new index of the grid size to use
	*/
void STransformViewportToolBar::SetGridSize( int32 InIndex )
{
	GEditor->SetGridSize( InIndex );
}


/**
	* Sets the rotation grid size
	*
	* @param	InIndex		The new index of the rotation grid size to use
	* @param	InGridMode	Controls whether to use Preset or User selected values
	*/
void STransformViewportToolBar::SetRotationGridSize( int32 InIndex, ERotationGridMode InGridMode )
{
	GEditor->SetRotGridSize( InIndex, InGridMode );
}

		
/**
	* Sets the scale grid size
	*
	* @param	InIndex	The new index of the scale grid size to use
	*/
void STransformViewportToolBar::SetScaleGridSize( int32 InIndex )
{
	GEditor->SetScaleGridSize( InIndex );
}

/**
 * Sets the active 2d snap layer 
 *
 * @param	InIndex	The new index of the 2d layer to use
 */
void STransformViewportToolBar::SetLayer2D( int32 Layer2DIndex )
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->bEnableLayerSnap = true;
	ViewportSettings->ActiveSnapLayerIndex = Layer2DIndex;
	ViewportSettings->PostEditChange();
}

		
/**
	* Checks to see if the specified grid size index is the current grid size index
	*
	* @param	GridSizeIndex	The grid size index to test
	*
	* @return	True if the specified grid size index is the current one
	*/
bool STransformViewportToolBar::IsGridSizeChecked( int32 GridSizeIndex )
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	return (ViewportSettings->CurrentPosGridSize == GridSizeIndex);
}


/**
	* Checks to see if the specified rotation grid angle is the current rotation grid angle
	*
	* @param	GridSizeIndex	The grid size index to test
	* @param	InGridMode		Controls whether to use Preset or User selected values
	*
	* @return	True if the specified rotation grid size angle is the current one
	*/
bool STransformViewportToolBar::IsRotationGridSizeChecked( int32 GridSizeIndex, ERotationGridMode GridMode )
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	return (ViewportSettings->CurrentRotGridSize == GridSizeIndex) && (ViewportSettings->CurrentRotGridMode == GridMode);
}


/**
	* Checks to see if the specified scale grid size is the current scale grid size
	*
	* @param	GridSizeIndex	The grid size index to test
	*
	* @return	True if the specified scale grid size is the current one
	*/
bool STransformViewportToolBar::IsScaleGridSizeChecked(int32 GridSizeIndex)
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	return (ViewportSettings->CurrentScalingGridSize == GridSizeIndex);
}

bool STransformViewportToolBar::IsLayer2DSelected(int32 LayerIndex)
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	return (ViewportSettings->ActiveSnapLayerIndex == LayerIndex);
}

void STransformViewportToolBar::TogglePreserveNonUniformScale()
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->PreserveNonUniformScale = !ViewportSettings->PreserveNonUniformScale;
}

bool STransformViewportToolBar::IsPreserveNonUniformScaleChecked()
{
	return GetDefault<ULevelEditorViewportSettings>()->PreserveNonUniformScale;
}

TSharedRef<SWidget> STransformViewportToolBar::FillLocationGridSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return BuildLocationGridCheckBoxList("Snap", LOCTEXT("LocationSnapText", "Snap Sizes"), ViewportSettings->bUsePowerOf2SnapSize ? ViewportSettings->Pow2GridSizes : ViewportSettings->DecimalGridSizes );
}

TSharedRef<SWidget> STransformViewportToolBar::BuildLocationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes) const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder LocationGridMenuBuilder( bShouldCloseWindowAfterMenuSelection, CommandList );

	LocationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for( int32 CurGridSizeIndex = 0; CurGridSizeIndex < InGridSizes.Num(); ++CurGridSizeIndex )
	{
		const float CurGridSize = InGridSizes[ CurGridSizeIndex ];

		LocationGridMenuBuilder.AddMenuEntry(
			FText::AsNumber( CurGridSize ),
			FText::Format( LOCTEXT("LocationGridSize_ToolTip", "Sets grid size to {0}"), FText::AsNumber( CurGridSize ) ),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateStatic( &STransformViewportToolBar::SetGridSize, CurGridSizeIndex ),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic( &STransformViewportToolBar::IsGridSizeChecked, CurGridSizeIndex ) ),
			NAME_None,
			EUserInterfaceActionType::RadioButton );
	}
	LocationGridMenuBuilder.EndSection();

	return LocationGridMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> STransformViewportToolBar::FillRotationGridSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return SNew(SUniformGridPanel)

		+ SUniformGridPanel::Slot(0, 0)
		[
			BuildRotationGridCheckBoxList("Common", LOCTEXT("RotationCommonText", "Common"), ViewportSettings->CommonRotGridSizes, GridMode_Common)
		]

	+ SUniformGridPanel::Slot(1, 0)
		[
			BuildRotationGridCheckBoxList("Div360", LOCTEXT("RotationDivisions360DegreesText", "Divisions of 360\u00b0"), ViewportSettings->DivisionsOf360RotGridSizes, GridMode_DivisionsOf360)
		];
}

TSharedRef<SWidget> STransformViewportToolBar::FillLayer2DSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	int32 LayerCount = Settings2D->SnapLayers.Num();
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList); // rename
	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		FName LayerName(*Settings2D->SnapLayers[LayerIndex].Name);

		FUIAction Action(FExecuteAction::CreateStatic(&STransformViewportToolBar::SetLayer2D, LayerIndex),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&STransformViewportToolBar::IsLayer2DSelected, LayerIndex));

		ShowMenuBuilder.AddMenuEntry(FText::FromName(LayerName), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
	}

	struct FLocalFunctions
	{
		static void ShowSettingsViewer()
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->ShowViewer("Project", "Editor", "LevelEditor2DSettings");
			}
		}
	};

	FUIAction ShowSettingsAction(FExecuteAction::CreateStatic(&FLocalFunctions::ShowSettingsViewer));
	ShowMenuBuilder.AddMenuEntry(LOCTEXT("2DSnap_EditLayer", "Edit Layers..."), FText::GetEmpty(), FSlateIcon(), ShowSettingsAction, NAME_None, EUserInterfaceActionType::Button);

	// -------------------------------------------------------
	ShowMenuBuilder.AddMenuSeparator();


	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().SnapTo2DLayer);

	ShowMenuBuilder.AddMenuSeparator();
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().MoveSelectionToTop2DLayer);
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().MoveSelectionUpIn2DLayers);
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().MoveSelectionDownIn2DLayers);
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().MoveSelectionToBottom2DLayer);
	
	ShowMenuBuilder.AddMenuSeparator();
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().Select2DLayerAbove);
	ShowMenuBuilder.AddMenuEntry(LevelEditor.GetLevelEditorCommands().Select2DLayerBelow);

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> STransformViewportToolBar::BuildRotationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes, ERotationGridMode InGridMode) const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder RotationGridMenuBuilder( bShouldCloseWindowAfterMenuSelection, CommandList );

	RotationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for( int32 CurGridAngleIndex = 0; CurGridAngleIndex < InGridSizes.Num(); ++CurGridAngleIndex )
	{
		const float CurGridAngle = InGridSizes[ CurGridAngleIndex ];

		FText MenuName = FText::Format( LOCTEXT("RotationGridAngle", "{0}\u00b0"), FText::AsNumber( CurGridAngle ) ); /*degree symbol*/
		FText ToolTipText = FText::Format( LOCTEXT("RotationGridAngle_ToolTip", "Sets rotation grid angle to {0}"), MenuName ) ; /*degree symbol*/

		RotationGridMenuBuilder.AddMenuEntry(
			MenuName,
			ToolTipText,
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateStatic( &STransformViewportToolBar::SetRotationGridSize, CurGridAngleIndex, InGridMode ),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic( &STransformViewportToolBar::IsRotationGridSizeChecked, CurGridAngleIndex, InGridMode ) ),
			NAME_None,
			EUserInterfaceActionType::RadioButton );
	}
	RotationGridMenuBuilder.EndSection();

	return RotationGridMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> STransformViewportToolBar::FillScaleGridSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const bool bShouldCloseWindowAfterMenuSelection = true;

	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 5;

	FMenuBuilder ScaleGridMenuBuilder( bShouldCloseWindowAfterMenuSelection, CommandList );

	ScaleGridMenuBuilder.BeginSection("ScaleSnapOptions", LOCTEXT("ScaleSnapOptions", "Scale Snap"));

	for( int32 CurGridAmountIndex = 0; CurGridAmountIndex < ViewportSettings->ScalingGridSizes.Num(); ++CurGridAmountIndex )
	{
		const float CurGridAmount = ViewportSettings->ScalingGridSizes[ CurGridAmountIndex ];

		FText MenuText;
		FText ToolTipText;

		if( GEditor->UsePercentageBasedScaling() )
		{
			MenuText = FText::AsPercent( CurGridAmount / 100.0f, &NumberFormattingOptions );
			ToolTipText = FText::Format( LOCTEXT("ScaleGridAmountOld_ToolTip", "Snaps scale values to {0}"), MenuText );
		}
		else
		{
			MenuText = FText::AsNumber( CurGridAmount, &NumberFormattingOptions );
			ToolTipText = FText::Format( LOCTEXT("ScaleGridAmount_ToolTip", "Snaps scale values to increments of {0}"), MenuText );
		}

		ScaleGridMenuBuilder.AddMenuEntry(
			MenuText,
			ToolTipText,
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateStatic( &STransformViewportToolBar::SetScaleGridSize, CurGridAmountIndex ),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic( &STransformViewportToolBar::IsScaleGridSizeChecked, CurGridAmountIndex ) ),
			NAME_None,
			EUserInterfaceActionType::RadioButton );
	}
	ScaleGridMenuBuilder.EndSection();

	if( !GEditor->UsePercentageBasedScaling() )
	{
		ScaleGridMenuBuilder.BeginSection("ScaleGeneralOptions", LOCTEXT("ScaleOptions", "Scaling Options"));

		ScaleGridMenuBuilder.AddMenuEntry(
			LOCTEXT("ScaleGridPreserveNonUniformScale", "Preserve Non-Uniform Scale"),
			LOCTEXT("ScaleGridPreserveNonUniformScale_ToolTip", "When this option is checked, scaling objects that have a non-uniform scale will preserve the ratios between each axis, snapping the axis with the largest value."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateStatic( &STransformViewportToolBar::TogglePreserveNonUniformScale ),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic( &STransformViewportToolBar::IsPreserveNonUniformScaleChecked ) ),
			NAME_None,
			EUserInterfaceActionType::Check );

		ScaleGridMenuBuilder.EndSection();
	}


	return ScaleGridMenuBuilder.MakeWidget();
}

ECheckBoxState STransformViewportToolBar::IsLocationGridSnapChecked() const
{
	return GetDefault<ULevelEditorViewportSettings>()->GridEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState STransformViewportToolBar::IsRotationGridSnapChecked() const
{
	return GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState STransformViewportToolBar::IsLayer2DSnapChecked() const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	const bool bChecked = ViewportSettings->bEnableLayerSnap && Settings2D->SnapLayers.IsValidIndex(ViewportSettings->ActiveSnapLayerIndex);
	return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility STransformViewportToolBar::IsLayer2DSnapVisible() const
{
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	return Settings2D->bEnableSnapLayers ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState STransformViewportToolBar::IsScaleGridSnapChecked() const
{
	return GetDefault<ULevelEditorViewportSettings>()->SnapScaleEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STransformViewportToolBar::HandleToggleLocationGridSnap( ECheckBoxState InState )
{
	GUnrealEd->Exec( GEditor->GetEditorWorldContext().World(), *FString::Printf( TEXT("MODE GRID=%d"), !GetDefault<ULevelEditorViewportSettings>()->GridEnabled ? 1 : 0 ) );
}

void STransformViewportToolBar::HandleToggleRotationGridSnap(ECheckBoxState InState)
{
	GUnrealEd->Exec(GEditor->GetEditorWorldContext().World(), *FString::Printf(TEXT("MODE ROTGRID=%d"), !GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled ? 1 : 0));
}

void STransformViewportToolBar::HandleToggleLayer2DSnap(ECheckBoxState InState)
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	if (!ViewportSettings->bEnableLayerSnap && (Settings2D->SnapLayers.Num() > 0))
	{
		ViewportSettings->bEnableLayerSnap = true;
		ViewportSettings->ActiveSnapLayerIndex = FMath::Clamp(ViewportSettings->ActiveSnapLayerIndex, 0, Settings2D->SnapLayers.Num() - 1);
	}
	else
	{
		ViewportSettings->bEnableLayerSnap = false;
	}
	ViewportSettings->PostEditChange();
}

void STransformViewportToolBar::HandleToggleScaleGridSnap(ECheckBoxState InState)
{
	GUnrealEd->Exec( GEditor->GetEditorWorldContext().World(), *FString::Printf( TEXT("MODE SCALEGRID=%d"), !GetDefault<ULevelEditorViewportSettings>()->SnapScaleEnabled ? 1 : 0 ) );
}

#undef LOCTEXT_NAMESPACE 
