// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "EditorUndoClient.h"
#include "Styling/ISlateStyle.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/AssetEditorToolkit.h"

class UMediaPlaylist;

/**
 * Implements an Editor toolkit for media play lists.
 */
class FMediaPlaylistEditorToolkit
	: public FAssetEditorToolkit
	, public FEditorUndoClient
	, public FGCObject
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use.
	 */
	FMediaPlaylistEditorToolkit(const TSharedRef<ISlateStyle>& InStyle);

	/** Virtual destructor. */
	virtual ~FMediaPlaylistEditorToolkit();

public:

	/**
	 * Initializes the editor tool kit.
	 *
	 * @param InMediaPlaylist The UMediaPlaylist asset to edit.
	 * @param InMode The mode to create the toolkit in.
	 * @param InToolkitHost The toolkit host.
	 */
	void Initialize(UMediaPlaylist* InMediaPlaylist, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost);

public:

	//~ FAssetEditorToolkit interface

	virtual FString GetDocumentationLink() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

public:

	//~ IToolkit interface

	virtual FText GetBaseToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;

public:

	//~ FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	
protected:

	//~ FEditorUndoClient interface

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:

	/** Callback for spawning tabs. */
	TSharedRef<SDockTab> HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier);

private:

	/** The media play list asset being edited. */
	UMediaPlaylist* MediaPlaylist;

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;
};
