// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UBlastMesh;
class UBlastFractureSettings;

typedef TSharedPtr<class FBlastChunkEditorModel> FBlastChunkEditorModelPtr;

class FBlastChunkEditorModel
{
public:
	FBlastChunkEditorModel(FName InName,
		bool bInBold,
		int32 InChunkIndex,
		bool InSupport,
		bool InStatic,
		FBlastChunkEditorModelPtr InParent = nullptr)
		: Name(InName)
		, bBold(bInBold)
		, bVisible(true)
		, bSupport(InSupport)
		, bStatic(InStatic)
		, ChunkIndex(InChunkIndex)
		, Parent(InParent)
	{
	}

	FName Name;
	bool bBold;
	bool bVisible;
	bool bSupport;
	bool bStatic;
	int32 ChunkIndex;
	TSharedPtr<TArray<FVector>> VoronoiSites;
	FBlastChunkEditorModelPtr Parent;
};

enum class EBlastViewportControlMode : uint8
{
	Normal,
	Point,
	TwoPoint,
	ThreePoint,
	None
};

/** BlastMesh Editor public interface */
class IBlastMeshEditor : public FAssetEditorToolkit
{

public:

	/** Returns the UBlastMesh being edited. */
	virtual UBlastMesh* GetBlastMesh() = 0;

	/** Returns the current preview depth selected in the UI */
	virtual int32 GetCurrentPreviewDepth() const = 0;
	
	/** Refreshes the Blast Mesh Editor's viewport. */
	virtual void RefreshViewport() = 0;

	/** Refreshes everything in the Blast Mesh Editor. */
	virtual void RefreshTool() = 0;

	/** Updated the selected chunks */
	virtual void UpdateChunkSelection() = 0;

	/** Get selected chunk indices */
	virtual TSet<int32>& GetSelectedChunkIndices() = 0;

	/** Get chunk editor models */
	virtual TArray<FBlastChunkEditorModelPtr>& GetChunkEditorModels() = 0;

	/** Get vieport control mode */
	//virtual EBlastViewportControlMode GetVieportControlMode() const = 0;

	/** Get edited blast vector */
	//virtual TArray<FVector> GetEditedBlastVector() const = 0;

	/** Get fracture settings */
	virtual UBlastFractureSettings* GetFractureSettings() = 0;

	/** Run fracture in specified position if appropriate EBlastViewportControlMode selected. */
	//virtual void ProcessClick(FVector ClickLocation, FVector ClickNormal, int32 ClickedChunkId) = 0;

	/** Remove all children for specified chunk (works only for fractured mesh). If chunk is not specifed then process all selected chunks. */
	virtual void RemoveChildren(int32 ChunkId = INDEX_NONE) = 0;
};


