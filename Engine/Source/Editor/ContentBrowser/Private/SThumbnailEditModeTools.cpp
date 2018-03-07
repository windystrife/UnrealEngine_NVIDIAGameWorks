// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SThumbnailEditModeTools.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "UnrealEdGlobals.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "AssetThumbnail.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

//////////////////////////////////////////////
// SThumbnailEditModeTools
//////////////////////////////////////////////

void SThumbnailEditModeTools::Construct( const FArguments& InArgs, const TSharedPtr<FAssetThumbnail>& InAssetThumbnail )
{
	AssetThumbnail = InAssetThumbnail;
	bModifiedThumbnailWhileDragging = false;
	DragStartLocation = FIntPoint(ForceInitToZero);
	bInSmallView = InArgs._SmallView;

	// Prime the SceneThumbnailInfo pointer
	GetSceneThumbnailInfo();

	if ( AssetThumbnail.IsValid() )
	{
		AssetThumbnail.Pin()->OnAssetDataChanged().AddSP(this, &SThumbnailEditModeTools::OnAssetDataChanged);
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Primitive tools
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(1)
		[
			SNew(SButton)
			.Visibility(this, &SThumbnailEditModeTools::GetPrimitiveToolsVisibility)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SThumbnailEditModeTools::ChangePrimitive)
			.ToolTipText(LOCTEXT("CyclePrimitiveThumbnailShapes", "Cycle through primitive shape for this thumbnail"))
			.Content()
			[
				SNew(SImage).Image(this, &SThumbnailEditModeTools::GetCurrentPrimitiveBrush)
			]
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Visibility(this, &SThumbnailEditModeTools::GetPrimitiveToolsResetToDefaultVisibility)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SThumbnailEditModeTools::ResetToDefault)
			.ToolTipText(LOCTEXT("ResetThumbnailToDefault", "Resets thumbnail to the default"))
			.Content()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("ContentBrowser.ResetPrimitiveToDefault"))
			]
		]
	];
}

EVisibility SThumbnailEditModeTools::GetPrimitiveToolsVisibility() const
{
	const bool bIsVisible = !bInSmallView && (ConstGetSceneThumbnailInfoWithPrimitive() != NULL);
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SThumbnailEditModeTools::GetPrimitiveToolsResetToDefaultVisibility() const
{
	USceneThumbnailInfo* ThumbnailInfo = SceneThumbnailInfo.Get();
	
	EVisibility ResetToDefaultVisibility = EVisibility::Collapsed;
	if (ThumbnailInfo)
	{
		ResetToDefaultVisibility = ThumbnailInfo && ThumbnailInfo->DiffersFromDefault() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return ResetToDefaultVisibility;
}

const FSlateBrush* SThumbnailEditModeTools::GetCurrentPrimitiveBrush() const
{
	USceneThumbnailInfoWithPrimitive* ThumbnailInfo = ConstGetSceneThumbnailInfoWithPrimitive();
	if ( ThumbnailInfo )
	{
		// Note this is for the icon only.  we are assuming the thumbnail renderer does the right thing when rendering
		EThumbnailPrimType PrimType = ThumbnailInfo->bUserModifiedShape ? ThumbnailInfo->PrimitiveType.GetValue() : GetDefaultThumbnailType();
		switch (PrimType)
		{
		case TPT_None: return FEditorStyle::GetBrush("ContentBrowser.PrimitiveCustom");
		case TPT_Sphere: return FEditorStyle::GetBrush("ContentBrowser.PrimitiveSphere");
		case TPT_Cube: return FEditorStyle::GetBrush("ContentBrowser.PrimitiveCube");
		case TPT_Cylinder: return FEditorStyle::GetBrush("ContentBrowser.PrimitiveCylinder");
		case TPT_Plane:
		default:
			// Fall through and return a plane
			break;
		}
	}

	return FEditorStyle::GetBrush( "ContentBrowser.PrimitivePlane" );
}

FReply SThumbnailEditModeTools::ChangePrimitive()
{
	USceneThumbnailInfoWithPrimitive* ThumbnailInfo = GetSceneThumbnailInfoWithPrimitive();
	if ( ThumbnailInfo )
	{
		uint8 PrimitiveIdx = ThumbnailInfo->PrimitiveType + 1;
		if ( PrimitiveIdx >= TPT_MAX )
		{
			if ( ThumbnailInfo->PreviewMesh.IsValid() )
			{
				PrimitiveIdx = TPT_None;
			}
			else
			{
				PrimitiveIdx = TPT_None + 1;
			}
		}

		ThumbnailInfo->PrimitiveType = TEnumAsByte<EThumbnailPrimType>(PrimitiveIdx);
		ThumbnailInfo->bUserModifiedShape = true;

		if ( AssetThumbnail.IsValid() )
		{
			AssetThumbnail.Pin()->RefreshThumbnail();
		}

		ThumbnailInfo->MarkPackageDirty();
	}

	return FReply::Handled();
}

FReply SThumbnailEditModeTools::ResetToDefault()
{
	USceneThumbnailInfo* ThumbnailInfo = GetSceneThumbnailInfo();
	if (ThumbnailInfo)
	{
		ThumbnailInfo->ResetToDefault();

		if (AssetThumbnail.IsValid())
		{
			AssetThumbnail.Pin()->RefreshThumbnail();
		}

		ThumbnailInfo->MarkPackageDirty();

	}

	return FReply::Handled();
}

FReply SThumbnailEditModeTools::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( AssetThumbnail.IsValid() &&
		(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		)
	{
		// Load the asset, unless it is in an unloaded map package or already loaded
		const FAssetData& AssetData = AssetThumbnail.Pin()->GetAssetData();
		
		// Getting the asset loads it, if it isn't already.
		UObject* Asset = AssetData.GetAsset();

		USceneThumbnailInfo* ThumbnailInfo = GetSceneThumbnailInfo();
		if ( ThumbnailInfo )
		{
			FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo(Asset);
			if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
			{
				bModifiedThumbnailWhileDragging = false;
				DragStartLocation = FIntPoint(MouseEvent.GetScreenSpacePosition().X, MouseEvent.GetScreenSpacePosition().Y);

				return FReply::Handled().CaptureMouse( AsShared() ).UseHighPrecisionMouseMovement( AsShared() );
			}
		}

		// This thumbnail does not have a scene thumbnail info but thumbnail editing is enabled. Just consume the input.
		return FReply::Handled();
	}
		
	return FReply::Unhandled();
}

FReply SThumbnailEditModeTools::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( HasMouseCapture() )
	{
		if ( bModifiedThumbnailWhileDragging )
		{
			USceneThumbnailInfo* ThumbnailInfo = GetSceneThumbnailInfo();
			if ( ThumbnailInfo )
			{
				ThumbnailInfo->MarkPackageDirty();
			}

			bModifiedThumbnailWhileDragging = false;
		}

		return FReply::Handled().ReleaseMouseCapture().SetMousePos(DragStartLocation);
	}

	return FReply::Unhandled();
}

FReply SThumbnailEditModeTools::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( HasMouseCapture() )
	{
		if ( !MouseEvent.GetCursorDelta().IsZero() )
		{
			USceneThumbnailInfo* ThumbnailInfo = GetSceneThumbnailInfo();
			if ( ThumbnailInfo )
			{
				const bool bLeftMouse = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
				const bool bRightMouse = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);

				if ( bLeftMouse )
				{
					ThumbnailInfo->OrbitYaw += -MouseEvent.GetCursorDelta().X;
					ThumbnailInfo->OrbitPitch += -MouseEvent.GetCursorDelta().Y;

					// Normalize the values
					if ( ThumbnailInfo->OrbitYaw > 180 )
					{
						ThumbnailInfo->OrbitYaw -= 360;
					}
					else if ( ThumbnailInfo->OrbitYaw < -180 )
					{
						ThumbnailInfo->OrbitYaw += 360;
					}
					
					if ( ThumbnailInfo->OrbitPitch > 90 )
					{
						ThumbnailInfo->OrbitPitch = 90;
					}
					else if ( ThumbnailInfo->OrbitPitch < -90 )
					{
						ThumbnailInfo->OrbitPitch = -90;
					}
				}
				else if ( bRightMouse )
				{
					// Since zoom is a modifier of on the camera distance from the bounding sphere of the object, it is normalized in the thumbnail preview scene.
					ThumbnailInfo->OrbitZoom += MouseEvent.GetCursorDelta().Y;
				}

				// Dirty the package when the mouse is released
				bModifiedThumbnailWhileDragging = true;
			}
		}

		// Refresh the thumbnail. Do this even if the mouse did not move in case the thumbnail varies with time.
		if ( AssetThumbnail.IsValid() )
		{
			AssetThumbnail.Pin()->RefreshThumbnail();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FCursorReply SThumbnailEditModeTools::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	return HasMouseCapture() ? 
		FCursorReply::Cursor( EMouseCursor::None ) :
		FCursorReply::Cursor( EMouseCursor::Default );
}

USceneThumbnailInfo* SThumbnailEditModeTools::GetSceneThumbnailInfo()
{
	if ( !SceneThumbnailInfo.IsValid() )
	{
		if ( AssetThumbnail.IsValid() )
		{
			UObject* Asset = AssetThumbnail.Pin()->GetAsset();

			if ( Asset )
			{
				static const FName AssetToolsName("AssetTools");
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsName);
				TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( Asset->GetClass() );
				if ( AssetTypeActions.IsValid() )
				{
					SceneThumbnailInfo = Cast<USceneThumbnailInfo>(AssetTypeActions.Pin()->GetThumbnailInfo(Asset));
				}
			}
		}
	}

	return SceneThumbnailInfo.Get();
}

USceneThumbnailInfoWithPrimitive* SThumbnailEditModeTools::GetSceneThumbnailInfoWithPrimitive()
{
	USceneThumbnailInfo* ThumbnailInfo = SceneThumbnailInfo.Get();
	if ( !ThumbnailInfo )
	{
		ThumbnailInfo = GetSceneThumbnailInfo();
	}

	return Cast<USceneThumbnailInfoWithPrimitive>( ThumbnailInfo );
}

USceneThumbnailInfoWithPrimitive* SThumbnailEditModeTools::ConstGetSceneThumbnailInfoWithPrimitive() const
{
	return Cast<USceneThumbnailInfoWithPrimitive>( SceneThumbnailInfo.Get() );
}

EThumbnailPrimType SThumbnailEditModeTools::GetDefaultThumbnailType() const 
{
	EThumbnailPrimType DefaultPrimitiveType = TPT_Sphere;

	if (AssetThumbnail.IsValid())
	{
		UObject* Asset = AssetThumbnail.Pin()->GetAsset();

		if (Asset)
		{
			static const FName AssetToolsName("AssetTools");

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsName);
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Asset->GetClass());
			if (AssetTypeActions.IsValid())
			{
				DefaultPrimitiveType = AssetTypeActions.Pin()->GetDefaultThumbnailPrimitiveType(Asset);
			}
		}
	}

	return DefaultPrimitiveType;
}

void SThumbnailEditModeTools::OnAssetDataChanged()
{
	// Set the SceneThumbnailInfo pointer as needed
	GetSceneThumbnailInfo();
}


#undef LOCTEXT_NAMESPACE
