// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Editor.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackInstFloatProp.h"

#include "FbxImporter.h"

namespace UnFbx {

/**
 * Retrieves whether there are any unknown camera instances within the FBX document that the camera is not in Unreal scene.
 */
inline bool _HasUnknownCameras( AMatineeActor* InMatineeActor, FbxNode* Node, const TCHAR* Name )
{
	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	if (Attr && Attr->GetAttributeType() == FbxNodeAttribute::eCamera)
	{
		// If we have a Matinee, try to name-match the node with a Matinee group name
		if( InMatineeActor != NULL && InMatineeActor->MatineeData != NULL )
		{
			UInterpGroupInst* GroupInst = InMatineeActor->FindFirstGroupInstByName( FString( Name ) );
			if( GroupInst != NULL )
			{
				AActor* GrActor = GroupInst->GetGroupActor();
				// Make sure we have an actor
				if( GrActor != NULL &&
					GrActor->IsA( ACameraActor::StaticClass() ) )
				{
					// OK, we found an existing camera!
					return false;
				}
			}
		}
		
		// Attempt to name-match the scene node for this camera with one of the actors.
		AActor* Actor = FindObject<AActor>( ANY_PACKAGE, Name );
		if ( Actor == NULL || Actor->IsPendingKill() )
		{
			return true;
		}
		else
		{
			// If you trigger this assertion, then you've got a name
			// clash between the FBX file and the level.
			check( Actor->IsA( ACameraActor::StaticClass() ) );
		}
	}
	
	return false;
}

bool FFbxImporter::HasUnknownCameras( AMatineeActor* InMatineeActor ) const
{
	if ( Scene == NULL )
	{
		return false;
	}

	// check recursively
	FbxNode* RootNode = Scene->GetRootNode();
	int32 NodeCount = RootNode->GetChildCount();
	for ( int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex )
	{
		FbxNode* Node = RootNode->GetChild(NodeIndex);
		if ( _HasUnknownCameras( InMatineeActor, Node, UTF8_TO_TCHAR(Node->GetName()) ) )
		{
			return true;
		}

		// Look through children as well
		int32 ChildNodeCount = Node->GetChildCount();
		for( int32 ChildIndex = 0; ChildIndex < ChildNodeCount; ++ChildIndex )
		{
			FbxNode* ChildNode = Node->GetChild(ChildIndex);
			if( _HasUnknownCameras( InMatineeActor, ChildNode, UTF8_TO_TCHAR(ChildNode->GetName() ) ) )
			{
				return true;
			}
		}
	}

	return false;
}

bool FFbxImporter::IsNodeAnimated(FbxNode* Node, FbxAnimLayer* AnimLayer)
{
	if (!AnimLayer)
	{
		FbxAnimStack* AnimStack = Scene->GetMember<FbxAnimStack>(0);
		if (!AnimStack) return false;

		AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
		if (AnimLayer == NULL) return false;
	}
	
	// verify that the node is animated.
	bool bIsAnimated = false;
	FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);

	// translation animation
	FbxProperty TransProp = Node->LclTranslation;
	for (int32 i = 0; i < TransProp.GetSrcObjectCount<FbxAnimCurveNode>(); i++)
	{
		FbxAnimCurveNode* CurveNode = TransProp.GetSrcObject<FbxAnimCurveNode>(i);
		if (CurveNode && AnimLayer->IsConnectedSrcObject(CurveNode))
		{
			bIsAnimated |= (bool)CurveNode->GetAnimationInterval(AnimTimeSpan);
			break;
		}
	}
	// rotation animation
	FbxProperty RotProp = Node->LclRotation;
	for (int32 i = 0; bIsAnimated == false && i < RotProp.GetSrcObjectCount<FbxAnimCurveNode>(); i++)
	{
		FbxAnimCurveNode* CurveNode = RotProp.GetSrcObject<FbxAnimCurveNode>(i);
		if (CurveNode && AnimLayer->IsConnectedSrcObject(CurveNode))
		{
			bIsAnimated |= (bool)CurveNode->GetAnimationInterval(AnimTimeSpan);
		}
	}
	
	return bIsAnimated;
}

/** 
 * Finds a camera in the passed in node or any child nodes 
 * @return NULL if the camera is not found, a valid pointer if it is
 */
static FbxCamera* FindCamera( FbxNode* Parent )
{
	FbxCamera* Camera = Parent->GetCamera();
	if( !Camera )
	{
		int32 NodeCount = Parent->GetChildCount();
		for ( int32 NodeIndex = 0; NodeIndex < NodeCount && !Camera; ++NodeIndex )
		{
			FbxNode* Child = Parent->GetChild( NodeIndex );
			Camera = Child->GetCamera();
		}
	}

	return Camera;
}

bool FFbxImporter::ImportMatineeSequence(AMatineeActor* InMatineeActor)
{
	if (Scene == NULL || InMatineeActor == NULL) return false;
	
	// merge animation layer at first
	FbxAnimStack* AnimStack = Scene->GetMember<FbxAnimStack>(0);
	if (!AnimStack) return false;
		
	MergeAllLayerAnimation(AnimStack, FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()));

	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	if (AnimLayer == NULL) return false;

	// If the Matinee editor is not open, we need to initialize the sequence.
	//bool InitializeMatinee = InMatineeActor->MatineeData == NULL;
	//if (InitializeMatinee)
	//{
		// Force the initialization of the sequence
		// This sets the sequence in editor mode as well?
	//	InMatineeActor->InitInterp();
	//}

	UInterpData* MatineeData = InMatineeActor->MatineeData;
	float InterpLength = -1.0f;

	FbxNode* RootNode = Scene->GetRootNode();
	int32 NodeCount = RootNode->GetChildCount();
	for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
	{
		FbxNode* Node = RootNode->GetChild(NodeIndex);

		AActor* Actor = NULL;

		const bool bIsCameraAnim = InMatineeActor->IsA<AMatineeActorCameraAnim>();

		// Find a group instance to import into
		UInterpGroupInst* FoundGroupInst = nullptr;
		if ( bIsCameraAnim )
		{
			// We can only ever import into the camera anim group
			FoundGroupInst = InMatineeActor->GroupInst[0];
		}
		else
		{
			// Check to see if the scene node name matches a Matinee group name
			FoundGroupInst = InMatineeActor->FindFirstGroupInstByName( FString( Node->GetName() ) );
		}

		if( FoundGroupInst != NULL )
		{
			// OK, we found an actor bound to a Matinee group that matches this scene node name
			Actor = FoundGroupInst->GetGroupActor();
		}


		// Attempt to name-match the scene node with one of the actors.
		if( Actor == NULL )
		{
			Actor = FindObject<AActor>( ANY_PACKAGE, UTF8_TO_TCHAR(Node->GetName()) );
		}

		FbxCamera* CameraNode = NULL;
		if ( Actor == NULL || Actor->IsPendingKill() )
		{
			CameraNode = FindCamera(Node);
			if ( bCreateUnknownCameras && CameraNode != NULL )
			{
				Actor = GEditor->AddActor( InMatineeActor->GetWorld()->GetCurrentLevel(), ACameraActor::StaticClass(), FTransform::Identity );
				Actor->SetActorLabel( UTF8_TO_TCHAR(CameraNode->GetName()) );
			}
			else
			{
				continue;
			}
		}

		UInterpGroupInst* MatineeGroup = InMatineeActor->FindGroupInst(Actor);

		// Before attempting to create/import a movement track: verify that the node is animated.
		bool IsAnimated = IsNodeAnimated(Node, AnimLayer);

		if (IsAnimated)
		{
			if (MatineeGroup == NULL)
			{
				MatineeGroup = CreateMatineeGroup(InMatineeActor, Actor, FString(Node->GetName()));
			}
			else if ( bIsCameraAnim )
			{
				MatineeGroup->Group->GroupName = Node->GetName();
			}

			float TimeLength = ImportMatineeActor(Node, MatineeGroup);
			InterpLength = FMath::Max(InterpLength, TimeLength);
		}

		// Right now, cameras are the only supported import entity type.
		if (Actor->IsA(ACameraActor::StaticClass()))
		{
			// there is a pivot node between the FbxNode and node attribute
			if( !CameraNode )
			{
				CameraNode = FindCamera(Node);
			}

			if (CameraNode)
			{
				if (MatineeGroup == NULL)
				{
					MatineeGroup = CreateMatineeGroup(InMatineeActor, Actor, FString(Node->GetName()));
				}
				else if ( bIsCameraAnim )
				{
					MatineeGroup->Group->GroupName = Node->GetName();
				}

				ImportCamera((ACameraActor*) Actor, MatineeGroup, CameraNode);
			}
		}

		if (MatineeGroup != NULL)
		{
			MatineeGroup->Modify();
		}
	}

	MatineeData->InterpLength = (InterpLength < 0.0f) ? 5.0f : InterpLength;
	InMatineeActor->Modify();

	return true;
	//if (InitializeMatinee)
	//{
	//	InMatineeActor->TermInterp();
	//}
}

void FFbxImporter::ImportCamera(ACameraActor* Actor, UInterpGroupInst* MatineeGroup, FbxCamera* Camera)
{
	// Get the real camera node that stores customed camera attributes
	// Note: there is a pivot node between the Fbx camera Node and node attribute
	FbxNode* FbxCameraNode = Camera->GetNode()->GetParent();
	// Import the aspect ratio
	Actor->GetCameraComponent()->AspectRatio = Camera->FilmAspectRatio.Get(); // Assumes the FBX comes from Unreal or Maya
	ImportAnimatedProperty(&Actor->GetCameraComponent()->AspectRatio, TEXT("AspectRatio"), MatineeGroup, 
				Actor->GetCameraComponent()->AspectRatio, FbxCameraNode->FindProperty("UE_AspectRatio") );

	FbxPropertyT<FbxDouble> AperatureModeProperty;


	if( Camera->FocalLength.IsValid() && Camera->GetApertureMode() == FbxCamera::eFocalLength )
	{
		Actor->GetCameraComponent()->FieldOfView = Camera->ComputeFieldOfView(Camera->FocalLength.Get()); // Assumes the FBX comes from Unreal or Maya

		AperatureModeProperty = Camera->FocalLength;
	}
	else 
	{
		Actor->GetCameraComponent()->FieldOfView = Camera->FieldOfView.Get();

		AperatureModeProperty = Camera->FieldOfView;
	}


	ImportAnimatedProperty(&Actor->GetCameraComponent()->FieldOfView, TEXT("FOVAngle"), MatineeGroup, AperatureModeProperty.Get(), AperatureModeProperty, true, Camera );
}

void FFbxImporter::ImportAnimatedProperty(float* Value, const TCHAR* ValueName, UInterpGroupInst* MatineeGroup, const float FbxValue, FbxProperty InProperty, bool bImportFOV, FbxCamera* Camera )
{
	if (Scene == NULL || Value == NULL || MatineeGroup == NULL) return;

	// Retrieve the FBX animated element for this value and verify that it contains an animation curve.
	if ( !InProperty.IsValid() || !InProperty.GetFlag(FbxPropertyFlags::eAnimatable) )
	{
		return;
	}
	
	// verify the animation curve and it has valid key
	FbxAnimCurveNode* CurveNode = InProperty.GetCurveNode();
	if (!CurveNode)
	{
		return;
	}
	FbxAnimCurve* FbxCurve = CurveNode->GetCurve(0U);
	if (!FbxCurve || FbxCurve->KeyGetCount() <= 1)
	{
		return;
	}
	
	*Value = FbxValue;

	// Look for a track for this property in the Matinee group.
	UInterpTrackFloatProp* PropertyTrack = NULL;
	int32 TrackCount = MatineeGroup->Group->InterpTracks.Num();
	for (int32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
	{
		UInterpTrackFloatProp* Track = Cast<UInterpTrackFloatProp>( MatineeGroup->Group->InterpTracks[TrackIndex] );
		if (Track != NULL && Track->PropertyName == ValueName)
		{
			PropertyTrack = Track;
			PropertyTrack->FloatTrack.Reset(); // Remove all the existing keys from this track.
			break;
		}
	}

	// If a track for this property was not found, create one.
	if (PropertyTrack == NULL)
	{
		PropertyTrack = NewObject<UInterpTrackFloatProp>(MatineeGroup->Group, NAME_None, RF_Transactional);
		MatineeGroup->Group->InterpTracks.Add(PropertyTrack);
		UInterpTrackInstFloatProp* PropertyTrackInst = NewObject<UInterpTrackInstFloatProp>(MatineeGroup, NAME_None, RF_Transactional);
		MatineeGroup->TrackInst.Add(PropertyTrackInst);
		PropertyTrack->PropertyName = ValueName;
		PropertyTrack->TrackTitle = ValueName;
		PropertyTrackInst->InitTrackInst(PropertyTrack);
	}
	FInterpCurveFloat& Curve = PropertyTrack->FloatTrack;


	int32 KeyCount = FbxCurve->KeyGetCount();
	// create each key in the first path
	// for animation curve for all property in one track, they share time and interpolation mode in animation keys
	for (int32 KeyIndex = Curve.Points.Num(); KeyIndex < KeyCount; ++KeyIndex)
	{
		FbxAnimCurveKey CurKey = FbxCurve->KeyGet(KeyIndex);

		// Create the curve keys
		FInterpCurvePoint<float> Key;
		Key.InVal = CurKey.GetTime().GetSecondDouble();

		Key.InterpMode = GetUnrealInterpMode(CurKey);

		// Add this new key to the curve
		Curve.Points.Add(Key);
	}

	// Fill in the curve keys with the correct data for this dimension.
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FbxAnimCurveKey CurKey = FbxCurve->KeyGet(KeyIndex);
		FInterpCurvePoint<float>& UnrealKey = Curve.Points[ KeyIndex ];
		
		float OutVal;

		if( bImportFOV && Camera && Camera->GetApertureMode() == FbxCamera::eFocalLength )
		{
			OutVal = Camera->ComputeFieldOfView( CurKey.GetValue() );
		}
		else
		{
			OutVal = CurKey.GetValue();
		}

		float ArriveTangent = 0.0f;
		float LeaveTangent = 0.0f;

		// Convert the Bezier control points, if available, into Hermite tangents
		if( CurKey.GetInterpolation() == FbxAnimCurveDef::eInterpolationCubic )
		{
			float LeftTangent = FbxCurve->KeyGetLeftDerivative(KeyIndex);
			float RightTangent = FbxCurve->KeyGetRightDerivative(KeyIndex);

			if (KeyIndex > 0)
			{
				ArriveTangent = LeftTangent * (CurKey.GetTime().GetSecondDouble() - FbxCurve->KeyGetTime(KeyIndex-1).GetSecondDouble());
			}

			if (KeyIndex < KeyCount - 1)
			{
				LeaveTangent = RightTangent * (FbxCurve->KeyGetTime(KeyIndex+1).GetSecondDouble() - CurKey.GetTime().GetSecondDouble());
			}
		}

		UnrealKey.OutVal = OutVal;
		UnrealKey.ArriveTangent = ArriveTangent;
		UnrealKey.LeaveTangent = LeaveTangent;
	}
}

UInterpGroupInst* FFbxImporter::CreateMatineeGroup(AMatineeActor* InMatineeActor, AActor* Actor, FString GroupName)
{
	// There are no groups for this actor: create the Matinee group data structure.
	UInterpGroup* MatineeGroupData = NewObject<UInterpGroup>(InMatineeActor->MatineeData, NAME_None, RF_Transactional);
	MatineeGroupData->GroupName = FName( *GroupName );
	InMatineeActor->MatineeData->InterpGroups.Add(MatineeGroupData);

	// Instantiate the Matinee group data structure.
	UInterpGroupInst* MatineeGroup = NewObject<UInterpGroupInst>(InMatineeActor, NAME_None, RF_Transactional);
	InMatineeActor->GroupInst.Add(MatineeGroup);
	MatineeGroup->InitGroupInst(MatineeGroupData, Actor);
	MatineeGroup->SaveGroupActorState();
	InMatineeActor->InitGroupActorForGroup( MatineeGroupData, Actor );

	return MatineeGroup;
}


/**
 * Imports a FBX scene node into a Matinee actor group.
 */
float FFbxImporter::ImportMatineeActor(FbxNode* Node, UInterpGroupInst* MatineeGroup)
{
	FName DefaultName(NAME_None);

	if (Scene == NULL || Node == NULL || MatineeGroup == NULL) return -1.0f;

	// Bake the pivots.
	// Based in sample code in kfbxnode.h, re: Pivot Management
	{
		FbxVector4 ZeroVector(0,0,0);
		Node->SetPivotState(FbxNode::eSourcePivot, FbxNode::ePivotActive);
		Node->SetPivotState(FbxNode::eDestinationPivot, FbxNode::ePivotActive);

		EFbxRotationOrder RotationOrder;
		Node->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);
		Node->SetRotationOrder(FbxNode::eDestinationPivot, RotationOrder);

		// For cameras and lights (without targets) let's compensate the postrotation.
		if (Node->GetCamera() || Node->GetLight())
		{
			if (!Node->GetTarget())
			{
				FbxVector4 RotationVector(90, 0, 0);
				if (Node->GetCamera())    
					RotationVector.Set(0, 90, 0);

				FbxAMatrix RotationMtx;
				RotationMtx.SetR(RotationVector);

				FbxVector4 PostRotationVector = Node->GetPostRotation(FbxNode::eSourcePivot);

				// Rotation order don't affect post rotation, so just use the default XYZ order
				FbxAMatrix lSourceR;
				lSourceR.SetR(PostRotationVector);

				RotationMtx = lSourceR * RotationMtx;

				PostRotationVector = RotationMtx.GetR();

				Node->SetPostRotation(FbxNode::eSourcePivot, PostRotationVector);
			}

			// Point light do not need to be adjusted (since they radiate in all the directions).
			if (Node->GetLight() && Node->GetLight()->LightType.Get() == FbxLight::ePoint)
			{
				Node->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(0,0,0,0));
			}

			// apply Pre rotations only on bones / end of chains
			if((Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
				|| (Node->GetMarker() && Node->GetMarker()->GetType() == FbxMarker::eEffectorFK)
				|| (Node->GetMarker() && Node->GetMarker()->GetType() == FbxMarker::eEffectorIK))
			{
				Node->SetPreRotation(FbxNode::eDestinationPivot, Node->GetPreRotation(FbxNode::eSourcePivot));

				// No pivots on bones
				Node->SetRotationPivot(FbxNode::eDestinationPivot, ZeroVector);    
				Node->SetScalingPivot(FbxNode::eDestinationPivot,  ZeroVector);    
				Node->SetRotationOffset(FbxNode::eDestinationPivot,ZeroVector);    
				Node->SetScalingOffset(FbxNode::eDestinationPivot, ZeroVector);
			}
			else
			{
				// any other type: no pre-rotation support but...
				Node->SetPreRotation(FbxNode::eDestinationPivot, ZeroVector);

				// support for rotation and scaling pivots.
				Node->SetRotationPivot(FbxNode::eDestinationPivot, Node->GetRotationPivot(FbxNode::eSourcePivot));    
				Node->SetScalingPivot(FbxNode::eDestinationPivot,  Node->GetScalingPivot(FbxNode::eSourcePivot));    
				// Rotation and scaling offset are supported
				Node->SetRotationOffset(FbxNode::eDestinationPivot, Node->GetRotationOffset(FbxNode::eSourcePivot));    
				Node->SetScalingOffset(FbxNode::eDestinationPivot,  Node->GetScalingOffset(FbxNode::eSourcePivot));
				//
				// If we supported scaling pivots, we could simply do:
				// Node->SetRotationPivot(FbxNode::eDESTINATION_SET, ZeroVector);
				// Node->SetScalingPivot(FbxNode::eDESTINATION_SET, ZeroVector);
			}
		}

		// Recursively convert the animation data according to pivot settings.
		Node->ConvertPivotAnimationRecursive(
			NULL,																// Use the first animation stack by default
			FbxNode::eDestinationPivot,											// Convert from Source set to Destination set
			FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()),	// Resampling frame rate in frames per second
			false);																// Do not apply key reducing filter
	}


	// Search for a Movement track in the Matinee group.
	UInterpTrackMove* MovementTrack = NULL;
	int32 TrackCount = MatineeGroup->Group->InterpTracks.Num();
	for (int32 TrackIndex = 0; TrackIndex < TrackCount && MovementTrack == NULL; ++TrackIndex)
	{
		MovementTrack = Cast<UInterpTrackMove>( MatineeGroup->Group->InterpTracks[TrackIndex] );
	}

	// Check whether the actor should be pivoted in the FBX document.

	AActor* Actor = MatineeGroup->GetGroupActor();
	check( Actor != NULL ); // would this ever be triggered?

	// Find out whether the FBX node is animated.
	// Bucket the transforms at the same time.
	// The Matinee Movement track can take in a Translation vector
	// and three animated Euler rotation angles.
	FbxAnimStack* AnimStack = Scene->GetMember<FbxAnimStack>(0);
	if (!AnimStack) return -1.0f;

	MergeAllLayerAnimation(AnimStack, FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()));

	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	if (AnimLayer == NULL) return -1.0f;
	
	bool bNodeAnimated = IsNodeAnimated(Node, AnimLayer);
	bool ForceImportSampling = false;

	// Add a Movement track if the node is animated and the group does not already have one.
	if (MovementTrack == NULL && bNodeAnimated)
	{
		MovementTrack = NewObject<UInterpTrackMove>(MatineeGroup->Group, NAME_None, RF_Transactional);
		MatineeGroup->Group->InterpTracks.Add(MovementTrack);
		UInterpTrackInstMove* MovementTrackInst = NewObject<UInterpTrackInstMove>(MatineeGroup, NAME_None, RF_Transactional);
		MatineeGroup->TrackInst.Add(MovementTrackInst);
		MovementTrackInst->InitTrackInst(MovementTrack);
	}

	// List of casted subtracks in this movement track.
	TArray< UInterpTrackMoveAxis* > SubTracks;

	// Remove all the keys in the Movement track
	if (MovementTrack != NULL)
	{
		MovementTrack->PosTrack.Reset();
		MovementTrack->EulerTrack.Reset();
		MovementTrack->LookupTrack.Points.Reset();

		if( MovementTrack->SubTracks.Num() > 0 )
		{
			for( int32 SubTrackIndex = 0; SubTrackIndex < MovementTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrackMoveAxis* SubTrack = CastChecked<UInterpTrackMoveAxis>( MovementTrack->SubTracks[ SubTrackIndex ] );
				SubTrack->FloatTrack.Reset();
				SubTrack->LookupTrack.Points.Reset();
				SubTracks.Add( SubTrack );
			}
		}
	}

	float TimeLength = -1.0f;

	// Fill in the Movement track with the FBX keys
	if (bNodeAnimated)
	{
		// Check: The position and rotation tracks must have the same number of keys, the same key timings and
		// the same segment interpolation types.
		FbxAnimCurve *TransCurves[6], *RealCurves[6];
		
		TransCurves[0] = Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		TransCurves[1] = Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		TransCurves[2] = Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		TransCurves[3] = Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		TransCurves[4] = Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		TransCurves[5] = Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
		// remove empty curves
		int32 CurveIndex;
		int32 RealCurveNum = 0;
		for (CurveIndex = 0; CurveIndex < 6; CurveIndex++)
		{
			if (TransCurves[CurveIndex] && TransCurves[CurveIndex]->KeyGetCount() > 1)
			{
				RealCurves[RealCurveNum++] = TransCurves[CurveIndex];
			}
		}
		
		bool bResample = false;
		if (RealCurveNum > 1)
		{
			int32 KeyCount = RealCurves[0]->KeyGetCount();
			// check key count of all curves
			for (CurveIndex = 1; CurveIndex < RealCurveNum; CurveIndex++)
			{
				if (KeyCount != RealCurves[CurveIndex]->KeyGetCount())
				{
					bResample = true;
					break;
				}
			}
			// check key time for each key
			for (int32 KeyIndex = 0; !bResample && KeyIndex < KeyCount; KeyIndex++)
			{
				FbxTime KeyTime = RealCurves[0]->KeyGetTime(KeyIndex);
				FbxAnimCurveDef::EInterpolationType Interpolation = RealCurves[0]->KeyGetInterpolation(KeyIndex);
				//KFbxAnimCurveDef::ETangentMode Tangent = RealCurves[0]->KeyGetTangentMode(KeyIndex);
				
				for (CurveIndex = 1; CurveIndex < RealCurveNum; CurveIndex++)
				{
					if (KeyTime != RealCurves[CurveIndex]->KeyGetTime(KeyIndex) ||
						Interpolation != RealCurves[CurveIndex]->KeyGetInterpolation(KeyIndex) ) // ||
						//Tangent != RealCurves[CurveIndex]->KeyGetTangentMode(KeyIndex))
					{
						bResample = true;
						break;
					}
				}
			}
			
			if (bResample)
			{
				// Get the re-sample time span
				FbxTime Start, Stop;
				Start = RealCurves[0]->KeyGetTime(0);
				Stop = RealCurves[0]->KeyGetTime(RealCurves[0]->KeyGetCount() - 1);
				for (CurveIndex = 1; CurveIndex < RealCurveNum; CurveIndex++)
				{
					if (Start > RealCurves[CurveIndex]->KeyGetTime(0))
					{
						Start = RealCurves[CurveIndex]->KeyGetTime(0);
					}
					
					if (Stop < RealCurves[CurveIndex]->KeyGetTime(RealCurves[CurveIndex]->KeyGetCount() - 1))
					{
						Stop = RealCurves[CurveIndex]->KeyGetTime(RealCurves[CurveIndex]->KeyGetCount() - 1);
					}
				}
				
				double ResampleRate;
				ResampleRate = FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode());
				FbxTime FramePeriod;
				FramePeriod.SetSecondDouble(1.0 / ResampleRate);
				
				for (CurveIndex = 0; CurveIndex < 6; CurveIndex++)
				{
					bool bRemoveConstantKey = false;
					// for the constant animation curve, the key may be not in the resample time range,
					// so we need to remove the constant key after resample,
					// otherwise there must be one more key
					if (TransCurves[CurveIndex]->KeyGetCount() == 1 && TransCurves[CurveIndex]->KeyGetTime(0) < Start)
					{
						bRemoveConstantKey = true;
					}

					// only re-sample from Start to Stop
					FbxAnimCurveFilterResample CurveResampler;
					CurveResampler.SetPeriodTime(FramePeriod);
					CurveResampler.SetStartTime(Start);
					CurveResampler.SetStopTime(Stop);
					CurveResampler.SetKeysOnFrame(true);
					CurveResampler.Apply(*TransCurves[CurveIndex]);

					// remove the key that is not in the resample time range
					// the constant key always at the time 0, so it is OK to remove the first key
					if (bRemoveConstantKey)
					{
						TransCurves[CurveIndex]->KeyRemove(0);
					}
				}

			}
			
		}


		FbxAMatrix Matrix		= ComputeTotalMatrix(Node);
		FbxVector4 DefaultPos	= Node->LclTranslation.Get();
		FbxVector4 DefaultRot	= Node->LclRotation.Get();

		FbxAMatrix FbxCamToUnrealRHMtx, InvFbxCamToUnrealRHMtx;
		FbxAMatrix UnrealRHToUnrealLH, InUnrealRHToUnrealLH;

		Actor->SetActorLocation( FVector( -DefaultPos[1], -DefaultPos[0], DefaultPos[2] ), false );
	
		bool bIsCamera = false;
		if (!Node->GetCamera())
		{
			Actor->SetActorRotation( FRotator::MakeFromEuler( FVector( DefaultRot[0], -DefaultRot[1], -DefaultRot[2] ) ) );
		}
		else
		{
			// Note: the camera rotations contain rotations from the Fbx Camera to the converted
			// right-hand Unreal coordinates.  So we must negate the Fbx Camera -> Unreal WorldRH, then convert
			// the remaining rotation to left-handed coordinates

			// Describing coordinate systems as <Up, Forward, Left>:
			// Fbx Camera:					< Y, -Z, -X>
			// Unreal Right-handed World:	< Z, -Y,  X>
			// Unreal Left-handed World:	< Z,  X, -Y>

			FbxAMatrix DefaultRotMtx;
			DefaultRotMtx.SetR(DefaultRot);

			FbxCamToUnrealRHMtx[0] = FbxVector4(-1.f,  0.f,  0.f, 0.f );
			FbxCamToUnrealRHMtx[1] = FbxVector4( 0.f,  0.f,  1.f, 0.f );
			FbxCamToUnrealRHMtx[2] = FbxVector4( 0.f,  1.f,  0.f, 0.f );
			InvFbxCamToUnrealRHMtx = FbxCamToUnrealRHMtx.Inverse();

			UnrealRHToUnrealLH[0] = FbxVector4( 0.f,  1.f,  0.f, 0.f );
			UnrealRHToUnrealLH[1] = FbxVector4( 1.f,  0.f,  0.f, 0.f );
			UnrealRHToUnrealLH[2] = FbxVector4( 0.f,  0.f,  1.f, 0.f );
			InUnrealRHToUnrealLH = UnrealRHToUnrealLH.Inverse();

			// Remove the FbxCamera's local to world rotation
			FbxAMatrix UnrealCameraRotationMtx		= DefaultRotMtx * InvFbxCamToUnrealRHMtx;

			// Convert the remaining rotation into world space
			UnrealCameraRotationMtx					= UnrealRHToUnrealLH * UnrealCameraRotationMtx * InUnrealRHToUnrealLH;

			FbxVector4 UnrealCameraRotationEuler	= UnrealCameraRotationMtx.GetR();

			Actor->SetActorRotation( FRotator::MakeFromEuler( FVector( UnrealCameraRotationEuler[0], UnrealCameraRotationEuler[1], UnrealCameraRotationEuler[2] ) ) );
			bIsCamera = true;
		}

		if (MovementTrack && MovementTrack->SubTracks.Num() > 0)
		{
			check (bIsCamera == false);
			ImportMoveSubTrack(TransCurves[0], 0, SubTracks[0], 0, false, RealCurves[0], DefaultPos[0]);
			ImportMoveSubTrack(TransCurves[1], 1, SubTracks[1], 1, true, RealCurves[0], DefaultPos[1]);
			ImportMoveSubTrack(TransCurves[2], 2, SubTracks[2], 2, false, RealCurves[0], DefaultPos[2]);
			ImportMoveSubTrack(TransCurves[3], 3, SubTracks[3], 0, false, RealCurves[0], DefaultRot[0]);
			ImportMoveSubTrack(TransCurves[4], 4, SubTracks[4], 1, true, RealCurves[0], DefaultRot[1]);
			ImportMoveSubTrack(TransCurves[5], 5, SubTracks[5], 2, true, RealCurves[0], DefaultRot[2]);

			for( int32 SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrackMoveAxis* SubTrack = SubTracks[ SubTrackIndex ];
				// Generate empty look-up keys.
				int32 KeyIndex;
				for ( KeyIndex = 0; KeyIndex < SubTrack->FloatTrack.Points.Num(); ++KeyIndex )
				{
					SubTrack->LookupTrack.AddPoint( SubTrack->FloatTrack.Points[ KeyIndex].InVal, DefaultName );
				}
			}

			float StartTime;
			// Scale the track timing to ensure that it is large enough
			MovementTrack->GetTimeRange( StartTime, TimeLength );
		}
		else if (MovementTrack)
		{
			ImportMatineeAnimated(TransCurves[0], MovementTrack->PosTrack, 1, true, RealCurves[0], DefaultPos[0]);
			ImportMatineeAnimated(TransCurves[1], MovementTrack->PosTrack, 0, true, RealCurves[0], DefaultPos[1]);
			ImportMatineeAnimated(TransCurves[2], MovementTrack->PosTrack, 2, false, RealCurves[0], DefaultPos[2]);

			if (bIsCamera)
			{
				// Import the rotation data unmodified
				ImportMatineeAnimated(TransCurves[3], MovementTrack->EulerTrack, 0, false, RealCurves[0], DefaultRot[0]);
				ImportMatineeAnimated(TransCurves[4], MovementTrack->EulerTrack, 1, false, RealCurves[0], DefaultRot[1]);
				ImportMatineeAnimated(TransCurves[5], MovementTrack->EulerTrack, 2, false, RealCurves[0], DefaultRot[2]);

				// Once the individual Euler channels are imported, then convert the rotation into Unreal coords
				for(int32 PointIndex = 0; PointIndex < MovementTrack->EulerTrack.Points.Num(); ++PointIndex)
				{
					FInterpCurvePoint<FVector>& CurveKey = MovementTrack->EulerTrack.Points[ PointIndex ];

					FbxAMatrix CurveMatrix;
					CurveMatrix.SetR( FbxVector4(CurveKey.OutVal.X, CurveKey.OutVal.Y, CurveKey.OutVal.Z) );

					// Remove the FbxCamera's local to world rotation
					FbxAMatrix UnrealCameraRotationMtx		= CurveMatrix * InvFbxCamToUnrealRHMtx;

					// Convert the remaining rotation into world space
					UnrealCameraRotationMtx					= UnrealRHToUnrealLH * UnrealCameraRotationMtx * InUnrealRHToUnrealLH;

					FbxVector4 UnrealCameraRotationEuler	= UnrealCameraRotationMtx.GetR();
					CurveKey.OutVal.X = UnrealCameraRotationEuler[0];
					CurveKey.OutVal.Y = UnrealCameraRotationEuler[1];
					CurveKey.OutVal.Z = UnrealCameraRotationEuler[2];
				}

				
				// The FInterpCurve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
				// will cause the camera to rotate all the way around through 0 degrees.  So here we make a second pass over the 
				// Euler track to convert the angles into a more interpolation-friendly format.  
				float CurrentAngleOffset[3] = { 0.f, 0.f, 0.f };
				for(int32 PointIndex = 1; PointIndex < MovementTrack->EulerTrack.Points.Num(); ++PointIndex)
				{
					const FInterpCurvePoint<FVector>& CurveKeyPrev	= MovementTrack->EulerTrack.Points[ PointIndex-1 ];
					FInterpCurvePoint<FVector>& CurveKey			= MovementTrack->EulerTrack.Points[ PointIndex ];
					
					FVector PreviousOutVal	= CurveKeyPrev.OutVal;
					FVector CurrentOutVal	= CurveKey.OutVal;

					for(int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
					{
						float DeltaAngle = (CurrentOutVal[AxisIndex] + CurrentAngleOffset[AxisIndex]) - PreviousOutVal[AxisIndex];

						if(DeltaAngle >= 180)
						{
							CurrentAngleOffset[AxisIndex] -= 360;
						}
						else if(DeltaAngle <= -180)
						{
							CurrentAngleOffset[AxisIndex] += 360;
						}

						CurrentOutVal[AxisIndex] += CurrentAngleOffset[AxisIndex];
					}

					CurveKey.OutVal = CurrentOutVal;
				}

				// we don't support different interpolation modes for position & rotation sub-tracks, so unify them here
				ConsolidateMovementTrackInterpModes(MovementTrack);

				// Recalculate the tangents using the new data
				MovementTrack->EulerTrack.AutoSetTangents();
			}
			else
			{
				ImportMatineeAnimated(TransCurves[3], MovementTrack->EulerTrack, 0, false, RealCurves[0], DefaultRot[0]);
				ImportMatineeAnimated(TransCurves[4], MovementTrack->EulerTrack, 1, true, RealCurves[0], DefaultRot[1]);
				ImportMatineeAnimated(TransCurves[5], MovementTrack->EulerTrack, 2, true, RealCurves[0], DefaultRot[2]);

				// we don't support different interpolation modes for position & rotation sub-tracks, so unify them here
				ConsolidateMovementTrackInterpModes(MovementTrack);
			}

			// Generate empty look-up keys.
			int32 KeyIndex;
			for ( KeyIndex = 0; KeyIndex < RealCurves[0]->KeyGetCount(); ++KeyIndex )
			{
				MovementTrack->LookupTrack.AddPoint( (float)RealCurves[0]->KeyGet(KeyIndex).GetTime().GetSecondDouble(), DefaultName );
			}

			// Scale the track timing to ensure that it is large enough
			TimeLength = (float)RealCurves[0]->KeyGet(KeyIndex - 1).GetTime().GetSecondDouble();
		}

	}

	// Inform the engine and UI that the tracks have been modified.
	if (MovementTrack != NULL)
	{
		MovementTrack->Modify();
	}
	MatineeGroup->Modify();
	
	return TimeLength;
}

void FFbxImporter::ConsolidateMovementTrackInterpModes(UInterpTrackMove* MovementTrack)
{
	check(MovementTrack->EulerTrack.Points.Num() == MovementTrack->PosTrack.Points.Num());
	for(int32 KeyIndex = 0; KeyIndex < MovementTrack->PosTrack.Points.Num(); KeyIndex++)
	{
		MovementTrack->EulerTrack.Points[KeyIndex].InterpMode = MovementTrack->PosTrack.Points[KeyIndex].InterpMode;
	}
}

EInterpCurveMode FFbxImporter::GetUnrealInterpMode(FbxAnimCurveKey FbxKey)
{
	EInterpCurveMode Mode = CIM_CurveUser;
	// Convert the interpolation type from FBX to Unreal.
	switch( FbxKey.GetInterpolation() )
	{
		case FbxAnimCurveDef::eInterpolationCubic:
		{
			switch (FbxKey.GetTangentMode())
			{
				// Auto tangents will now be imported as user tangents to allow the
				// user to modify them without inadvertently resetting other tangents
// 				case KFbxAnimCurveDef::eTANGENT_AUTO:
// 					if ((KFbxAnimCurveDef::eTANGENT_GENERIC_CLAMP & FbxKey.GetTangentMode(true)))
// 					{
// 						Mode = CIM_CurveAutoClamped;
// 					}
// 					else
// 					{
// 						Mode = CIM_CurveAuto;
// 					}
// 					break;
				case FbxAnimCurveDef::eTangentBreak:
					Mode = CIM_CurveBreak;
					break;
				case FbxAnimCurveDef::eTangentAuto:
					Mode = CIM_CurveAuto;
					break;
				case FbxAnimCurveDef::eTangentUser:
				case FbxAnimCurveDef::eTangentTCB:
					Mode = CIM_CurveUser;
					break;
				default:
					break;
			}
			break;
		}

		case FbxAnimCurveDef::eInterpolationConstant:
			if (FbxKey.GetTangentMode() != (FbxAnimCurveDef::ETangentMode)FbxAnimCurveDef::eConstantStandard)
			{
				// warning not support
				;
			}
			Mode = CIM_Constant;
			break;

		case FbxAnimCurveDef::eInterpolationLinear:
			Mode = CIM_Linear;
			break;
	}
	return Mode;
}

void FFbxImporter::ImportMoveSubTrack( FbxAnimCurve* FbxCurve, int32 FbxDimension, UInterpTrackMoveAxis* SubTrack, int32 CurveIndex, bool bNegative, FbxAnimCurve* RealCurve, float DefaultVal )
{
	if (CurveIndex >= 3) return;

	FInterpCurveFloat& Curve = SubTrack->FloatTrack;
	// the FBX curve has no valid keys, so fake the Unreal Matinee curve
	if (FbxCurve == NULL || FbxCurve->KeyGetCount() < 2)
	{
		int32 KeyIndex;
		for ( KeyIndex = Curve.Points.Num(); KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			float Time = (float)RealCurve->KeyGet(KeyIndex).GetTime().GetSecondDouble();
			// Create the curve keys
			FInterpCurvePoint<float> Key;
			Key.InVal = Time;
			Key.InterpMode = GetUnrealInterpMode(RealCurve->KeyGet(KeyIndex));

			Curve.Points.Add(Key);
		}

		for ( KeyIndex = 0; KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			FInterpCurvePoint<float>& Key = Curve.Points[ KeyIndex ];
			Key.OutVal = DefaultVal;
			Key.ArriveTangent = 0;
			Key.LeaveTangent = 0;
		}
	}
	else
	{
		int32 KeyCount = (int32) FbxCurve->KeyGetCount();

		for (int32 KeyIndex = Curve.Points.Num(); KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );

			// Create the curve keys
			FInterpCurvePoint<float> Key;
			Key.InVal = CurKey.GetTime().GetSecondDouble();

			Key.InterpMode = GetUnrealInterpMode(CurKey);

			// Add this new key to the curve
			Curve.Points.Add(Key);
		}

		// Fill in the curve keys with the correct data for this dimension.
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );
			FInterpCurvePoint<float>& UnrealKey = Curve.Points[ KeyIndex ];

			// Prepare the FBX values to import into the track key.
			// Convert the Bezier control points, if available, into Hermite tangents
			float OutVal = bNegative ? -CurKey.GetValue() : CurKey.GetValue();

			float ArriveTangent = 0.0f;
			float LeaveTangent = 0.0f;

			if( CurKey.GetInterpolation() == FbxAnimCurveDef::eInterpolationCubic )
			{
				ArriveTangent = bNegative? -FbxCurve->KeyGetLeftDerivative(KeyIndex): FbxCurve->KeyGetLeftDerivative(KeyIndex);
				LeaveTangent = bNegative? -FbxCurve->KeyGetRightDerivative(KeyIndex): FbxCurve->KeyGetRightDerivative(KeyIndex);
			}

			// Fill in the track key with the prepared values
			UnrealKey.OutVal = OutVal;
			UnrealKey.ArriveTangent = ArriveTangent;
			UnrealKey.LeaveTangent = LeaveTangent;
		}
	}
}

void FFbxImporter::ImportMatineeAnimated(FbxAnimCurve* FbxCurve, FInterpCurveVector& Curve, int32 CurveIndex, bool bNegative, FbxAnimCurve* RealCurve, float DefaultVal)
{
	if (CurveIndex >= 3) return;
	
	// the FBX curve has no valid keys, so fake the Unreal Matinee curve
	if (FbxCurve == NULL || FbxCurve->KeyGetCount() < 2)
	{
		int32 KeyIndex;
		for ( KeyIndex = Curve.Points.Num(); KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			float Time = (float)RealCurve->KeyGet(KeyIndex).GetTime().GetSecondDouble();
			// Create the curve keys
			FInterpCurvePoint<FVector> Key;
			Key.InVal = Time;
			Key.InterpMode = GetUnrealInterpMode(RealCurve->KeyGet(KeyIndex));
			
			Curve.Points.Add(Key);
		}
		
		for ( KeyIndex = 0; KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			FInterpCurvePoint<FVector>& Key = Curve.Points[ KeyIndex ];
			switch (CurveIndex)
			{
			case 0:
				Key.OutVal.X = DefaultVal;
				Key.ArriveTangent.X = 0;
				Key.LeaveTangent.X = 0;
				break;
			case 1:
				Key.OutVal.Y = DefaultVal;
				Key.ArriveTangent.Y = 0;
				Key.LeaveTangent.Y = 0;
				break;
			case 2:
			default:
				Key.OutVal.Z = DefaultVal;
				Key.ArriveTangent.Z = 0;
				Key.LeaveTangent.Z = 0;
				break;
			}
		}
	}
	else
	{
		int32 KeyCount = (int32) FbxCurve->KeyGetCount();
		
		for (int32 KeyIndex = Curve.Points.Num(); KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );

			// Create the curve keys
			FInterpCurvePoint<FVector> Key;
			Key.InVal = CurKey.GetTime().GetSecondDouble();

			Key.InterpMode = GetUnrealInterpMode(CurKey);

			// Add this new key to the curve
			Curve.Points.Add(Key);
		}

		// Fill in the curve keys with the correct data for this dimension.
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );
			FInterpCurvePoint<FVector>& UnrealKey = Curve.Points[ KeyIndex ];
			
			// Prepare the FBX values to import into the track key.
			// Convert the Bezier control points, if available, into Hermite tangents
			float OutVal = ( bNegative ? -CurKey.GetValue() : CurKey.GetValue() );

			float ArriveTangent = 0.0f;
			float LeaveTangent = 0.0f;
			
			if( CurKey.GetInterpolation() == FbxAnimCurveDef::eInterpolationCubic )
			{
				ArriveTangent = bNegative? -FbxCurve->KeyGetLeftDerivative(KeyIndex): FbxCurve->KeyGetLeftDerivative(KeyIndex);
				LeaveTangent = bNegative? -FbxCurve->KeyGetRightDerivative(KeyIndex): FbxCurve->KeyGetRightDerivative(KeyIndex);
			}

			// Fill in the track key with the prepared values
			switch (CurveIndex)
			{
			case 0:
				UnrealKey.OutVal.X = OutVal;
				UnrealKey.ArriveTangent.X = ArriveTangent;
				UnrealKey.LeaveTangent.X = LeaveTangent;
				break;
			case 1:
				UnrealKey.OutVal.Y = OutVal;
				UnrealKey.ArriveTangent.Y = ArriveTangent;
				UnrealKey.LeaveTangent.Y = LeaveTangent;
				break;
			case 2:
			default:
				UnrealKey.OutVal.Z = OutVal;
				UnrealKey.ArriveTangent.Z = ArriveTangent;
				UnrealKey.LeaveTangent.Z = LeaveTangent;
				break;
			}
		}
	}
}

} // namespace UnFBX

