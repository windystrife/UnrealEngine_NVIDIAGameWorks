// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "EditorUndoClient.h"
#include "Styling/ISlateStyle.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/AssetEditorToolkit.h"

class UMediaPlayer;

/**
 * Implements an Editor toolkit for media players.
 */
class FMediaPlayerEditorToolkit
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
	FMediaPlayerEditorToolkit(const TSharedRef<ISlateStyle>& InStyle);

	/** Virtual destructor. */
	virtual ~FMediaPlayerEditorToolkit();

public:

	/**
	 * Initializes the editor tool kit.
	 *
	 * @param InMediaPlayer The UMediaPlayer asset to edit.
	 * @param InMode The mode to create the toolkit in.
	 * @param InToolkitHost The toolkit host.
	 */
	void Initialize(UMediaPlayer* InMediaPlayer, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost);

public:

	//~ FAssetEditorToolkit interface

	virtual FString GetDocumentationLink() const override;
	virtual bool OnRequestClose() override;
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

protected:

	/** Binds the UI commands to delegates. */
	void BindCommands();

	/** Builds the toolbar widget for the media player editor. */
	void ExtendToolBar();

	/**
	 * Gets the playback rate for fast forward.
	 *
	 * @return Forward playback rate.
	 */
	float GetForwardRate() const;

	/**
	 * Gets the playback rate for reverse.
	 *
	 * @return Reverse playback rate.
	 */
	float GetReverseRate() const;

private:

	/** Callback for spawning tabs. */
	TSharedRef<SDockTab> HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier);

private:

	/** The media player asset being edited. */
	UMediaPlayer* MediaPlayer;

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;
};
