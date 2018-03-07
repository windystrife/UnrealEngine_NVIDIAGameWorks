// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMeshEdit.cpp: Unreal editor skeletal mesh/anim support
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Object.h"
#include "Misc/PackageName.h"
#include "Animation/AnimTypes.h"
#include "Animation/SmartName.h"
#include "Animation/Skeleton.h"
#include "Curves/KeyHandle.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimSequence.h"
#include "Editor/EditorEngine.h"
#include "Factories/Factory.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxImportUI.h"
#include "SkelImport.h"
#include "ObjectTools.h"
#include "AssetRegistryModule.h"
#include "Logging/TokenizedMessage.h"
#include "FbxImporter.h"
#include "Misc/FbxErrors.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "Components/SkeletalMeshComponent.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshEdit"

UAnimSequence * UEditorEngine::ImportFbxAnimation( USkeleton* Skeleton, UObject* Outer, UFbxAnimSequenceImportData* TemplateImportData, const TCHAR* InFilename, const TCHAR* AnimName, bool bImportMorphTracks )
{
	check(Skeleton);

	UAnimSequence * NewAnimation=NULL;

	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();

	const bool bPrevImportMorph = FFbxImporter->ImportOptions->bImportMorph;
	FFbxImporter->ImportOptions->bImportMorph = bImportMorphTracks;
	if ( !FFbxImporter->ImportFromFile( InFilename, FPaths::GetExtension( InFilename ), true ) )
	{
		// Log the error message and fail the import.
		FFbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Error);
	}
	else
	{
		// Log the import message and import the mesh.
		FFbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Warning);

		const FString Filename( InFilename );

		// Get Mesh nodes array that bind to the skeleton system, then morph animation is imported.
		TArray<FbxNode*> FBXMeshNodeArray;
		FbxNode* SkeletonRoot = FFbxImporter->FindFBXMeshesByBone(Skeleton->GetReferenceSkeleton().GetBoneName(0), true, FBXMeshNodeArray);

		if (!SkeletonRoot)
		{
			FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_CouldNotFindFbxTrack", "Mesh contains {0} bone as root but animation doesn't contain the root track.\nImport failed."), FText::FromName(Skeleton->GetReferenceSkeleton().GetBoneName(0)))), FFbxErrors::Animation_CouldNotFindRootTrack);

			FFbxImporter->ReleaseScene();
			return NULL;
		}

		// Check for blend shape curves that are not skinned.  Unskinned geometry can still contain morph curves
		if( bImportMorphTracks )
		{
			TArray<FbxNode*> MeshNodes;
			FFbxImporter->FillFbxMeshArray( FFbxImporter->Scene->GetRootNode(), MeshNodes, FFbxImporter );

			for( int32 NodeIndex = 0; NodeIndex < MeshNodes.Num(); ++NodeIndex )
			{
				// Its possible the nodes already exist so make sure they are only added once
				FBXMeshNodeArray.AddUnique( MeshNodes[NodeIndex] );
			}
		}

		TArray<FbxNode*> SortedLinks;
		FFbxImporter->RecursiveBuildSkeleton(SkeletonRoot, SortedLinks);

		if(SortedLinks.Num() == 0)
		{
			FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Error_CouldNotBuildValidSkeleton", "Could not create a valid skeleton from the import data that matches the given Skeletal Mesh.  Check the bone names of both the Skeletal Mesh for this AnimSet and the animation data you are trying to import.")), 
				FFbxErrors::Animation_CouldNotBuildSkeleton);
		}
		else
		{
			NewAnimation = FFbxImporter->ImportAnimations( Skeleton, Outer, SortedLinks, AnimName, TemplateImportData, FBXMeshNodeArray);

			if( NewAnimation )
			{
				// since to know full path, reimport will need to do same
				UFbxAnimSequenceImportData* ImportData = UFbxAnimSequenceImportData::GetImportDataForAnimSequence(NewAnimation, TemplateImportData);
				ImportData->Update(UFactory::GetCurrentFilename(), &(FFbxImporter->Md5Hash));
			}
		}
	}

	FFbxImporter->ImportOptions->bImportMorph = bPrevImportMorph;
	FFbxImporter->ReleaseScene();

	return NewAnimation;
}

bool UEditorEngine::ReimportFbxAnimation( USkeleton* Skeleton, UAnimSequence* AnimSequence, UFbxAnimSequenceImportData* ImportData, const TCHAR* InFilename)
{
	check(Skeleton);
	bool bResult = true;
	GWarn->BeginSlowTask( LOCTEXT("ImportingFbxAnimations", "Importing FBX animations"), true );

	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();
	
	const bool bPrevImportMorph = (AnimSequence->RawCurveData.FloatCurves.Num() > 0) ;

	UFbxImportUI* ReimportUI = NewObject<UFbxImportUI>();
	ReimportUI->MeshTypeToImport = FBXIT_Animation;
	ReimportUI->bOverrideFullName = false;
	ReimportUI->bImportAnimations = true;

	const bool ShowImportDialogAtReimport = GetDefault<UEditorPerProjectUserSettings>()->bShowImportDialogAtReimport && !GIsAutomationTesting;
	if (ImportData && !ShowImportDialogAtReimport)
	{
		// Prepare the import options
		ReimportUI->AnimSequenceImportData = ImportData;
		ReimportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy = ImportData->bImportMeshesInBoneHierarchy;
		
		ApplyImportUIToImportOptions(ReimportUI, *FbxImporter->ImportOptions);
	}
	else if(ShowImportDialogAtReimport)
	{
		if (ImportData == nullptr)
		{
			// An existing import data object was not found, make one here and show the options dialog
			ImportData = UFbxAnimSequenceImportData::GetImportDataForAnimSequence(AnimSequence, ReimportUI->AnimSequenceImportData);
			AnimSequence->AssetImportData = ImportData;
		}
		ReimportUI->bIsReimport = true;
		ReimportUI->AnimSequenceImportData = ImportData;

		bool bImportOperationCanceled = false;
		bool bShowOptionDialog = true;
		bool bForceImportType = true;
		bool bOutImportAll = false;
		bool bIsObjFormat = false;
		bool bIsAutomated = false;

		// @hack to make sure skeleton is set before opening the dialog
		FbxImporter->ImportOptions->SkeletonForAnimation = Skeleton;

		GetImportOptions(FbxImporter, ReimportUI, bShowOptionDialog, bIsAutomated, AnimSequence->GetPathName(), bImportOperationCanceled, bOutImportAll, bIsObjFormat, bForceImportType, FBXIT_Animation, AnimSequence);

		if (bImportOperationCanceled)
		{
			//User cancel the re-import
			bResult = false;
			GWarn->EndSlowTask();
			return bResult;
		}
	}
	else
	{
		FbxImporter->ImportOptions->ResetForReimportAnimation();	
	}

	if ( !FbxImporter->ImportFromFile( InFilename, FPaths::GetExtension( InFilename ), true ) )
	{
		// Log the error message and fail the import.
		FbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Error);
		bResult = false;
	}
	else
	{
		// Log the import message and import the mesh.
		FbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Warning);


		const FString Filename( InFilename );

		// Get Mesh nodes array that bind to the skeleton system, then morph animation is imported.
		TArray<FbxNode*> FBXMeshNodeArray;
		FbxNode* SkeletonRoot = FbxImporter->FindFBXMeshesByBone(Skeleton->GetReferenceSkeleton().GetBoneName(0), true, FBXMeshNodeArray);

		if (!SkeletonRoot)
		{
			FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_CouldNotFindFbxTrack", "Mesh contains {0} bone as root but animation doesn't contain the root track.\nImport failed."), FText::FromName(Skeleton->GetReferenceSkeleton().GetBoneName(0)))), FFbxErrors::Animation_CouldNotFindTrack);
			bResult = false;
		}

		if (bResult)
		{
			// for now import all the time?
			bool bImportMorphTracks = true;
			// Check for blend shape curves that are not skinned.  Unskinned geometry can still contain morph curves
			if (bImportMorphTracks)
			{
				TArray<FbxNode*> MeshNodes;
				FbxImporter->FillFbxMeshArray(FbxImporter->Scene->GetRootNode(), MeshNodes, FbxImporter);

				for (int32 NodeIndex = 0; NodeIndex < MeshNodes.Num(); ++NodeIndex)
				{
					// Its possible the nodes already exist so make sure they are only added once
					FBXMeshNodeArray.AddUnique(MeshNodes[NodeIndex]);
				}
			}

			TArray<FbxNode*> SortedLinks;
			FbxImporter->RecursiveBuildSkeleton(SkeletonRoot, SortedLinks);

			if (SortedLinks.Num() == 0)
			{
				FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Error_CouldNotBuildValidSkeleton", "Could not create a valid skeleton from the import data that matches the given Skeletal Mesh.  Check the bone names of both the Skeletal Mesh for this AnimSet and the animation data you are trying to import.")), FFbxErrors::Animation_CouldNotBuildSkeleton);
			}
			else
			{
				check(ImportData);

				// find the correct animation based on import data
				FbxAnimStack* CurAnimStack = nullptr;

				//ignore the source animation name if there's only one animation in the file.
				//this is to make it easier for people who use content creation programs that only export one animation and/or ones that don't allow naming animations			
				if (FbxImporter->Scene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId)) > 1 && !ImportData->SourceAnimationName.IsEmpty())
				{
					CurAnimStack = FbxCast<FbxAnimStack>(FbxImporter->Scene->FindSrcObject(FbxCriteria::ObjectType(FbxAnimStack::ClassId), TCHAR_TO_UTF8(*ImportData->SourceAnimationName), 0));
				}
				else
				{
					CurAnimStack = FbxCast<FbxAnimStack>(FbxImporter->Scene->GetSrcObject(FbxCriteria::ObjectType(FbxAnimStack::ClassId), 0));
				}

				if (CurAnimStack)
				{
					// set current anim stack
					int32 ResampleRate = DEFAULT_SAMPLERATE;
					if (FbxImporter->ImportOptions->bResample)
					{
						ResampleRate = FbxImporter->GetMaxSampleRate(SortedLinks, FBXMeshNodeArray);
					}
					FbxTimeSpan AnimTimeSpan = FbxImporter->GetAnimationTimeSpan(SortedLinks[0], CurAnimStack, ResampleRate);
					// for now it's not importing morph - in the future, this should be optional or saved with asset
					if (FbxImporter->ValidateAnimStack(SortedLinks, FBXMeshNodeArray, CurAnimStack, ResampleRate, bImportMorphTracks, AnimTimeSpan))
					{
						FbxImporter->ImportAnimation(Skeleton, AnimSequence, Filename, SortedLinks, FBXMeshNodeArray, CurAnimStack, ResampleRate, AnimTimeSpan);
					}
				}
				else
				{
					// no track is found

					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("Error_CouldNotFindTrack", "Could not find needed track.")), FFbxErrors::Animation_CouldNotFindTrack);

					bResult = false;
				}
			}
		}
	}

	FbxImporter->ImportOptions->bImportMorph = bPrevImportMorph;
	FbxImporter->ReleaseScene();
	GWarn->EndSlowTask();

	return bResult;
}

// The Unroll filter expects only rotation curves, we need to walk the scene and extract the
// rotation curves from the nodes property. This can become time consuming but we have no choice.
static void ApplyUnroll(FbxNode *pNode, FbxAnimLayer* pLayer, FbxAnimCurveFilterUnroll* pUnrollFilter)
{
	if (!pNode || !pLayer || !pUnrollFilter)
	{
		return;
	}

	FbxAnimCurveNode* lCN = pNode->LclRotation.GetCurveNode(pLayer);
	if (lCN)
	{
		FbxAnimCurve* lRCurve[3];
		lRCurve[0] = lCN->GetCurve(0);
		lRCurve[1] = lCN->GetCurve(1);
		lRCurve[2] = lCN->GetCurve(2);


		// Set bone rotation order
		EFbxRotationOrder RotationOrder = eEulerXYZ;
		pNode->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);
		pUnrollFilter->SetRotationOrder((FbxEuler::EOrder)(RotationOrder));

		pUnrollFilter->Apply(lRCurve, 3);
	}

	for (int32 i = 0; i < pNode->GetChildCount(); i++)
	{
		ApplyUnroll(pNode->GetChild(i), pLayer, pUnrollFilter);
	}
}

void UnFbx::FFbxImporter::MergeAllLayerAnimation(FbxAnimStack* AnimStack, int32 ResampleRate)
{
	FbxTime lFramePeriod;
	lFramePeriod.SetSecondDouble(1.0 / ResampleRate);

	FbxTimeSpan lTimeSpan = AnimStack->GetLocalTimeSpan();
	AnimStack->BakeLayers(Scene->GetAnimationEvaluator(), lTimeSpan.GetStart(), lTimeSpan.GetStop(), lFramePeriod);

	// always apply unroll filter
	FbxAnimCurveFilterUnroll UnrollFilter;

	FbxAnimLayer* lLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	UnrollFilter.Reset();
	ApplyUnroll(Scene->GetRootNode(), lLayer, &UnrollFilter);
}

bool UnFbx::FFbxImporter::IsValidAnimationData(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, int32& ValidTakeCount)
{
	// If there are no valid links, then we cannot import the anim set
	if(SortedLinks.Num() == 0)
	{
		return false;
	}

	ValidTakeCount = 0;

	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();

	int32 AnimStackIndex;
	for (AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++ )
	{
		FbxAnimStack* CurAnimStack = Scene->GetSrcObject<FbxAnimStack>(AnimStackIndex);
		// set current anim stack
		Scene->SetCurrentAnimationStack(CurAnimStack);

		// debug purpose
		for (int32 BoneIndex = 0; BoneIndex < SortedLinks.Num(); BoneIndex++)
		{
			FString BoneName = UTF8_TO_TCHAR(MakeName(SortedLinks[BoneIndex]->GetName()));
			UE_LOG(LogFbx, Log, TEXT("SortedLinks :(%d) %s"), BoneIndex, *BoneName );
		}

		//@note: the reason we give default sample rate is because we just want to make sure it has duration
		// we don't want to accept input of [20, 20], but the sample rate should be recalculated after this verification
		// and proper timeline will be calculated
		FbxTimeSpan AnimTimeSpan = GetAnimationTimeSpan(SortedLinks[0], CurAnimStack, DEFAULT_SAMPLERATE);
		if (AnimTimeSpan.GetDuration() <= 0)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FBXImport_ZeroLength", "Animation Stack {0} does not contain any valid key. Try different time options when import."), FText::FromString(UTF8_TO_TCHAR(CurAnimStack->GetName())))), FFbxErrors::Animation_ZeroLength);
			continue;
		}

		ValidTakeCount++;
		{
			bool bBlendCurveFound = false;

			for ( int32 NodeIndex = 0; !bBlendCurveFound && NodeIndex < NodeArray.Num(); NodeIndex++ )
			{
				// consider blendshape animation curve
				FbxGeometry* Geometry = (FbxGeometry*)NodeArray[NodeIndex]->GetNodeAttribute();
				if (Geometry)
				{
					int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);
					for(int32 BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
					{
						FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);

						int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
						for(int32 ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
						{
							FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

							if(Channel)
							{
								// Get the percentage of influence of the shape.
								FbxAnimCurve* Curve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)CurAnimStack->GetMember(0));
								if (Curve && Curve->KeyGetCount() > 0)
								{
									bBlendCurveFound = true;
									break;
								}
							}
						}
					}
				}
			}
		}
	}		

	return ( ValidTakeCount != 0 );
}

void UnFbx::FFbxImporter::FillAndVerifyBoneNames(USkeleton* Skeleton, TArray<FbxNode*>& SortedLinks, TArray<FName>& OutRawBoneNames, FString Filename)
{
	int32 TrackNum = SortedLinks.Num();

	OutRawBoneNames.AddUninitialized(TrackNum);
	// copy to the data
	for (int32 BoneIndex = 0; BoneIndex < TrackNum; BoneIndex++)
	{
		OutRawBoneNames[BoneIndex] = FName(*FSkeletalMeshImportData::FixupBoneName( UTF8_TO_TCHAR(MakeName(SortedLinks[BoneIndex]->GetName())) ));
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	// make sure at least root bone matches
	if ( OutRawBoneNames[0] != RefSkeleton.GetBoneName(0) )
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FBXImport_RootMatchFail", "Root bone name does not match (FBX: {0} | Skeleton: {1})"), FText::FromName(OutRawBoneNames[0]), FText::FromName(RefSkeleton.GetBoneName(0)))), FFbxErrors::Animation_RootTrackMismatch);

		return;
	}

	// ensure there are no duplicated names
	for (int32 I = 0; I < TrackNum; I++)
	{
		for ( int32 J = I+1; J < TrackNum; J++ )
		{
			if (OutRawBoneNames[I] == OutRawBoneNames[J])
			{
				FString RawBoneName = OutRawBoneNames[J].ToString();
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FBXImport_DupeBone", "Could not import {0}.\nDuplicate bone name found ('{1}'). Each bone must have a unique name."), FText::FromString(Filename), FText::FromString(RawBoneName))), FFbxErrors::Animation_DuplicatedBone);
			}
		}
	}

	// make sure all bone names are included, if not warn user
	FString BoneNames;
	for (int32 I = 0; I < TrackNum; ++I)
	{
		FName RawBoneName = OutRawBoneNames[I];
		if ( RefSkeleton.FindBoneIndex(RawBoneName) == INDEX_NONE)
		{
			BoneNames += RawBoneName.ToString();
			BoneNames += TEXT("  \n");
		}
	}

	if (BoneNames.IsEmpty() == false)
	{
		// warn user
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FBXImport_MissingBone", "The following bones exist in the imported animation, but not in the Skeleton asset {0}.  Any animation on these bones will not be imported: \n\n {1}"), FText::FromString(Skeleton->GetName()), FText::FromString(BoneNames) )), FFbxErrors::Animation_MissingBones);
	}
}
//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

FbxTimeSpan UnFbx::FFbxImporter::GetAnimationTimeSpan(FbxNode* RootNode, FbxAnimStack* AnimStack, int32 ResampleRate)
{
	FBXImportOptions* ImportOption = GetImportOptions();
	FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
	if (ImportOption)
	{
		bool bUseDefault = ImportOption->AnimationLengthImportType == FBXALIT_ExportedTime || ResampleRate == 0;
		if  (bUseDefault)
		{
			AnimTimeSpan = AnimStack->GetLocalTimeSpan();
		}
		else if (ImportOption->AnimationLengthImportType == FBXALIT_AnimatedKey)
		{
			RootNode->GetAnimationInterval(AnimTimeSpan, AnimStack);
		}
		else // then it's range 
		{
			AnimTimeSpan = AnimStack->GetLocalTimeSpan();

			FbxTimeSpan AnimatedInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
			RootNode->GetAnimationInterval(AnimatedInterval, AnimStack);

			// find the most range that covers by both method, that'll be used for clamping
			FbxTime StartTime = FMath::Min<FbxTime>(AnimTimeSpan.GetStart(), AnimatedInterval.GetStart());
			FbxTime StopTime = FMath::Max<FbxTime>(AnimTimeSpan.GetStop(),AnimatedInterval.GetStop());

			// make inclusive time between localtimespan and animation interval
			AnimTimeSpan.SetStart(StartTime);
			AnimTimeSpan.SetStop(StopTime);

			FbxTime EachFrame = FBXSDK_TIME_ONE_SECOND/ResampleRate;
			int32 StartFrame = StartTime.Get()/EachFrame.Get();
			int32 StopFrame = StopTime.Get()/EachFrame.Get();
			if (StartFrame != StopFrame)
			{
				FbxTime Duration = AnimTimeSpan.GetDuration();

				ImportOption->AnimationRange.X = FMath::Clamp<int32>(ImportOption->AnimationRange.X, StartFrame, StopFrame);
				ImportOption->AnimationRange.Y = FMath::Clamp<int32>(ImportOption->AnimationRange.Y, StartFrame, StopFrame);

				FbxLongLong Interval = EachFrame.Get();

				// now set new time
				if (StartFrame != ImportOption->AnimationRange.X)
				{
					FbxTime NewTime(ImportOption->AnimationRange.X*Interval);
					AnimTimeSpan.SetStart(NewTime);
				}

				if (StopFrame != ImportOption->AnimationRange.Y)
				{
					FbxTime NewTime(ImportOption->AnimationRange.Y*Interval);
					AnimTimeSpan.SetStop(NewTime);
				}
			}
		}
	}

	return AnimTimeSpan;
}
/**
* Add to the animation set, the animations contained within the FBX document, for the given skeleton
*/
UAnimSequence * UnFbx::FFbxImporter::ImportAnimations(USkeleton* Skeleton, UObject* Outer, TArray<FbxNode*>& SortedLinks, const FString& Name, UFbxAnimSequenceImportData* TemplateImportData, TArray<FbxNode*>& NodeArray)
{
	// we need skeleton to create animsequence
	if (Skeleton == NULL)
	{
		return NULL;
	}

	int32 ValidTakeCount = 0;
	if (IsValidAnimationData(SortedLinks, NodeArray, ValidTakeCount) == false)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FBXImport_InvalidAnimationData", "This does not contain any valid animation takes.")), FFbxErrors::Animation_InvalidData);
		return NULL;
	}

	UAnimSequence* LastCreatedAnim = NULL;

	int32 ResampleRate = DEFAULT_SAMPLERATE;
	if ( ImportOptions->bResample )
	{
		// For FBX data, "Frame Rate" is just the speed at which the animation is played back.  It can change
		// arbitrarily, and the underlying data can stay the same.  What we really want here is the Sampling Rate,
		// ie: the number of animation keys per second.  These are the individual animation curve keys
		// on the FBX nodes of the skeleton.  So we loop through the nodes of the skeleton and find the maximum number 
		// of keys that any node has, then divide this by the total length (in seconds) of the animation to find the 
		// sampling rate of this set of data 

		// we want the maximum resample rate, so that we don't lose any precision of fast anims,
		// and don't mind creating lerped frames for slow anims
		int32 MaxStackResampleRate = GetMaxSampleRate(SortedLinks, NodeArray);

		if(MaxStackResampleRate != 0)
		{
			ResampleRate = MaxStackResampleRate;
		}

	}

	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	for( int32 AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++ )
	{
		FbxAnimStack* CurAnimStack = Scene->GetSrcObject<FbxAnimStack>(AnimStackIndex);

		FbxTimeSpan AnimTimeSpan = GetAnimationTimeSpan(SortedLinks[0], CurAnimStack, ResampleRate);
		bool bValidAnimStack = ValidateAnimStack(SortedLinks, NodeArray, CurAnimStack, ResampleRate, ImportOptions->bImportMorph, AnimTimeSpan);
		// no animation
		if (!bValidAnimStack)
		{
			continue;
		}
		
		FString SequenceName = Name;
		FString SourceAnimationName = UTF8_TO_TCHAR(CurAnimStack->GetName());
		if (ValidTakeCount > 1)
		{
			SequenceName += "_";
			SequenceName += SourceAnimationName;
		}

		// See if this sequence already exists.
		SequenceName = ObjectTools::SanitizeObjectName(SequenceName);

		FString 	ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*Outer->GetName()), *SequenceName);
		UObject* 	ParentPackage = CreatePackage(NULL, *ParentPath);
		UObject* Object = LoadObject<UObject>(ParentPackage, *SequenceName, NULL, (LOAD_Quiet | LOAD_NoWarn), NULL);
		UAnimSequence * DestSeq = Cast<UAnimSequence>(Object);
		// if object with same name exists, warn user
		if (Object && !DestSeq)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("Error_AssetExist", "Asset with same name exists. Can't overwrite another asset")), FFbxErrors::Generic_SameNameAssetExists);
			continue; // Move on to next sequence...
		}

		// If not, create new one now.
		if(!DestSeq)
		{
			DestSeq = NewObject<UAnimSequence>(ParentPackage, *SequenceName, RF_Public | RF_Standalone);
	
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(DestSeq);
		}
		else
		{
			DestSeq->CleanAnimSequenceForImport();
		}

		DestSeq->SetSkeleton(Skeleton);

		// since to know full path, reimport will need to do same
		UFbxAnimSequenceImportData* ImportData = UFbxAnimSequenceImportData::GetImportDataForAnimSequence(DestSeq, TemplateImportData);
		ImportData->Update(UFactory::GetCurrentFilename(), &Md5Hash);
		ImportData->SourceAnimationName = SourceAnimationName;

		ImportAnimation(Skeleton, DestSeq, Name, SortedLinks, NodeArray, CurAnimStack, ResampleRate, AnimTimeSpan);

		LastCreatedAnim = DestSeq;
	}

	return LastCreatedAnim;
}

//Get the smallest sample rate(integer) representing the DeltaTime(time between 0.0f and 1.0f).
//@DeltaTime: the time to find the rate between 0.0f and 1.0f
//@MaxReferenceRate: the maximum rate we can find
int32 GetTimeSampleRate(const float DeltaTime, const float MaxReferenceRate)
{
	float OriginalSampleRateDivider = 1.0f / DeltaTime;
	float SampleRateDivider = OriginalSampleRateDivider;
	float SampleRemainder = FPlatformMath::Fractional(SampleRateDivider);
	float Multiplier = 2.0f;
	float IntegerPrecision = FMath::Min(FMath::Max(KINDA_SMALL_NUMBER*SampleRateDivider, KINDA_SMALL_NUMBER), 0.1f); //The precision is limit between KINDA_SMALL_NUMBER and 0.1f
	while (!FMath::IsNearlyZero(SampleRemainder, IntegerPrecision) && !FMath::IsNearlyEqual(SampleRemainder, 1.0f, IntegerPrecision))
	{
		SampleRateDivider = OriginalSampleRateDivider * Multiplier;
		SampleRemainder = FPlatformMath::Fractional(SampleRateDivider);
		if (SampleRateDivider > MaxReferenceRate)
		{
			SampleRateDivider = DEFAULT_SAMPLERATE;
			break;
		}
		Multiplier += 1.0f;
	}
	return FMath::Min(FPlatformMath::RoundToInt(SampleRateDivider), FPlatformMath::RoundToInt(MaxReferenceRate));
}

int32 GetAnimationCurveRate(FbxAnimCurve* CurrentCurve, float MaxReferenceRate)
{
	if (CurrentCurve == nullptr)
		return 0;

	int32 KeyCount = CurrentCurve->KeyGetCount();
	
	FbxTimeSpan TimeInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
	bool bValidTimeInterval = CurrentCurve->GetTimeInterval(TimeInterval);
	if (KeyCount > 1 && bValidTimeInterval)
	{
		double KeyAnimLength = TimeInterval.GetDuration().GetSecondDouble();
		if (KeyAnimLength != 0.0)
		{
			//////////////////////////////////////////////////////////////////////////
			// 1. Look if we have high frequency keys(resampling).

			//Basic sample rate is compute by dividing the KeyCount by the anim length. This is valid only if
			//all keys are time equidistant. But if we find a rate over DEFAULT_SAMPLERATE, we can estimate that
			//there is a constant frame rate between the key and simply return the rate.
			int32 SampleRate = FPlatformMath::RoundToInt((KeyCount - 1) / KeyAnimLength);
			if (SampleRate >= DEFAULT_SAMPLERATE)
			{
				//We import a curve with more then 30 keys per frame
				return SampleRate;
			}
			
			//////////////////////////////////////////////////////////////////////////
			// 2. Compute the sample rate of every keys with there time. Use the
			//    least common multiplier to get a sample rate that go through all keys.

			SampleRate = 1;
			float OldKeyTime = 0.0f;
			TSet<int32> DeltaComputed;
			//Reserve some space
			DeltaComputed.Reserve(30);
			const float KeyMultiplier = (1.0f / KINDA_SMALL_NUMBER);
			//Find also the smallest delta time between keys
			for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
			{
				float KeyTime = (float)(CurrentCurve->KeyGet(KeyIndex).GetTime().GetSecondDouble());
				//Collect the smallest delta time
				float Delta = KeyTime - OldKeyTime;
				//use the fractional part of the delta to have the delta between 0.0f and 1.0f
				Delta = FPlatformMath::Fractional(Delta);
				int32 DeltaKey = FPlatformMath::RoundToInt(Delta*KeyMultiplier);
				if (!FMath::IsNearlyZero(Delta, KINDA_SMALL_NUMBER) && !DeltaComputed.Contains(DeltaKey))
				{
					int32 ComputeSampleRate = GetTimeSampleRate(Delta, MaxReferenceRate);
					DeltaComputed.Add(DeltaKey);
					//Use the least common multiplier with the new delta entry
					int32 LeastCommonMultiplier = FMath::Min(FMath::LeastCommonMultiplier(SampleRate, ComputeSampleRate), FPlatformMath::RoundToInt(MaxReferenceRate));
					SampleRate = LeastCommonMultiplier != 0 ? LeastCommonMultiplier : FMath::Max3(FPlatformMath::RoundToInt(DEFAULT_SAMPLERATE), SampleRate, ComputeSampleRate);
				}
				OldKeyTime = KeyTime;
			}
			return SampleRate;
		}
	}

	return 0;
}

int32 UnFbx::FFbxImporter::GetMaxSampleRate(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray)
{
	//The max reference rate is use to cap the maximum rate we support.
	//It must be base on DEFAULT_SAMPLERATE*2ExpX where X is a integer with range [1 to 6] because we use KINDA_SMALL_NUMBER(0.0001) we do not want to pass 1920Hz 1/1920 = 0.0005
	float MaxReferenceRate = 1920.0f;
	int32 MaxStackResampleRate = 0;
	TArray<int32> CurveAnimSampleRates;
	const FBXImportOptions* ImportOption = GetImportOptions();
	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	for( int32 AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++)
	{
		FbxAnimStack* CurAnimStack = Scene->GetSrcObject<FbxAnimStack>(AnimStackIndex);

		// @note: here we iterate through all timeline to figure out sample rate, not just in range
		// we have chicken/egg problem if we don't. We need samplerate to figure out time range for the (start, end)
		// so when you get time range for the sample rate, we just walk through all range
		FbxTimeSpan AnimStackTimeSpan = GetAnimationTimeSpan(SortedLinks[0], CurAnimStack, 0);

		double AnimStackStart = AnimStackTimeSpan.GetStart().GetSecondDouble();
		double AnimStackStop = AnimStackTimeSpan.GetStop().GetSecondDouble();

		FbxAnimLayer* AnimLayer = (FbxAnimLayer*)CurAnimStack->GetMember(0);
		for(int32 LinkIndex = 0; LinkIndex < SortedLinks.Num(); ++LinkIndex)
		{
			FbxNode* CurrentLink = SortedLinks[LinkIndex];

			const int32 MaxElement = 9;
			FbxAnimCurve* Curves[MaxElement];

			Curves[0] = CurrentLink->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false);
			Curves[1] = CurrentLink->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false);
			Curves[2] = CurrentLink->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false);
			Curves[3] = CurrentLink->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false);
			Curves[4] = CurrentLink->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false);
			Curves[5] = CurrentLink->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false);
			Curves[6] = CurrentLink->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false);
			Curves[7] = CurrentLink->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false);
			Curves[8] = CurrentLink->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false);

			
			for(int32 CurveIndex = 0; CurveIndex < MaxElement; ++CurveIndex)
			{
				FbxAnimCurve* CurrentCurve = Curves[CurveIndex];
				if(CurrentCurve)
				{
					int32 CurveAnimRate = GetAnimationCurveRate(CurrentCurve, MaxReferenceRate);
					if (CurveAnimRate != 0)
					{
						CurveAnimSampleRates.AddUnique(CurveAnimRate);
					}
				}
			}
		}

		// it doens't matter whether you choose to import morphtarget or not
		// blendshape are always imported. Import morphtarget is only used for morphtarget for mesh
		{
			for (int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); NodeIndex++)
			{
				// consider blendshape animation curve
				FbxGeometry* Geometry = (FbxGeometry*)NodeArray[NodeIndex]->GetNodeAttribute();
				if (Geometry)
				{
					int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);
					for (int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeDeformerCount; ++BlendShapeIndex)
					{
						FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);

						int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
						for (int32 ChannelIndex = 0; ChannelIndex < BlendShapeChannelCount; ++ChannelIndex)
						{
							FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

							if (Channel)
							{
								FbxAnimCurve* CurrentCurve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, AnimLayer);
								if (CurrentCurve)
								{
									int32 CurveAnimRate = GetAnimationCurveRate(CurrentCurve, MaxReferenceRate);
									if (CurveAnimRate != 0)
									{
										CurveAnimSampleRates.AddUnique(CurveAnimRate);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	MaxStackResampleRate = CurveAnimSampleRates.Num() > 0 ? 1 : MaxStackResampleRate;
	//Find the lowest sample rate that will pass by all the keys from all curves
	for (int32 CurveSampleRate : CurveAnimSampleRates)
	{
		if (CurveSampleRate >= MaxReferenceRate && MaxStackResampleRate < CurveSampleRate)
		{
			MaxStackResampleRate = CurveSampleRate;
		}
		else if (MaxStackResampleRate < MaxReferenceRate)
		{
			int32 LeastCommonMultiplier = FMath::LeastCommonMultiplier(MaxStackResampleRate, CurveSampleRate);
			MaxStackResampleRate = LeastCommonMultiplier != 0 ? LeastCommonMultiplier : FMath::Max3(FPlatformMath::RoundToInt(DEFAULT_SAMPLERATE), MaxStackResampleRate, CurveSampleRate);
			if (MaxStackResampleRate >= MaxReferenceRate)
			{
				MaxStackResampleRate = MaxReferenceRate;
			}
		}
	}

	// Make sure we're not hitting 0 for samplerate
	if ( MaxStackResampleRate != 0 )
	{
		return MaxStackResampleRate;
	}

	return DEFAULT_SAMPLERATE;
}

bool UnFbx::FFbxImporter::ValidateAnimStack(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, FbxAnimStack* CurAnimStack, int32 ResampleRate, bool bImportMorph, FbxTimeSpan &AnimTimeSpan)
{
	// set current anim stack
	Scene->SetCurrentAnimationStack(CurAnimStack);

	UE_LOG(LogFbx, Log, TEXT("Parsing AnimStack %s"),UTF8_TO_TCHAR(CurAnimStack->GetName()));

	// There are a FBX unroll filter bug, so don't bake animation layer at all
	MergeAllLayerAnimation(CurAnimStack, ResampleRate);

	bool bValidAnimStack = true;

	AnimTimeSpan = GetAnimationTimeSpan(SortedLinks[0], CurAnimStack, ResampleRate);
	
	// if no duration is found, return false
	if (AnimTimeSpan.GetDuration() <= 0)
	{
		return false;
	}

	const FBXImportOptions* ImportOption = GetImportOptions();
	// only add morph time if not setrange. If Set Range there is no reason to override time
	if ( bImportMorph && ImportOption->AnimationLengthImportType != FBXALIT_SetRange)
	{
		for ( int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); NodeIndex++ )
		{
			// consider blendshape animation curve
			FbxGeometry* Geometry = (FbxGeometry*)NodeArray[NodeIndex]->GetNodeAttribute();
			if (Geometry)
			{
				int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);
				for(int32 BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
				{
					FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);

					int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
					for(int32 ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
					{
						FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

						if(Channel)
						{
							// Get the percentage of influence of the shape.
							FbxAnimCurve* Curve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)CurAnimStack->GetMember(0));
							if (Curve && Curve->KeyGetCount() > 0)
							{
								FbxTimeSpan TmpAnimSpan;

								if (Curve->GetTimeInterval(TmpAnimSpan))
								{
									bValidAnimStack = true;
									// update animation interval to include morph target range
									AnimTimeSpan.UnionAssignment(TmpAnimSpan);
								}
							}
						}
					}
				}
			}
		}
	}

	return bValidAnimStack;
}

bool UnFbx::FFbxImporter::ImportCurve(const FbxAnimCurve* FbxCurve, FRichCurve& RichCurve, const FbxTimeSpan &AnimTimeSpan, const float ValueScale/*=1.f*/) const
{
	static float DefaultCurveWeight = FbxAnimCurveDef::sDEFAULT_WEIGHT;

	if ( FbxCurve )
	{
		for ( int32 KeyIndex=0; KeyIndex<FbxCurve->KeyGetCount(); ++KeyIndex )
		{
			FbxAnimCurveKey Key = FbxCurve->KeyGet(KeyIndex);
			FbxTime KeyTime = Key.GetTime() - AnimTimeSpan.GetStart();
			float Value = Key.GetValue() * ValueScale;
			FKeyHandle NewKeyHandle = RichCurve.AddKey(KeyTime.GetSecondDouble(), Value, false);

			FbxAnimCurveDef::ETangentMode KeyTangentMode = Key.GetTangentMode();
			FbxAnimCurveDef::EInterpolationType KeyInterpMode = Key.GetInterpolation();
			FbxAnimCurveDef::EWeightedMode KeyTangentWeightMode = Key.GetTangentWeightMode();

			ERichCurveInterpMode NewInterpMode = RCIM_Linear;
			ERichCurveTangentMode NewTangentMode = RCTM_Auto;
			ERichCurveTangentWeightMode NewTangentWeightMode = RCTWM_WeightedNone;

			float LeaveTangent = 0.f; 
			float ArriveTangent = 0.f;
			float LeaveTangentWeight = 0.f;
			float ArriveTangentWeight = 0.f;

			switch (KeyInterpMode)
			{
			case FbxAnimCurveDef::eInterpolationConstant://! Constant value until next key.
				NewInterpMode = RCIM_Constant;
				break;
			case FbxAnimCurveDef::eInterpolationLinear://! Linear progression to next key.
				NewInterpMode = RCIM_Linear;
				break;
			case FbxAnimCurveDef::eInterpolationCubic://! Cubic progression to next key.
				NewInterpMode = RCIM_Cubic;
				// get tangents
				{
					LeaveTangent = Key.GetDataFloat(FbxAnimCurveDef::eRightSlope);

					if ( KeyIndex > 0 )
					{
						FbxAnimCurveKey PrevKey = FbxCurve->KeyGet(KeyIndex-1);
						ArriveTangent = PrevKey.GetDataFloat(FbxAnimCurveDef::eNextLeftSlope);
					}
					else
					{
						ArriveTangent = 0.f;
					}

				}
				break;
			}

			// break or any other tangent mode doesn't work well with DCC
			// it's because we don't support tangent weights, break with tangent weights won't work
			// I added new ticket to support this, but meanwhile, we'll have to just import using auto. 
			// @Todo: fix me: UE-20414
			// when we import tangent, we only support break or user
			// since it's modified by DCC and we only assume these two are valid
			// auto does our own stuff, which doesn't work with what you see in DCC
// 			if (KeyTangentMode & FbxAnimCurveDef::eTangentGenericBreak)
// 			{
// 				NewTangentMode = RCTM_Break;
// 			}
// 			else
// 			{
// 				NewTangentMode = RCTM_User;
// 			}

			// @fix me : weight of tangent is not used, but we'll just save this for future where we might use it. 
			switch (KeyTangentWeightMode)
			{
			case FbxAnimCurveDef::eWeightedNone://! Tangent has default weights of 0.333; we define this state as not weighted.
				LeaveTangentWeight = ArriveTangentWeight = DefaultCurveWeight;
				NewTangentWeightMode = RCTWM_WeightedNone;
				break;
			case FbxAnimCurveDef::eWeightedRight: //! Right tangent is weighted.
				NewTangentWeightMode = RCTWM_WeightedLeave;
				LeaveTangentWeight = Key.GetDataFloat(FbxAnimCurveDef::eRightWeight);
				ArriveTangentWeight = DefaultCurveWeight;
				break;
			case FbxAnimCurveDef::eWeightedNextLeft://! Left tangent is weighted.
				NewTangentWeightMode = RCTWM_WeightedArrive;
				LeaveTangentWeight = DefaultCurveWeight;
				if ( KeyIndex > 0 )
				{
					FbxAnimCurveKey PrevKey = FbxCurve->KeyGet(KeyIndex-1);
					ArriveTangentWeight = PrevKey.GetDataFloat(FbxAnimCurveDef::eNextLeftWeight);
				}
				else
				{
					ArriveTangentWeight = 0.f;
				}
				break;
			case FbxAnimCurveDef::eWeightedAll://! Both left and right tangents are weighted.
				NewTangentWeightMode = RCTWM_WeightedBoth;
				LeaveTangentWeight = Key.GetDataFloat(FbxAnimCurveDef::eRightWeight);
				if ( KeyIndex > 0 )
				{
					FbxAnimCurveKey PrevKey = FbxCurve->KeyGet(KeyIndex-1);
					ArriveTangentWeight = PrevKey.GetDataFloat(FbxAnimCurveDef::eNextLeftWeight);
				}
				else
				{
					ArriveTangentWeight = 0.f;
				}
				break;
			}

			RichCurve.SetKeyInterpMode(NewKeyHandle, NewInterpMode);
			RichCurve.SetKeyTangentMode(NewKeyHandle, NewTangentMode);
			RichCurve.SetKeyTangentWeightMode(NewKeyHandle, NewTangentWeightMode);

			FRichCurveKey& NewKey = RichCurve.GetKey(NewKeyHandle);
			// apply 1/100 - that seems like the tangent unit difference with FBX
			NewKey.ArriveTangent = ArriveTangent * 0.01f;
			NewKey.LeaveTangent = LeaveTangent * 0.01f;
			NewKey.ArriveTangentWeight = ArriveTangentWeight;
			NewKey.LeaveTangentWeight = LeaveTangentWeight;
		}

		return true;
	}

	return false;
}

/** This is to debug FBX importing animation. It saves source data and compare with what we use internally, so that it does detect earlier to find out there is transform issue
 *	We don't support skew(shearing), so if you have animation that has shearing(skew), this won't be preserved. Instead it will try convert to our format, which will visually look wrong. 
 *	If you have shearing(skew), please use "Preserve Local Transform" option, but it won't preserve its original animated transform */
namespace AnimationTransformDebug
{
	// Data sturctutre to debug bone transform of animation issues
	struct FAnimationTransformDebugData
	{
		int32 TrackIndex;
		int32 BoneIndex;
		FName BoneName;
		TArray<FTransform>	RecalculatedLocalTransform;
		// this is used to calculate for intermediate result, not the source parent global transform
		TArray<FTransform>	RecalculatedParentTransform;

		//source data to convert from
		TArray<FTransform>	SourceGlobalTransform;
		TArray<FTransform>	SourceParentGlobalTransform;

		FAnimationTransformDebugData()
			: TrackIndex(INDEX_NONE), BoneIndex(INDEX_NONE), BoneName(NAME_None)
		{}

		void SetTrackData(int32 InTrackIndex, int32 InBoneIndex, FName InBoneName)
		{
			TrackIndex = InTrackIndex;
			BoneIndex = InBoneIndex;
			BoneName = InBoneName;
		}
	};

	void OutputAnimationTransformDebugData(TArray<AnimationTransformDebug::FAnimationTransformDebugData> &TransformDebugData, int32 TotalNumKeys, const FReferenceSkeleton& RefSkeleton)
	{
		bool bShouldOutputToMessageLog = true;

		for(int32 Key=0; Key<TotalNumKeys; ++Key)
		{
			// go through all bones and find 
			for(int32 BoneIndex=0; BoneIndex<TransformDebugData.Num(); ++BoneIndex)
			{
				FAnimationTransformDebugData& Data = TransformDebugData[BoneIndex];
				int32 ParentIndex = RefSkeleton.GetParentIndex(Data.BoneIndex);
				int32 ParentTransformDebugDataIndex = 0;

				check(Data.RecalculatedLocalTransform.Num() == TotalNumKeys);
				check(Data.SourceGlobalTransform.Num() == TotalNumKeys);
				check(Data.SourceParentGlobalTransform.Num() == TotalNumKeys);

				for(; ParentTransformDebugDataIndex<BoneIndex; ++ParentTransformDebugDataIndex)
				{
					if(ParentIndex == TransformDebugData[ParentTransformDebugDataIndex].BoneIndex)
					{
						FTransform ParentTransform = TransformDebugData[ParentTransformDebugDataIndex].RecalculatedLocalTransform[Key] * TransformDebugData[ParentTransformDebugDataIndex].RecalculatedParentTransform[Key];
						Data.RecalculatedParentTransform.Add(ParentTransform);
						break;
					}
				}

				// did not find Parent
				if(ParentTransformDebugDataIndex == BoneIndex)
				{
					Data.RecalculatedParentTransform.Add(FTransform::Identity);
				}

				check(Data.RecalculatedParentTransform.Num() == Key+1);

				FTransform GlobalTransform = Data.RecalculatedLocalTransform[Key] * Data.RecalculatedParentTransform[Key];
				// makes more generous on the threshold. 
				if(GlobalTransform.Equals(Data.SourceGlobalTransform[Key], 0.1f) == false)
				{
					// so that we don't spawm with this message
					if(bShouldOutputToMessageLog)
					{
						UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
						// now print information - it doesn't match well, find out what it is
						FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FBXImport_TransformError", "Imported bone transform is different from original. Please check Output Log to see detail of error. "),
							FText::FromName(Data.BoneName), FText::AsNumber(Data.BoneIndex), FText::FromString(Data.SourceGlobalTransform[Key].ToString()), FText::FromString(GlobalTransform.ToString()))), FFbxErrors::Animation_TransformError);

						bShouldOutputToMessageLog = false;
					}
					
					// now print information - it doesn't match well, find out what it is
					UE_LOG(LogFbx, Warning, TEXT("IMPORT TRANSFORM ERROR : Bone (%s:%d) \r\nSource Global Transform (%s), \r\nConverted Global Transform (%s)"),
						*Data.BoneName.ToString(), Data.BoneIndex, *Data.SourceGlobalTransform[Key].ToString(), *GlobalTransform.ToString());
				}
			}
		}
	}
}

/**
 * We only support float values, so these are the numbers we can take
 */
bool IsSupportedCurveDataType(EFbxType DatatType)
{

	switch (DatatType)
	{
	case eFbxShort:		//!< 16 bit signed integer.
	case eFbxUShort:		//!< 16 bit unsigned integer.
	case eFbxUInt:		//!< 32 bit unsigned integer.
	case eFbxHalfFloat:	//!< 16 bit floating point.
	case eFbxInt:		//!< 32 bit signed integer.
	case eFbxFloat:		//!< Floating point value.
	case eFbxDouble:		//!< Double width floating point value.
	case eFbxDouble2:	//!< Vector of two double values.
	case eFbxDouble3:	//!< Vector of three double values.
	case eFbxDouble4:	//!< Vector of four double values.
	case eFbxDouble4x4:	//!< Four vectors of four double values.
		return true;
	}

	return false;
}

bool UnFbx::FFbxImporter::ImportCurveToAnimSequence(class UAnimSequence * TargetSequence, const FString& CurveName, const FbxAnimCurve* FbxCurve, int32 CurveFlags,const FbxTimeSpan AnimTimeSpan, const float ValueScale/*=1.f*/) const
{
	if (TargetSequence && FbxCurve)
	{
		FName Name = *CurveName;
		USkeleton* Skeleton = TargetSequence->GetSkeleton();
		const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

		// Add or retrieve curve
		if (!NameMapping->Exists(Name))
		{
			// mark skeleton dirty
			Skeleton->Modify();
		}

		FSmartName NewName;
		Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, Name, NewName);

		FFloatCurve * CurveToImport = static_cast<FFloatCurve *>(TargetSequence->RawCurveData.GetCurveData(NewName.UID, ERawCurveTrackTypes::RCT_Float));
		if(CurveToImport==NULL)
		{
			if (TargetSequence->RawCurveData.AddCurveData(NewName, AACF_DefaultCurve | CurveFlags))
			{
				CurveToImport = static_cast<FFloatCurve *> (TargetSequence->RawCurveData.GetCurveData(NewName.UID, ERawCurveTrackTypes::RCT_Float));
				CurveToImport->Name = NewName;
			}
			else
			{
				// this should not happen, we already checked before adding
				ensureMsgf(0, TEXT("FBX Import: Critical error: no memory?"));
			}
		}
		else
		{
			CurveToImport->FloatCurve.Reset();
			// if existing add these curve flags. 
			CurveToImport->SetCurveTypeFlags(CurveFlags | CurveToImport->GetCurveTypeFlags());
		}

		// update last observed name. If not, sometimes it adds new UID while fixing up that will confuse Compressed Raw Data
		const FSmartNameMapping* Mapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		TargetSequence->RawCurveData.RefreshName(Mapping);

		TargetSequence->MarkRawDataAsModified();
		if (CurveToImport && ImportCurve(FbxCurve, CurveToImport->FloatCurve, AnimTimeSpan, ValueScale))
		{
			if (ImportOptions->bRemoveRedundantKeys)
			{
				CurveToImport->FloatCurve.RemoveRedundantKeys(SMALL_NUMBER);
			}
			return true;
		}
	}

	return false;
}

bool ShouldImportCurve(FbxAnimCurve* Curve, bool bDoNotImportWithZeroValues)
{
	if (Curve && Curve->KeyGetCount() > 0)
	{
		if (bDoNotImportWithZeroValues)
		{
			for (int32 KeyIndex = 0; KeyIndex < Curve->KeyGetCount(); ++KeyIndex)
			{
				if (!FMath::IsNearlyZero(Curve->KeyGetValue(KeyIndex)))
				{
					return true;
				}
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

bool UnFbx::FFbxImporter::ImportAnimation(USkeleton* Skeleton, UAnimSequence * DestSeq, const FString& FileName, TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, FbxAnimStack* CurAnimStack, const int32 ResampleRate, const FbxTimeSpan AnimTimeSpan)
{
	//This destroy all previously imported animation raw data
	DestSeq->CleanAnimSequenceForImport();

	// @todo : the length might need to change w.r.t. sampling keys
	FbxTime SequenceLength = AnimTimeSpan.GetDuration();
	float PreviousSequenceLength = DestSeq->SequenceLength;

	// if you have one pose(thus 0.f duration), it still contains animation, so we'll need to consider that as MINIMUM_ANIMATION_LENGTH time length
	DestSeq->SequenceLength = FGenericPlatformMath::Max<float>(SequenceLength.GetSecondDouble(), MINIMUM_ANIMATION_LENGTH);

	if(PreviousSequenceLength > MINIMUM_ANIMATION_LENGTH && DestSeq->RawCurveData.FloatCurves.Num() > 0)
	{
		// The sequence already existed when we began the import. We need to scale the key times for all curves to match the new 
		// duration before importing over them. This is to catch any user-added curves
		float ScaleFactor = DestSeq->SequenceLength / PreviousSequenceLength;
		for(FFloatCurve& Curve : DestSeq->RawCurveData.FloatCurves)
		{
			Curve.FloatCurve.ScaleCurve(0.0f, ScaleFactor);
		}
	}

	USkeleton* MySkeleton = DestSeq->GetSkeleton();
	check(MySkeleton);

	if (ImportOptions->bDeleteExistingMorphTargetCurves)
	{
		for (int32 CurveIdx=0; CurveIdx<DestSeq->RawCurveData.FloatCurves.Num(); ++CurveIdx)
		{
			auto& Curve = DestSeq->RawCurveData.FloatCurves[CurveIdx];
			const FCurveMetaData* MetaData = MySkeleton->GetCurveMetaData(Curve.Name);
			if (MetaData && MetaData->Type.bMorphtarget)
			{
				DestSeq->RawCurveData.FloatCurves.RemoveAt(CurveIdx, 1, false);
				--CurveIdx;
			}
		}

		DestSeq->RawCurveData.FloatCurves.Shrink();
	}

	// Store float curve tracks which use to exist on the animation
	TArray<FString> ExistingCurveNames;
	for (int32 CurveIdx = 0; CurveIdx < DestSeq->RawCurveData.FloatCurves.Num(); ++CurveIdx)
	{
		auto& Curve = DestSeq->RawCurveData.FloatCurves[CurveIdx];
		const FCurveMetaData* MetaData = MySkeleton->GetCurveMetaData(Curve.Name);
		
		if (MetaData && !MetaData->Type.bMorphtarget)
		{
			ExistingCurveNames.Add(Curve.Name.DisplayName.ToString());
		}
	}

	const bool bReimportWarnings = GetDefault<UEditorPerProjectUserSettings>()->bAnimationReimportWarnings;
	
	if (bReimportWarnings && !FMath::IsNearlyZero(PreviousSequenceLength) && !FMath::IsNearlyEqual(DestSeq->SequenceLength, PreviousSequenceLength))
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Warning_SequenceLengthChanged", "Animation Sequence ({0}) length {1} is different from previous {2}."), FText::FromName(DestSeq->GetFName()), DestSeq->SequenceLength, PreviousSequenceLength)), FFbxErrors::Animation_DifferentLength);
	}

	FbxNode *SkeletalMeshRootNode = NodeArray.Num() > 0 ? NodeArray[0] : nullptr;
	
	//
	// import blend shape curves
	//
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginImportMorphTargetCurves", "Importing Morph Target Curves"), true);
		for ( int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); NodeIndex++ )
		{
			// consider blendshape animation curve
			FbxGeometry* Geometry = (FbxGeometry*)NodeArray[NodeIndex]->GetNodeAttribute();
			if (Geometry)
			{
				int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);
				for(int32 BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
				{
					FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);

					const int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();

					FString BlendShapeName = UTF8_TO_TCHAR(MakeName(BlendShape->GetName()));

					// see below where this is used for explanation...
					const bool bMightBeBadMAXFile = (BlendShapeName == FString("Morpher"));

					for(int32 ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
					{
						FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

						if(Channel)
						{
							FString ChannelName = UTF8_TO_TCHAR(MakeName(Channel->GetName()));
							// Maya adds the name of the blendshape and an underscore or point to the front of the channel name, so remove it
							// Also avoid to endup with a empty name, we prefer having the Blendshapename instead of nothing
							if(ChannelName.StartsWith(BlendShapeName) && ChannelName.Len() > BlendShapeName.Len())
							{
								ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
							}
							
							if (bMightBeBadMAXFile)
							{
								FbxShape *TargetShape = Channel->GetTargetShapeCount() > 0 ? Channel->GetTargetShape(0) : nullptr;
								if (TargetShape)
								{
									FString TargetShapeName = UTF8_TO_TCHAR(MakeName(TargetShape->GetName()));
									ChannelName = TargetShapeName.IsEmpty() ? ChannelName : TargetShapeName;
								}
							}

							FbxAnimCurve* Curve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)CurAnimStack->GetMember(0));
							if (ShouldImportCurve(Curve, ImportOptions->bDoNotImportCurveWithZero))
							{
								FFormatNamedArguments Args;
								Args.Add(TEXT("BlendShape"), FText::FromString(ChannelName));
								const FText StatusUpate = FText::Format(LOCTEXT("ImportingMorphTargetCurvesDetail", "Importing Morph Target Curves [{BlendShape}]"), Args);
								GWarn->StatusUpdate(NodeIndex + 1, NodeArray.Num(), StatusUpate);
								// now see if we have one already exists. If so, just overwrite that. if not, add new one. 

								if (ImportCurveToAnimSequence(DestSeq, *ChannelName, Curve, 0, AnimTimeSpan, 0.01f /** for some reason blend shape values are coming as 100 scaled **/))
								{
									// this one doesn't reset Material curve to false, it just accumulate if true. 
									MySkeleton->AccumulateCurveMetaData(*ChannelName, false, true);
								}

							}
							else
							{
								UE_LOG(LogFbx, Warning, TEXT("CurveName(%s) is skipped because it only contains invalid values."), *ChannelName);
							}
						}
					}
				}
			}
		}
		GWarn->EndSlowTask();
	}

	// 
	// importing custom attribute START
	//
	if (ImportOptions->bImportCustomAttribute)
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginImportCustomAttributeCurves", "Importing Custom Attribute Curves"), true);
		const int32 TotalLinks = SortedLinks.Num();
		int32 CurLinkIndex=0;
		for(auto Node: SortedLinks)
		{
			FbxProperty Property = Node->GetFirstProperty();
			while (Property.IsValid())
			{
				FbxAnimCurveNode* CurveNode = Property.GetCurveNode();
				// do this if user defined and animated and leaf node
				if( CurveNode && Property.GetFlag(FbxPropertyFlags::eUserDefined) &&
					CurveNode->IsAnimated() && IsSupportedCurveDataType(Property.GetPropertyDataType().GetType()) )
				{
					FString CurveName = UTF8_TO_TCHAR(CurveNode->GetName());
					UE_LOG(LogFbx, Log, TEXT("CurveName : %s"), *CurveName );

					int32 TotalCount = CurveNode->GetChannelsCount();
					for (int32 ChannelIndex=0; ChannelIndex<TotalCount; ++ChannelIndex)
					{
						FbxAnimCurve * AnimCurve = CurveNode->GetCurve(ChannelIndex);
						FString ChannelName = CurveNode->GetChannelName(ChannelIndex).Buffer();

						if (ShouldImportCurve(AnimCurve, ImportOptions->bDoNotImportCurveWithZero))
						{
							FString FinalCurveName;
							if (TotalCount == 1)
							{
								FinalCurveName = CurveName;
							}
							else
							{
								FinalCurveName = CurveName + "_" + ChannelName;
							}

							FFormatNamedArguments Args;
							Args.Add(TEXT("CurveName"), FText::FromString(FinalCurveName));
							const FText StatusUpate = FText::Format(LOCTEXT("ImportingCustomAttributeCurvesDetail", "Importing Custom Attribute [{CurveName}]"), Args);
							GWarn->StatusUpdate(CurLinkIndex + 1, TotalLinks, StatusUpate);

							int32 CurveFlags = AACF_DefaultCurve;
							if (ImportCurveToAnimSequence(DestSeq, FinalCurveName, AnimCurve, CurveFlags, AnimTimeSpan))
							{
								// first let them override material curve if required
								if (ImportOptions->bSetMaterialDriveParameterOnCustomAttribute)
								{
									// now mark this curve as morphtarget
									MySkeleton->AccumulateCurveMetaData(FName(*FinalCurveName), true, false);
								}
								else
								{
									// if not material set by default, apply naming convention for material
									for (const auto& Suffix : ImportOptions->MaterialCurveSuffixes)
									{
										int32 TotalSuffix = Suffix.Len();
										if (CurveName.Right(TotalSuffix) == Suffix)
										{
											MySkeleton->AccumulateCurveMetaData(FName(*FinalCurveName), true, false);
											break;
										}
									}
								}

								ExistingCurveNames.Remove(FinalCurveName);
							}
						}
						else
						{
							UE_LOG(LogFbx, Log, TEXT("CurveName(%s) is skipped because it only contains invalid values."), *CurveName);
						}
					}
				}

				Property = Node->GetNextProperty(Property); 
			}

			CurLinkIndex++;
		}

		GWarn->EndSlowTask();
	}

	if (bReimportWarnings && ExistingCurveNames.Num())
	{
		for (const FString CurveName : ExistingCurveNames)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Warning_NonExistingCurve", "Curve ({0}) was not found in the new Animation."), FText::FromString(CurveName))), FFbxErrors::Animation_CurveNotFound);
		}
	}

	// importing custom attribute END
	
	const bool bSourceDataExists = DestSeq->HasSourceRawData();
	TArray<AnimationTransformDebug::FAnimationTransformDebugData> TransformDebugData;
	int32 TotalNumKeys = 0;
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	// import animation
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginImportAnimation", "Importing Animation"), true);

		TArray<FName> FbxRawBoneNames;
		FillAndVerifyBoneNames(Skeleton, SortedLinks, FbxRawBoneNames, FileName);

		UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

		const bool bPreserveLocalTransform = FbxImporter->GetImportOptions()->bPreserveLocalTransform;

		// Build additional transform matrix
		UFbxAnimSequenceImportData* TemplateData = Cast<UFbxAnimSequenceImportData>(DestSeq->AssetImportData);
		FbxAMatrix FbxAddedMatrix;
		BuildFbxMatrixForImportTransform(FbxAddedMatrix, TemplateData);
		FMatrix AddedMatrix = Converter.ConvertMatrix(FbxAddedMatrix);

		bool bIsRigidMeshAnimation = false;
		if (ImportOptions->bImportScene && SortedLinks.Num() > 0)
		{
			for (int32 BoneIdx = 0; BoneIdx < SortedLinks.Num(); ++BoneIdx)
			{
				FbxNode* Link = SortedLinks[BoneIdx];
				if (Link->GetMesh() && Link->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0)
				{
					bIsRigidMeshAnimation = true;
					break;
				}
			}
		}


		const int32 NumSamplingKeys = FMath::FloorToInt(AnimTimeSpan.GetDuration().GetSecondDouble() * ResampleRate);
		const FbxTime TimeIncrement = AnimTimeSpan.GetDuration() / FMath::Max(NumSamplingKeys, 1);
		for(int32 SourceTrackIdx = 0; SourceTrackIdx < FbxRawBoneNames.Num(); ++SourceTrackIdx)
		{
			int32 NumKeysForTrack = 0;

			// see if it's found in Skeleton
			FName BoneName = FbxRawBoneNames[SourceTrackIdx];
			int32 BoneTreeIndex = RefSkeleton.FindBoneIndex(BoneName);

			// update status
			FFormatNamedArguments Args;
			Args.Add(TEXT("TrackName"), FText::FromName(BoneName));
			Args.Add(TEXT("TotalKey"), FText::AsNumber(NumSamplingKeys));
			Args.Add(TEXT("TrackIndex"), FText::AsNumber(SourceTrackIdx+1));
			Args.Add(TEXT("TotalTracks"), FText::AsNumber(FbxRawBoneNames.Num()));
			const FText StatusUpate = FText::Format(LOCTEXT("ImportingAnimTrackDetail", "Importing Animation Track [{TrackName}] ({TrackIndex}/{TotalTracks}) - TotalKey {TotalKey}"), Args);
			GWarn->StatusForceUpdate(SourceTrackIdx + 1, FbxRawBoneNames.Num(), StatusUpate);

			if (BoneTreeIndex!=INDEX_NONE)
			{
				bool bSuccess = true;

				FRawAnimSequenceTrack RawTrack;
				RawTrack.PosKeys.Empty();
				RawTrack.RotKeys.Empty();
				RawTrack.ScaleKeys.Empty();

				AnimationTransformDebug::FAnimationTransformDebugData NewDebugData;

				FbxNode* Link = SortedLinks[SourceTrackIdx];
				FbxNode * LinkParent = Link->GetParent();
				for(FbxTime CurTime = AnimTimeSpan.GetStart(); CurTime <= AnimTimeSpan.GetStop(); CurTime += TimeIncrement)
				{
					// save global trasnform
					FbxAMatrix GlobalMatrix = Link->EvaluateGlobalTransform(CurTime) * FFbxDataConverter::GetJointPostConversionMatrix();
					// we'd like to verify this before going to Transform. 
					// currently transform has tons of NaN check, so it will crash there
					FMatrix GlobalUEMatrix = Converter.ConvertMatrix(GlobalMatrix);
					if (GlobalUEMatrix.ContainsNaN())
					{
						bSuccess = false;
						AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_InvalidTransform",
							"Track {0} contains invalid transform. Could not import the track."), FText::FromName(BoneName))), FFbxErrors::Animation_TransformError);
						break;
					}

					FTransform GlobalTransform =  Converter.ConvertTransform(GlobalMatrix);
					if (GlobalTransform.ContainsNaN())
					{
						bSuccess = false;
						AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_InvalidUnrealTransform",
											"Track {0} has invalid transform(NaN). Zero scale transform can cause this issue."), FText::FromName(BoneName))), FFbxErrors::Animation_TransformError);
						break;
					}

					// debug data, including import transformation
					FTransform AddedTransform(AddedMatrix);
					NewDebugData.SourceGlobalTransform.Add(GlobalTransform * AddedTransform);

					FTransform LocalTransform;
					if( !bPreserveLocalTransform && LinkParent)
					{
						// I can't rely on LocalMatrix. I need to recalculate quaternion/scale based on global transform if Parent exists
						FbxAMatrix ParentGlobalMatrix = Link->GetParent()->EvaluateGlobalTransform(CurTime);
						if (BoneTreeIndex != 0)
						{
							ParentGlobalMatrix = ParentGlobalMatrix * FFbxDataConverter::GetJointPostConversionMatrix();
						}
						FTransform ParentGlobalTransform =  Converter.ConvertTransform(ParentGlobalMatrix);
						//In case we do a scene import we need to add the skeletal mesh root node matrix to the parent link.
						if (ImportOptions->bImportScene && !ImportOptions->bTransformVertexToAbsolute && BoneTreeIndex == 0 && SkeletalMeshRootNode != nullptr)
						{
							//In the case of a rigidmesh animation we have to use the skeletalMeshRootNode position at zero since the mesh can be animate.
							FbxAMatrix GlobalSkeletalNodeFbx = bIsRigidMeshAnimation ? SkeletalMeshRootNode->EvaluateGlobalTransform(0) : SkeletalMeshRootNode->EvaluateGlobalTransform(CurTime);
							FTransform GlobalSkeletalNode = Converter.ConvertTransform(GlobalSkeletalNodeFbx);
							ParentGlobalTransform = ParentGlobalTransform * GlobalSkeletalNode;
						}

						LocalTransform = GlobalTransform.GetRelativeTransform(ParentGlobalTransform);
						NewDebugData.SourceParentGlobalTransform.Add(ParentGlobalTransform);
					} 
					else
					{
						FbxAMatrix& LocalMatrix = Link->EvaluateLocalTransform(CurTime); 
						FbxVector4 NewLocalT = LocalMatrix.GetT();
						FbxVector4 NewLocalS = LocalMatrix.GetS();
						FbxQuaternion NewLocalQ = LocalMatrix.GetQ();

						LocalTransform.SetTranslation(Converter.ConvertPos(NewLocalT));
						LocalTransform.SetScale3D(Converter.ConvertScale(NewLocalS));
						LocalTransform.SetRotation(Converter.ConvertRotToQuat(NewLocalQ));

						NewDebugData.SourceParentGlobalTransform.Add(FTransform::Identity);
					}

					if(TemplateData && BoneTreeIndex == 0)
					{
						// If we found template data earlier, apply the import transform matrix to
						// the root track.
						LocalTransform.SetFromMatrix(LocalTransform.ToMatrixWithScale() * AddedMatrix);
					}

					if (LocalTransform.ContainsNaN())
					{
						bSuccess = false;
						AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_InvalidUnrealLocalTransform",
											"Track {0} has invalid transform(NaN). If you have zero scale transform, that can cause this."), FText::FromName(BoneName))), FFbxErrors::Animation_TransformError);
						break;
					}

					RawTrack.ScaleKeys.Add(LocalTransform.GetScale3D());
					RawTrack.PosKeys.Add(LocalTransform.GetTranslation());
					RawTrack.RotKeys.Add(LocalTransform.GetRotation());

					NewDebugData.RecalculatedLocalTransform.Add(LocalTransform);
					++NumKeysForTrack;
				}

				if (bSuccess)
				{
					//add new track
					int32 NewTrackIdx = DestSeq->AddNewRawTrack(BoneName, &RawTrack);

					NewDebugData.SetTrackData(NewTrackIdx, BoneTreeIndex, BoneName);

					// add mapping to skeleton bone track
					TransformDebugData.Add(NewDebugData);
				}
			}

			TotalNumKeys = FMath::Max( TotalNumKeys, NumKeysForTrack );
		}

		DestSeq->NumFrames = TotalNumKeys;

		DestSeq->MarkRawDataAsModified();

		GWarn->EndSlowTask();
	}

	// compress animation
	{
		GWarn->BeginSlowTask( LOCTEXT("BeginCompressAnimation", "Compress Animation"), true);
		GWarn->StatusForceUpdate(1, 1, LOCTEXT("CompressAnimation", "Compressing Animation"));
		// if source data exists, you should bake it to Raw to apply
		if(bSourceDataExists)
		{
			DestSeq->BakeTrackCurvesToRawAnimation();
		}
		else
		{
			// otherwise just compress
			DestSeq->PostProcessSequence();
		}

		// run debug mode
		AnimationTransformDebug::OutputAnimationTransformDebugData(TransformDebugData, TotalNumKeys, RefSkeleton);
		GWarn->EndSlowTask();
	}

	// Reregister skeletal mesh components so they reflect the updated animation
	for (TObjectIterator<USkeletalMeshComponent> Iter; Iter; ++Iter)
	{
		FComponentReregisterContext ReregisterContext(*Iter);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
