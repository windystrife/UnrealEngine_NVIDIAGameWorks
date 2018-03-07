// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlastMeshEditor.h"
#include "SButton.h"
#include "IDetailsView.h"

//////////////////////////////////////////////////////////////////////////
// SSelectStaticMeshDialog
//////////////////////////////////////////////////////////////////////////

class SSelectStaticMeshDialog : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SSelectStaticMeshDialog) {}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	void MeshSelected();
	FReply LoadClicked();
	FReply CancelClicked();

	// Show the dialog, returns true if successfully edited fracture script
	static UStaticMesh* ShowWindow();

	void CloseContainingWindow();

	TSharedPtr<SButton> LoadButton;
	TSharedPtr<IDetailsView> MeshView;
	class UBlastStaticMeshHolder* StaticMeshHolder;
	bool IsLoad = false;
};

//////////////////////////////////////////////////////////////////////////
// SFixChunkHierarchyDialog
//////////////////////////////////////////////////////////////////////////

class SFixChunkHierarchyDialog : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SFixChunkHierarchyDialog) {}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	FReply OnClicked(bool isFix);

	// Show the dialog, returns true if successfully edited fracture script
	static bool ShowWindow(TSharedPtr<class FBlastFracture> Fracturer, UBlastFractureSettings* FractureSettings);

	void CloseContainingWindow();

	TSharedPtr<IDetailsView> PropertyView;
	class UBlastFixChunkHierarchyProperties* Properties;
	bool IsFix = false;
};

//////////////////////////////////////////////////////////////////////////
// SUVCoordinatesDialog
//////////////////////////////////////////////////////////////////////////

class SFitUvCoordinatesDialog : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SFitUvCoordinatesDialog) {}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	inline void OnSquareSizeChanged(float value)
	{
		mSquareSize = value;
	}
	inline TOptional<float> getSquareSize() const
	{
		return mSquareSize;
	}

	inline void OnIsSelectedToggleChanged(ECheckBoxState vl)
	{
		isOnlySelectedToggle = vl;
	}
	inline ECheckBoxState getIsOnlySelectedToggle() const
	{
		return isOnlySelectedToggle;
	}

	FReply OnClicked(bool isFix);

	// Show the dialog, returns true if successfully edited fracture script
	static bool ShowWindow(TSharedPtr<FBlastFracture> Fracturer, UBlastFractureSettings* FractureSettings, TSet<int32>& ChunkIndices);

	void CloseContainingWindow();

	bool shouldFix;
	float mSquareSize;
	ECheckBoxState isOnlySelectedToggle;

};

//////////////////////////////////////////////////////////////////////////
// SRebuildCollisionMeshDialog
//////////////////////////////////////////////////////////////////////////

class SRebuildCollisionMeshDialog : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SRebuildCollisionMeshDialog) {}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	FReply OnClicked(bool InIsRebuild);

	// Show the dialog, returns true if successfully edited fracture script
	static bool ShowWindow(TSharedPtr<class FBlastFracture> Fracturer, UBlastFractureSettings* FractureSettings, TSet<int32>& ChunkIndices);

	void CloseContainingWindow();

	TSharedPtr<IDetailsView> PropertyView;
	class UBlastRebuildCollisionMeshProperties* Properties;
	bool IsRebuild = false;
};


////////////////////////////////////////////////////////////////////////////
//// SUVCoordinatesDialog
////////////////////////////////////////////////////////////////////////////

class SExportAssetToFileDialog : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SExportAssetToFileDialog) {}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	FReply OnClicked(bool isFix);

	// Show the dialog, returns true if successfully edited fracture script
	static bool ShowWindow(TSharedPtr<FBlastFracture> Fracturer, UBlastFractureSettings* FractureSettings);

	void CloseContainingWindow();
};