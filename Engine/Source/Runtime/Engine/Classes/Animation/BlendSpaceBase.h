// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Blend Space Base. Contains base functionality shared across all blend space objects
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "Animation/AnimationAsset.h"
#include "AnimationRuntime.h"
#include "BlendSpaceBase.generated.h"

/** Interpolation data types. */
UENUM()
enum EBlendSpaceAxis
{
	BSA_None UMETA(DisplayName = "None"),
	BSA_X UMETA(DisplayName = "Horizontal (X) Axis"),
	BSA_Y UMETA(DisplayName = "Vertical (Y) Axis"),
	BSA_Max
};

USTRUCT()
struct FInterpolationParameter
{
	GENERATED_USTRUCT_BODY()

	/** Interpolation Time for input, when it gets input, it will use this time to interpolate to target, used for smoother interpolation. */
	UPROPERTY(EditAnywhere, Category=Parameter)
	float InterpolationTime;

	/** Type of interpolation used for filtering the input value to decide how to get to target. */
	UPROPERTY(EditAnywhere, Category=Parameter)
	TEnumAsByte<EFilterInterpolationType> InterpolationType;
};

USTRUCT()
struct FBlendParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, DisplayName = "Name", Category=BlendParameter)
	FString DisplayName;

	/** Min value for this parameter. */
	UPROPERTY(EditAnywhere, DisplayName = "Minimum Axis Value", Category=BlendParameter)
	float Min;

	/** Max value for this parameter. */
	UPROPERTY(EditAnywhere, DisplayName = "Maximum Axis Value", Category=BlendParameter)
	float Max;

	/** The number of grid divisions for this parameter (axis). */
	UPROPERTY(EditAnywhere, DisplayName = "Number of Grid Divisions", Category=BlendParameter, meta=(UIMin="1", ClampMin="1"))
	int32 GridNum;

	FBlendParameter()
		: DisplayName(TEXT("None"))
		, Min(0.f)
		, Max(100.f)
		, GridNum(4) // TODO when changing GridNum's default value, it breaks all grid samples ATM - provide way to rebuild grid samples during loading
	{
	}

	float GetRange() const
	{
		return Max-Min;
	}
	/** Return size of each grid. */
	float GetGridSize() const
	{
		return GetRange()/(float)GridNum;
	}
	
};

/** Sample data */
USTRUCT()
struct FBlendSample
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=BlendSample)
	class UAnimSequence* Animation;

	//blend 0->x, blend 1->y, blend 2->z

	UPROPERTY(EditAnywhere, Category=BlendSample)
	FVector SampleValue;
	
	UPROPERTY(EditAnywhere, Category = BlendSample, meta=(UIMin="0.01", UIMax="2.0", ClampMin="0.01", ClampMax="64.0"))
	float RateScale;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	bool bIsValid;
#endif // WITH_EDITORONLY_DATA

	FBlendSample()
		: Animation(NULL)
		, SampleValue(0.f)
		, RateScale(1.0f)
#if WITH_EDITORONLY_DATA
		, bIsValid(false)		
#endif // WITH_EDITORONLY_DATA
	{		
	}
	
	FBlendSample(class UAnimSequence* InAnim, FVector InValue, bool bInIsValid) 
		: Animation(InAnim)
		, SampleValue(InValue)
		, RateScale(1.0f)
#if WITH_EDITORONLY_DATA
		, bIsValid(bInIsValid)
#endif // WITH_EDITORONLY_DATA
	{		
	}
	
	bool operator==( const FBlendSample& Other ) const 
	{
		return (Other.Animation == Animation && Other.SampleValue == SampleValue && FMath::IsNearlyEqual(Other.RateScale, RateScale));
	}
};

/**
 * Each elements in the grid
 */
USTRUCT()
struct FEditorElement
{
	GENERATED_USTRUCT_BODY()

	// for now we only support triangles
	static const int32 MAX_VERTICES = 3;

	UPROPERTY(EditAnywhere, Category=EditorElement)
	int32 Indices[MAX_VERTICES];

	UPROPERTY(EditAnywhere, Category=EditorElement)
	float Weights[MAX_VERTICES];

	FEditorElement()
	{
		for (int32 ElementIndex = 0; ElementIndex < MAX_VERTICES; ElementIndex++)
		{
			Indices[ElementIndex] = INDEX_NONE;
		}
		for (int32 ElementIndex = 0; ElementIndex < MAX_VERTICES; ElementIndex++)
		{
			Weights[ElementIndex] = 0;
		}
	}
	
};

/** result of how much weight of the grid element **/
USTRUCT()
struct FGridBlendSample
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	struct FEditorElement GridElement;

	UPROPERTY()
	float BlendWeight;

	FGridBlendSample()
		: BlendWeight(0)
	{
	}

};

USTRUCT()
struct FPerBoneInterpolation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=FPerBoneInterpolation)
	FBoneReference BoneReference;

	UPROPERTY(EditAnywhere, Category=FPerBoneInterpolation)
	float InterpolationSpeedPerSec;

	FPerBoneInterpolation()
		: InterpolationSpeedPerSec(6.f)
	{}

	void Initialize(const USkeleton* Skeleton)
	{
		BoneReference.Initialize(Skeleton);
	}
};

UENUM()
namespace ENotifyTriggerMode
{
	enum Type
	{
		AllAnimations UMETA(DisplayName="All Animations"),
		HighestWeightedAnimation UMETA(DisplayName="Highest Weighted Animation"),
		None,
	};
}

/**
 * Allows multiple animations to be blended between based on input parameters
 */
UCLASS(config=Engine, hidecategories=Object, MinimalAPI, BlueprintType)
class UBlendSpaceBase : public UAnimationAsset, public IInterpolationIndexProvider
{
	GENERATED_UCLASS_BODY()

	/** Required for accessing protected variable names */
	friend class FBlendSpaceDetails;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UAnimationAsset Interface
	virtual void TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const override;
	// this is used in editor only when used for transition getter
	// this doesn't mean max time. In Sequence, this is SequenceLength,
	// but for BlendSpace CurrentTime is normalized [0,1], so this is 1
	virtual float GetMaxCurrentTime() override { return 1.f; }	
	virtual TArray<FName>* GetUniqueMarkerNames() override { return (SampleIndexWithMarkers != INDEX_NONE && SampleData.Num() > 0) ? SampleData[SampleIndexWithMarkers].Animation->GetUniqueMarkerNames() : nullptr; }
	virtual bool IsValidAdditive() const override;
#if WITH_EDITOR
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive = true) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap) override;
	virtual int32 GetMarkerUpdateCounter() const;
#endif
	//~ End UAnimationAsset Interface
	
	// Begin IInterpolationIndexProvider Overrides
	/**
	* Get PerBoneInterpolationIndex for the input BoneIndex
	* If nothing found, return INDEX_NONE
	*/
	virtual int32 GetPerBoneInterpolationIndex(int32 BoneIndex, const FBoneContainer& RequiredBones) const override;	
	// End UBlendSpaceBase Overrides

	/** Returns whether or not the given additive animation type is compatible with the blendspace type */
	ENGINE_API virtual bool IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const;

	/**
	 * BlendSpace Get Animation Pose function
	 */
	ENGINE_API void GetAnimationPose(TArray<FBlendSampleData>& BlendSampleDataCache, /*out*/ FCompactPose& OutPose, /*out*/ FBlendedCurve& OutCurve);

	/** Accessor for blend parameter **/
	ENGINE_API const FBlendParameter& GetBlendParameter(const int32 Index) const;

	/** Get this blend spaces sample data */
	const TArray<struct FBlendSample>& GetBlendSamples() const { return SampleData; }

	/** Returns the Blend Sample at the given index, will assert on invalid indices */
	ENGINE_API const struct FBlendSample& GetBlendSample(const int32 SampleIndex) const;

	/**
	* Get Grid Samples from BlendInput
	* It will return all samples that has weight > KINDA_SMALL_NUMBER
	*
	* @param	BlendInput	BlendInput X, Y, Z corresponds to BlendParameters[0], [1], [2]
	*
	* @return	true if it has valid OutSampleDataList, false otherwise
	*/
	ENGINE_API bool GetSamplesFromBlendInput(const FVector &BlendInput, TArray<FBlendSampleData> & OutSampleDataList) const;

	/** Initialize BlendSpace for runtime. It needs certain data to be reinitialized per instsance **/
	ENGINE_API void InitializeFilter(FBlendFilter* Filter) const;
	
#if WITH_EDITOR	
	/** Validates sample data for blendspaces using the given animation sequence */
	ENGINE_API static void UpdateBlendSpacesUsingAnimSequence(UAnimSequenceBase* Sequence);

	/** Validates the contained data */
	ENGINE_API void ValidateSampleData();

	/** Add samples */
	ENGINE_API bool	AddSample(UAnimSequence* AnimationSequence, const FVector& SampleValue);

	/** edit samples */
	ENGINE_API bool	EditSampleValue(const int32 BlendSampleIndex, const FVector& NewValue);

	/** update animation on grid sample */
	ENGINE_API bool	UpdateSampleAnimation(UAnimSequence* AnimationSequence, const FVector& SampleValue);

	/** delete samples */
	ENGINE_API bool	DeleteSample(const int32 BlendSampleIndex);
	
	/** Get the number of sample points for this blend space */
	ENGINE_API int32 GetNumberOfBlendSamples()  const { return SampleData.Num(); }

	/** Check whether or not the sample index is valid in combination with the stored sample data */
	ENGINE_API bool IsValidBlendSampleIndex(const int32 SampleIndex) const;

	/**
	* return GridSamples from this BlendSpace
	*
	* @param	OutGridElements
	*
	* @return	Number of OutGridElements
	*/
	ENGINE_API const TArray<FEditorElement>& GetGridSamples() const;

	/** Fill up local GridElements from the grid elements that are created using the sorted points
	*	This will map back to original index for result
	*
	*  @param	SortedPointList		This is the pointlist that are used to create the given GridElements
	*								This list contains subsets of the points it originally requested for visualization and sorted
	*
	*/
	ENGINE_API void FillupGridElements(const TArray<int32> & PointListToSampleIndices, const TArray<FEditorElement> & GridElements);
		
	ENGINE_API void EmptyGridElements();
	
	/** Validate that the given animation sequence and contained blendspace data */
	ENGINE_API bool ValidateAnimationSequence(const UAnimSequence* AnimationSequence) const;

	/** Check if the blend spaces contains samples whos additive type match that of the animation sequence */
	ENGINE_API bool DoesAnimationMatchExistingSamples(const UAnimSequence* AnimationSequence) const;
	
	/** Check if the the blendspace contains additive samples only */	
	ENGINE_API bool ShouldAnimationBeAdditive() const;

	/** Check if the animation sequence's skeleton is compatible with this blendspace */
	ENGINE_API bool IsAnimationCompatibleWithSkeleton(const UAnimSequence* AnimationSequence) const;

	/** Check if the animation sequence additive type is compatible with this blend space */
	ENGINE_API bool IsAnimationCompatible(const UAnimSequence* AnimationSequence) const;

	/** Validates supplied blend sample against current contents of blendspace */
	ENGINE_API bool ValidateSampleValue(const FVector& SampleValue, int32 OriginalIndex = INDEX_NONE) const;

	ENGINE_API bool IsSampleWithinBounds(const FVector &SampleValue) const;

	/** Check if given sample value isn't too close to existing sample point **/
	ENGINE_API bool IsTooCloseToExistingSamplePoint(const FVector& SampleValue, int32 OriginalIndex) const;
#endif

protected:
	/**
	* Get Grid Samples from BlendInput, From Input, it will populate OutGridSamples with the closest grid points.
	*
	* @param	BlendInput			BlendInput X, Y, Z corresponds to BlendParameters[0], [1], [2]
	* @param	OutBlendSamples		Populated with the samples nearest the BlendInput
	*
	*/
	virtual void GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample, TInlineAllocator<4> > & OutBlendSamples) const {}
	/** Let derived blend space decided how to handle scaling */
	virtual EBlendSpaceAxis GetAxisToScale() const PURE_VIRTUAL(UBlendSpaceBase::GetAxisToScale, return BSA_None;);

	/** Initialize Per Bone Blend **/
	void InitializePerBoneBlend();

	void TickFollowerSamples(TArray<FBlendSampleData> &SampleDataList, const int32 HighestWeightIndex, FAnimAssetTickContext &Context, bool bResetMarkerDataOnFollowers) const;

	/** Utility function to calculate animation length from sample data list **/
	float GetAnimationLengthFromSampleData(const TArray<FBlendSampleData> & SampleDataList) const;

	/** Clamp blend input to valid point **/
	FVector ClampBlendInput(const FVector& BlendInput) const;
	
	/** Translates BlendInput to grid space */
	FVector GetNormalizedBlendInput(const FVector& BlendInput) const;

	/** Returns the grid element at Index or NULL if Index is not valid */
	const FEditorElement* GetGridSampleInternal(int32 Index) const;
	
	/** Utility function to interpolate weight of samples from OldSampleDataList to NewSampleDataList and copy back the interpolated result to FinalSampleDataList **/
	bool InterpolateWeightOfSampleData(float DeltaTime, const TArray<FBlendSampleData> & OldSampleDataList, const TArray<FBlendSampleData> & NewSampleDataList, TArray<FBlendSampleData> & FinalSampleDataList) const;

	/** Interpolate BlendInput based on Filter data **/
	FVector FilterInput(FBlendFilter* Filter, const FVector& BlendInput, float DeltaTime) const;

	/** Returns whether or not all animation set on the blend space samples match the given additive type */
	bool ContainsMatchingSamples(EAdditiveAnimationType AdditiveType) const;

	/** Checks if the given samples points overlap */
	virtual bool IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const PURE_VIRTUAL(UBlendSpaceBase::IsSameSamplePoint, return false;);	

#if WITH_EDITOR
	bool ContainsNonAdditiveSamples() const;
	void UpdatePreviewBasePose();
	/** If around border, snap to the border to avoid empty hole of data that is not valid **/
	virtual void SnapSamplesToClosestGridPoint() PURE_VIRTUAL(UBlendSpaceBase::SnapSamplesToClosestGridPoint, return;);

	virtual void RemapSamplesToNewAxisRange() PURE_VIRTUAL(UBlendSpaceBase::RemapSamplesToNewAxisRange, return;);
#endif // WITH_EDITOR
	
public:
	/**
	* When you use blend per bone, allows rotation to blend in mesh space. This only works if this does not contain additive animation samples
	* This is more performance intensive
	*/
	UPROPERTY()
	bool bRotationBlendInMeshSpace;

#if WITH_EDITORONLY_DATA
	/** Preview Base pose for additive BlendSpace **/
	UPROPERTY(EditAnywhere, Category = AdditiveSettings)
	UAnimSequence* PreviewBasePose;
#endif // WITH_EDITORONLY_DATA

	/** This animation length changes based on current input (resulting in different blend time)**/
	UPROPERTY(transient)
	float AnimLength;

	/** Input interpolation parameter for all 3 axis, for each axis input, decide how you'd like to interpolate input to*/
	UPROPERTY(EditAnywhere, Category = InputInterpolation)
	FInterpolationParameter	InterpolationParam[3];

	/**
	* Target weight interpolation. When target samples are set, how fast you'd like to get to target. Improve target blending.
	* i.e. for locomotion, if you interpolate input, when you move from left to right rapidly, you'll interpolate through forward, but if you use target weight interpolation,
	* you'll skip forward, but interpolate between left to right
	*/
	UPROPERTY(EditAnywhere, Category = SampleInterpolation)
	float TargetWeightInterpolationSpeedPerSec;

	/** The current mode used by the blendspace to decide which animation notifies to fire. Valid options are:
	- AllAnimations - All notify events will fire
	- HighestWeightedAnimation - Notify events will only fire from the highest weighted animation
	- None - No notify events will fire from any animations
	*/
	UPROPERTY(EditAnywhere, Category = AnimationNotifies)
	TEnumAsByte<ENotifyTriggerMode::Type> NotifyTriggerMode;

protected:

	/**
	* Define target weight interpolation per bone. This will blend in different speed per each bone setting
	*/
	UPROPERTY(EditAnywhere, Category = SampleInterpolation)
	TArray<FPerBoneInterpolation> PerBoneBlend;

	/** Track index to get marker data from. Samples are tested for the suitability of marker based sync
	    during load and if we can use marker based sync we cache an index to a representative sample here */
	UPROPERTY()
	int32 SampleIndexWithMarkers;

	/** Sample animation data **/
	UPROPERTY(EditAnywhere, Category=BlendSamples)
	TArray<struct FBlendSample> SampleData;

	/** Grid samples, indexing scheme imposed by subclass **/
	UPROPERTY()
	TArray<struct FEditorElement> GridSamples;
	
	/** Blend Parameters for each axis. **/
	UPROPERTY(EditAnywhere, Category = BlendParametersTest)
	struct FBlendParameter BlendParameters[3];

#if WITH_EDITOR
private:
	// Track whether we have updated markers so cached data can be updated
	int32 MarkerDataUpdateCounter;
protected:
	FVector PreviousAxisMinMaxValues[3];
#endif	
};
