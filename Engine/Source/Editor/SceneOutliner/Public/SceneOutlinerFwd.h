// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "GameFramework/Actor.h"

class ISceneOutliner;
class ISceneOutlinerColumn;

namespace SceneOutliner
{
	struct FTreeItemID;

	struct FInitializationOptions;
	struct FSharedOutlinerData;
	
	struct ITreeItem;
	struct FActorTreeItem;
	struct FWorldTreeItem;
	struct FFolderTreeItem;

	struct ITreeItemVisitor;
	struct IMutableTreeItemVisitor;
	
	typedef TSharedPtr<ITreeItem> FTreeItemPtr;
	typedef TSharedRef<ITreeItem> FTreeItemRef;

	typedef TMap<FTreeItemID, FTreeItemPtr> FTreeItemMap;

	class SSceneOutliner;

	class FOutlinerFilter;
	struct FOutlinerFilters;

	struct FDragDropPayload;
	struct FDragValidationInfo;

	/** Typedef to define an array of actors, used during dragging */
	typedef TArray<TWeakObjectPtr<AActor>> FActorArray;

	/** Typedef to define an array of folder names, used during dragging */
	typedef TArray<FName> FFolderPaths;
}

/** Delegate used with the Scene Outliner in 'actor picking' mode.  You'll bind a delegate when the
    outliner widget is created, which will be fired off when an actor is selected in the list */
DECLARE_DELEGATE_OneParam( FOnActorPicked, AActor* );
DECLARE_DELEGATE_OneParam( FOnSceneOutlinerItemPicked, TSharedRef<SceneOutliner::ITreeItem> );

DECLARE_DELEGATE_OneParam( FCustomSceneOutlinerDeleteDelegate, const TArray< TWeakObjectPtr< AActor > >&  )

/** A delegate used to factory a new column type */
DECLARE_DELEGATE_RetVal_OneParam( TSharedRef< ISceneOutlinerColumn >, FCreateSceneOutlinerColumn, ISceneOutliner& );

/** A delegate used to factory a new filter type */
DECLARE_DELEGATE_RetVal( TSharedRef< SceneOutliner::FOutlinerFilter >, FCreateSceneOutlinerFilter );
