// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityCaptureComponent.h"
#include "MotionControllerComponent.h"
#include "MixedRealityBillboard.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MediaPlayer.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h" // for GetWorld() 
#include "Engine/World.h" // for GetPlayerControllerIterator()
#include "GameFramework/PlayerController.h" // for GetPawn()
#include "MixedRealityUtilLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MixedRealityConfigurationSaveGame.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaCaptureSupport.h"
#include "MixedRealityGarbageMatteCaptureComponent.h"
#include "Engine/StaticMesh.h"

#if WITH_EDITORONLY_DATA
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMixedReality, Log, All);

/* FChromaKeyParams
 *****************************************************************************/

//------------------------------------------------------------------------------
void FChromaKeyParams::ApplyToMaterial(UMaterialInstanceDynamic* Material) const
{
	if (Material)
	{
		static FName ColorName("ChromaColor");
		Material->SetVectorParameterValue(ColorName, ChromaColor);

		static FName ClipThresholdName("ChromaClipThreshold");
		Material->SetScalarParameterValue(ClipThresholdName, ChromaClipThreshold);

		static FName ToleranceCapName("ChromaToleranceCap");
		Material->SetScalarParameterValue(ToleranceCapName, ChromaToleranceCap);

		static FName EdgeSoftnessName("EdgeSoftness");
		Material->SetScalarParameterValue(EdgeSoftnessName, EdgeSoftness);
	}
}

/* UMixedRealityCaptureComponent
 *****************************************************************************/

//------------------------------------------------------------------------------
UMixedRealityCaptureComponent::UMixedRealityCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAutoTracking(false)
	, TrackingDevice(EControllerHand::Special_1)
{
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UMediaPlayer> DefaultMediaSource;
		ConstructorHelpers::FObjectFinder<UMaterial>    DefaultVideoProcessingMaterial;
		ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> DefaultRenderTarget;
		ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> DefaultGarbageMatteRenderTarget;
		ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultGarbageMatteMesh;
#if WITH_EDITORONLY_DATA
		ConstructorHelpers::FObjectFinder<UStaticMesh>  EditorCameraMesh;
#endif

		FConstructorStatics()
			: DefaultMediaSource(TEXT("/MixedRealityFramework/MRCameraSource"))
			, DefaultVideoProcessingMaterial(TEXT("/MixedRealityFramework/M_MRCamSrcProcessing"))
			, DefaultRenderTarget(TEXT("/MixedRealityFramework/T_MRRenderTarget"))
			, DefaultGarbageMatteRenderTarget(TEXT("/MixedRealityFramework/T_MRGarbageMatteRenderTarget"))
			, DefaultGarbageMatteMesh(TEXT("/MixedRealityFramework/GarbageMattePlane"))
#if WITH_EDITORONLY_DATA
			, EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"))
#endif
		{}
	};
	static FConstructorStatics ConstructorStatics;

	MediaSource = ConstructorStatics.DefaultMediaSource.Object;
	VideoProcessingMaterial = ConstructorStatics.DefaultVideoProcessingMaterial.Object;
	TextureTarget = ConstructorStatics.DefaultRenderTarget.Object;
	GarbageMatteCaptureTextureTarget = ConstructorStatics.DefaultGarbageMatteRenderTarget.Object;
	GarbageMatteMesh = ConstructorStatics.DefaultGarbageMatteMesh.Object;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		ProxyMesh = ConstructorStatics.EditorCameraMesh.Object;
	}
#endif

	// ensure InitializeComponent() gets called
	bWantsInitializeComponent = true;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
#if WITH_EDITORONLY_DATA
	UMixedRealityCaptureComponent* This = CastChecked<UMixedRealityCaptureComponent>(InThis);
	Collector.AddReferencedObject(This->ProxyMeshComponent);
#endif

	Super::AddReferencedObjects(InThis, Collector);
}
 
//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnRegister()
{
	// may reattach this component:
	RefreshDevicePairing();

	Super::OnRegister();

	if (!ProjectionActor)
	{
		ProjectionActor = NewObject<UChildActorComponent>(this, TEXT("MR_ProjectionPlane"), RF_Transient | RF_TextExportTransient);
		ProjectionActor->SetChildActorClass(AMixedRealityProjectionActor::StaticClass());
		ProjectionActor->SetupAttachment(this);

		ProjectionActor->RegisterComponent();

		AMixedRealityProjectionActor* ProjectionActorObj = CastChecked<AMixedRealityProjectionActor>(ProjectionActor->GetChildActor());
		ProjectionActorObj->SetProjectionMaterial(VideoProcessingMaterial);
		ProjectionActorObj->SetProjectionAspectRatio(GetDesiredAspectRatio());
	}

#if WITH_EDITORONLY_DATA
	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		if (ProxyMeshComponent == nullptr)
		{
			ProxyMeshComponent = NewObject<UStaticMeshComponent>(MyOwner, NAME_None, RF_Transactional | RF_TextExportTransient);
			ProxyMeshComponent->SetupAttachment(this);
			ProxyMeshComponent->bIsEditorOnly = true;
			ProxyMeshComponent->SetStaticMesh(ProxyMesh);
			ProxyMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			ProxyMeshComponent->bHiddenInGame = true;
			ProxyMeshComponent->CastShadow    = false;
			ProxyMeshComponent->PostPhysicsComponentTick.bCanEverTick = false;
			ProxyMeshComponent->CreationMethod = CreationMethod;
			ProxyMeshComponent->RegisterComponent();
		}
	}
#endif

	if (!GarbageMatteCaptureComponent)
	{
		GarbageMatteCaptureComponent = NewObject<UMixedRealityGarbageMatteCaptureComponent>(this, TEXT("MR_GarbageMatteCapture"), RF_Transient | RF_TextExportTransient);
		GarbageMatteCaptureComponent->CaptureSortPriority = CaptureSortPriority + 1;
		GarbageMatteCaptureComponent->TextureTarget = GarbageMatteCaptureTextureTarget;
		GarbageMatteCaptureComponent->GarbageMatteMesh = GarbageMatteMesh;
		GarbageMatteCaptureComponent->SetupAttachment(this);
		GarbageMatteCaptureComponent->RegisterComponent();
	}

	AttachMediaListeners();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (MediaSource)
	{
		AttachMediaListeners();
	}

	if (!VideoProcessingMaterial->IsA<UMaterialInstanceDynamic>())
	{
		SetVidProjectionMat(UMaterialInstanceDynamic::Create(VideoProcessingMaterial, this));
	}

	LoadDefaultConfiguration();

	if (CaptureDeviceURL.IsEmpty())
	{
		TArray<FMediaCaptureDeviceInfo> CaptureDevices;
		MediaCaptureSupport::EnumerateVideoCaptureDevices(CaptureDevices);

		if (CaptureDevices.Num() > 0)
		{
			CaptureDeviceURL = CaptureDevices[0].Url;
		}
	}
	RefreshCameraFeed();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
#if WITH_EDITORONLY_DATA
	if (ProxyMeshComponent)
	{
		const FTransform WorldXform = GetComponentToWorld();
		ProxyMeshComponent->SetWorldTransform(WorldXform);
	}
#endif

	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DetachMediaListeners();

#if WITH_EDITORONLY_DATA
	if (ProxyMeshComponent)
	{
		ProxyMeshComponent->DestroyComponent();
	}
#endif 

	if (ProjectionActor)
	{
		ProjectionActor->DestroyComponent();
	}

	if (PairedTracker)
	{
		PairedTracker->DestroyComponent();
	}

	if (GarbageMatteCaptureComponent)
	{
		GarbageMatteCaptureComponent->ShowOnlyActors.Empty();
		GarbageMatteCaptureComponent->DestroyComponent();
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

//------------------------------------------------------------------------------
#if WITH_EDITOR
void UMixedRealityCaptureComponent::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	const FName PropertyName = (PropertyThatWillChange != nullptr)
		? PropertyThatWillChange->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMixedRealityCaptureComponent, MediaSource))
	{
		DetachMediaListeners();
	}
}
#endif	// WITH_EDITOR

//------------------------------------------------------------------------------
#if WITH_EDITOR
void UMixedRealityCaptureComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMixedRealityCaptureComponent, MediaSource))
	{
		AttachMediaListeners();
	}
}
#endif	// WITH_EDITOR

//------------------------------------------------------------------------------
#if WITH_EDITOR
bool UMixedRealityCaptureComponent::GetEditorPreviewInfo(float /*DeltaTime*/, FMinimalViewInfo& ViewOut)
{
	ViewOut.Location = GetComponentLocation();
	ViewOut.Rotation = GetComponentRotation();

	ViewOut.FOV = FOVAngle;

	ViewOut.AspectRatio = GetDesiredAspectRatio();
	ViewOut.bConstrainAspectRatio = true;

	// see default in FSceneViewInitOptions
	ViewOut.bUseFieldOfViewForLOD = true;
	
 	ViewOut.ProjectionMode = ProjectionType;
	ViewOut.OrthoWidth = OrthoWidth;

	// see BuildProjectionMatrix() in SceneCaptureRendering.cpp
	ViewOut.OrthoNearClipPlane = 0.0f;
	ViewOut.OrthoFarClipPlane  = WORLD_MAX / 8.0f;;

	ViewOut.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		ViewOut.PostProcessSettings = PostProcessSettings;
	}

	return true;
}
#endif	// WITH_EDITOR

//------------------------------------------------------------------------------
const AActor* UMixedRealityCaptureComponent::GetViewOwner() const
{
	return GetProjectionActor();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::RefreshCameraFeed()
{
	SetCaptureDevice(CaptureDeviceURL);
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::RefreshDevicePairing()
{
	AActor* MyOwner = GetOwner();
#if WITH_EDITORONLY_DATA
	UWorld* MyWorld = (MyOwner) ? MyOwner->GetWorld() : nullptr;
	const bool bIsGameInst = MyWorld && MyWorld->WorldType != EWorldType::Editor && MyWorld->WorldType != EWorldType::EditorPreview;

	if (bIsGameInst)
#else 
	if (MyOwner)
#endif
	{
		if (bAutoTracking)
		{
			USceneComponent* Parent = GetAttachParent();
			UMotionControllerComponent* PreDefinedTracker = Cast<UMotionControllerComponent>(Parent);
			const bool bNeedsInternalController = (!PreDefinedTracker || PreDefinedTracker->Hand != TrackingDevice);

			if (bNeedsInternalController)
			{
				if (!PairedTracker)
				{
					PairedTracker = NewObject<UMotionControllerComponent>(this, TEXT("MR_MotionController"), RF_Transient | RF_TextExportTransient);
					
					USceneComponent* HMDRoot = UMixedRealityUtilLibrary::FindAssociatedHMDRoot(MyOwner);
					if (HMDRoot && HMDRoot->GetOwner() == MyOwner)
					{
						PairedTracker->SetupAttachment(HMDRoot);
					}
					else if (Parent)
					{
						PairedTracker->SetupAttachment(Parent, GetAttachSocketName());
					}
					else
					{
						MyOwner->SetRootComponent(PairedTracker);
					}
					PairedTracker->RegisterComponent();

					FAttachmentTransformRules ReattachRules(EAttachmentRule::KeepRelative, /*bWeldSimulatedBodies =*/false);
					AttachToComponent(PairedTracker, ReattachRules);
				}

				PairedTracker->Hand = TrackingDevice;
			}			
		}
		else if (PairedTracker)
		{
			PairedTracker->DestroyComponent(/*bPromoteChildren =*/true);
			PairedTracker = nullptr;
		}
	}
}

//------------------------------------------------------------------------------
AMixedRealityProjectionActor* UMixedRealityCaptureComponent::GetProjectionActor() const
{
	return ProjectionActor ? Cast<AMixedRealityProjectionActor>(ProjectionActor->GetChildActor()) : nullptr;
}

//------------------------------------------------------------------------------
AActor* UMixedRealityCaptureComponent::GetProjectionActor_K2() const
{
	return GetProjectionActor();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetVidProjectionMat(UMaterialInterface* NewMaterial)
{
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(NewMaterial);
	if (MID != nullptr)
	{
		ChromaKeySettings.ApplyToMaterial(MID);
	}
	// else, should we convert it to be a MID?

	VideoProcessingMaterial = NewMaterial;
	if (AMixedRealityProjectionActor* ProjectionTarget = GetProjectionActor())
	{
		ProjectionTarget->SetProjectionMaterial(NewMaterial);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetChromaSettings(const FChromaKeyParams& NewChromaSettings)
{
	NewChromaSettings.ApplyToMaterial(Cast<UMaterialInstanceDynamic>(VideoProcessingMaterial));
	ChromaKeySettings = NewChromaSettings;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetUnmaskedPixelHighlightColor(const FLinearColor& NewColor)
{
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(VideoProcessingMaterial);
	if (MID != nullptr)
	{
		static FName ParamName("UnmaskedPixelHighlightColor");
		MID->SetVectorParameterValue(ParamName, NewColor);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetDeviceAttachment(EControllerHand DeviceId)
{
	bAutoTracking = true;
	TrackingDevice = DeviceId;

	RefreshDevicePairing();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::DetatchFromDevice()
{
	bAutoTracking = false;
	RefreshDevicePairing();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::SetCaptureDevice(const FString& DeviceURL)
{
	const bool bIsInitialized = HasBeenInitialized();
	if (bIsInitialized && MediaSource && MediaSource->GetUrl() != DeviceURL)
	{
		MediaSource->Close();
		if (!DeviceURL.IsEmpty())
		{
			
#if WITH_EDITORONLY_DATA
			AActor* MyOwner = GetOwner();
			UWorld* MyWorld = (MyOwner) ? MyOwner->GetWorld() : nullptr;
			const bool bIsGameInst = MyWorld && MyWorld->WorldType != EWorldType::Editor && MyWorld->WorldType != EWorldType::EditorPreview;

			if (bIsGameInst)
#endif
			{
				if (!MediaSource->OpenUrl(DeviceURL))
				{
					UE_LOG(LogMixedReality, Warning, TEXT("Failed to open the specified capture device ('%s'). Falling back to the default."), *DeviceURL);
					MediaSource->OpenUrl(CaptureDeviceURL);
					return;
				}
			}
		}
	}
	CaptureDeviceURL = DeviceURL;
}

//------------------------------------------------------------------------------
float UMixedRealityCaptureComponent::GetDesiredAspectRatio() const
{
	float DesiredAspectRatio = 0.0f;

	if (MediaSource)
	{
		const int32 SelectedTrack = MediaSource->GetSelectedTrack(EMediaPlayerTrack::Video);
		DesiredAspectRatio = MediaSource->GetVideoTrackAspectRatio(SelectedTrack, MediaSource->GetTrackFormat(EMediaPlayerTrack::Video, SelectedTrack));
	}

	if (DesiredAspectRatio == 0.0f)
	{
		if (TextureTarget)
		{
			DesiredAspectRatio = TextureTarget->GetSurfaceWidth() / TextureTarget->GetSurfaceHeight();
		}
		else
		{
			DesiredAspectRatio = 16.f / 9.f;
		}
	}

	return DesiredAspectRatio;
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::AttachMediaListeners() const
{
	if (MediaSource)
	{
		MediaSource->OnMediaOpened.AddUniqueDynamic(this, &UMixedRealityCaptureComponent::OnVideoFeedOpened);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::DetachMediaListeners() const
{
	if (MediaSource)
	{
		MediaSource->OnMediaOpened.RemoveDynamic(this, &UMixedRealityCaptureComponent::OnVideoFeedOpened);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::OnVideoFeedOpened(FString MediaUrl)
{
	RefreshProjectionDimensions();
}

//------------------------------------------------------------------------------
void UMixedRealityCaptureComponent::RefreshProjectionDimensions()
{
	if (AMixedRealityProjectionActor* VidProjection = GetProjectionActor())
	{
		VidProjection->SetProjectionAspectRatio( GetDesiredAspectRatio() );
	}
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveAsDefaultConfiguration_K2()
{
	return SaveAsDefaultConfiguration();
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveAsDefaultConfiguration() const
{
	FString EmptySlotName;
	return SaveConfiguration(EmptySlotName, INDEX_NONE);
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveConfiguration_K2(const FString& SlotName, int32 UserIndex)
{
	return SaveConfiguration(SlotName, UserIndex);
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::SaveConfiguration(const FString& SlotName, int32 UserIndex) const
{
	const UMixedRealityConfigurationSaveGame* DefaultSaveData = GetDefault<UMixedRealityConfigurationSaveGame>();
	check(DefaultSaveData);
	const FString& LocalSlotName = SlotName.Len() > 0 ? SlotName : DefaultSaveData->SaveSlotName;
	const uint32 LocalUserIndex = SlotName.Len() > 0 ? UserIndex : DefaultSaveData->UserIndex;

	UMixedRealityConfigurationSaveGame* SaveGameInstance = Cast<UMixedRealityConfigurationSaveGame>(UGameplayStatics::LoadGameFromSlot(LocalSlotName, LocalUserIndex));
	if (SaveGameInstance)
	{
		UE_LOG(LogMixedReality, Log, TEXT("UMixedRealityCaptureComponent::SaveConfiguration to slot %s user %i has loaded the pre-existing save so we can update it."), *SlotName, UserIndex);
	}
	else
	{
		UE_LOG(LogMixedReality, Log, TEXT("UMixedRealityCaptureComponent::SaveConfiguration to slot %s user %i found no pre-existing save.  Creating a new one."), *SlotName, UserIndex);
		SaveGameInstance = Cast<UMixedRealityConfigurationSaveGame>(UGameplayStatics::CreateSaveGameObject(UMixedRealityConfigurationSaveGame::StaticClass()));
	}
	check(SaveGameInstance);

	// Clear as necessary before writing new information into the SaveGameInstance.

	// Write as necessary into the SaveGameInstance.
	// alignment data
	{
		const FTransform RelativeXform = GetRelativeTransform();
		SaveGameInstance->AlignmentData.CameraOrigin = RelativeXform.GetLocation();
		SaveGameInstance->AlignmentData.LookAtDir = RelativeXform.GetUnitAxis(EAxis::X);
		SaveGameInstance->AlignmentData.FOV = FOVAngle;
	}
	// compositing data
	{
		SaveGameInstance->CompositingData.ChromaKeySettings = ChromaKeySettings;
		SaveGameInstance->CompositingData.CaptureDeviceURL  = CaptureDeviceURL; 
	}	
	// garbage matte data is only read by this component.

	const bool Success = UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveGameInstance->SaveSlotName, SaveGameInstance->UserIndex);
	if (Success)
	{
		UE_LOG(LogMixedReality, Log, TEXT("UMixedRealityCaptureComponent::SaveConfiguration to slot %s user %i Succeeded."), *SlotName, UserIndex);
	} 
	else
	{
		UE_LOG(LogMixedReality, Warning, TEXT("UMixedRealityCaptureComponent::SaveConfiguration to slot %s user %i Failed!"), *SlotName, UserIndex);
	}
	return Success;
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::LoadDefaultConfiguration()
{
	FString EmptySlotName;
	return LoadConfiguration(EmptySlotName, INDEX_NONE);
}

//------------------------------------------------------------------------------
bool UMixedRealityCaptureComponent::LoadConfiguration(const FString& SlotName, int32 UserIndex)
{
	const UMixedRealityConfigurationSaveGame* DefaultSaveData = GetDefault<UMixedRealityConfigurationSaveGame>();
	check(DefaultSaveData);
	const FString& LocalSlotName = SlotName.Len() > 0 ? SlotName : DefaultSaveData->SaveSlotName;
	const uint32 LocalUserIndex = SlotName.Len() > 0 ? UserIndex : DefaultSaveData->UserIndex;

	UMixedRealityConfigurationSaveGame* SaveGameInstance = Cast<UMixedRealityConfigurationSaveGame>(UGameplayStatics::LoadGameFromSlot(LocalSlotName, LocalUserIndex));
	if (SaveGameInstance == nullptr)
	{
		UE_LOG(LogMixedReality, Warning, TEXT("UMixedRealityCaptureComponent::LoadConfiguration from slot %s user %i Failed!"), *SlotName, UserIndex);
		return false;
	}

	// Here one applies the information from the SaveGameInstance.
	// alignment data
	{
		SetRelativeLocation(SaveGameInstance->AlignmentData.CameraOrigin);
		SetRelativeRotation(FRotationMatrix::MakeFromX(SaveGameInstance->AlignmentData.LookAtDir).Rotator());
		FOVAngle = SaveGameInstance->AlignmentData.FOV;
	}
	// compositing data
	{
		SetChromaSettings(SaveGameInstance->CompositingData.ChromaKeySettings);
		SetCaptureDevice(SaveGameInstance->CompositingData.CaptureDeviceURL);
	}
	// GarbageMatteCaptureComponent
	{
		GarbageMatteCaptureComponent->ApplyConfiguration(*SaveGameInstance);
	}
	bCalibrated = true;

	UE_LOG(LogMixedReality, Log, TEXT("UMixedRealityCaptureComponent::LoadConfiguration from slot %s user %i Succeeded."), *SlotName, UserIndex);
	return true;
}

void UMixedRealityCaptureComponent::SetExternalGarbageMatteActor(AActor* Actor)
{
	check(GarbageMatteCaptureComponent);
	GarbageMatteCaptureComponent->SetExternalGarbageMatteActor(Actor);
}

void UMixedRealityCaptureComponent::ClearExternalGarbageMatteActor()
{
	check(GarbageMatteCaptureComponent);
	GarbageMatteCaptureComponent->ClearExternalGarbageMatteActor();
}