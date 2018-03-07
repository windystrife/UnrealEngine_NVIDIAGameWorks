// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SDestructibleMeshEditorViewport.h"
#include "Misc/Paths.h"
#include "Framework/Commands/UICommandList.h"
#include "Components/SkinnedMeshComponent.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "DestructibleComponent.h"
#include "EditorDirectories.h"
#include "UObject/Package.h"
#include "DestructibleMeshEditor.h"
#include "DestructibleChunkParamsProxy.h"
#include "Slate/SceneViewport.h"
#include "ApexDestructibleAssetImport.h"
#include "DesktopPlatformModule.h"
#include "FbxImporter.h"
#include "UObject/UObjectHash.h"
#include "ComponentReregisterContext.h"
#include "DestructibleMesh.h"
#include "Widgets/Docking/SDockableTab.h"
#include "Engine/StaticMesh.h"
#include "PhysXPublic.h"
#include "Settings/SkeletalMeshEditorSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDestructibleMeshEditor, Log, All);

static const float	AnimationEditorViewport_LightRotSpeed = 0.22f;

/////////////////////////////////////////////////////////////////////////
// FDestructibleMeshEditorViewportClient

class FDestructibleMeshEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FDestructibleMeshEditorViewportClient>
{
protected:
	/** Skeletal Mesh Component used for preview */
	TWeakObjectPtr<UDestructibleComponent> PreviewDestructibleComp;

public:
	FDestructibleMeshEditorViewportClient(TWeakPtr<IDestructibleMeshEditor> InDestructibleMeshEditor, FPreviewScene& InPreviewScene, const TSharedRef<SDestructibleMeshEditorViewport>& InDestructibleMeshEditorViewport);

	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FGCObject interface

	// FEditorViewportClient interface
	virtual void Tick(float DeltaTime) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	FLinearColor GetBackgroundColor() const override { return FLinearColor::Black; }
	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;

	void UpdateLighting();

	/** Binds commands associated with the viewport client. */
	void BindCommands();

	/** Function to set the mesh component used for preview */
	void SetPreviewComponent( UDestructibleComponent* InPreviewDestructibleComp );

	/** Updated the selected chunks */
	void UpdateChunkSelection(TArray<int32> SelectedChunkIndices);

private:

	/** Callback for fracturing. */
	void Fracture();

	/** Callback for refresh */
	void RefreshFromStaticMesh();

	/** Callback to check if the destructible mesh needs to be refreshed. */
	bool CanRefreshFromStaticMesh();

	/** Callback for fbx import */
	void ImportFBXChunks();

private:

	/** Pointer back to the DestructibleMesh editor tool that owns us */
	TWeakPtr<IDestructibleMeshEditor> DestructibleMeshEditorPtr;

	/** List of chunk indices currently selected */
	TArray<int32> SelectedChunkIndices;

	/* List of currently selected chunks */
	TArray<class UDestructibleChunkParamsProxy*> SelectedChunks;

	/** Pool of currently unused chunk proxies */
	TArray<class UDestructibleChunkParamsProxy*> UnusedProxies;
};

FDestructibleMeshEditorViewportClient::FDestructibleMeshEditorViewportClient(TWeakPtr<IDestructibleMeshEditor> InDestructibleMeshEditor, FPreviewScene& InPreviewScene, const TSharedRef<SDestructibleMeshEditorViewport>& InDestructibleMeshEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InDestructibleMeshEditorViewport))
	, DestructibleMeshEditorPtr(InDestructibleMeshEditor)
{
	SetViewMode(VMI_Lit);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;

	UpdateLighting();

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(70, 70, 70);
	DrawHelper.GridColorMajor = FColor(40, 40, 40);
	DrawHelper.GridColorMinor =  FColor(20, 20, 20);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
}

void FDestructibleMeshEditorViewportClient::UpdateLighting()
{
	const USkeletalMeshEditorSettings* Options = GetDefault<USkeletalMeshEditorSettings>();

	PreviewScene->SetLightDirection(Options->AnimPreviewLightingDirection);
	PreviewScene->SetLightColor(Options->AnimPreviewDirectionalColor);
	PreviewScene->SetLightBrightness(Options->AnimPreviewLightBrightness);
}

void FDestructibleMeshEditorViewportClient::BindCommands()
{
	const FDestructibleMeshEditorCommands& Commands = FDestructibleMeshEditorCommands::Get();

	const TSharedRef<FUICommandList>& UICommandList = DestructibleMeshEditorPtr.Pin()->GetToolkitCommands();

	UICommandList->MapAction(
		Commands.Fracture,
		FExecuteAction::CreateSP( this, &FDestructibleMeshEditorViewportClient::Fracture ),
		FCanExecuteAction(),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.Refresh,
		FExecuteAction::CreateSP( this, &FDestructibleMeshEditorViewportClient::RefreshFromStaticMesh ),
		FCanExecuteAction::CreateSP( this, &FDestructibleMeshEditorViewportClient::CanRefreshFromStaticMesh ),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.ImportFBXChunks,
		FExecuteAction::CreateSP( this, &FDestructibleMeshEditorViewportClient::ImportFBXChunks ),
		FCanExecuteAction(),
		FIsActionChecked() );
}

void FDestructibleMeshEditorViewportClient::SetPreviewComponent( UDestructibleComponent* InPreviewDestructibleComp )
{
	PreviewDestructibleComp = InPreviewDestructibleComp;

	UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorPtr.Pin()->GetDestructibleMesh();
	if (DestructibleMesh != NULL)
	{
		FBoxSphereBounds MeshBounds = DestructibleMesh->GetImportedBounds();
		SetViewLocation(FVector(0.0f, -MeshBounds.SphereRadius / (75.0f * (float)PI / 360.0f), 0.5f * MeshBounds.BoxExtent.Z));
		SetViewRotation(FRotator(0.0f, 90.0f, 0.0f));
	}
}


void FDestructibleMeshEditorViewportClient::UpdateChunkSelection( TArray<int32> InSelectedChunkIndices )
{
	// Store the proxies in the ppol
	UnusedProxies.Append(SelectedChunks);

	// Clear selected chunks
	SelectedChunks.Empty(InSelectedChunkIndices.Num());

	// make sure we have enough proxies to fill the selection array */
	while(UnusedProxies.Num() < InSelectedChunkIndices.Num())
	{
		UnusedProxies.Add(NewObject<UDestructibleChunkParamsProxy>());
	}

	UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorPtr.Pin()->GetDestructibleMesh();
	UDestructibleFractureSettings* FractureSettings = DestructibleMesh->FractureSettings;

	TArray<UObject*> SelectedObjects;
	// Setup the selection array
	for (int32 i=0; i < InSelectedChunkIndices.Num(); ++i)
	{
		UDestructibleChunkParamsProxy* Proxy = UnusedProxies.Pop();

		Proxy->DestructibleMesh = DestructibleMesh;
		Proxy->ChunkIndex = InSelectedChunkIndices[i];
		Proxy->DestructibleMeshEditorPtr = DestructibleMeshEditorPtr;

		if (FractureSettings != NULL && FractureSettings->ChunkParameters.Num() > Proxy->ChunkIndex)
		{
			Proxy->ChunkParams = Proxy->DestructibleMesh->FractureSettings->ChunkParameters[Proxy->ChunkIndex];
		}

		SelectedChunks.Add(Proxy);
		SelectedObjects.Add(Proxy);
	}


	FDestructibleMeshEditor* MeshEd = (FDestructibleMeshEditor*)DestructibleMeshEditorPtr.Pin().Get();
	MeshEd->SetSelectedChunks(SelectedObjects);
}


void FDestructibleMeshEditorViewportClient::Fracture()
{
#if WITH_APEX
	UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorPtr.Pin()->GetDestructibleMesh();
	if (DestructibleMesh != NULL)
	{
		TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;

		DestructibleMesh->ReleaseResources();
		DestructibleMesh->ReleaseResourcesFence.Wait();

		if (DestructibleMesh->SourceStaticMesh)
		{
			DestructibleMesh->BuildFractureSettingsFromStaticMesh(DestructibleMesh->SourceStaticMesh);
		}
		else if (DestructibleMesh->ApexDestructibleAsset != NULL)
		{
			DestructibleMesh = ImportDestructibleMeshFromApexDestructibleAsset(DestructibleMesh->GetOuter(), *DestructibleMesh->ApexDestructibleAsset, DestructibleMesh->GetFName(), DestructibleMesh->GetFlags(), NULL,
 																			   EDestructibleImportOptions::PreserveSettings);
		}
		
		DestructibleMesh->FractureSettings->CreateVoronoiSitesInRootMesh();
		DestructibleMesh->FractureSettings->VoronoiSplitMesh();
		
		BuildDestructibleMeshFromFractureSettings(*DestructibleMesh, NULL);
	}
	
	DestructibleMeshEditorPtr.Pin()->RefreshTool();
	DestructibleMeshEditorPtr.Pin()->SetCurrentPreviewDepth(0xFFFFFFFF);	// This will get clamped to the max depth
#endif // WITH_APEX
}


void FDestructibleMeshEditorViewportClient::RefreshFromStaticMesh()
{
#if WITH_APEX
	UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorPtr.Pin()->GetDestructibleMesh();
	
	DestructibleMesh->BuildFromStaticMesh(*DestructibleMesh->SourceStaticMesh);
	Fracture();
#endif // WITH_APEX
}

bool FDestructibleMeshEditorViewportClient::CanRefreshFromStaticMesh()
{
#if WITH_APEX
	UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorPtr.Pin()->GetDestructibleMesh();
	if (!DestructibleMesh || !DestructibleMesh->SourceStaticMesh)
	{
		return false;
	}

	const auto* ImportInfo = DestructibleMesh->SourceStaticMesh->AssetImportData;
	FDateTime CurrentSourceTimestamp = FDateTime::MinValue();
	if (ImportInfo && ImportInfo->SourceData.SourceFiles.Num() == 1)
	{
		CurrentSourceTimestamp = ImportInfo->SourceData.SourceFiles[0].Timestamp;
	}

	return (CurrentSourceTimestamp > DestructibleMesh->SourceSMImportTimestamp);
#else
	return false;
#endif // WITH_APEX
}

void FDestructibleMeshEditorViewportClient::ProcessClick( class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY )
{
#if WITH_APEX
	bool bKeepSelection = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bSelectionChanged = false;

	if (Key == EKeys::LeftMouseButton && Event == EInputEvent::IE_Released)
	{
		UDestructibleComponent* Comp = PreviewDestructibleComp.Get();
		apex::DestructibleAsset* Asset = Comp->DestructibleMesh->ApexDestructibleAsset;
		const apex::RenderMeshAsset* RenderMesh = Asset->getRenderMeshAsset();

		FVector2D ScreenPos(HitX, HitY);
		FVector ClickOrigin, ViewDir;
		View.DeprojectFVector2D(ScreenPos, ClickOrigin, ViewDir);

		float NearestHitDistance = FLT_MAX;
		int32 ClickedChunk = -1;

		for (uint32 i=0; i < Asset->getChunkCount(); ++i)
		{
			int32 PartIdx = Asset->getPartIndex(i);
			int32 BoneIdx = i+1;
			
			if (!Comp->IsBoneHidden(BoneIdx))
			{
				PxBounds3 PBounds = RenderMesh->getBounds(PartIdx);

				FVector Center = P2UVector(PBounds.getCenter()) + Comp->GetBoneLocation(Comp->GetBoneName(BoneIdx));
				FVector Extent = P2UVector(PBounds.getExtents());

				FBox Bounds(Center - Extent, Center + Extent);

				FVector HitLoc, HitNorm;
				float HitTime;
				
				if (FMath::LineExtentBoxIntersection(Bounds, ClickOrigin, ClickOrigin + ViewDir * 1000.0f, FVector(0,0,0), HitLoc, HitNorm, HitTime))
				{
					float dist = (HitLoc - ClickOrigin).SizeSquared();

					if (dist < NearestHitDistance)
					{
						NearestHitDistance = dist;
						ClickedChunk = i;
					}
				}
			}
		}

		if (ClickedChunk >= 0)
		{
			int32 Idx = SelectedChunkIndices.Find(ClickedChunk);
		
			if (Idx < 0)
			{
				if (!bKeepSelection) { SelectedChunkIndices.Empty(); }

				SelectedChunkIndices.Add(ClickedChunk);
				bSelectionChanged = true;
			}
			else
			{
				SelectedChunkIndices.RemoveAt(Idx);
				bSelectionChanged = true;
			}
		}
		else if (!bKeepSelection)
		{
			SelectedChunkIndices.Empty();
 			bSelectionChanged = true;
		}
	}

	if (bSelectionChanged)
	{
		UpdateChunkSelection(SelectedChunkIndices);
	}
#endif // WITH_APEX
}

void FDestructibleMeshEditorViewportClient::Tick(float DeltaTime)
{
	FEditorViewportClient::Tick(DeltaTime);

	if (PreviewScene)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaTime);
	}
}

void FDestructibleMeshEditorViewportClient::Draw( const FSceneView* View,FPrimitiveDrawInterface* PDI )
{
	FEditorViewportClient::Draw(View, PDI);

#if WITH_APEX
	const bool DrawChunkMarker = true;

	UDestructibleComponent* Comp = PreviewDestructibleComp.Get();
	
	if (Comp)
	{
		if (Comp->DestructibleMesh != NULL && Comp->DestructibleMesh->FractureSettings != NULL)
		{
			if (Comp->DestructibleMesh->ApexDestructibleAsset != NULL)
			{
				apex::DestructibleAsset* Asset = Comp->DestructibleMesh->ApexDestructibleAsset;
				const apex::RenderMeshAsset* RenderMesh = Asset->getRenderMeshAsset();

				for (uint32 i=0; i < Asset->getChunkCount(); ++i)
				{
					int32 PartIdx = Asset->getPartIndex(i);
					int32 BoneIdx = i+1;

					if ( SelectedChunkIndices.Contains(i) )
					{
						PxBounds3 PBounds = RenderMesh->getBounds(PartIdx);

						FVector Center = P2UVector(PBounds.getCenter()) + Comp->GetBoneLocation(Comp->GetBoneName(BoneIdx));
						FVector Extent = P2UVector(PBounds.getExtents());

						FBox Bounds(Center - Extent, Center + Extent);
						DrawWireBox(PDI, Bounds, FColor::Blue, SDPG_World);
					}
				}
			}
		}
	}
#endif // WITH_APEX
}

void FDestructibleMeshEditorViewportClient::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (int32 i=0; i < SelectedChunks.Num(); ++i)
	{
		Collector.AddReferencedObject(SelectedChunks[i]);
	}

	for (int32 i=0; i < UnusedProxies.Num(); ++i)
	{
		Collector.AddReferencedObject(UnusedProxies[i]);
	}
}

void FDestructibleMeshEditorViewportClient::ImportFBXChunks()
{
	// Get the FBX that we want to import
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform != NULL)
	{
		bOpened = DesktopPlatform->OpenFileDialog(
			NULL, 
			NSLOCTEXT("UnrealEd", "ImportMatineeSequence", "Import UnrealMatinee Sequence").ToString(),
			*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT)),
			TEXT(""),
			TEXT("FBX document|*.fbx"),
			EFileDialogFlags::None, 
			OpenFilenames);
	}

	if (bOpened)
	{
		// Get the filename from dialog
		FString ImportFilename = OpenFilenames[0];
		FString FileName = OpenFilenames[0];
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, FPaths::GetPath(FileName)); // Save path as default for next time.

		const FString FileExtension = FPaths::GetExtension(FileName);
		const bool bIsFBX = FCString::Stricmp(*FileExtension, TEXT("FBX")) == 0;

		if (bIsFBX)
		{
			FlushRenderingCommands();

			UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
			if (FFbxImporter->ImportFromFile( *ImportFilename, FPaths::GetExtension( ImportFilename ) ) )
			{
				TArray<FbxNode*> FbxMeshArray;
				FFbxImporter->FillFbxMeshArray(FFbxImporter->Scene->GetRootNode(), FbxMeshArray, FFbxImporter);

				UFbxStaticMeshImportData* ImportData = NewObject<UFbxStaticMeshImportData>(GetTransientPackage(), NAME_None, RF_NoFlags, NULL);

				TArray<UStaticMesh*> ChunkMeshes;

				for (int32 i=0; i < FbxMeshArray.Num(); ++i)
				{
					UStaticMesh* TempStaticMesh = NULL;
					TempStaticMesh = (UStaticMesh*)FFbxImporter->ImportStaticMesh(GetTransientPackage(), FbxMeshArray[i], NAME_None, RF_NoFlags, ImportData, 0);
					TArray<FbxNode*> TempFbxMeshArray;
					TempFbxMeshArray.Add(FbxMeshArray[i]);
					FFbxImporter->PostImportStaticMesh(TempStaticMesh, TempFbxMeshArray);
					ChunkMeshes.Add(TempStaticMesh);
				}

				UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorPtr.Pin()->GetDestructibleMesh();
				if (DestructibleMesh)
				{
					DestructibleMesh->SetupChunksFromStaticMeshes(ChunkMeshes);
				}
			}

			FFbxImporter->ReleaseScene();

			// Update the viewport
			DestructibleMeshEditorPtr.Pin()->RefreshTool();
			DestructibleMeshEditorPtr.Pin()->SetCurrentPreviewDepth(0xFFFFFFFF);	// This will get clamped to the max depth
		}
		else
		{
			// Invalid filename 
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// SDestructibleMeshEditorViewport

void SDestructibleMeshEditorViewport::Construct(const FArguments& InArgs)
{
	DestructibleMeshEditorPtr = InArgs._DestructibleMeshEditor;
	CurrentViewMode = VMI_Lit;

	SEditorViewport::Construct(SEditorViewport::FArguments());

	PreviewComponent = NewObject<UDestructibleComponent>(GetTransientPackage(), NAME_None, RF_Transient );

	SetPreviewMesh(InArgs._ObjectToEdit);

	EditorViewportClient->BindCommands();

	PreviewDepth = 0;
	ExplodeAmount = 0.1f;
}

SDestructibleMeshEditorViewport::~SDestructibleMeshEditorViewport()
{
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

void SDestructibleMeshEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( PreviewComponent );
}

void SDestructibleMeshEditorViewport::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged)
{
	for( FEditPropertyChain::TIterator It(PropertyThatChanged->GetHead()); It; ++It )
	{
		if (*It->GetName() == FName(TEXT("Materials")))
		{
			if (PreviewComponent != NULL)
			{
				PreviewComponent->MarkRenderStateDirty();
				break;
			}
		}
	}
}

void SDestructibleMeshEditorViewport::RefreshViewport()
{
	// Update chunk visibilities
#if WITH_APEX
#if WITH_EDITORONLY_DATA
	if (DestructibleMesh != NULL && DestructibleMesh->FractureSettings != NULL && DestructibleMesh->ApexDestructibleAsset != NULL && PreviewComponent->IsRegistered())
	{
		const apex::RenderMeshAsset* ApexRenderMeshAsset = DestructibleMesh->ApexDestructibleAsset->getRenderMeshAsset();
		if (ApexRenderMeshAsset != NULL)
		{
			apex::ExplicitHierarchicalMesh& EHM =  DestructibleMesh->FractureSettings->ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh();
			if (DestructibleMesh->ApexDestructibleAsset->getPartIndex(0) < ApexRenderMeshAsset->getPartCount())
			{
				const PxBounds3& Level0Bounds = ApexRenderMeshAsset->getBounds(DestructibleMesh->ApexDestructibleAsset->getPartIndex(0));
				const PxVec3 Level0Center = !Level0Bounds.isEmpty() ? Level0Bounds.getCenter() : PxVec3(0.0f);
				for (uint32 ChunkIndex = 0; ChunkIndex < DestructibleMesh->ApexDestructibleAsset->getChunkCount(); ++ChunkIndex)
				{
					const uint32 PartIndex = DestructibleMesh->ApexDestructibleAsset->getPartIndex(ChunkIndex);

					if (PartIndex >= ApexRenderMeshAsset->getPartCount())
					{
						continue;
					}
					
					uint32 ChunkDepth = 0;			
					for (int32 ParentIndex = DestructibleMesh->ApexDestructibleAsset->getChunkParentIndex(ChunkIndex); 
						ParentIndex >= 0; 
						ParentIndex = DestructibleMesh->ApexDestructibleAsset->getChunkParentIndex(ParentIndex))
					{
						++ChunkDepth;
					}

					const bool bChunkVisible = ChunkDepth == PreviewDepth;
					PreviewComponent->SetChunkVisible(ChunkIndex, bChunkVisible);
					if (bChunkVisible)
					{
						const PxBounds3& ChunkBounds = ApexRenderMeshAsset->getBounds(PartIndex);
						const PxVec3 ChunkCenter = !ChunkBounds.isEmpty() ? ChunkBounds.getCenter() : PxVec3(0.0f);
						const PxVec3 Displacement = ExplodeAmount*(ChunkCenter - Level0Center);
						PreviewComponent->SetChunkWorldRT(ChunkIndex, FQuat(0.0f, 0.0f, 0.0f, 1.0f), P2UVector(Displacement));
					}
				}

				PreviewComponent->BoundsScale = 100;
				// Send bounds to render thread at end of frame
				PreviewComponent->UpdateComponentToWorld();
		
				// Send bones to render thread right now, so the invalidated display is rerendered with
				// uptodate information
				PreviewComponent->DoDeferredRenderUpdates_Concurrent();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
#endif // WITH_APEX

	// Invalidate the viewport's display.
	SceneViewport->InvalidateDisplay();
}

void SDestructibleMeshEditorViewport::SetPreviewMesh(UDestructibleMesh* InDestructibleMesh)
{
	DestructibleMesh = InDestructibleMesh;

	FComponentReregisterContext ReregisterContext( PreviewComponent );

	PreviewComponent->SetSkeletalMesh(InDestructibleMesh);
	
	FTransform Transform = FTransform::Identity;
	PreviewScene.AddComponent( PreviewComponent, Transform );

	EditorViewportClient->SetPreviewComponent(PreviewComponent);
}

void SDestructibleMeshEditorViewport::UpdatePreviewMesh(UDestructibleMesh* InDestructibleMesh)
{
	if (PreviewComponent)
	{
		PreviewScene.RemoveComponent(PreviewComponent);
		PreviewComponent = NULL;
	}

	DestructibleMesh = InDestructibleMesh;

	PreviewComponent = NewObject<UDestructibleComponent>();

	PreviewComponent->SetSkeletalMesh(InDestructibleMesh);

	PreviewScene.AddComponent(PreviewComponent,FTransform::Identity);

	EditorViewportClient->SetPreviewComponent(PreviewComponent);
}

bool SDestructibleMeshEditorViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground());
}

void SDestructibleMeshEditorViewport::SetPreviewDepth(uint32 InPreviewDepth)
{
	uint32 DepthCount = 0;

#if WITH_APEX
	if (DestructibleMesh != NULL && DestructibleMesh->ApexDestructibleAsset != NULL)
	{
		DepthCount = DestructibleMesh->ApexDestructibleAsset->getDepthCount();
	}
#endif // WITH_APEX

	uint32 NewPreviewDepth = 0;
	if (DepthCount > 0)
	{
		NewPreviewDepth = FMath::Clamp(InPreviewDepth, (uint32)0, DepthCount-1);
	}

	if (NewPreviewDepth != PreviewDepth)
	{
		PreviewDepth = NewPreviewDepth;
		RefreshViewport();
	}
}

void SDestructibleMeshEditorViewport::SetExplodeAmount(float InExplodeAmount)
{
	float NewExplodeAmount = 0.0f;
	if (InExplodeAmount >= 0.0f)
	{
		NewExplodeAmount = InExplodeAmount;
	}

	if (NewExplodeAmount != ExplodeAmount)
	{
		ExplodeAmount = NewExplodeAmount;
		RefreshViewport();
	}
}

UDestructibleComponent* SDestructibleMeshEditorViewport::GetDestructibleComponent() const
{
	return PreviewComponent;
}

void SDestructibleMeshEditorViewport::SetViewModeWireframe()
{
	if(CurrentViewMode != VMI_Wireframe)
	{
		CurrentViewMode = VMI_Wireframe;
	}
	else
	{
		CurrentViewMode = VMI_Lit;
	}

	EditorViewportClient->SetViewMode(CurrentViewMode);
	SceneViewport->Invalidate();

}

bool SDestructibleMeshEditorViewport::IsInViewModeWireframeChecked() const
{
	return CurrentViewMode == VMI_Wireframe;
}

TSharedRef<FEditorViewportClient> SDestructibleMeshEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable( new FDestructibleMeshEditorViewportClient(DestructibleMeshEditorPtr, PreviewScene, SharedThis(this)) );

	EditorViewportClient->bSetListenerPosition = false;

	EditorViewportClient->SetRealtime( false );
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SDestructibleMeshEditorViewport::IsVisible );

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SDestructibleMeshEditorViewport::MakeViewportToolbar()
{
	return nullptr;
}

void SDestructibleMeshEditorViewport::BindCommands()
{
	// No commands. Overridden to prevent the base SEditorViewport commands from being bound.
}
