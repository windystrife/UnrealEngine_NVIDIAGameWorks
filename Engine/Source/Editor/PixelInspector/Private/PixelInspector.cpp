// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PixelInspector.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PixelInspectorView.h"
#include "PixelInspectorStyle.h"

#include "EngineGlobals.h"
#include "EditorViewportClient.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "LevelEditor.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define PIXEL_INSPECTOR_REQUEST_TIMEOUT 10
#define MINIMUM_TICK_BETWEEN_CREATE_REQUEST 10
#define LOCTEXT_NAMESPACE "PixelInspector"

namespace PixelInspector
{
	SPixelInspector::SPixelInspector()		
	{
		DisplayResult = nullptr;
		LastViewportInspectionPosition = FIntPoint(-1, -1);
		LastViewportId = 0;
		LastViewportInspectionSize = FIntPoint(1, 1);
		bIsPixelInspectorEnable = false;

		Buffer_FinalColor_RGB8[0] = nullptr;
		Buffer_FinalColor_RGB8[1] = nullptr;
		Buffer_SceneColor_Float[0] = nullptr;
		Buffer_SceneColor_Float[1] = nullptr;
		Buffer_HDR_Float[0] = nullptr;
		Buffer_HDR_Float[1] = nullptr;
		Buffer_Depth_Float[0] = nullptr;
		Buffer_Depth_Float[1] = nullptr;
		Buffer_A_Float[0] = nullptr;
		Buffer_A_Float[1] = nullptr;
		Buffer_A_RGB8[0] = nullptr;
		Buffer_A_RGB8[1] = nullptr;
		Buffer_A_RGB10[0] = nullptr;
		Buffer_A_RGB10[1] = nullptr;
		Buffer_BCDE_Float[0] = nullptr;
		Buffer_BCDE_Float[1] = nullptr;
		Buffer_BCDE_RGB8[0] = nullptr;
		Buffer_BCDE_RGB8[1] = nullptr;

		TickSinceLastCreateRequest = 0;
		LastBufferIndex = 0;

		OnLevelActorDeletedDelegateHandle = GEngine->OnLevelActorDeleted().AddRaw(this, &SPixelInspector::OnLevelActorDeleted);
		OnEditorCloseHandle = GEditor->OnEditorClose().AddRaw(this, &SPixelInspector::ReleaseRessource);

		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(FName(TEXT("LevelEditor")));
		OnRedrawViewportHandle = LevelEditor.OnRedrawLevelEditingViewports().AddRaw(this, &SPixelInspector::OnRedrawViewport);

		OnApplicationPreInputKeyDownListenerHandle = FSlateApplication::Get().OnApplicationPreInputKeyDownListener().AddRaw(this, &SPixelInspector::OnApplicationPreInputKeyDownListener);
	}

	SPixelInspector::~SPixelInspector()
	{
		ReleaseRessource();
	}
	
	void SPixelInspector::OnApplicationPreInputKeyDownListener(const FKeyEvent& InKeyEvent)
	{
		if (InKeyEvent.GetKey() == EKeys::Escape && (bIsPixelInspectorEnable))
		{
			// disable the pixel inspector
			bIsPixelInspectorEnable = false;
		}
	}

	void SPixelInspector::ReleaseRessource()
	{
		if (DisplayResult != nullptr)
		{
			DisplayResult->RemoveFromRoot();
			DisplayResult->ClearFlags(RF_Standalone);
			DisplayResult = nullptr;
		}

		ReleaseAllRequests();

		if (OnLevelActorDeletedDelegateHandle.IsValid())
		{
			GEngine->OnLevelActorDeleted().Remove(OnLevelActorDeletedDelegateHandle);
			OnLevelActorDeletedDelegateHandle = FDelegateHandle();
		}

		if (OnEditorCloseHandle.IsValid())
		{
			GEditor->OnEditorClose().Remove(OnEditorCloseHandle);
			OnEditorCloseHandle = FDelegateHandle();
		}

		if (OnRedrawViewportHandle.IsValid())
		{
			FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(FName(TEXT("LevelEditor")));
			LevelEditor.OnRedrawLevelEditingViewports().Remove(OnRedrawViewportHandle);
			OnRedrawViewportHandle = FDelegateHandle();
		}

		if (OnApplicationPreInputKeyDownListenerHandle.IsValid())
		{
			FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(OnApplicationPreInputKeyDownListenerHandle);
			OnApplicationPreInputKeyDownListenerHandle = FDelegateHandle();
		}

		if (DisplayDetailsView.IsValid())
		{
			DisplayDetailsView->SetObject(nullptr);
			DisplayDetailsView = nullptr;
		}
	}

	void SPixelInspector::ReleaseAllRequests()
	{
		//Clear all pending requests because buffer will be clear by the graphic
		for (int i = 0; i < 2; ++i)
		{
			Requests[i].RenderingCommandSend = true;
			Requests[i].RequestComplete = true;
			ReleaseBuffers(i);
		}
		if (DisplayResult != nullptr)
		{
			DisplayResult->RemoveFromRoot();
			DisplayResult->ClearFlags(RF_Standalone);
			DisplayResult = nullptr;
		}
	}

	void SPixelInspector::OnLevelActorDeleted(AActor* Actor)
	{
		ReleaseAllRequests();
	}
	
	void SPixelInspector::OnRedrawViewport(bool bInvalidateHitProxies)
	{
		ReleaseAllRequests();
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void SPixelInspector::Construct(const FArguments& InArgs)
	{
		//Set the LastViewportId to point on the active viewport
		FViewport *ActiveViewport = GEditor->GetActiveViewport();
		for (FEditorViewportClient *EditorViewport : GEditor->AllViewportClients)
		{
			if (ActiveViewport == EditorViewport->Viewport && EditorViewport->ViewState.GetReference() != nullptr)
			{
				LastViewportId = EditorViewport->ViewState.GetReference()->GetViewKey();
			}
		}

		TSharedPtr<SBox> InspectorBox;
		//Create the PixelInspector UI
		TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 3.0f, 0.0f, 3.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ToolTipText(this, &SPixelInspector::GetPixelInspectorEnableButtonTooltipText)
				.OnClicked(this, &SPixelInspector::HandleTogglePixelInspectorEnableButton)
				[
					SNew(SImage)
					.Image(this, &SPixelInspector::GetPixelInspectorEnableButtonBrush)
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(6.0f, 3.0f, 0.0f, 3.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.MinDesiredWidth(75)
				.Text(this, &SPixelInspector::GetPixelInspectorEnableButtonText)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 16.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.MinDesiredWidth(75)
				.Text(LOCTEXT("PixelInspector_ViewportIdValue", "Viewport Id"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 0.0f, 3.0f)
			[
				SNew(SNumericEntryBox<uint32>)
				.IsEnabled(false)
				.MinDesiredValueWidth(75)
				.Value(this, &SPixelInspector::GetCurrentViewportId)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 16.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.MinDesiredWidth(75)
				.Text(LOCTEXT("PixelInspector_ViewportCoordinate", "Coordinate"))
				.ToolTipText(LOCTEXT("PixelInspector_ViewportCoordinateTooltip", "Coordinate relative to the inspected viewport"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 8.0f, 3.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.IsEnabled(this, &SPixelInspector::IsPixelInspectorEnable)
				.Value(this, &SPixelInspector::GetCurrentCoordinateX)
				.OnValueChanged(this, &SPixelInspector::SetCurrentCoordinateX)
				.OnValueCommitted(this, &SPixelInspector::SetCurrentCoordinateXCommit)
				.AllowSpin(true)
				.MinValue(0)
				.MaxSliderValue(this, &SPixelInspector::GetMaxCoordinateX)
				.MinDesiredValueWidth(75)
				.Label()
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CoordinateViewport_X", "X"))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 3.0f, 8.0f, 3.0f)
			.AutoWidth()
			[
				SNew(SNumericEntryBox<int32>)
				.IsEnabled(this, &SPixelInspector::IsPixelInspectorEnable)
				.Value(this, &SPixelInspector::GetCurrentCoordinateY)
				.OnValueChanged(this, &SPixelInspector::SetCurrentCoordinateY)
				.OnValueCommitted(this, &SPixelInspector::SetCurrentCoordinateYCommit)
				.AllowSpin(true)
				.MinValue(0)
				.MaxSliderValue(this, &SPixelInspector::GetMaxCoordinateY)
				.MinDesiredValueWidth(75)
				.Label()
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CoordinateViewport_Y", "Y"))
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.Padding(0.0f, 12.0f, 0.0f, 3.0f)
		.FillHeight(1.0f)
		[
			SAssignNew(InspectorBox, SBox)
		];

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bShowActorLabel = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bSearchInitialKeyFocus = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DisplayDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		InspectorBox->SetContent(DisplayDetailsView->AsShared());
		//Create a property Detail view
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SOverlay)

				// Overlay slot for the main HLOD window area
				+ SOverlay::Slot()
				[
					VerticalBox.ToSharedRef()
				]
			]
		];
		
	}

	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	FReply SPixelInspector::HandleTogglePixelInspectorEnableButton()
	{
		bIsPixelInspectorEnable = !bIsPixelInspectorEnable;
		if (bIsPixelInspectorEnable)
		{
			if (LastViewportInspectionPosition == FIntPoint(-1, -1))
			{
				//Let the system inspect a pixel so the user can see the UI appear
				LastViewportInspectionPosition = FIntPoint(0, 0);
			}
			//Make sure the viewport is switch to realtime
			SetCurrentViewportInRealtime();
		}
		return FReply::Handled();
	}

	FText SPixelInspector::GetPixelInspectorEnableButtonText() const
	{
		if (bIsPixelInspectorEnable)
		{
			return LOCTEXT("PixelInspector_EnableCheckbox_Inspecting", "Inspecting");
		}

		return LOCTEXT("PixelInspectorMouseHover_EnableCheckbox", "Start Pixel Inspector");
	}

	FText SPixelInspector::GetPixelInspectorEnableButtonTooltipText() const
	{
		if (bIsPixelInspectorEnable)
		{
			return LOCTEXT("PixelInspector_EnableCheckbox_ESC", "Inspecting (ESC to stop)");
		}

		return LOCTEXT("PixelInspectorMouseHover_EnableCheckbox", "Start Pixel Inspector");
	}

	const FSlateBrush* SPixelInspector::GetPixelInspectorEnableButtonBrush() const
	{
		return bIsPixelInspectorEnable ? FPixelInspectorStyle::Get()->GetBrush("PixelInspector.Enabled") : FPixelInspectorStyle::Get()->GetBrush("PixelInspector.Disabled");
	}

	void SPixelInspector::SetCurrentCoordinateXCommit(int32 NewValue, ETextCommit::Type)
	{
		ReleaseAllRequests();
		SetCurrentCoordinateX(NewValue);
	}

	void SPixelInspector::SetCurrentCoordinateX(int32 NewValue)
	{
		LastViewportInspectionPosition.X = NewValue;
	}

	void SPixelInspector::SetCurrentCoordinateYCommit(int32 NewValue, ETextCommit::Type)
	{
		ReleaseAllRequests();
		SetCurrentCoordinateY(NewValue);
	}
	void SPixelInspector::SetCurrentCoordinateY(int32 NewValue)
	{
		LastViewportInspectionPosition.Y = NewValue;
	}

	void SPixelInspector::SetCurrentCoordinate(FIntPoint NewCoordinate, bool ReleaseAllRequest)
	{
		if (ReleaseAllRequest)
		{
			ReleaseAllRequests();
		}
		LastViewportInspectionPosition.X = NewCoordinate.X;
		LastViewportInspectionPosition.Y = NewCoordinate.Y;
	}

	TOptional<int32> SPixelInspector::GetMaxCoordinateX() const
	{
		return LastViewportInspectionSize.X - 1;
	}

	TOptional<int32> SPixelInspector::GetMaxCoordinateY() const
	{
		return LastViewportInspectionSize.Y - 1;
	}

	void SPixelInspector::SetCurrentViewportInRealtime()
	{
		//Force viewport refresh
		for (FEditorViewportClient *EditorViewport : GEditor->AllViewportClients)
		{
			if (EditorViewport->ViewState.GetReference() != nullptr)
			{
				if (EditorViewport->ViewState.GetReference()->GetViewKey() == LastViewportId)
				{
					if (!EditorViewport->IsRealtime())
					{
						EditorViewport->SetRealtime(true);
					}
				}
			}
		}
	}

	void SPixelInspector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		TickSinceLastCreateRequest++;
	}
	
	void SPixelInspector::CreatePixelInspectorRequest(FIntPoint ScreenPosition, int32 viewportUniqueId, FSceneInterface *SceneInterface, bool bInGameViewMode)
	{
		if (TickSinceLastCreateRequest < MINIMUM_TICK_BETWEEN_CREATE_REQUEST)
			return;

		if (ScreenPosition == FIntPoint(-1, -1))
		{
			return;
		}
		//Make sure we dont get value outside the viewport size
		if ( ScreenPosition.X >= LastViewportInspectionSize.X || ScreenPosition.Y >= LastViewportInspectionSize.Y )
		{
			return;
		}

		TickSinceLastCreateRequest = 0;
		// We need to know if the GBuffer is in low, default or high precision buffer
		const auto CVarGBufferFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GBufferFormat"));
		//0: lower precision (8bit per component, for profiling)
		//1: low precision (default)
		//5: high precision
		const int32 GBufferFormat = CVarGBufferFormat != nullptr ? CVarGBufferFormat->GetValueOnGameThread() : 1;

		// We need to know the static lighting mode to decode properly the buffers
		const auto CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		//0: false
		//1: true
		//default: true
		const bool AllowStaticLighting = CVarAllowStaticLighting != nullptr ? CVarAllowStaticLighting->GetValueOnGameThread() == 1 : true;
		
		//Try to create the request buffer
		int32 BufferIndex = CreateRequestBuffer(SceneInterface, GBufferFormat, bInGameViewMode);
		if (BufferIndex == -1)
			return;
		
		Requests[BufferIndex].SetRequestData(ScreenPosition, BufferIndex, viewportUniqueId, GBufferFormat, AllowStaticLighting);
		SceneInterface->AddPixelInspectorRequest(&(Requests[BufferIndex]));
	}

	void SPixelInspector::ReleaseBuffers(int32 BufferIndex)
	{
		check(BufferIndex >= 0 && BufferIndex < 2);
		if (Buffer_FinalColor_RGB8[BufferIndex] != nullptr)
		{
			Buffer_FinalColor_RGB8[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_FinalColor_RGB8[BufferIndex]->RemoveFromRoot();
			Buffer_FinalColor_RGB8[BufferIndex] = nullptr;
		}
		if (Buffer_SceneColor_Float[BufferIndex] != nullptr)
		{
			Buffer_SceneColor_Float[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_SceneColor_Float[BufferIndex]->RemoveFromRoot();
			Buffer_SceneColor_Float[BufferIndex] = nullptr;
		}
		if (Buffer_HDR_Float[BufferIndex] != nullptr)
		{
			Buffer_HDR_Float[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_HDR_Float[BufferIndex]->RemoveFromRoot();
			Buffer_HDR_Float[BufferIndex] = nullptr;
		}
		if (Buffer_Depth_Float[BufferIndex] != nullptr)
		{
			Buffer_Depth_Float[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_Depth_Float[BufferIndex]->RemoveFromRoot();
			Buffer_Depth_Float[BufferIndex] = nullptr;
		}
		if (Buffer_A_Float[BufferIndex] != nullptr)
		{
			Buffer_A_Float[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_A_Float[BufferIndex]->RemoveFromRoot();
			Buffer_A_Float[BufferIndex] = nullptr;
		}
		if (Buffer_A_RGB8[BufferIndex] != nullptr)
		{
			Buffer_A_RGB8[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_A_RGB8[BufferIndex]->RemoveFromRoot();
			Buffer_A_RGB8[BufferIndex] = nullptr;
		}
		if (Buffer_A_RGB10[BufferIndex] != nullptr)
		{
			Buffer_A_RGB10[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_A_RGB10[BufferIndex]->RemoveFromRoot();
			Buffer_A_RGB10[BufferIndex] = nullptr;
		}
		if (Buffer_BCDE_Float[BufferIndex] != nullptr)
		{
			Buffer_BCDE_Float[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_BCDE_Float[BufferIndex]->RemoveFromRoot();
			Buffer_BCDE_Float[BufferIndex] = nullptr;
		}
		if (Buffer_BCDE_RGB8[BufferIndex] != nullptr)
		{
			Buffer_BCDE_RGB8[BufferIndex]->ClearFlags(RF_Standalone);
			Buffer_BCDE_RGB8[BufferIndex]->RemoveFromRoot();
			Buffer_BCDE_RGB8[BufferIndex] = nullptr;
		}
	}

	int32 SPixelInspector::CreateRequestBuffer(FSceneInterface *SceneInterface, const int32 GBufferFormat, bool bInGameViewMode)
	{
		//Toggle the last buffer Index
		LastBufferIndex = (LastBufferIndex + 1) % 2;
		
		//Check if we have an available request
		if (Requests[LastBufferIndex].RequestComplete == false)
		{
			//Put back the last buffer position
			LastBufferIndex = (LastBufferIndex - 1) % 2;
			return -1;
		}
		
		//Release the old buffer
		ReleaseBuffers(LastBufferIndex);

		FTextureRenderTargetResource* FinalColorRenderTargetResource = nullptr;
		FTextureRenderTargetResource* SceneColorRenderTargetResource = nullptr;
		FTextureRenderTargetResource* HDRRenderTargetResource = nullptr;
		FTextureRenderTargetResource* DepthRenderTargetResource = nullptr;
		FTextureRenderTargetResource* BufferARenderTargetResource = nullptr;
		FTextureRenderTargetResource* BufferBCDERenderTargetResource = nullptr;

		//Final color is in RGB8 format
		Buffer_FinalColor_RGB8[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferFinalColorTarget"), RF_Standalone);
		Buffer_FinalColor_RGB8[LastBufferIndex]->AddToRoot();
		Buffer_FinalColor_RGB8[LastBufferIndex]->InitCustomFormat(FinalColorContextGridSize, FinalColorContextGridSize, PF_B8G8R8A8, true);
		Buffer_FinalColor_RGB8[LastBufferIndex]->ClearColor = FLinearColor::Black;
		Buffer_FinalColor_RGB8[LastBufferIndex]->UpdateResourceImmediate(true);
		FinalColorRenderTargetResource = Buffer_FinalColor_RGB8[LastBufferIndex]->GameThread_GetRenderTargetResource();

		//Scene color is in RGB8 format
		Buffer_SceneColor_Float[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferSceneColorTarget"), RF_Standalone);
		Buffer_SceneColor_Float[LastBufferIndex]->AddToRoot();
		Buffer_SceneColor_Float[LastBufferIndex]->InitCustomFormat(1, 1, PF_FloatRGBA, true);
		Buffer_SceneColor_Float[LastBufferIndex]->ClearColor = FLinearColor::Black;
		Buffer_SceneColor_Float[LastBufferIndex]->UpdateResourceImmediate(true);
		SceneColorRenderTargetResource = Buffer_SceneColor_Float[LastBufferIndex]->GameThread_GetRenderTargetResource();

		//HDR is in float RGB format
		Buffer_HDR_Float[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferHDRTarget"), RF_Standalone);
		Buffer_HDR_Float[LastBufferIndex]->AddToRoot();
		if (!bInGameViewMode)
		{
			Buffer_HDR_Float[LastBufferIndex]->InitCustomFormat(1, 1, PF_FloatRGBA, true);
		}
		else
		{
			Buffer_HDR_Float[LastBufferIndex]->InitCustomFormat(1, 1, PF_FloatRGB, true);
		}
		Buffer_HDR_Float[LastBufferIndex]->ClearColor = FLinearColor::Black;
		Buffer_HDR_Float[LastBufferIndex]->UpdateResourceImmediate(true);
		HDRRenderTargetResource = Buffer_HDR_Float[LastBufferIndex]->GameThread_GetRenderTargetResource();

		//TODO support Non render buffer to be able to read the depth stencil
/*		Buffer_Depth_Float[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferDepthTarget"), RF_Standalone);
		Buffer_Depth_Float[LastBufferIndex]->AddToRoot();
		Buffer_Depth_Float[LastBufferIndex]->InitCustomFormat(1, 1, PF_DepthStencil, true);
		Buffer_Depth_Float[LastBufferIndex]->ClearColor = FLinearColor::Black;
		Buffer_Depth_Float[LastBufferIndex]->UpdateResourceImmediate(true);
		DepthRenderTargetResource = Buffer_Depth_Float[LastBufferIndex]->GameThread_GetRenderTargetResource();*/


		//Low precision GBuffer
		if (GBufferFormat == EGBufferFormat::Force8BitsPerChannel)
		{
			//All buffer are PF_B8G8R8A8
			Buffer_A_RGB8[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferATarget"), RF_Standalone );
			Buffer_A_RGB8[LastBufferIndex]->AddToRoot();
			Buffer_A_RGB8[LastBufferIndex]->InitCustomFormat(1, 1, PF_B8G8R8A8, true);
			Buffer_A_RGB8[LastBufferIndex]->ClearColor = FLinearColor::Black;
			Buffer_A_RGB8[LastBufferIndex]->UpdateResourceImmediate(true);
			BufferARenderTargetResource = Buffer_A_RGB8[LastBufferIndex]->GameThread_GetRenderTargetResource();

			Buffer_BCDE_RGB8[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferBTarget"), RF_Standalone );
			Buffer_BCDE_RGB8[LastBufferIndex]->AddToRoot();
			Buffer_BCDE_RGB8[LastBufferIndex]->InitCustomFormat(4, 1, PF_B8G8R8A8, true);
			Buffer_BCDE_RGB8[LastBufferIndex]->ClearColor = FLinearColor::Black;
			Buffer_BCDE_RGB8[LastBufferIndex]->UpdateResourceImmediate(true);
			BufferBCDERenderTargetResource = Buffer_BCDE_RGB8[LastBufferIndex]->GameThread_GetRenderTargetResource();
		}
		else if(GBufferFormat == EGBufferFormat::Default)
		{
			//Default is PF_A2B10G10R10
			Buffer_A_RGB10[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferATarget"), RF_Standalone );
			Buffer_A_RGB10[LastBufferIndex]->AddToRoot();
			Buffer_A_RGB10[LastBufferIndex]->InitCustomFormat(1, 1, PF_A2B10G10R10, true);
			Buffer_A_RGB10[LastBufferIndex]->ClearColor = FLinearColor::Black;
			Buffer_A_RGB10[LastBufferIndex]->UpdateResourceImmediate(true);
			BufferARenderTargetResource = Buffer_A_RGB10[LastBufferIndex]->GameThread_GetRenderTargetResource();

			//Default is PF_B8G8R8A8
			Buffer_BCDE_RGB8[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferBTarget"), RF_Standalone );
			Buffer_BCDE_RGB8[LastBufferIndex]->AddToRoot();
			Buffer_BCDE_RGB8[LastBufferIndex]->InitCustomFormat(4, 1, PF_B8G8R8A8, true);
			Buffer_BCDE_RGB8[LastBufferIndex]->ClearColor = FLinearColor::Black;
			Buffer_BCDE_RGB8[LastBufferIndex]->UpdateResourceImmediate(true);
			BufferBCDERenderTargetResource = Buffer_BCDE_RGB8[LastBufferIndex]->GameThread_GetRenderTargetResource();
		}
		else if (GBufferFormat == EGBufferFormat::HighPrecisionNormals || GBufferFormat == EGBufferFormat::Force16BitsPerChannel)
		{
			//All buffer are PF_FloatRGBA
			Buffer_A_Float[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferATarget"), RF_Standalone );
			Buffer_A_Float[LastBufferIndex]->AddToRoot();
			Buffer_A_Float[LastBufferIndex]->InitCustomFormat(1, 1, PF_FloatRGBA, true);
			Buffer_A_Float[LastBufferIndex]->ClearColor = FLinearColor::Black;
			Buffer_A_Float[LastBufferIndex]->UpdateResourceImmediate(true);
			BufferARenderTargetResource = Buffer_A_Float[LastBufferIndex]->GameThread_GetRenderTargetResource();

			Buffer_BCDE_Float[LastBufferIndex] = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("PixelInspectorBufferBTarget"), RF_Standalone );
			Buffer_BCDE_Float[LastBufferIndex]->AddToRoot();
			Buffer_BCDE_Float[LastBufferIndex]->InitCustomFormat(4, 1, PF_FloatRGBA, true);
			Buffer_BCDE_Float[LastBufferIndex]->ClearColor = FLinearColor::Black;
			Buffer_BCDE_Float[LastBufferIndex]->UpdateResourceImmediate(true);
			BufferBCDERenderTargetResource = Buffer_BCDE_Float[LastBufferIndex]->GameThread_GetRenderTargetResource();
		}
		else
		{
			checkf(0, TEXT("Unhandled gbuffer format (%i) during pixel inspector initializtion."), GBufferFormat);
		}	
		
		SceneInterface->InitializePixelInspector(FinalColorRenderTargetResource, SceneColorRenderTargetResource, DepthRenderTargetResource, HDRRenderTargetResource, BufferARenderTargetResource, BufferBCDERenderTargetResource, LastBufferIndex);

		return LastBufferIndex;
	}

	void SPixelInspector::ReadBackRequestData()
	{
		for (int RequestIndex = 0; RequestIndex < 2; ++RequestIndex)
		{
			if (Requests[RequestIndex].RequestComplete == false && Requests[RequestIndex].RenderingCommandSend == true)
			{
				if (Requests[RequestIndex].FrameCountAfterRenderingCommandSend >= WAIT_FRAMENUMBER_BEFOREREADING)
				{
					if (Requests[RequestIndex].SourcePixelPosition == FIntPoint(-1, -1))
					{
						continue;
					}
					PixelInspectorResult PixelResult;
					PixelResult.ScreenPosition = Requests[RequestIndex].SourcePixelPosition;
					PixelResult.ViewUniqueId = Requests[RequestIndex].ViewId;

					TArray<FColor> BufferFinalColorValue;
					FTextureRenderTargetResource* RTResourceFinalColor = Buffer_FinalColor_RGB8[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
					if (RTResourceFinalColor->ReadPixels(BufferFinalColorValue) == false)
					{
						BufferFinalColorValue.Empty();
					}
					PixelResult.DecodeFinalColor(BufferFinalColorValue);

					TArray<FLinearColor> BufferSceneColorValue;
					FTextureRenderTargetResource* RTResourceSceneColor = Buffer_SceneColor_Float[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
					if (RTResourceSceneColor->ReadLinearColorPixels(BufferSceneColorValue) == false)
					{
						BufferSceneColorValue.Empty();
					}
					PixelResult.DecodeSceneColor(BufferSceneColorValue);
					
					if (Buffer_Depth_Float[Requests[RequestIndex].BufferIndex] != nullptr)
					{
						TArray<FLinearColor> BufferDepthValue;
						FTextureRenderTargetResource* RTResourceDepth = Buffer_Depth_Float[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceDepth->ReadLinearColorPixels(BufferDepthValue) == false)
						{
							BufferDepthValue.Empty();
						}
						PixelResult.DecodeDepth(BufferDepthValue);
					}

					TArray<FLinearColor> BufferHDRValue;
					FTextureRenderTargetResource* RTResourceHDR = Buffer_HDR_Float[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
					if (RTResourceHDR->ReadLinearColorPixels(BufferHDRValue) == false)
					{
						BufferHDRValue.Empty();
					}
					PixelResult.DecodeHDR(BufferHDRValue);

					if (Requests[RequestIndex].GBufferPrecision == EGBufferFormat::Force8BitsPerChannel)
					{
						TArray<FColor> BufferAValue;
						FTextureRenderTargetResource* RTResourceA = Buffer_A_RGB8[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceA->ReadPixels(BufferAValue) == false)
						{
							BufferAValue.Empty();
						}

						TArray<FColor> BufferBCDEValue;
						FTextureRenderTargetResource* RTResourceBCDE = Buffer_BCDE_RGB8[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceA->ReadPixels(BufferBCDEValue) == false)
						{
							BufferBCDEValue.Empty();
						}

						PixelResult.DecodeBufferData(BufferAValue, BufferBCDEValue, Requests[RequestIndex].AllowStaticLighting);
					}
					else if (Requests[RequestIndex].GBufferPrecision == EGBufferFormat::Default)
					{
						//PF_A2B10G10R10 format is not support yet
						TArray<FLinearColor> BufferAValue;
						FTextureRenderTargetResource* RTResourceA = Buffer_A_RGB10[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceA->ReadLinearColorPixels(BufferAValue) == false)
						{
							BufferAValue.Empty();
						}

						TArray<FColor> BufferBCDEValue;
						FTextureRenderTargetResource* RTResourceBCDE = Buffer_BCDE_RGB8[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceBCDE->ReadPixels(BufferBCDEValue) == false)
						{
							BufferBCDEValue.Empty();
						}
						PixelResult.DecodeBufferData(BufferAValue, BufferBCDEValue, Requests[RequestIndex].AllowStaticLighting);
					}
					else if (Requests[RequestIndex].GBufferPrecision == EGBufferFormat::HighPrecisionNormals || Requests[RequestIndex].GBufferPrecision == EGBufferFormat::Force16BitsPerChannel)
					{
						//PF_A2B10G10R10 format is not support yet
						TArray<FFloat16Color> BufferAValue;
						FTextureRenderTargetResource* RTResourceA = Buffer_A_Float[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceA->ReadFloat16Pixels(BufferAValue) == false)
						{
							BufferAValue.Empty();
						}

						TArray<FFloat16Color> BufferBCDEValue;
						FTextureRenderTargetResource* RTResourceBCDE = Buffer_BCDE_Float[Requests[RequestIndex].BufferIndex]->GameThread_GetRenderTargetResource();
						if (RTResourceA->ReadFloat16Pixels(BufferBCDEValue) == false)
						{
							BufferBCDEValue.Empty();
						}
						PixelResult.DecodeBufferData(BufferAValue, BufferBCDEValue, Requests[RequestIndex].AllowStaticLighting);
					}
					else
					{
						checkf(0, TEXT("Unhandled gbuffer format (%i) during pixel inspector readback."), Requests[RequestIndex].GBufferPrecision);
					}

					AccumulationResult.Add(PixelResult);
					ReleaseBuffers(RequestIndex);
					Requests[RequestIndex].RequestComplete = true;
					Requests[RequestIndex].RenderingCommandSend = true;
					Requests[RequestIndex].FrameCountAfterRenderingCommandSend = 0;
					Requests[RequestIndex].RequestTickSinceCreation = 0;
				}
				else
				{
					Requests[RequestIndex].FrameCountAfterRenderingCommandSend++;
				}
			}
			else if (Requests[RequestIndex].RequestComplete == false)
			{
				Requests[RequestIndex].RequestTickSinceCreation++;
				if (Requests[RequestIndex].RequestTickSinceCreation > PIXEL_INSPECTOR_REQUEST_TIMEOUT)
				{
					ReleaseBuffers(RequestIndex);
					Requests[RequestIndex].RequestComplete = true;
					Requests[RequestIndex].RenderingCommandSend = true;
					Requests[RequestIndex].FrameCountAfterRenderingCommandSend = 0;
					Requests[RequestIndex].RequestTickSinceCreation = 0;
				}
			}
		}
		if (AccumulationResult.Num() > 0)
		{
			if (DisplayResult == nullptr)
			{
				DisplayResult = NewObject<UPixelInspectorView>(GetTransientPackage(), FName(TEXT("PixelInspectorDisplay")), RF_Standalone);
				DisplayResult->AddToRoot();
			}
			DisplayResult->SetFromResult(AccumulationResult[0]);
			DisplayDetailsView->SetObject(DisplayResult, true);
			if (AccumulationResult[0].ScreenPosition != LastViewportInspectionPosition)
			{
				LastViewportInspectionPosition = AccumulationResult[0].ScreenPosition;
			}
			LastViewportId = AccumulationResult[0].ViewUniqueId;
			AccumulationResult.RemoveAt(0);
		}
	}
};

#undef LOCTEXT_NAMESPACE
