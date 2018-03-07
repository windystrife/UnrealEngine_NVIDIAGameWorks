// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimEnums.h"
#include "Animation/AnimCurveTypes.h"

#include "AnimationBlueprintLibrary.generated.h"

struct FRawAnimSequenceTrack;
class USkeleton;

UENUM()
enum class ESmartNameContainerType : uint8
{
	SNCT_CurveMapping UMETA(DisplayName = "Curve Names"),
	SNCT_TrackCurveMapping	UMETA(DisplayName = "Track Curve Names"),
	SNCT_MAX
};

/** Blueprint library for altering and analyzing animation / skeletal data */
UCLASS()
class ANIMATIONMODIFIERS_API UAnimationBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Retrieves the number of animation frames for the given Animation Sequence */
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm = "AnimationSequence"), Category = "AnimationBlueprintLibrary|Animation")
	static void GetNumFrames(const UAnimSequence* AnimationSequence, int32& NumFrames);
	
	/** Retrieves the Names of the individual ATracks for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Animation")
	static void GetAnimationTrackNames(const UAnimSequence* AnimationSequence, TArray<FName>& TrackNames);

	/** Retrieves the Raw Translation Animation Data for the given Animation Track Name and Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RawTrackData")
	static void GetRawTrackPositionData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FVector>& PositionData);

	/** Retrieves the Raw Rotation Animation Data for the given Animation Track Name and Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RawTrackData")
	static void GetRawTrackRotationData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FQuat>& RotationData );

	/** Retrieves the Raw Scale Animation Data for the given Animation Track Name and Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RawTrackData")
	static void GetRawTrackScaleData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FVector>& ScaleData);

	/** Retrieves the Raw Animation Data for the given Animation Track Name and Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RawTrackData")
	static void GetRawTrackData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FVector>& PositionKeys,TArray<FQuat>& RotationKeys, TArray<FVector>& ScalingKeys);

	/** Checks whether or not the given Animation Track Name is contained within the Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Helpers")
	static bool IsValidRawAnimationTrackName(const UAnimSequence* AnimationSequence, const FName TrackName);

	static const FRawAnimSequenceTrack& GetRawAnimationTrackByName(const UAnimSequence* AnimationSequence, const FName TrackName);

	// Compression

	/** Retrieves the Compression Scheme for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Compression")
	static void GetCompressionScheme(const UAnimSequence* AnimationSequence, UAnimCompress*& CompressionScheme);

	/** Sets the Compression Scheme for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Compression")
	static void SetCompressionScheme(UAnimSequence* AnimationSequence, UAnimCompress* CompressionScheme);

	// Additive 
	/** Retrieves the Additive Animation type for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Additive")
	static void GetAdditiveAnimationType(const UAnimSequence* AnimationSequence, TEnumAsByte<enum EAdditiveAnimationType>& AdditiveAnimationType);

	/** Sets the Additive Animation type for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Additive")
	static void SetAdditiveAnimationType(UAnimSequence* AnimationSequence, const TEnumAsByte<enum EAdditiveAnimationType> AdditiveAnimationType);

	/** Retrieves the Additive Base Pose type for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Additive")
	static void GetAdditiveBasePoseType(const UAnimSequence* AnimationSequence, TEnumAsByte<enum EAdditiveBasePoseType>& AdditiveBasePoseType);

	/** Sets the Additive Base Pose type for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Additive")
	static void SetAdditiveBasePoseType(UAnimSequence* AnimationSequence, const TEnumAsByte<enum EAdditiveBasePoseType> AdditiveBasePoseType);

	// Interpolation

	/** Retrieves the Animation Interpolation type for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Interpolation")
	static void GetAnimationInterpolationType(const UAnimSequence* AnimationSequence, EAnimInterpolationType& InterpolationType);

	/** Sets the Animation Interpolation type for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Interpolation")
	static void SetAnimationInterpolationType(UAnimSequence* AnimationSequence, EAnimInterpolationType InterpolationType);

	// Root motion

	/** Checks whether or not Root Motion is Enabled for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RootMotion")
	static bool IsRootMotionEnabled(const UAnimSequence* AnimationSequence);

	/** Sets whether or not Root Motion is Enabled for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|RootMotion")
	static void SetRootMotionEnabled(UAnimSequence* AnimationSequence, bool bEnabled);

	/** Retrieves the Root Motion Lock Type for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RootMotion")
	static void GetRootMotionLockType(const UAnimSequence* AnimationSequence, TEnumAsByte<ERootMotionRootLock::Type>& LockType);

	/** Sets the Root Motion Lock Type for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|RootMotion")
	static void SetRootMotionLockType(UAnimSequence* AnimationSequence, TEnumAsByte<ERootMotionRootLock::Type> RootMotionLockType);

	/** Checks whether or not Root Motion locking is Forced for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|RootMotion")
	static bool IsRootMotionLockForced(const UAnimSequence* AnimationSequence);

	/** Sets whether or not Root Motion locking is Forced for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|RootMotion")
	static void SetIsRootMotionLockForced(UAnimSequence* AnimationSequence, bool bForced);

	// Markers

	/** Retrieves all the Animation Sync Markers for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static void GetAnimationSyncMarkers(const UAnimSequence* AnimationSequence, TArray<FAnimSyncMarker>& Markers);

	/** Retrieves all the Unique Names for the Animation Sync Markers contained by the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static void GetUniqueMarkerNames(const UAnimSequence* AnimationSequence, TArray<FName>& MarkerNames);

	/** Adds an Animation Sync Marker to Notify track in the given Animation with the corresponding Marker Name and Time */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static void AddAnimationSyncMarker(UAnimSequence* AnimationSequence, FName MarkerName, float Time, FName NotifyTrackName);

	/** Checks whether or not the given Marker Name is a valid Animation Sync Marker Name */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Helpers")
	static bool IsValidAnimationSyncMarkerName(const UAnimSequence* AnimationSequence, FName MarkerName);

	/** Removes All Animation Sync Marker found within the Animation Sequence whose name matches MarkerName, and returns the number of removed instances */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static int32 RemoveAnimationSyncMarkersByName(UAnimSequence* AnimationSequence, FName MarkerName);

	/** Removes All Animation Sync Marker found within the Animation Sequence that belong to the specific Notify Track, and returns the number of removed instances */	
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static int32 RemoveAnimationSyncMarkersByTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName);

	/** Removes All Animation Sync Markers found within the Animation Sequence, and returns the number of removed instances */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static void RemoveAllAnimationSyncMarkers(UAnimSequence* AnimationSequence);

	// Notifies

	/** Retrieves all Animation Notify Events found within the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static void GetAnimationNotifyEvents(const UAnimSequence* AnimationSequence, TArray<FAnimNotifyEvent>& NotifyEvents);

	/** Retrieves all Unique Animation Notify Events found within the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static void GetAnimationNotifyEventNames(const UAnimSequence* AnimationSequence, TArray<FName>& EventNames);

	/** Adds an Animation Notify Event to Notify track in the given Animation with the given Notify creation data */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static UAnimNotify* AddAnimationNotifyEvent(UAnimSequence* AnimationSequence, FName NotifyTrackName, float StartTime, float Duration, TSubclassOf<UAnimNotifyState> NotifyClass);

	/** Adds an the specific Animation Notify to the Animation Sequence (requires Notify's outer to be the Animation Sequence) */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static void AddAnimationNotifyEventObject(UAnimSequence* AnimationSequence, float StartTime, UAnimNotify* Notify, FName NotifyTrackName);

	/** Removes Animation Notify Events found by Name within the Animation Sequence, and returns the number of removed name instances */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static int32 RemoveAnimationNotifyEventsByName(UAnimSequence* AnimationSequence, FName NotifyName);

	/** Removes Animation Notify Events found by Track within the Animation Sequence, and returns the number of removed name instances */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static int32 RemoveAnimationNotifyEventsByTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName);	

	// Notify Tracks

	/** Retrieves all Unique Animation Notify Track Names found within the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|AnimationNotifies")
	static void GetAnimationNotifyTrackNames(const UAnimSequence* AnimationSequence, TArray<FName>& TrackNames);

	/** Adds an Animation Notify Track to the Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|AnimationNotifies")
	static void AddAnimationNotifyTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName, FLinearColor TrackColor = FLinearColor::White);

	/** Removes an Animation Notify Track from Animation Sequence by Name */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|AnimationNotifies")
	static void RemoveAnimationNotifyTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName);

	/** Removes All Animation Notify Tracks from Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|AnimationNotifies")
	static void RemoveAllAnimationNotifyTracks(UAnimSequence* AnimationSequence);

	/** Checks whether or not the given Track Name is a valid Animation Notify Track in the Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Helpers")
	static bool IsValidAnimNotifyTrackName(const UAnimSequence* AnimationSequence, FName NotifyTrackName);

	static int32 GetTrackIndexForAnimationNotifyTrackName(const UAnimSequence* AnimationSequence, FName NotifyTrackName);
	static const FAnimNotifyTrack& GetNotifyTrackByName(const UAnimSequence* AnimationSequence, FName NotifyTrackName);

	/** Retrieves all Animation Sync Markers for the given Notify Track Name from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|MarkerSyncing")
	static void GetAnimationSyncMarkersForTrack(const UAnimSequence* AnimationSequence, FName NotifyTrackName, TArray<FAnimSyncMarker>& Markers);

	/** Retrieves all Animation Notify Events for the given Notify Track Name from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|NotifyEvents")
	static void GetAnimationNotifyEventsForTrack(const UAnimSequence* AnimationSequence, FName NotifyTrackName, TArray<FAnimNotifyEvent>& Events);

	// Curves

	/** Adds an Animation Curve by Type and Name to the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddCurve(UAnimSequence* AnimationSequence, FName CurveName, ERawCurveTrackTypes CurveType = ERawCurveTrackTypes::RCT_Float, bool bMetaDataCurve = false);

	/** Removes an Animation Curve by Name from the given Animation Sequence (Raw Animation Curves [Names] may not be removed from the Skeleton) */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void RemoveCurve(UAnimSequence* AnimationSequence, FName CurveName, bool bRemoveNameFromSkeleton = false);

	/** Removes all Animation Curve Data from the given Animation Sequence (Raw Animation Curves [Names] may not be removed from the Skeleton) */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void RemoveAllCurveData(UAnimSequence* AnimationSequence);

	/** Adds a Transformation Key to the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddTransformationCurveKey(UAnimSequence* AnimationSequence, FName CurveName, const float Time, const FTransform& Transform);

	/** Adds a multiple of Transformation Keys to the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddTransformationCurveKeys(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<FTransform>& Transforms);

	/** Adds a Float Key to the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddFloatCurveKey(UAnimSequence* AnimationSequence, FName CurveName, const float Time, const float Value);

	/** Adds a multiple of Float Keys to the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddFloatCurveKeys(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<float>& Values);

	/** Adds a Vector Key to the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddVectorCurveKey(UAnimSequence* AnimationSequence, FName CurveName, const float Time, const FVector Vector);

	/** Adds a multiple of Vector Keys to the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void AddVectorCurveKeys(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<FVector>& Vectors);

	// Curve helper functions
	template <typename DataType, typename CurveClass>
	static void AddCurveKeysInternal(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<DataType>& KeyData, ERawCurveTrackTypes CurveType);

	// Returns true if successfully added, false if it was already existing
	static bool AddCurveInternal(UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName, int32 CurveFlags, ERawCurveTrackTypes SupportedCurveType);
	static bool RemoveCurveInternal(UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName, bool bRemoveNameFromSkeleton);

	/** Checks whether or not the given Bone Name exist on the Skeleton referenced by the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Skeleton")
	static void DoesBoneNameExist(UAnimSequence* AnimationSequence, FName BoneName, bool& bExists);

	static bool DoesBoneNameExistInternal(USkeleton* Skeleton, FName BoneName);

	/** Retrieves, a multiple of, Float Key(s) from the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void GetFloatKeys(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<float>& Values);

	/** Retrieves, a multiple of, Vector Key(s) from the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void GetVectorKeys(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<FVector>& Values);

	/** Retrieves, a multiple of, Transformation Key(s) from the specified Animation Curve inside of the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static void GetTransformationKeys(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<FTransform>& Values);

	template <typename DataType, typename CurveClass>
	static void GetCurveKeysInternal(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<DataType>& KeyData, ERawCurveTrackTypes CurveType);
	
	// Smart name helper functions

	/** Checks whether or not the given Curve Name exist on the Skeleton referenced by the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Curves")
	static bool DoesCurveExist(UAnimSequence* AnimationSequence, FName CurveName, ERawCurveTrackTypes CurveType);
	static bool DoesSmartNameExist(UAnimSequence* AnimationSequence, FName Name);
	
	static FSmartName RetrieveSmartNameForCurve(const UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName);
	static bool RetrieveSmartNameForCurve(const UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName, FSmartName& SmartName);
	static FName RetrieveContainerNameForCurve(const UAnimSequence* AnimationSequence, FName CurveName);

	// MetaData

	/** Creates and Adds an instance of the specified MetaData Class to the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MetaData")
	static void AddMetaData(UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass, UAnimMetaData*& MetaDataInstance);

	/** Adds an instance of the specified MetaData Class to the given Animation Sequence (requires MetaDataObject's outer to be the Animation Sequence) */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MetaData")
	static void AddMetaDataObject(UAnimSequence* AnimationSequence, UAnimMetaData* MetaDataObject);

	/** Removes all Meta Data from the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MetaData")
	static void RemoveAllMetaData(UAnimSequence* AnimationSequence);

	/** Removes the specified Meta Data Instance from the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MetaData")
	static void RemoveMetaData(UAnimSequence* AnimationSequence, UAnimMetaData* MetaDataObject);

	/** Removes all Meta Data Instance of the specified Class from the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|MetaData")
	static void RemoveMetaDataOfClass(UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass);

	/** Retrieves all Meta Data Instances from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|MetaData")
	static void GetMetaData(const UAnimSequence* AnimationSequence, TArray<UAnimMetaData*>& MetaData);

	/** Retrieves all Meta Data Instances from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|MetaData")
	static void GetMetaDataOfClass(const UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass, TArray<UAnimMetaData*>& MetaDataOfClass);

	/** Checks whether or not the given Animation Sequences contains Meta Data Instance of the specified Meta Data Class */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|MetaData")
	static bool ContainsMetaDataOfClass(const UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass);

	// Poses

	/** Retrieves Bone Pose data for the given Bone Name at the specified Time from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Pose")
	static void GetBonePoseForTime(const UAnimSequence* AnimationSequence, FName BoneName, float Time, bool bExtractRootMotion, FTransform& Pose);

	/** Retrieves Bone Pose data for the given Bone Name at the specified Frame from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Pose")
	static void GetBonePoseForFrame(const UAnimSequence* AnimationSequence, FName BoneName, int32 Frame, bool bExtractRootMotion, FTransform& Pose);

	/** Retrieves Bone Pose data for the given Bone Names at the specified Time from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Pose")
	static void GetBonePosesForTime(const UAnimSequence* AnimationSequence, TArray<FName> BoneNames, float Time, bool bExtractRootMotion, TArray<FTransform>& Poses);

	/** Retrieves Bone Pose data for the given Bone Names at the specified Frame from the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Pose")
	static void GetBonePosesForFrame(const UAnimSequence* AnimationSequence, TArray<FName> BoneNames, int32 Frame, bool bExtractRootMotion, TArray<FTransform>& Poses);

	// Virtual bones

	/** Adds a Virtual Bone between the Source and Target Bones to the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|VirtualBones")
	static void AddVirtualBone(const UAnimSequence* AnimationSequence, FName SourceBoneName, FName TargetBoneName, FName& VirtualBoneName);

	/** Removes a Virtual Bone with the specified Bone Name from the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|VirtualBones")
	static void RemoveVirtualBone(const UAnimSequence* AnimationSequence, FName VirtualBoneName);

	/** Removes Virtual Bones with the specified Bone Names from the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|VirtualBones")
	static void RemoveVirtualBones(const UAnimSequence* AnimationSequence, TArray<FName> VirtualBoneNames);

	/** Removes all Virtual Bones from the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|VirtualBones")
	static void RemoveAllVirtualBones(const UAnimSequence* AnimationSequence);

	static bool DoesVirtualBoneNameExistInternal(USkeleton* Skeleton, FName BoneName);

	// Misc

	/** Retrieves the Length of the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Animation")
	static void GetSequenceLength(const UAnimSequence* AnimationSequence, float& Length);

	/** Retrieves the (Play) Rate Scale of the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Animation")
	static void GetRateScale(const UAnimSequence* AnimationSequence, float& RateScale);

	/** Sets the (Play) Rate Scale for the given Animation Sequence */
	UFUNCTION(BlueprintCallable, Category = "AnimationBlueprintLibrary|Animation")
	static void SetRateScale(UAnimSequence* AnimationSequence, float RateScale);

	/** Retrieves the Frame Index at the specified Time Value for the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Helpers")
	static void GetFrameAtTime(const UAnimSequence* AnimationSequence, const float Time, int32& Frame);

	/** Retrieves the Time Value at the specified Frame Indexfor the given Animation Sequence */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Helpers")
	static void GetTimeAtFrame(const UAnimSequence* AnimationSequence, const int32 Frame, float& Time);
	
	static float GetTimeAtFrameInternal(const UAnimSequence* AnimationSequence, const int32 Frame);

	/** Checks whether or not the given Time Value lies within the given Animation Sequence's Length */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Helpers")
	static void IsValidTime(const UAnimSequence* AnimationSequence, const float Time, bool& IsValid);

	static bool IsValidTimeInternal(const UAnimSequence* AnimationSequence, const float Time);

	/** Finds the Bone Path from the given Bone to the Root Bone */
	UFUNCTION(BlueprintPure, Category = "AnimationBlueprintLibrary|Helpers")
	static void FindBonePathToRoot(const UAnimSequence* AnimationSequence, FName BoneName, TArray<FName>& BonePath);

	static const FName SmartContainerNames[(int32)ESmartNameContainerType::SNCT_MAX];
};
