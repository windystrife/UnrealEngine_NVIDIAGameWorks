// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

class FEdModeTileMap;
class FSingleTileEditorViewportClient;

//////////////////////////////////////////////////////////////////////////
// SSingleTileEditorViewport

class SSingleTileEditorViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SSingleTileEditorViewport) {}
	SLATE_END_ARGS()

	~SSingleTileEditorViewport();

	void Construct(const FArguments& InArgs, TSharedPtr<FSingleTileEditorViewportClient> InViewportClient);

	// SEditorViewport interface
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	// End of SEditorViewport interface

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

protected:
	FText GetTitleText() const;

private:
	TSharedPtr<FSingleTileEditorViewportClient> TypedViewportClient;
	FEdModeTileMap* TileMapEditor;
};
