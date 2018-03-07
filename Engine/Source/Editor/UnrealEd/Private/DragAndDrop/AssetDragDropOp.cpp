// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DragAndDrop/AssetDragDropOp.h"
#include "Engine/Level.h"
#include "ActorFactories/ActorFactory.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "AssetThumbnail.h"
#include "ClassIconFinder.h"

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(const FAssetData& InAssetData, UActorFactory* ActorFactory)
{
	TArray<FAssetData> AssetDataArray;
	AssetDataArray.Emplace(InAssetData);
	return New(MoveTemp(AssetDataArray), TArray<FString>(), ActorFactory);
}

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(TArray<FAssetData> InAssetData, UActorFactory* ActorFactory)
{
	return New(MoveTemp(InAssetData), TArray<FString>(), ActorFactory);
}

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(FString InAssetPath)
{
	TArray<FString> AssetPathsArray;
	AssetPathsArray.Emplace(MoveTemp(InAssetPath));
	return New(TArray<FAssetData>(), MoveTemp(AssetPathsArray), nullptr);
}

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(TArray<FString> InAssetPaths)
{
	return New(TArray<FAssetData>(), MoveTemp(InAssetPaths), nullptr);
}

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(TArray<FAssetData> InAssetData, TArray<FString> InAssetPaths, UActorFactory* ActorFactory)
{
	TSharedRef<FAssetDragDropOp> Operation = MakeShared<FAssetDragDropOp>();

	Operation->MouseCursor = EMouseCursor::GrabHandClosed;

	Operation->ThumbnailSize = 64;

	Operation->AssetData = MoveTemp(InAssetData);
	Operation->AssetPaths = MoveTemp(InAssetPaths);
	Operation->ActorFactory = ActorFactory;

	Operation->Init();

	Operation->Construct();
	return Operation;
}

FAssetDragDropOp::~FAssetDragDropOp()
{
	if (ThumbnailPool.IsValid())
	{
		// Release all rendering resources being held onto
		ThumbnailPool->ReleaseResources();
	}
}

TSharedPtr<SWidget> FAssetDragDropOp::GetDefaultDecorator() const
{
	const int32 TotalCount = AssetData.Num() + AssetPaths.Num();

	TSharedPtr<SWidget> ThumbnailWidget;
	if (AssetThumbnail.IsValid())
	{
		ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
	}
	else if (AssetPaths.Num() > 0)
	{
		ThumbnailWidget = 
			SNew(SOverlay)

			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("ContentBrowser.ListViewFolderIcon.Base"))
				.ColorAndOpacity(FLinearColor::Gray)
			]
		
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("ContentBrowser.ListViewFolderIcon.Mask"))
			];
	}
	else
	{
		ThumbnailWidget = 
			SNew(SImage)
			.Image(FEditorStyle::GetDefaultBrush());
	}
	
	const FSlateBrush* SubTypeBrush = FEditorStyle::GetDefaultBrush();
	FLinearColor SubTypeColor = FLinearColor::White;
	if (AssetThumbnail.IsValid() && AssetPaths.Num() > 0)
	{
		SubTypeBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
		SubTypeColor = FLinearColor::Gray;
	}
	else if (ActorFactory.IsValid() && AssetData.Num() > 0)
	{
		AActor* DefaultActor = ActorFactory->GetDefaultActor(AssetData[0]);
		SubTypeBrush = FClassIconFinder::FindIconForActor(DefaultActor);
	}

	return 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
		.Content()
		[
			SNew(SHorizontalBox)

			// Left slot is for the thumbnail
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(SBox) 
				.WidthOverride(ThumbnailSize) 
				.HeightOverride(ThumbnailSize)
				.Content()
				[
					SNew(SOverlay)

					+SOverlay::Slot()
					[
						ThumbnailWidget.ToSharedRef()
					]

					+SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(FMargin(0, 4, 0, 0))
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
						.Visibility(TotalCount > 1 ? EVisibility::Visible : EVisibility::Collapsed)
						.Content()
						[
							SNew(STextBlock)
							.Text(FText::AsNumber(TotalCount))
						]
					]

					+SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(FMargin(4, 4))
					[
						SNew(SImage)
						.Image(SubTypeBrush)
						.Visibility(SubTypeBrush != FEditorStyle::GetDefaultBrush() ? EVisibility::Visible : EVisibility::Collapsed)
						.ColorAndOpacity(SubTypeColor)
					]
				]
			]

			// Right slot is for optional tooltip
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(80)
				.Content()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SImage) 
						.Image(this, &FAssetDragDropOp::GetIcon)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0,0,3,0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock) 
						.Text(this, &FAssetDragDropOp::GetDecoratorText)
					]
				]
			]
		];
}

FText FAssetDragDropOp::GetDecoratorText() const
{
	if (CurrentHoverText.IsEmpty())
	{
		const int32 TotalCount = AssetData.Num() + AssetPaths.Num();
		if (TotalCount > 0)
		{
			const FText FirstItemText = AssetData.Num() > 0 ? FText::FromName(AssetData[0].AssetName) : FText::FromString(AssetPaths[0]);
			return (TotalCount == 1)
				? FirstItemText
				: FText::Format(NSLOCTEXT("ContentBrowser", "AssetDragDropOpDescriptionMulti", "'{0}' and {1} {1}|plural(one=other,other=others)"), FirstItemText, TotalCount - 1);
		}
	}
	
	return CurrentHoverText;
}

void FAssetDragDropOp::Init()
{
	if (AssetData.Num() > 0 && ThumbnailSize > 0)
	{
		// Load all assets first so that there is no loading going on while attempting to drag
		// Can cause unsafe frame reentry 
		for (FAssetData& Data : AssetData)
		{
			Data.GetAsset();
		}

		// Create a thumbnail pool to hold the single thumbnail rendered
		ThumbnailPool = MakeShared<FAssetThumbnailPool>(1, /*InAreRealTileThumbnailsAllowed=*/false);

		// Create the thumbnail handle
		AssetThumbnail = MakeShared<FAssetThumbnail>(AssetData[0], ThumbnailSize, ThumbnailSize, ThumbnailPool);

		// Request the texture then tick the pool once to render the thumbnail
		AssetThumbnail->GetViewportRenderTargetTexture();
		ThumbnailPool->Tick(0);
	}
}
