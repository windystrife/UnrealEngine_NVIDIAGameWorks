// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	
=============================================================================*/

#include "Components/SceneCaptureComponent.h"
#include "Misc/ScopeLock.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Actor.h"
#include "RenderingThread.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Components/BillboardComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "Engine/StaticMesh.h"
#include "Engine/SceneCapture.h"
#include "Engine/SceneCapture2D.h"
// WaveWorks Start
#include "Engine/WaveWorksShorelineCapture.h"
#include "Engine/TextureRenderTarget2D.h"
// WaveWorks End
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/DrawFrustumComponent.h"
#include "Engine/PlanarReflection.h"
#include "Components/PlanarReflectionComponent.h"
#include "PlanarReflectionSceneProxy.h"
#include "Components/BoxComponent.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "SceneCaptureComponent"

// WaveWorks Start

#include "Components/WaveWorksShorelineCaptureComponent.h"
#include "WaveWorksGlobalShader.h"

#if WITH_EDITOR
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#endif

IMPLEMENT_SHADER_TYPE(, FPreprocessShorelineDistanceFieldTexCS, TEXT("/Engine/Private/GFSDK_WaveWorks_ShorelineDistanceField.usf"),  TEXT("Preprocess_Main"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGetNearestPixelToShorelineCS, TEXT("/Engine/Private/GFSDK_WaveWorks_ShorelineDistanceField.usf"),  TEXT("GetNearestPixel_Main"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FBlurShorelineDistanceFieldCS, TEXT("/Engine/Private/GFSDK_WaveWorks_ShorelineDistanceField.usf"),  TEXT("Blur_Main"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGetGradientShorelineDistanceFieldCS, TEXT("/Engine/Private/GFSDK_WaveWorks_ShorelineDistanceField.usf"), TEXT("GetGradient_Main"), SF_Compute);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FWaveWorksShorelineDFUniformParameters, TEXT("WaveWorksShorelineDFParam"));

// WaveWorks End

static TMultiMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<USceneCaptureComponent> > SceneCapturesToUpdateMap;

ASceneCapture::ASceneCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CamMesh0"));

	MeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	MeshComp->bHiddenInGame = true;
	MeshComp->CastShadow = false;
	MeshComp->PostPhysicsComponentTick.bCanEverTick = false;
	RootComponent = MeshComp;
}
// -----------------------------------------------

ASceneCapture2D::ASceneCapture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawFrustum = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("DrawFrust0"));
	DrawFrustum->bIsEditorOnly = true;
	DrawFrustum->SetupAttachment(GetMeshComp());

	CaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("NewSceneCaptureComponent2D"));
	CaptureComponent2D->SetupAttachment(GetMeshComp());
}

void ASceneCapture2D::OnInterpToggle(bool bEnable)
{
	CaptureComponent2D->SetVisibility(bEnable);
}

void ASceneCapture2D::UpdateDrawFrustum()
{
	if(DrawFrustum && CaptureComponent2D)
	{
		DrawFrustum->FrustumStartDist = GNearClippingPlane;
		
		// 1000 is the default frustum distance, ideally this would be infinite but that might cause rendering issues
		DrawFrustum->FrustumEndDist = (CaptureComponent2D->MaxViewDistanceOverride > DrawFrustum->FrustumStartDist)
			? CaptureComponent2D->MaxViewDistanceOverride : 1000.0f;

		DrawFrustum->FrustumAngle = CaptureComponent2D->FOVAngle;
		//DrawFrustum->FrustumAspectRatio = CaptureComponent2D->AspectRatio;
	}
}

void ASceneCapture2D::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if(GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if( !GetMeshComp()->GetStaticMesh())
			{
				UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(CamMesh);
			}
		}
	}
#endif

	// Sync component with CameraActor frustum settings.
	UpdateDrawFrustum();
}

// WaveWorks Begin

AWaveWorksShorelineCapture::AWaveWorksShorelineCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaveWorksShorelineCaptureComponent = CreateDefaultSubobject<UWaveWorksShorelineCaptureComponent>(TEXT("WaveWorksShorelineCaptureComponent"));
	WaveWorksShorelineCaptureComponent->SetupAttachment(GetMeshComp());
	WaveWorksShorelineCaptureComponent->SetRelativeLocation(FVector::ZeroVector);
	WaveWorksShorelineCaptureComponent->SetRelativeRotation(FRotator::ZeroRotator);
	WaveWorksShorelineCaptureComponent->SetRelativeScale3D(FVector(1,1,1));

	FRotator rotator(-90, 0, 0);
	this->SetActorRotation(rotator);
	this->SetActorScale3D(FVector(1, 1, 1));
}

void AWaveWorksShorelineCapture::OnInterpToggle(bool bEnable)
{
	WaveWorksShorelineCaptureComponent->SetVisibility(bEnable);
}

void AWaveWorksShorelineCapture::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if (GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if (!GetMeshComp()->GetStaticMesh())
			{
				UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(CamMesh);
			}
		}
	}
#endif
}

UWaveWorksShorelineCaptureComponent* AWaveWorksShorelineCapture::GetWaveWorksShorelineCaptureComponent() const 
{ 
	return WaveWorksShorelineCaptureComponent; 
}
// WaveWorks End

ASceneCaptureCube::ASceneCaptureCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawFrustum = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("DrawFrust0"));
	DrawFrustum->bIsEditorOnly = true;
	DrawFrustum->SetupAttachment(GetMeshComp());

	CaptureComponentCube = CreateDefaultSubobject<USceneCaptureComponentCube>(TEXT("NewSceneCaptureComponentCube"));
	CaptureComponentCube->SetupAttachment(GetMeshComp());
}

void ASceneCaptureCube::OnInterpToggle(bool bEnable)
{
	CaptureComponentCube->SetVisibility(bEnable);
}

void ASceneCaptureCube::UpdateDrawFrustum()
{
	if(DrawFrustum && CaptureComponentCube)
	{
		DrawFrustum->FrustumStartDist = GNearClippingPlane;

		// 1000 is the default frustum distance, ideally this would be infinite but that might cause rendering issues
		DrawFrustum->FrustumEndDist = (CaptureComponentCube->MaxViewDistanceOverride > DrawFrustum->FrustumStartDist)
			? CaptureComponentCube->MaxViewDistanceOverride : 1000.0f;

		DrawFrustum->FrustumAngle = 90;
	}
}

void ASceneCaptureCube::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if(GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if( !GetMeshComp()->GetStaticMesh())
			{
				UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(CamMesh);
			}
		}
	}
#endif

	// Sync component with CameraActor frustum settings.
	UpdateDrawFrustum();
}
#if WITH_EDITOR

void ASceneCaptureCube::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if(bFinished && CaptureComponentCube->bCaptureOnMovement)
	{
		CaptureComponentCube->CaptureSceneDeferred();
	}
}
#endif
// -----------------------------------------------
USceneCaptureComponent::USceneCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ShowFlags(FEngineShowFlags(ESFIM_Game))
{
	bCaptureEveryFrame = true;
	bCaptureOnMovement = true;
	bAlwaysPersistRenderingState = false;
	LODDistanceFactor = 1.0f;
	MaxViewDistanceOverride = -1;
	CaptureSortPriority = 0;

	// Disable features that are not desired when capturing the scene
	ShowFlags.SetMotionBlur(0); // motion blur doesn't work correctly with scene captures.
	ShowFlags.SetSeparateTranslucency(0);
	ShowFlags.SetHMDDistortion(0);

    CaptureStereoPass = EStereoscopicPass::eSSP_FULL;

	// NVCHANGE_BEGIN: Add VXGI
	bEnableVxgi = false;
	// NVCHANGE_END: Add VXGI
}

void USceneCaptureComponent::OnRegister()
{
	Super::OnRegister();

	// Make sure any loaded saved flag settings are reflected in our FEngineShowFlags
	UpdateShowFlags();
}

void USceneCaptureComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USceneCaptureComponent* This = CastChecked<USceneCaptureComponent>(InThis);

	for (int32 ViewIndex = 0; ViewIndex < This->ViewStates.Num(); ViewIndex++)
	{
		FSceneViewStateInterface* Ref = This->ViewStates[ViewIndex].GetReference();
		if (Ref)
		{
			Ref->AddReferencedObjects(Collector);
		}
	}

	Super::AddReferencedObjects(This, Collector);
}

void USceneCaptureComponent::HideComponent(UPrimitiveComponent* InComponent)
{
	if (InComponent)
	{
		HiddenComponents.AddUnique(InComponent);
	}
}

void USceneCaptureComponent::HideActorComponents(AActor* InActor)
{
	if (InActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		InActor->GetComponents(PrimitiveComponents);
		for (int32 ComponentIndex = 0, NumComponents = PrimitiveComponents.Num(); ComponentIndex < NumComponents; ++ComponentIndex)
		{
			HiddenComponents.AddUnique(PrimitiveComponents[ComponentIndex]);
		}
	}
}

void USceneCaptureComponent::ShowOnlyComponent(UPrimitiveComponent* InComponent)
{
	if (InComponent)
	{
		// Backward compatibility - set PrimitiveRenderMode to PRM_UseShowOnlyList if BP / game code tries to add a ShowOnlyComponent
		PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		ShowOnlyComponents.Add(InComponent);
	}
}

void USceneCaptureComponent::ShowOnlyActorComponents(AActor* InActor)
{
	if (InActor)
	{
		// Backward compatibility - set PrimitiveRenderMode to PRM_UseShowOnlyList if BP / game code tries to add a ShowOnlyComponent
		PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		InActor->GetComponents(PrimitiveComponents);
		for (int32 ComponentIndex = 0, NumComponents = PrimitiveComponents.Num(); ComponentIndex < NumComponents; ++ComponentIndex)
		{
			ShowOnlyComponents.Add(PrimitiveComponents[ComponentIndex]);
		}
	}
}

void USceneCaptureComponent::RemoveShowOnlyComponent(UPrimitiveComponent* InComponent)
{
	ShowOnlyComponents.Remove(InComponent);
}

void USceneCaptureComponent::RemoveShowOnlyActorComponents(AActor* InActor)
{
	if (InActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		InActor->GetComponents(PrimitiveComponents);
		for (int32 ComponentIndex = 0, NumComponents = PrimitiveComponents.Num(); ComponentIndex < NumComponents; ++ComponentIndex)
		{
			ShowOnlyComponents.Remove(PrimitiveComponents[ComponentIndex]);
		}
	}
}

void USceneCaptureComponent::ClearShowOnlyComponents(UPrimitiveComponent* InComponent)
{
	ShowOnlyComponents.Reset();
}

void USceneCaptureComponent::ClearHiddenComponents()
{
	HiddenComponents.Reset();
}

void USceneCaptureComponent::SetCaptureSortPriority(int32 NewCaptureSortPriority)
{
	CaptureSortPriority = NewCaptureSortPriority;
}

FSceneViewStateInterface* USceneCaptureComponent::GetViewState(int32 ViewIndex)
{
	if (ViewIndex >= ViewStates.Num())
	{
		ViewStates.AddZeroed(ViewIndex - ViewStates.Num() + 1);
	}

	FSceneViewStateInterface* ViewStateInterface = ViewStates[ViewIndex].GetReference();
	if ((bCaptureEveryFrame || bAlwaysPersistRenderingState) && ViewStateInterface == NULL)
	{
		ViewStates[ViewIndex].Allocate();
		ViewStateInterface = ViewStates[ViewIndex].GetReference();
	}
	else if (!bCaptureEveryFrame && ViewStateInterface && !bAlwaysPersistRenderingState)
	{
		ViewStates[ViewIndex].Destroy();
		ViewStateInterface = NULL;
	}
	return ViewStateInterface;
}

void USceneCaptureComponent::UpdateShowFlags()
{
	USceneCaptureComponent* Archetype = Cast<USceneCaptureComponent>(GetArchetype());
	if (Archetype)
	{
		ShowFlags = Archetype->ShowFlags;
	}

	for (FEngineShowFlagsSetting ShowFlagSetting : ShowFlagSettings)
	{
		int32 SettingIndex = ShowFlags.FindIndexByName(*(ShowFlagSetting.ShowFlagName));
		if (SettingIndex != INDEX_NONE)
		{ 
			ShowFlags.SetSingleFlag(SettingIndex, ShowFlagSetting.Enabled);
		}
	}
}

#if WITH_EDITOR

bool USceneCaptureComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent, HiddenActors))
		{
			return PrimitiveRenderMode == ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent, ShowOnlyActors))
		{
			return PrimitiveRenderMode == ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		}
	}

	return true;
}

void USceneCaptureComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// If our ShowFlagSetting UStruct changed, (or if PostEditChange was called without specifying a property) update the actual show flags
	if (MemberPropertyName.IsEqual("ShowFlagSettings") || MemberPropertyName.IsNone())
	{
		UpdateShowFlags();
	}
}
#endif

void USceneCaptureComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::AddedbUseShowOnlyList)
	{
		if (ShowOnlyActors.Num() > 0 || ShowOnlyComponents.Num() > 0)
		{
			PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		}
	}
}

void USceneCaptureComponent::UpdateDeferredCaptures(FSceneInterface* Scene)
{
	UWorld* World = Scene->GetWorld();
	if (!World || SceneCapturesToUpdateMap.Num() == 0)
	{
		return;
	}

	// Only update the scene captures associated with the current scene.
	// Updating others not associated with the scene would cause invalid data to be rendered into the target
	TArray< TWeakObjectPtr<USceneCaptureComponent> > SceneCapturesToUpdate;
	SceneCapturesToUpdateMap.MultiFind(World, SceneCapturesToUpdate);
	SceneCapturesToUpdate.Sort([](const TWeakObjectPtr<USceneCaptureComponent>& A, const TWeakObjectPtr<USceneCaptureComponent>& B)
	{
		if (!A.IsValid())
		{
			return false;
		}
		else if (!B.IsValid())
		{
			return true;
		}
		return A->CaptureSortPriority > B->CaptureSortPriority;
	});

	for (TWeakObjectPtr<USceneCaptureComponent> Component : SceneCapturesToUpdate)
	{
		if (Component.IsValid())
		{
			Component->UpdateSceneCaptureContents(Scene);
		}
	}

	// All scene captures for this world have been updated
	SceneCapturesToUpdateMap.Remove(World);
}

void USceneCaptureComponent::OnUnregister()
{
	for (int32 ViewIndex = 0; ViewIndex < ViewStates.Num(); ViewIndex++)
	{
		ViewStates[ViewIndex].Destroy();
	}

	Super::OnUnregister();
}

// -----------------------------------------------


USceneCaptureComponent2D::USceneCaptureComponent2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FOVAngle = 90.0f;
	OrthoWidth = 512;
	bUseCustomProjectionMatrix = false;
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	// Tick in the editor so that bCaptureEveryFrame preview works
	bTickInEditor = true;
	// previous behavior was to capture from raw scene color 
	CaptureSource = SCS_SceneColorHDR;
	// default to full blend weight..
	PostProcessBlendWeight = 1.0f;
	CaptureStereoPass = EStereoscopicPass::eSSP_FULL;
	CustomProjectionMatrix.SetIdentity();
	ClipPlaneNormal = FVector(0, 0, 1);
	bCameraCutThisFrame = false;
	
	// Legacy initialization.
	{
		// previous behavior was to capture 2d scene captures before cube scene captures.
		CaptureSortPriority = 1;

		// previous behavior was not exposing MotionBlur and Temporal AA in scene capture 2d.
		ShowFlags.TemporalAA = false;
		ShowFlags.MotionBlur = false;
	}
}

void USceneCaptureComponent2D::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Update content on register to have at least one frames worth of good data.
	// Without updating here this component would not work in a blueprint construction script which recreates the component after each move in the editor
	CaptureSceneDeferred();
#endif
}

void USceneCaptureComponent2D::SendRenderTransform_Concurrent()
{	
	if (bCaptureOnMovement)
	{
		CaptureSceneDeferred();
	}

	Super::SendRenderTransform_Concurrent();
}

void USceneCaptureComponent2D::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCaptureEveryFrame)
	{
		CaptureSceneDeferred();
	}
}

void USceneCaptureComponent2D::CaptureSceneDeferred()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// Defer until after updates finish
		// Needs some CS because of parallel updates.
		static FCriticalSection CriticalSection;
		FScopeLock ScopeLock(&CriticalSection);
		SceneCapturesToUpdateMap.AddUnique(World, this);
	}	
}

void USceneCaptureComponent2D::CaptureScene()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// We must push any deferred render state recreations before causing any rendering to happen, to make sure that deleted resource references are updated
		World->SendAllEndOfFrameUpdates();
		World->Scene->UpdateSceneCaptureContents(this);
	}	

	if (bCaptureEveryFrame)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("CaptureScene", "CaptureScene: Scene capture with bCaptureEveryFrame enabled was told to update - major inefficiency."));
	}
}

#if WITH_EDITOR

bool USceneCaptureComponent2D::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (bUseCustomProjectionMatrix 
			&& (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, ProjectionType)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, FOVAngle)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, OrthoWidth)))
		{
			return false;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, FOVAngle))
		{
			return ProjectionType == ECameraProjectionMode::Perspective;
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, OrthoWidth))
		{
			return ProjectionType == ECameraProjectionMode::Orthographic;
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, CompositeMode))
		{
			return CaptureSource == SCS_SceneColorHDR;
		}

		static IConsoleVariable* ClipPlaneCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowGlobalClipPlane"));

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, bEnableClipPlane))
		{
			return ClipPlaneCVar->GetInt() != 0;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, ClipPlaneBase)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, ClipPlaneNormal))
		{
			return bEnableClipPlane && ClipPlaneCVar->GetInt() != 0;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, CustomProjectionMatrix))
		{
			return bUseCustomProjectionMatrix;
		}
	}

	return Super::CanEditChange(InProperty);
}

void USceneCaptureComponent2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CaptureSceneDeferred();
}
#endif // WITH_EDITOR

void USceneCaptureComponent2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		PostProcessSettings.OnAfterLoad();

		if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MotionBlurAndTAASupportInSceneCapture2d)
		{
			ShowFlags.TemporalAA = false;
			ShowFlags.MotionBlur = false;
		}
	}
}

void USceneCaptureComponent2D::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	Scene->UpdateSceneCaptureContents(this);
}


// -----------------------------------------------
// WaveWorks Start
UWaveWorksShorelineCaptureComponent::UWaveWorksShorelineCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxPixelsToShoreline = 20;
	ProjectionType = ECameraProjectionMode::Type::Orthographic;
	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	CompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;
	MaxViewDistanceOverride = -1;
	bEnableClipPlane = false;
}

static void PreprocessShorelineDistanceFieldTex_RenderThread(
	FRHICommandListImmediate& RHICmdList, 
	FUnorderedAccessViewRHIRef& OutShorelineDFUAV, 
	const FTexture2DRHIRef& ShorelineDFTexRef)
{
	TShaderMapRef<FPreprocessShorelineDistanceFieldTexCS> PreprocessShorelineDFCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(PreprocessShorelineDFCS->GetComputeShader());

	PreprocessShorelineDFCS->SetOutput(RHICmdList, OutShorelineDFUAV);
	PreprocessShorelineDFCS->SetParameters(RHICmdList, ShorelineDFTexRef);

	const uint32 XGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeX() + 32 - 1) / 32);
	const uint32 YGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeY() + 32 - 1) / 32);

	DispatchComputeShader(
		RHICmdList,
		*PreprocessShorelineDFCS,
		XGroupCount,
		YGroupCount,
		1);

	PreprocessShorelineDFCS->UnbindBuffers(RHICmdList);
}

static void GetNearestPixelToShoreline_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FUnorderedAccessViewRHIRef& OutShorelineDFUAV,
	const FTexture2DRHIRef& ShorelineDFTexRef,
	uint32 MaxPixelsToShoreline)
{
	TShaderMapRef<FGetNearestPixelToShorelineCS> GetNearestPixelToShorelineCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(GetNearestPixelToShorelineCS->GetComputeShader());

	GetNearestPixelToShorelineCS->SetOutput(RHICmdList, OutShorelineDFUAV);

	FWaveWorksShorelineDFUniformParameters WaveWorksShorelineDFUniformParameters;
	WaveWorksShorelineDFUniformParameters.MaxPixelsToShoreline = MaxPixelsToShoreline;
	FWaveWorksShorelineDFUniformBufferRef WaveWorksDFUniformBuffer = FWaveWorksShorelineDFUniformBufferRef::CreateUniformBufferImmediate(WaveWorksShorelineDFUniformParameters, UniformBuffer_SingleDraw);
	GetNearestPixelToShorelineCS->SetParameters(RHICmdList, ShorelineDFTexRef, WaveWorksDFUniformBuffer);

	const uint32 XGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeX() + 32 - 1) / 32);
	const uint32 YGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeY() + 32 - 1) / 32);

	DispatchComputeShader(
		RHICmdList,
		*GetNearestPixelToShorelineCS,
		XGroupCount,
		YGroupCount,
		1);

	GetNearestPixelToShorelineCS->UnbindBuffers(RHICmdList);
}

static void BlurShorelineDistanceFieldTex_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FUnorderedAccessViewRHIRef& OutShorelineDFUAV,
	const FTexture2DRHIRef& ShorelineDFTexRef)
{
	TShaderMapRef<FBlurShorelineDistanceFieldCS> BlurShorelineDFCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(BlurShorelineDFCS->GetComputeShader());

	BlurShorelineDFCS->SetOutput(RHICmdList, OutShorelineDFUAV);
	BlurShorelineDFCS->SetParameters(RHICmdList, ShorelineDFTexRef);

	const uint32 XGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeX() + 32 - 1) / 32);
	const uint32 YGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeY() + 32 - 1) / 32);

	DispatchComputeShader(
		RHICmdList,
		*BlurShorelineDFCS,
		XGroupCount,
		YGroupCount,
		1);

	BlurShorelineDFCS->UnbindBuffers(RHICmdList);
}

static void GetGradientShorelineDistanceFieldTex_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FUnorderedAccessViewRHIRef& OutShorelineDFUAV,
	const FTexture2DRHIRef& ShorelineDFTexRef)
{
	TShaderMapRef<FGetGradientShorelineDistanceFieldCS> GetGradientShorelineDFCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(GetGradientShorelineDFCS->GetComputeShader());

	GetGradientShorelineDFCS->SetOutput(RHICmdList, OutShorelineDFUAV);
	GetGradientShorelineDFCS->SetParameters(RHICmdList, ShorelineDFTexRef);

	const uint32 XGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeX() + 32 - 1) / 32);
	const uint32 YGroupCount = FMath::Max<uint32>(1, (ShorelineDFTexRef->GetSizeY() + 32 - 1) / 32);

	DispatchComputeShader(
		RHICmdList,
		*GetGradientShorelineDFCS,
		XGroupCount,
		YGroupCount,
		1);

	GetGradientShorelineDFCS->UnbindBuffers(RHICmdList);
}

#if WITH_EDITOR

bool UWaveWorksShorelineCaptureComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, FOVAngle)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, ProjectionType)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, CaptureSource)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, CompositeMode)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, MaxViewDistanceOverride)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, bEnableClipPlane)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, ClipPlaneBase)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, ClipPlaneNormal)
			)
		{
			return false;
		}
	}

	return true;
}
#endif // WITH_EDITOR

void UWaveWorksShorelineCaptureComponent::GenerateShorelineDFTexture()
{
#if WITH_EDITOR

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// get origin shoreline distance field texture
	const FTexture2DRHIRef& ShorelineDFTexRef = TextureTarget->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();

	// create UAV used for output distance field texture
	uint32 width = (uint32)(TextureTarget->GetSurfaceWidth());
	uint32 height = (uint32)(TextureTarget->GetSurfaceHeight());

	FRHIResourceCreateInfo CreateInfo;
	FTexture2DRHIRef OutShorelineDFTexture = RHICreateTexture2D(width, height, PF_FloatRGBA, 1, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
	FUnorderedAccessViewRHIRef OutShorelineDFUAV = RHICreateUnorderedAccessView(OutShorelineDFTexture);

	// Preprocess distance field texture
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		PreprocessShorelineDFTexCommand,
		FRHICommandListImmediate&, RHICmdListInput, RHICmdList,
		FUnorderedAccessViewRHIRef&, OutShorelineDFUAV, OutShorelineDFUAV,
		const FTexture2DRHIRef&, ShorelineDFTexRef, ShorelineDFTexRef,
		{
			PreprocessShorelineDistanceFieldTex_RenderThread(RHICmdListInput,OutShorelineDFUAV,ShorelineDFTexRef);
		});

	FlushRenderingCommands();

	// create a tmp texture
	FTexture2DRHIRef TempShorelineDFTexRef = RHICreateTexture2D(width, height, PF_FloatRGBA, 1, 1, TexCreate_ShaderResource, CreateInfo);
	RHICmdList.CopyToResolveTarget(OutShorelineDFTexture, TempShorelineDFTexRef, true, FResolveParams());

	// get nearest pixel to shoreline
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
		GetNearestPixelShorelineDFTexCommand,
		FRHICommandListImmediate&, RHICmdListInput, RHICmdList,
		FUnorderedAccessViewRHIRef&, OutShorelineDFUAV, OutShorelineDFUAV,
		const FTexture2DRHIRef&, TempShorelineDFTexRef, TempShorelineDFTexRef,
		uint32, MaxPixelsToShoreline, MaxPixelsToShoreline,
		{
			GetNearestPixelToShoreline_RenderThread(RHICmdListInput,OutShorelineDFUAV,TempShorelineDFTexRef,MaxPixelsToShoreline);
		});

	FlushRenderingCommands();

	// blur distance field texture
	for (int index = 0; index < 2; index++)
	{
		RHICmdList.CopyToResolveTarget(OutShorelineDFTexture, TempShorelineDFTexRef, true, FResolveParams());

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			BlurShorelineDFTexCommand,
			FRHICommandListImmediate&, RHICmdListInput, RHICmdList,
			FUnorderedAccessViewRHIRef&, OutShorelineDFUAV, OutShorelineDFUAV,
			const FTexture2DRHIRef&, TempShorelineDFTexRef, TempShorelineDFTexRef,
			{
				BlurShorelineDistanceFieldTex_RenderThread(RHICmdListInput,OutShorelineDFUAV,TempShorelineDFTexRef);
			});

		FlushRenderingCommands();
	}

	// get gradient 
	RHICmdList.CopyToResolveTarget(OutShorelineDFTexture, TempShorelineDFTexRef, true, FResolveParams());

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		GetGradientShorelineDFTexCommand,
		FRHICommandListImmediate&, RHICmdListInput, RHICmdList,
		FUnorderedAccessViewRHIRef&, OutShorelineDFUAV, OutShorelineDFUAV,
		const FTexture2DRHIRef&, TempShorelineDFTexRef, TempShorelineDFTexRef,
		{
			GetGradientShorelineDistanceFieldTex_RenderThread(RHICmdListInput,OutShorelineDFUAV,TempShorelineDFTexRef);
		});

	FlushRenderingCommands();

	// save result to texture asset
	{
		// read data to CPU
		TArray<FFloat16Color> OutputBuffer;
		OutputBuffer.Empty();
		OutputBuffer.AddUninitialized(width * height);

		FIntRect InRect(0, 0, width, height);

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			ReadOutDFTextureCommand,
			FTexture2DRHIRef&, OutShorelineDFTexture, OutShorelineDFTexture,
			FIntRect, InRect, InRect,
			TArray<FFloat16Color>&, OutputBuffer, OutputBuffer,
			{
				RHICmdList.ReadSurfaceFloatData(
				OutShorelineDFTexture,
					InRect,
					OutputBuffer,
					CubeFace_PosX,
					0,
					0
					);
			});

		FlushRenderingCommands();
		
		// save datas
		FString Name;
		FString PackageName;

		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(TextureTarget->GetOutermost()->GetName(), TEXT("_Tex"), PackageName, Name);

		UTexture2D* Result = NULL;
		{
			Result = NewObject<UTexture2D>(CreatePackage(NULL, *PackageName), FName(*Name), TextureTarget->GetMaskedFlags());
			Result->Source.Init(width, height, 1, 1, TSF_RGBA16F);

			uint32* TextureData = (uint32*)Result->Source.LockMip(0);		
			FMemory::Memcpy(TextureData, OutputBuffer.GetData(), Result->Source.CalcMipSize(0));
			Result->Source.UnlockMip(0);

			uint32 Flags = TC_Default;
			Flags &= ~CTF_SRGB;
			Result->SRGB = (Flags & CTF_SRGB) != 0;
			Result->MipGenSettings = TMGS_NoMipmaps;
			Result->AddressX = TextureAddress::TA_Clamp;
			Result->AddressY = TextureAddress::TA_Clamp;
			Result->Filter = TextureFilter::TF_Bilinear;

			Result->CompressionSettings = TC_HDR;
			// Disable compression
			Result->CompressionNone = true;
			Result->DeferCompression = false;

			Result->PostEditChange();

			// package needs saving
			Result->MarkPackageDirty();
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Result);
		}
	}

	// release textures
	TempShorelineDFTexRef.SafeRelease();
	OutShorelineDFTexture.SafeRelease();
	OutShorelineDFUAV.SafeRelease();

#endif
}
// WaveWorks End

// -----------------------------------------------

APlanarReflection::APlanarReflection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowPreviewPlane = true;

	PlanarReflectionComponent = CreateDefaultSubobject<UPlanarReflectionComponent>(TEXT("NewPlanarReflectionComponent"));
	RootComponent = PlanarReflectionComponent;

	UBoxComponent* DrawInfluenceBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DrawBox0"));
	DrawInfluenceBox->SetupAttachment(PlanarReflectionComponent);
	DrawInfluenceBox->bUseEditorCompositing = true;
	DrawInfluenceBox->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PlanarReflectionComponent->PreviewBox = DrawInfluenceBox;

	GetMeshComp()->SetWorldRotation(FRotator(0, 0, 0));
	GetMeshComp()->SetWorldScale3D(FVector(4, 4, 1));
	GetMeshComp()->SetupAttachment(PlanarReflectionComponent);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			FName NAME_ReflectionCapture;
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics()
				: NAME_ReflectionCapture(TEXT("ReflectionCapture"))
				, DecalTexture(TEXT("/Engine/EditorResources/S_ReflActorIcon"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bAbsoluteScale = true;
		SpriteComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif
}

void APlanarReflection::OnInterpToggle(bool bEnable)
{
	PlanarReflectionComponent->SetVisibility(bEnable);
}

void APlanarReflection::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if(GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if( !GetMeshComp()->GetStaticMesh())
			{
				UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/PlanarReflectionPlane.PlanarReflectionPlane"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(PlaneMesh);
				UMaterial* PlaneMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMeshes/ColorCalibrator/M_ChromeBall.M_ChromeBall"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetMaterial(0, PlaneMaterial);
			}
		}

		GetMeshComp()->bVisible = bShowPreviewPlane;
	}
#endif
}

#if WITH_EDITOR
void APlanarReflection::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyScale(FVector(DeltaScale.X, DeltaScale.Y, 0), PivotLocation, bAltDown, bShiftDown, bCtrlDown);

	UPlanarReflectionComponent* ReflectionComponent = GetPlanarReflectionComponent();
	check(ReflectionComponent);
	const FVector ModifiedScale = FVector(0, 0, DeltaScale.Z) * ( AActor::bUsePercentageBasedScaling ? 500.0f : 50.0f );
	FMath::ApplyScaleToFloat(ReflectionComponent->DistanceFromPlaneFadeoutStart, ModifiedScale);
	FMath::ApplyScaleToFloat(ReflectionComponent->DistanceFromPlaneFadeoutEnd, ModifiedScale);
	PostEditChange();
}

void APlanarReflection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GetMeshComp())
	{
		GetMeshComp()->bVisible = bShowPreviewPlane;
		GetMeshComp()->MarkRenderStateDirty();
	}
}

#endif

// -----------------------------------------------

// 0 is reserved to mean invalid
int32 NextPlanarReflectionId = 0;

UPlanarReflectionComponent::UPlanarReflectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
// WaveWorks Start
	bAlwaysVisible = false;
	TextureTarget = NULL;
// WaveWorks End
	bCaptureEveryFrame = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	// Tick in the editor so that bCaptureEveryFrame preview works
	bTickInEditor = true;
	RenderTarget = NULL;
	PrefilterRoughness = .01f;
	PrefilterRoughnessDistance = 10000;
	ScreenPercentage = 50;
	NormalDistortionStrength = 500;
	DistanceFromPlaneFadeStart_DEPRECATED = 400;
	DistanceFromPlaneFadeEnd_DEPRECATED = 600;
	DistanceFromPlaneFadeoutStart = 60;
	DistanceFromPlaneFadeoutEnd = 100;
	AngleFromPlaneFadeStart = 20;
	AngleFromPlaneFadeEnd = 30;
	ProjectionWithExtraFOV[0] = FMatrix::Identity;
	ProjectionWithExtraFOV[1] = FMatrix::Identity;

// WaveWorks Start
	WaterTransmittance = FVector(0.065, 0.028, 0.035);
	WaterTransmittancePower = 0.0f;
// WaveWorks End

	// Disable screen space effects that don't work properly with the clip plane
	ShowFlags.SetLightShafts(0);
	ShowFlags.SetContactShadows(0);
	ShowFlags.SetScreenSpaceReflections(0);

	NextPlanarReflectionId++;
	PlanarReflectionId = NextPlanarReflectionId;
}


void UPlanarReflectionComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::ChangedPlanarReflectionFadeDefaults)
	{
		DistanceFromPlaneFadeoutEnd = DistanceFromPlaneFadeEnd_DEPRECATED;
		DistanceFromPlaneFadeoutStart = DistanceFromPlaneFadeStart_DEPRECATED;
	}
}

void UPlanarReflectionComponent::CreateRenderState_Concurrent()
{
	UpdatePreviewShape();

	Super::CreateRenderState_Concurrent();

	if (ShouldComponentAddToScene() && ShouldRender())
	{
		SceneProxy = new FPlanarReflectionSceneProxy(this, RenderTarget);
		GetWorld()->Scene->AddPlanarReflection(this);
	}
}

void UPlanarReflectionComponent::SendRenderTransform_Concurrent()
{	
	UpdatePreviewShape();

	if (SceneProxy)
	{
		GetWorld()->Scene->UpdatePlanarReflectionTransform(this);
	}

	Super::SendRenderTransform_Concurrent();
}

void UPlanarReflectionComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (SceneProxy)
	{
		GetWorld()->Scene->RemovePlanarReflection(this);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroyPlanarReflectionCommand,
			FPlanarReflectionSceneProxy*,SceneProxy,SceneProxy,
		{
			delete SceneProxy;
		});

		SceneProxy = NULL;
	}
}

#if WITH_EDITOR

void UPlanarReflectionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	for (int32 ViewIndex = 0; ViewIndex < ViewStates.Num(); ViewIndex++)
	{
		// Recreate the view state to reset temporal history so that property changes can be seen immediately
		ViewStates[ViewIndex].Destroy();
		ViewStates[ViewIndex].Allocate();
	}
}

#endif

void UPlanarReflectionComponent::BeginDestroy()
{
	if (RenderTarget)
	{
		BeginReleaseResource(RenderTarget);
	}
	
	// Begin a fence to track the progress of the BeginReleaseResource being processed by the RT
	ReleaseResourcesFence.BeginFence();

	Super::BeginDestroy();
}

bool UPlanarReflectionComponent::IsReadyForFinishDestroy()
{
	// Wait until the fence is complete before allowing destruction
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.IsFenceComplete();
}

void UPlanarReflectionComponent::FinishDestroy()
{
	Super::FinishDestroy();

	delete RenderTarget;
	RenderTarget = NULL;
}

void UPlanarReflectionComponent::UpdatePreviewShape()
{
	if (PreviewBox)
	{
		PreviewBox->InitBoxExtent(FVector(500 * 4, 500 * 4, DistanceFromPlaneFadeoutEnd));
	}
}

// -----------------------------------------------


USceneCaptureComponentCube::USceneCaptureComponentCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	bTickInEditor = true;
}

void USceneCaptureComponentCube::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	// Update content on register to have at least one frames worth of good data.
	// Without updating here this component would not work in a blueprint construction script which recreates the component after each move in the editor
	CaptureSceneDeferred();
#endif
}

void USceneCaptureComponentCube::SendRenderTransform_Concurrent()
{	
	if (bCaptureOnMovement)
	{
		CaptureSceneDeferred();
	}

	Super::SendRenderTransform_Concurrent();
}

void USceneCaptureComponentCube::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCaptureEveryFrame)
	{
		CaptureSceneDeferred();
	}
}

void USceneCaptureComponentCube::CaptureSceneDeferred()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// Defer until after updates finish
		// Needs some CS because of parallel updates.
		static FCriticalSection CriticalSection;
		FScopeLock ScopeLock(&CriticalSection);
		SceneCapturesToUpdateMap.AddUnique( World, this );
	}	
}

void USceneCaptureComponentCube::CaptureScene()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// We must push any deferred render state recreations before causing any rendering to happen, to make sure that deleted resource references are updated
		World->SendAllEndOfFrameUpdates();
		World->Scene->UpdateSceneCaptureContents(this);
	}	

	if (bCaptureEveryFrame)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("CaptureScene", "CaptureScene: Scene capture with bCaptureEveryFrame enabled was told to update - major inefficiency."));
	}
}

void USceneCaptureComponentCube::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	Scene->UpdateSceneCaptureContents(this);
}

#if WITH_EDITOR
void USceneCaptureComponentCube::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CaptureSceneDeferred();
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE