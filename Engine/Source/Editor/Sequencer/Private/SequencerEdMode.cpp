// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerEdMode.h"
#include "EditorViewportClient.h"
#include "Curves/KeyHandle.h"
#include "ISequencer.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Sequencer.h"
#include "Framework/Application/SlateApplication.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "SequencerCommonHelpers.h"
#include "MovieSceneHitProxy.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "SubtitleManager.h"
#include "SequencerMeshTrail.h"
#include "SequencerKeyActor.h"
#include "EditorWorldExtension.h"
#include "ViewportWorldInteraction.h"
#include "Evaluation/MovieScene3DTransformTemplate.h"

const FEditorModeID FSequencerEdMode::EM_SequencerMode(TEXT("EM_SequencerMode"));

FSequencerEdMode::FSequencerEdMode()
{
	FSequencerEdModeTool* SequencerEdModeTool = new FSequencerEdModeTool(this);

	Tools.Add( SequencerEdModeTool );
	SetCurrentTool( SequencerEdModeTool );

	// todo vreditor: make this a setting
	bDrawMeshTrails = true;
}

FSequencerEdMode::~FSequencerEdMode()
{
}

void FSequencerEdMode::Enter()
{
	FEdMode::Enter();
}

void FSequencerEdMode::Exit()
{
	CleanUpMeshTrails();

	Sequencers.Reset();

	FEdMode::Exit();
}

bool FSequencerEdMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	// Compatible with all modes so that we can take over with the sequencer hotkeys
	return true;
}

bool FSequencerEdMode::InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event )
{
	TSharedPtr<FSequencer> ActiveSequencer;

	for (TWeakPtr<FSequencer> WeakSequencer : Sequencers)
	{
		ActiveSequencer = WeakSequencer.Pin();
		if (ActiveSequencer.IsValid())
		{
			break;
		}
	}

	if (ActiveSequencer.IsValid() && Event != IE_Released)
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();

		if (ActiveSequencer->GetCommandBindings(ESequencerCommandBindings::Shared).Get()->ProcessCommandBindings(Key, KeyState, (Event == IE_Repeat) ))
		{
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

void FSequencerEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

#if WITH_EDITORONLY_DATA
	// Draw spline trails using the PDI
	if (View->Family->EngineShowFlags.Splines)
	{
		DrawTracks3D(PDI);
	}
	// Draw mesh trails (doesn't use the PDI)
	else if (bDrawMeshTrails)
	{
		PDI = nullptr;
		DrawTracks3D(PDI);
	}
#endif
}

void FSequencerEdMode::DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient,Viewport,View,Canvas);

	if( ViewportClient->AllowsCinematicPreview() )
	{
		// Get the size of the viewport
		const int32 SizeX = Viewport->GetSizeXY().X;
		const int32 SizeY = Viewport->GetSizeXY().Y;

		// Draw subtitles (toggle is handled internally)
		FVector2D MinPos(0.f, 0.f);
		FVector2D MaxPos(1.f, .9f);
		FIntRect SubtitleRegion(FMath::TruncToInt(SizeX * MinPos.X), FMath::TruncToInt(SizeY * MinPos.Y), FMath::TruncToInt(SizeX * MaxPos.X), FMath::TruncToInt(SizeY * MaxPos.Y));
		FSubtitleManager::GetSubtitleManager()->DisplaySubtitles( Canvas, SubtitleRegion, ViewportClient->GetWorld()->GetAudioTimeSeconds() );
	}
}

void FSequencerEdMode::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FMeshTrailData& MeshTrail : MeshTrails)
	{
		Collector.AddReferencedObject(MeshTrail.Track);
		Collector.AddReferencedObject(MeshTrail.Trail);
	}
}

void FSequencerEdMode::OnKeySelected(FViewport* Viewport, HMovieSceneKeyProxy* KeyProxy)
{
	bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bAltDown = Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt);
	bool bShiftDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);

	for (TWeakPtr<FSequencer> WeakSequencer : Sequencers)
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			Sequencer->SetLocalTimeDirectly(KeyProxy->Key.Time);

			for (const FTrajectoryKey::FData KeyData : KeyProxy->Key.KeyData)
			{
				if (UMovieSceneSection* Section = KeyData.Section.Get())
				{
					Sequencer->SelectTrackKeys(Section, KeyProxy->Key.Time, bShiftDown, bCtrlDown);
				}
			}
		}
	}
}

void FSequencerEdMode::DrawMeshTransformTrailFromKey(const class ASequencerKeyActor* KeyActor)
{
	ASequencerMeshTrail* Trail = Cast<ASequencerMeshTrail>(KeyActor->GetOwner());
	if(Trail != nullptr)
	{
		FMeshTrailData* TrailPtr = MeshTrails.FindByPredicate([Trail](const FMeshTrailData InTrail)
		{
			return Trail == InTrail.Trail;
		});
		if(TrailPtr != nullptr)
		{
			// From the key, get the mesh trail, and then the track associated with that mesh trail
			UMovieScene3DTransformTrack* Track = TrailPtr->Track;
			// Draw a mesh trail for the key's associated actor
			TArray<TWeakObjectPtr<UObject>> KeyObjects;
			AActor* TrailActor = KeyActor->GetAssociatedActor();
			KeyObjects.Add(TrailActor);
			FPrimitiveDrawInterface* PDI = nullptr;

			for (TWeakPtr<FSequencer> WeakSequencer : Sequencers)
			{
				TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
				if (Sequencer.IsValid())
				{
					DrawTransformTrack(Sequencer, PDI, Track, KeyObjects, true);
				}
			}
		}
	}
}

void FSequencerEdMode::CleanUpMeshTrails()
{
	// Clean up any existing trails
	for (FMeshTrailData& MeshTrail : MeshTrails)
	{
		if (MeshTrail.Trail)
		{
			MeshTrail.Trail->Cleanup();
		}
	}
	MeshTrails.Empty();
}

namespace SequencerEdMode_Draw3D
{
static const FColor	KeySelectedColor(255,128,0);
static const float	DrawTrackTimeRes = 0.1f;
static const float	CurveHandleScale = 0.5f;
}

FTransform FSequencerEdMode::GetRefFrame(const TSharedPtr<FSequencer>& Sequencer, const UObject* InObject, float KeyTime)
{
	FTransform RefTM = FTransform::Identity;

	const AActor* Actor = Cast<AActor>(InObject);
	if (Actor != nullptr)
	{
		RefTM = GetRefFrame(Sequencer, Actor, KeyTime);
	}
	else
	{
		const USceneComponent* SceneComponent = Cast<USceneComponent>(InObject);

		if (SceneComponent != nullptr)
		{
			RefTM = GetRefFrame(Sequencer, SceneComponent, KeyTime);
		}
	}

	return RefTM;
}

FTransform FSequencerEdMode::GetRefFrame(const TSharedPtr<FSequencer>& Sequencer, const AActor* Actor, float KeyTime)
{
	FTransform RefTM = FTransform::Identity;

	if (Actor != nullptr && Actor->GetRootComponent() != nullptr && Actor->GetRootComponent()->GetAttachParent() != nullptr)
	{
		RefTM = Actor->GetRootComponent()->GetAttachParent()->GetSocketTransform(Actor->GetRootComponent()->GetAttachSocketName());
	}

	return RefTM;
}

FTransform FSequencerEdMode::GetRefFrame(const TSharedPtr<FSequencer>& Sequencer, const USceneComponent* SceneComponent, float KeyTime)
{
	FTransform RefTM = FTransform::Identity;

	if (SceneComponent != nullptr && SceneComponent->GetAttachParent() != nullptr)
	{
		FTransform ParentRefTM = GetRefFrame(Sequencer, SceneComponent->GetAttachParent(), KeyTime);

		// If our parent is the root component, get the RefFrame from the Actor
		if (SceneComponent->GetAttachParent() == SceneComponent->GetOwner()->GetRootComponent())
		{
			ParentRefTM = GetRefFrame(Sequencer, SceneComponent->GetAttachParent()->GetOwner(), KeyTime);
		}
		else
		{
			ParentRefTM = GetRefFrame(Sequencer, SceneComponent->GetAttachParent(), KeyTime);
		}
		
		FTransform CurrentRefTM = SceneComponent->GetAttachParent()->GetRelativeTransform();

		// Check if our parent is animated in this Sequencer

		UObject* ParentObject = SceneComponent->GetAttachParent() == SceneComponent->GetOwner()->GetRootComponent() ? static_cast<UObject*>(SceneComponent->GetOwner()) : SceneComponent->GetAttachParent();
		FGuid ObjectBinding = Sequencer->FindObjectId(*ParentObject, Sequencer->GetFocusedTemplateID());

		if (ObjectBinding.IsValid())
		{
			const TSharedPtr< FSequencerDisplayNode >& ObjectNode = Sequencer->GetNodeTree()->GetObjectBindingMap()[ObjectBinding];

			for (const TSharedRef< FSequencerDisplayNode >& ChildNode : ObjectNode->GetChildNodes())
			{
				if (ChildNode->GetType() == ESequencerNode::Track)
				{
					const TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(ChildNode);
					const UMovieSceneTrack* TrackNodeTrack = TrackNode->GetTrack();
					const UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(TrackNodeTrack);

					if (TransformTrack != nullptr)
					{
						for (const UMovieSceneSection* Section : TransformTrack->GetAllSections())
						{
							if (Section->IsTimeWithinSection(KeyTime))
							{
								const UMovieScene3DTransformSection* ParentSection = Cast<UMovieScene3DTransformSection>(Section);

								if (ParentSection != nullptr)
								{
									FVector ParentKeyPos;
									FRotator ParentKeyRot;

									const FMovieSceneEvaluationTemplateInstance* TemplateInstance = Sequencer->GetEvaluationTemplate().GetInstance(Sequencer->GetFocusedTemplateID());
									if (TemplateInstance)
									{
										for (FMovieSceneTrackIdentifier TrackID : TemplateInstance->Template->FindTracks(TransformTrack->GetSignature()))
										{
											if (const FMovieSceneEvaluationTrack* EvalTrack = TemplateInstance->Template->FindTrack(TrackID))
											{
												GetLocationAtTime(EvalTrack, ParentObject, KeyTime, ParentKeyPos, ParentKeyRot, Sequencer);

												CurrentRefTM = FTransform(ParentKeyRot, ParentKeyPos);

												return CurrentRefTM * ParentRefTM;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		RefTM = CurrentRefTM * ParentRefTM;
	}

	return RefTM;
}

void FSequencerEdMode::GetLocationAtTime(const FMovieSceneEvaluationTrack* Track, UObject* Object, float KeyTime, FVector& KeyPos, FRotator& KeyRot, const TSharedPtr<FSequencer>& Sequencer)
{
	FMovieSceneInterrogationData InterrogationData;
	Sequencer->GetEvaluationTemplate().CopyActuators(InterrogationData.GetAccumulator());

	FMovieSceneContext Context(KeyTime);
	Track->Interrogate(Context, InterrogationData, Object);

	for (const FTransform& Transform : InterrogationData.Iterate<FTransform>(UMovieScene3DTransformTrack::GetInterrogationKey()))
	{
		KeyPos = Transform.GetTranslation();
		KeyRot = Transform.GetRotation().Rotator();
		break;
	}
}

void FSequencerEdMode::DrawTransformTrack(const TSharedPtr<FSequencer>& Sequencer, FPrimitiveDrawInterface* PDI,
											UMovieScene3DTransformTrack* TransformTrack, const TArray<TWeakObjectPtr<UObject>>& BoundObjects, const bool bIsSelected)
{
	bool bHitTesting = true;
	if( PDI != nullptr )
	{
		bHitTesting = PDI->IsHitTesting();
	}
	
	ASequencerMeshTrail* TrailActor = nullptr;
	// Get the Trail Actor associated with this track if we are drawing mesh trails
	if (bDrawMeshTrails)
	{
		FMeshTrailData* TrailPtr = MeshTrails.FindByPredicate([TransformTrack](const FMeshTrailData InTrail)
		{
			return InTrail.Track == TransformTrack;
		});
		if (TrailPtr != nullptr)
		{
			TrailActor = TrailPtr->Trail;
		}
	}

	bool bShowTrajectory = TransformTrack->GetAllSections().ContainsByPredicate(
		[bIsSelected](UMovieSceneSection* Section)
		{
			UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
			if (TransformSection)
			{
				switch (TransformSection->GetShow3DTrajectory())
				{
				case EShow3DTrajectory::EST_Always:				return true;
				case EShow3DTrajectory::EST_Never:				return false;
				case EShow3DTrajectory::EST_OnlyWhenSelected:	return bIsSelected;
				}
			}
			return false;
		}
	);
	
	const FMovieSceneEvaluationTemplateInstance* TemplateInstance = Sequencer->GetEvaluationTemplate().GetInstance(Sequencer->GetFocusedTemplateID());
	if (!bShowTrajectory || !TemplateInstance || !TransformTrack->GetAllSections().ContainsByPredicate([](UMovieSceneSection* In){ return In->IsActive(); }))
	{
		return;
	}

	FLinearColor TrackColor = TransformTrack->GetColorTint();

	// Draw one line per-track (should only really ever be one)
	for (FMovieSceneTrackIdentifier TrackID : TemplateInstance->Template->FindTracks(TransformTrack->GetSignature()))
	{
		const FMovieSceneEvaluationTrack* EvalTrack = TemplateInstance->Template->FindTrack(TrackID);
		if (!EvalTrack)
		{
			continue;
		}

		TArray<FTrajectoryKey> TrajectoryKeys = TransformTrack->GetTrajectoryData(Sequencer->GetLocalTime(), Sequencer->GetSettings()->GetTrajectoryPathCap());
		for (TWeakObjectPtr<> WeakBinding : BoundObjects)
		{
			UObject* BoundObject = WeakBinding.Get();
			if (!BoundObject)
			{
				continue;
			}

			FVector OldKeyPos(0);
			float OldKeyTime = 0.f;
			int KeyTimeIndex = 0;

			for (const FTrajectoryKey& NewTrajectoryKey : TrajectoryKeys)
			{
				float NewKeyTime = NewTrajectoryKey.Time;

				FVector NewKeyPos(0);
				FRotator NewKeyRot(0,0,0);

				GetLocationAtTime(EvalTrack, BoundObject, NewKeyTime, NewKeyPos, NewKeyRot, Sequencer);

				// If not the first keypoint, draw a line to the last keypoint.
				if(KeyTimeIndex > 0)
				{
					int32 NumSteps = FMath::CeilToInt( (NewKeyTime - OldKeyTime)/SequencerEdMode_Draw3D::DrawTrackTimeRes );
									// Limit the number of steps to prevent a rendering performance hit
					NumSteps = FMath::Min( 100, NumSteps );
					float DrawSubstep = (NewKeyTime - OldKeyTime)/NumSteps;

					// Find position on first keyframe.
					float OldTime = OldKeyTime;

					FVector OldPos(0);
					FRotator OldRot(0,0,0);
					GetLocationAtTime(EvalTrack, BoundObject, OldKeyTime, OldPos, OldRot, Sequencer);

					const bool bIsConstantKey = NewTrajectoryKey.Is(ERichCurveInterpMode::RCIM_Constant);

					FTransform OldPosRefTM = GetRefFrame(Sequencer, BoundObject, OldKeyTime);
					FTransform NewPosRefTM = GetRefFrame(Sequencer, BoundObject, NewKeyTime);

					FVector OldPos_G = OldPosRefTM.TransformPosition(OldPos);
					FVector NewKeyPos_G = NewPosRefTM.TransformPosition(NewKeyPos);

					// For constant interpolation - don't draw ticks - just draw dotted line.
					if (bIsConstantKey)
					{
						if(PDI != nullptr)
						{
							DrawDashedLine(PDI, OldPos_G, NewKeyPos_G, TrackColor, 20, SDPG_Foreground);
						}
					}
					else
					{
						// Then draw a line for each substep.
						for (int32 j=1; j<NumSteps+1; j++)
						{
							float NewTime = OldKeyTime + j*DrawSubstep;

							FVector NewPos(0);
							FRotator NewRot(0,0,0);
							GetLocationAtTime(EvalTrack, BoundObject, NewTime, NewPos, NewRot, Sequencer);

							FTransform RefTM = GetRefFrame(Sequencer, BoundObject, NewTime);
							FVector NewPos_G = RefTM.TransformPosition(NewPos);
							if (PDI != nullptr)
							{
								PDI->DrawLine(OldPos_G, NewPos_G, TrackColor, SDPG_Foreground);
							}
							// Drawing frames
							// Don't draw point for last one - its the keypoint drawn above.
							if (j != NumSteps)
							{
								if (PDI != nullptr)
								{
									PDI->DrawPoint(NewPos_G, TrackColor, 3.f, SDPG_Foreground);
								}
								else if (TrailActor != nullptr)
								{
									TrailActor->AddFrameMeshComponent(NewTime, FTransform(NewRot, NewPos, FVector(3.0f)));
								}
							}
							OldTime = NewTime;
							OldPos_G = NewPos_G;
						}
					}
				}
					
				OldKeyTime = NewKeyTime;
				OldKeyPos = NewKeyPos;
				++KeyTimeIndex;
			}

			// Draw keypoints on top of curve
			for (const FTrajectoryKey& TrajectoryKey : TrajectoryKeys)
			{
				float NewKeyTime = TrajectoryKey.Time;

				// Find the time, position and orientation of this Key.
				FVector NewKeyPos(0);
				FRotator NewKeyRot(0,0,0);
				GetLocationAtTime(EvalTrack, BoundObject, NewKeyTime, NewKeyPos, NewKeyRot, Sequencer);

				FTransform RefTM = GetRefFrame(Sequencer, BoundObject, NewKeyTime);

				FColor KeyColor = TrackColor.ToFColor(true);

				if (bHitTesting && PDI) 
				{
					PDI->SetHitProxy(new HMovieSceneKeyProxy(TransformTrack, TrajectoryKey));
				}

				FVector NewKeyPos_G = RefTM.TransformPosition(NewKeyPos);
				// Drawing keys
				if (PDI != nullptr)
				{
					PDI->DrawPoint(NewKeyPos_G, KeyColor, 6.f, SDPG_Foreground);
				}
				else if (TrailActor != nullptr)
				{
					TArray<UMovieScene3DTransformSection*> AllSections;
					for (const FTrajectoryKey::FData& Value : TrajectoryKey.KeyData)
					{
						if (UMovieScene3DTransformSection* Section = Value.Section.Get())
						{
							AllSections.AddUnique(Section);
						}
					}

					for (UMovieScene3DTransformSection* Section : AllSections)
					{
						TrailActor->AddKeyMeshActor(NewKeyTime, FTransform(NewKeyRot, NewKeyPos, FVector(3.0f)), Section);
					}
				}

				if (bHitTesting && PDI) 
				{
					PDI->SetHitProxy(nullptr);
				}
			}
		}
	}
}


void FSequencerEdMode::DrawTracks3D(FPrimitiveDrawInterface* PDI)
{
	for (TWeakPtr<FSequencer> WeakSequencer : Sequencers)
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (!Sequencer.IsValid())
		{
			continue;
		}

		TSet<TSharedRef<FSequencerDisplayNode> > ObjectBindingNodes;

		// Map between object binding nodes and selection
		TMap<TSharedRef<FSequencerDisplayNode>, bool > ObjectBindingNodesSelectionMap;

		for (auto ObjectBinding : Sequencer->GetNodeTree()->GetObjectBindingMap() )
		{
			if (!ObjectBinding.Value.IsValid())
			{
				continue;
			}

			TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = ObjectBinding.Value.ToSharedRef();

			TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;
			SequencerHelpers::GetDescendantNodes(ObjectBindingNode, DescendantNodes);

			bool bSelected = Sequencer->GetSelection().IsSelected(ObjectBindingNode);

			if (!bSelected)
			{
				// If one of our child is selected, we're considered selected
				for (auto& DescendantNode : DescendantNodes)
				{
					if (Sequencer->GetSelection().IsSelected(DescendantNode) ||
						Sequencer->GetSelection().NodeHasSelectedKeysOrSections(DescendantNode))
					{
						bSelected = true;
						break;
					}
				}
			}

			// If one of our parent is selected, we're considered selected
			TSharedPtr<FSequencerDisplayNode> ParentNode = ObjectBindingNode->GetParent();

			while (!bSelected && ParentNode.IsValid())
			{
				if (Sequencer->GetSelection().IsSelected(ParentNode.ToSharedRef()) ||
					Sequencer->GetSelection().NodeHasSelectedKeysOrSections(ParentNode.ToSharedRef()))
				{
					bSelected = true;
				}

				ParentNode = ParentNode->GetParent();
			}

			ObjectBindingNodesSelectionMap.Add(ObjectBindingNode, bSelected);
		}

		// Gather up the transform track nodes from the object binding nodes
		for (auto& ObjectBindingNode : ObjectBindingNodesSelectionMap)
		{
			FGuid ObjectBinding = StaticCastSharedRef<FSequencerObjectBindingNode>(ObjectBindingNode.Key)->GetObjectBinding();

			TArray<TWeakObjectPtr<UObject>> BoundObjects;
			for (TWeakObjectPtr<> Ptr : Sequencer->FindObjectsInCurrentSequence(ObjectBinding))
			{
				BoundObjects.Add(Ptr);
			}

			for (auto& DisplayNode : ObjectBindingNode.Key.Get().GetChildNodes())
			{
				if (DisplayNode->GetType() == ESequencerNode::Track)
				{
					TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(DisplayNode);
					UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(TrackNode->GetTrack());
					if (TransformTrack != nullptr)
					{
						// If we are drawing mesh trails but we haven't made one for this track yet
						if (bDrawMeshTrails)
						{
							FMeshTrailData* TrailPtr = MeshTrails.FindByPredicate([TransformTrack](const FMeshTrailData InTrail)
							{
								return InTrail.Track == TransformTrack;
							});
							if (TrailPtr == nullptr)
							{
								UViewportWorldInteraction* WorldInteraction = Cast<UViewportWorldInteraction>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UViewportWorldInteraction::StaticClass() ) );
								if( WorldInteraction != nullptr )
								{
									ASequencerMeshTrail* TrailActor = WorldInteraction->SpawnTransientSceneActor<ASequencerMeshTrail>(TEXT("SequencerMeshTrail"), true);
									FMeshTrailData MeshTrail = FMeshTrailData(TransformTrack, TrailActor);
									MeshTrails.Add(MeshTrail);
								}
							}
						}

						DrawTransformTrack(Sequencer, PDI, TransformTrack, BoundObjects, ObjectBindingNode.Value);
					}
				}
			}
		}
	}
}

FSequencerEdModeTool::FSequencerEdModeTool(FSequencerEdMode* InSequencerEdMode) :
	SequencerEdMode(InSequencerEdMode)
{
}

FSequencerEdModeTool::~FSequencerEdModeTool()
{
}

bool FSequencerEdModeTool::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if( Key == EKeys::LeftMouseButton )
	{
		if( Event == IE_Pressed)
		{
			int32 HitX = ViewportClient->Viewport->GetMouseX();
			int32 HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy*HitResult = ViewportClient->Viewport->GetHitProxy(HitX, HitY);

			if(HitResult)
			{
				if( HitResult->IsA(HMovieSceneKeyProxy::StaticGetType()) )
				{
					HMovieSceneKeyProxy* KeyProxy = (HMovieSceneKeyProxy*)HitResult;
					SequencerEdMode->OnKeySelected(ViewportClient->Viewport, KeyProxy);
				}
			}
		}
	}

	return FModeTool::InputKey(ViewportClient, Viewport, Key, Event);
}
