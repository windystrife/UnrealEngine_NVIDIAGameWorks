// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraAnim.h"

class AMatineeActorCameraAnim;

class FAssetTypeActions_CameraAnim : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CameraAnim", "Camera Anim"); }
	virtual FColor GetTypeColor() const override { return FColor(128,0,64); }
	virtual UClass* GetSupportedClass() const override { return UCameraAnim::StaticClass(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }

private:
	/** Helper function used to create a preview matinee actor for use when editing a camera anim in Matinee */
	void CreateMatineeActorForCameraAnim(class UCameraAnim* InCameraAnim);

	/** Helper function used to create a preview camera actor for use when editing a camera anim in Matinee */
	void CreateCameraActorForCameraAnim(class UCameraAnim* InCameraAnim);

	/** Delegate fired when the editor mode is changed */
	void OnMatineeEditorClosed(class FEdMode* InEditorMode);

	/** Helper function to hookup preview pawn */
	void CreatePreviewPawnForCameraAnim(class UCameraAnim* InCameraAnim);

	/** Helper function to hookup interp group for preview pawn */
	class UInterpGroup* CreateInterpGroup(class UCameraAnim* InCameraAnim, struct FCameraPreviewInfo& PreviewInfo);

	/** Helper function to create preview pawn */
	void CreatePreviewPawn(class UCameraAnim* InCameraAnim, class UClass* PreviewPawnClass, const FVector& Location, const FRotator& Rotation);

private:
	/** The camera actor we will use for previewing the camera anim */
	TWeakObjectPtr<class ACameraActor> PreviewCamera;

	/** The matinee actor we will use for previewing the camera anim */
	TWeakObjectPtr<class AMatineeActorCameraAnim> PreviewMatineeActor;

	/** The pawn we we will use for previewing the camera anim */
	TWeakObjectPtr<class APawn> PreviewPawn;

	/** Handle to the registered OnMatineeEditorClosed delegate */
	FDelegateHandle OnMatineeEditorClosedDelegateHandle;
};
