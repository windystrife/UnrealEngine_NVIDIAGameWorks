// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/FbxAssetImportData.h"
#include "FbxAnimSequenceImportData.generated.h"

class UAnimSequence;
struct FPropertyChangedEvent;

/**
* I know these descriptions don't make sense, but the functions I use act a bit different depending on situation.
a) FbxAnimStack::GetLocalTimeSpan will return you the start and stop time for this stack (or take). It's simply two time values that can be set to anything regardless of animation curves. Generally speaking, applications set these to the start and stop time of the timeline.
b) As for FbxNode::GetAnimationInternval, this one will iterate through all properties recursively, and then for all animation curves it finds, for the animation layer index specified. So in other words, if one property has been animated, it will modify this result. This is completely different from GetLocalTimeSpan since it calculates the time span depending on the keys rather than just using the start and stop time that was saved in the file.
*/

/** Animation length type when importing */
UENUM()
enum EFBXAnimationLengthImportType
{
	/** This option imports animation frames based on what is defined at the time of export */
	FBXALIT_ExportedTime			UMETA(DisplayName = "Exported Time"),
	/** Will import the range of frames that have animation. Can be useful if the exported range is longer than the actual animation in the FBX file */
	FBXALIT_AnimatedKey				UMETA(DisplayName = "Animated Time"),
	/** This will enable the Start Frame and End Frame properties for you to define the frames of animation to import */
	FBXALIT_SetRange				UMETA(DisplayName = "Set Range"),

	FBXALIT_MAX,
};

/**
* Import data and options used when importing any mesh from FBX
*/
UCLASS(config = EditorPerProjectUserSettings, configdonotcheckdefaults)
class UNREALED_API UFbxAnimSequenceImportData : public UFbxAssetImportData
{
	GENERATED_UCLASS_BODY()
	
	/** If checked, meshes nested in bone hierarchies will be imported instead of being converted to bones. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = ImportSettings, meta = (ImportType = "Animation"))
	bool bImportMeshesInBoneHierarchy;
	
	/** Which animation range to import. The one defined at Exported, at Animated time or define a range manually */
	UPROPERTY(EditAnywhere, Category = ImportSettings, config, meta = (DisplayName = "Animation Length"))
	TEnumAsByte<enum EFBXAnimationLengthImportType> AnimationLength;

	/** Start frame when Set Range is used in Animation Length */
	UPROPERTY()
	int32	StartFrame_DEPRECATED;

	/** End frame when Set Range is used in Animation Length  */
	UPROPERTY()
	int32	EndFrame_DEPRECATED;

	/** Frame range used when Set Range is used in Animation Length */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = ImportSettings, meta=(UIMin=0, ClampMin=0))
	FInt32Interval FrameImportRange;

	/** Enable this option to use default sample rate for the imported animation at 30 frames per second */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings, meta = (ToolTip = "If enabled, samples all animation curves to 30 FPS"))
	bool bUseDefaultSampleRate;

	/** Name of source animation that was imported, used to reimport correct animation from the FBX file */
	UPROPERTY()
	FString SourceAnimationName;

	/** Import if custom attribute as a curve within the animation */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings)
	bool bImportCustomAttribute;

	/** Set Material Curve Type for all custom attributes that exists */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings, meta = (EditCondition = "bImportCustomAttribute", DisplayName="Set Material Curve Type"))
	bool bSetMaterialDriveParameterOnCustomAttribute;

	/** Set Material Curve Type for the custom attribute with the following suffixes. This doesn't matter if Set Material Curve Type is true  */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings, meta = (EditCondition = "bImportCustomAttribute", DisplayName = "Material Curve Suffixes"))
	TArray<FString> MaterialCurveSuffixes;

	/** When importing custom attribute as curve, remove redundant keys */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings, meta = (EditCondition = "bImportCustomAttribute", DisplayName = "Remove Redundant Keys"))
	bool bRemoveRedundantKeys;

	/** If enabled, this will delete this type of asset from the FBX */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings)
	bool bDeleteExistingMorphTargetCurves;

	/** When importing custom attribute or morphtarget as curve, do not import if it doens't have any value other than zero. This is to avoid adding extra curves to evaluate */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings, meta = (DisplayName = "Do not import curves with only 0 values"))
	bool bDoNotImportCurveWithZero;

	/** If enabled, this will import a curve within the animation */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = ImportSettings)
	bool bPreserveLocalTransform;

	/** Gets or creates fbx import data for the specified anim sequence */
	static UFbxAnimSequenceImportData* GetImportDataForAnimSequence(UAnimSequence* AnimSequence, UFbxAnimSequenceImportData* TemplateForCreation);

	virtual bool CanEditChange(const UProperty* InProperty) const override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

};
