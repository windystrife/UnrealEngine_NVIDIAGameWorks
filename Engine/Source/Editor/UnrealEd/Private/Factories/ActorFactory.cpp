// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ActorFactory.cpp: 
=============================================================================*/

#include "ActorFactories/ActorFactory.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Model.h"
#include "ActorFactories/ActorFactoryAmbientSound.h"
#include "ActorFactories/ActorFactoryAtmosphericFog.h"
#include "ActorFactories/ActorFactoryBlueprint.h"
#include "ActorFactories/ActorFactoryBoxReflectionCapture.h"
#include "ActorFactories/ActorFactoryBoxVolume.h"
#include "ActorFactories/ActorFactoryCameraActor.h"
#include "ActorFactories/ActorFactoryCharacter.h"
#include "ActorFactories/ActorFactoryClass.h"
#include "ActorFactories/ActorFactoryCylinderVolume.h"
#include "ActorFactories/ActorFactoryDeferredDecal.h"
#include "ActorFactories/ActorFactoryDirectionalLight.h"
#include "ActorFactories/ActorFactoryEmitter.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "ActorFactories/ActorFactoryPawn.h"
#include "ActorFactories/ActorFactoryExponentialHeightFog.h"
#include "ActorFactories/ActorFactoryMatineeActor.h"
#include "ActorFactories/ActorFactoryNote.h"
#include "ActorFactories/ActorFactoryPhysicsAsset.h"
#include "ActorFactories/ActorFactoryPlaneReflectionCapture.h"
#include "ActorFactories/ActorFactoryPlayerStart.h"
#include "ActorFactories/ActorFactoryPointLight.h"
#include "ActorFactories/ActorFactorySkeletalMesh.h"
#include "ActorFactories/ActorFactoryAnimationAsset.h"
#include "ActorFactories/ActorFactorySkyLight.h"
#include "ActorFactories/ActorFactorySphereReflectionCapture.h"
#include "ActorFactories/ActorFactorySphereVolume.h"
#include "ActorFactories/ActorFactorySpotLight.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
#include "ActorFactories/ActorFactoryInteractiveFoliage.h"
#include "ActorFactories/ActorFactoryTargetPoint.h"
#include "ActorFactories/ActorFactoryTextRender.h"
#include "ActorFactories/ActorFactoryTriggerBox.h"
#include "ActorFactories/ActorFactoryTriggerCapsule.h"
#include "ActorFactories/ActorFactoryTriggerSphere.h"
#include "ActorFactories/ActorFactoryVectorFieldVolume.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Materials/Material.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/BrushBuilder.h"
#include "Builders/CubeBuilder.h"
#include "Builders/CylinderBuilder.h"
#include "Builders/TetrahedronBuilder.h"
#include "AssetData.h"
#include "Editor/EditorEngine.h"
#include "Animation/AnimBlueprint.h"
#include "Particles/ParticleSystem.h"
#include "Engine/Texture2D.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/Character.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "Particles/Emitter.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Sound/AmbientSound.h"
#include "GameFramework/Volume.h"
#include "Engine/DecalActor.h"
#include "Atmosphere/AtmosphericFog.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/Note.h"
#include "Engine/BoxReflectionCapture.h"
#include "Engine/PlaneReflectionCapture.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "VectorField/VectorFieldVolume.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Engine/Polys.h"
#include "StaticMeshResources.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BSPOps.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "InteractiveFoliageActor.h"

#include "AssetRegistryModule.h"



#include "VectorField/VectorField.h"

#include "Engine/TriggerBox.h"
#include "Engine/TriggerSphere.h"
#include "Engine/TriggerCapsule.h"
#include "Engine/TextRenderActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Components/AudioComponent.h"
#include "Components/BrushComponent.h"
#include "Components/VectorFieldComponent.h"
#include "ActorFactories/ActorFactoryPlanarReflection.h"
#include "Engine/PlanarReflection.h"

#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "Factories/ActorFactoryMovieScene.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#endif

// NVCHANGE_BEGIN: Add VXGI
#include "ActorFactories/ActorFactoryVxgiAnchor.h"
#include "Engine/VxgiAnchor.h"
// NVCHANGE_END: Add VXGI

DEFINE_LOG_CATEGORY(LogActorFactory);

#if WITH_FLEX
#include "ActorFactories/ActorFactoryFlex.h"
#include "PhysicsEngine/FlexActor.h"
#endif

#define LOCTEXT_NAMESPACE "ActorFactory"

/**
 * Find am alignment transform for the specified actor rotation, given a model-space axis to align, and a world space normal to align to.
 * This function attempts to find a 'natural' looking rotation by rotating around a local pitch axis, and a world Z. Rotating in this way
 * should retain the roll around the model space axis, removing rotation artifacts introduced by a simpler quaternion rotation.
 */
FQuat FindActorAlignmentRotation(const FQuat& InActorRotation, const FVector& InModelAxis, const FVector& InWorldNormal)
{
	FVector TransformedModelAxis = InActorRotation.RotateVector(InModelAxis);

	const auto InverseActorRotation = InActorRotation.Inverse();
	const auto DestNormalModelSpace = InverseActorRotation.RotateVector(InWorldNormal);

	FQuat DeltaRotation = FQuat::Identity;

	const float VectorDot = InWorldNormal | TransformedModelAxis;
	if (1.f - FMath::Abs(VectorDot) <= KINDA_SMALL_NUMBER)
	{
		if (VectorDot < 0.f)
		{
			// Anti-parallel
			return InActorRotation * FQuat::FindBetween(InModelAxis, DestNormalModelSpace);
		}
	}
	else
	{
		const FVector Z(0.f, 0.f, 1.f);

		// Find a reference axis to measure the relative pitch rotations between the source axis, and the destination axis.
		FVector PitchReferenceAxis = InverseActorRotation.RotateVector(Z);
		if (FMath::Abs(FVector::DotProduct(InModelAxis, PitchReferenceAxis)) > 0.7f)
		{
			PitchReferenceAxis = DestNormalModelSpace;
		}
		
		// Find a local 'pitch' axis to rotate around
		const FVector OrthoPitchAxis = FVector::CrossProduct(PitchReferenceAxis, InModelAxis);
		const float Pitch = FMath::Acos(PitchReferenceAxis | DestNormalModelSpace) - FMath::Acos(PitchReferenceAxis | InModelAxis);//FMath::Asin(OrthoPitchAxis.Size());

		DeltaRotation = FQuat(OrthoPitchAxis.GetSafeNormal(), Pitch);
		DeltaRotation.Normalize();

		// Transform the model axis with this new pitch rotation to see if there is any need for yaw
		TransformedModelAxis = (InActorRotation * DeltaRotation).RotateVector(InModelAxis);

		const float ParallelDotThreshold = 0.98f; // roughly 11.4 degrees (!)
		if (!FVector::Coincident(InWorldNormal, TransformedModelAxis, ParallelDotThreshold))
		{
			const float Yaw = FMath::Atan2(InWorldNormal.X, InWorldNormal.Y) - FMath::Atan2(TransformedModelAxis.X, TransformedModelAxis.Y);

			// Rotation axis for yaw is the Z axis in world space
			const FVector WorldYawAxis = (InActorRotation * DeltaRotation).Inverse().RotateVector(Z);
			DeltaRotation *= FQuat(WorldYawAxis, -Yaw);
		}
	}

	return InActorRotation * DeltaRotation;
}

/*-----------------------------------------------------------------------------
UActorFactory
-----------------------------------------------------------------------------*/
UActorFactory::UActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DefaultName","Actor");
	bShowInEditorQuickMenu = false;
}

bool UActorFactory::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	// By Default we assume the factory can't work with existing asset data
	return !AssetData.IsValid() || 
		AssetData.ObjectPath == FName(*GetDefaultActor( AssetData )->GetPathName()) || 
		AssetData.ObjectPath == FName(*GetDefaultActor( AssetData )->GetClass()->GetPathName());
}

AActor* UActorFactory::GetDefaultActor( const FAssetData& AssetData )
{
	if ( NewActorClassName != TEXT("") )
	{
		UE_LOG(LogActorFactory, Log, TEXT("Loading ActorFactory Class %s"), *NewActorClassName);
		NewActorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *NewActorClassName, NULL, LOAD_NoWarn, NULL));
		NewActorClassName = TEXT("");
		if ( NewActorClass == NULL )
		{
			UE_LOG(LogActorFactory, Log, TEXT("ActorFactory Class LOAD FAILED"));
		}
	}
	return NewActorClass ? NewActorClass->GetDefaultObject<AActor>() : NULL;
}

UClass* UActorFactory::GetDefaultActorClass( const FAssetData& AssetData )
{
	if ( !NewActorClass )
	{
		GetDefaultActor( AssetData );
	}

	return NewActorClass;
}

UObject* UActorFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	return NULL;
}

FQuat UActorFactory::AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const
{
	if (bUseSurfaceOrientation)
	{
		// By default we align the X axis with the inverse of the surface normal (so things look at the surface)
		return FindActorAlignmentRotation(ActorRotation, FVector(-1.f, 0.f, 0.f), InSurfaceNormal);
	}
	else
	{
		return FQuat::Identity;
	}
}

AActor* UActorFactory::CreateActor( UObject* Asset, ULevel* InLevel, FTransform SpawnTransform, EObjectFlags InObjectFlags, const FName Name )
{
	AActor* NewActor = NULL;

	if ( PreSpawnActor(Asset, SpawnTransform) )
	{
		NewActor = SpawnActor(Asset, InLevel, SpawnTransform, InObjectFlags, Name);

		if ( NewActor )
		{
			PostSpawnActor(Asset, NewActor);

			// Only do this if the actor wasn't already given a name
			if (Name == NAME_None && Asset)
			{
				FActorLabelUtilities::SetActorLabelUnique(NewActor, Asset->GetName());
			}
		}
	}

	return NewActor;
}

UBlueprint* UActorFactory::CreateBlueprint( UObject* Asset, UObject* Outer, const FName Name, const FName CallingContext )
{
	// @todo sequencer major: Needs to be overridden on any class that needs any custom setup for the new blueprint
	//	(e.g. static mesh reference assignment.)  Basically, anywhere that PostSpawnActor() or CreateActor() is overridden,
	//	we should consider overriding CreateBlueprint(), too.
	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(NewActorClass, Outer, Name, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
	AActor* CDO = CastChecked<AActor>( NewBlueprint->GeneratedClass->ClassDefaultObject );
	PostCreateBlueprint( Asset, CDO );
	return NewBlueprint;
}

bool UActorFactory::PreSpawnActor( UObject* Asset, FTransform& InOutLocation)
{
	// Subclasses may implement this to set up a spawn or to adjust the spawn location or rotation.
	return true;
}

AActor* UActorFactory::SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName Name )
{
	AActor* DefaultActor = GetDefaultActor( FAssetData( Asset ) );
	if ( DefaultActor )
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = InLevel;
		SpawnInfo.ObjectFlags = InObjectFlags;
		SpawnInfo.Name = Name;
#if WITH_EDITOR
		SpawnInfo.bTemporaryEditorActor = GEditor->bIsSimulatingInEditor ? FLevelEditorViewportClient::IsDroppingPreviewActor(): true;
#endif
		return InLevel->OwningWorld->SpawnActor( DefaultActor->GetClass(), &Transform, SpawnInfo );
	}

	return NULL;
}

void UActorFactory::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
}

void UActorFactory::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	// Override this in derived actor factories to initialize the blueprint's CDO based on the asset assigned to the factory!
}


/*-----------------------------------------------------------------------------
UActorFactoryStaticMesh
-----------------------------------------------------------------------------*/
UActorFactoryStaticMesh::UActorFactoryStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("StaticMeshDisplayName", "Static Mesh");
	NewActorClass = AStaticMeshActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactoryStaticMesh::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UStaticMesh::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoStaticMesh", "A valid static mesh must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryStaticMesh::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(Asset);

	UE_LOG(LogActorFactory, Log, TEXT("Actor Factory created %s"), *StaticMesh->GetName());

	// Change properties
	AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>( NewActor );
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
	check(StaticMeshComponent);

	StaticMeshComponent->UnregisterComponent();

	StaticMeshComponent->SetStaticMesh(StaticMesh);
	StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;

	// Init Component
	StaticMeshComponent->RegisterComponent();
}

UObject* UActorFactoryStaticMesh::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AStaticMeshActor* SMA = CastChecked<AStaticMeshActor>(Instance);

	check(SMA->GetStaticMeshComponent());
	return SMA->GetStaticMeshComponent()->GetStaticMesh();
}

void UActorFactoryStaticMesh::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(Asset);
		AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>(CDO);
		UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();

		StaticMeshComponent->SetStaticMesh(StaticMesh);
		StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
	}
}

FQuat UActorFactoryStaticMesh::AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const
{
	// Meshes align the Z (up) axis with the surface normal
	return FindActorAlignmentRotation(ActorRotation, FVector(0.f, 0.f, 1.f), InSurfaceNormal);
}

#if WITH_FLEX

AActor* UActorFactoryStaticMesh::SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlagsIn, const FName Name)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);
	if (StaticMesh && StaticMesh->FlexAsset && NewActorClassName == TEXT(""))
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = InLevel;
		SpawnInfo.ObjectFlags = ObjectFlagsIn;
		SpawnInfo.Name = Name;
		return InLevel->OwningWorld->SpawnActor(AFlexActor::StaticClass(), &Transform, SpawnInfo);
	}
	return Super::SpawnActor(Asset, InLevel, Transform, ObjectFlagsIn, Name);
}

#endif // WITH_FLEX

/*-----------------------------------------------------------------------------
UActorFactoryBasicShape
-----------------------------------------------------------------------------*/

const FName UActorFactoryBasicShape::BasicCube("/Engine/BasicShapes/Cube.Cube");
const FName UActorFactoryBasicShape::BasicSphere("/Engine/BasicShapes/Sphere.Sphere");
const FName UActorFactoryBasicShape::BasicCylinder("/Engine/BasicShapes/Cylinder.Cylinder");
const FName UActorFactoryBasicShape::BasicCone("/Engine/BasicShapes/Cone.Cone");
const FName UActorFactoryBasicShape::BasicPlane("/Engine/BasicShapes/Plane.Plane");

UActorFactoryBasicShape::UActorFactoryBasicShape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("UActorFactoryBasicShapeDisplayName", "Basic Shape");
	NewActorClass = AStaticMeshActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactoryBasicShape::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if(AssetData.IsValid() && (AssetData.ObjectPath == BasicCube || AssetData.ObjectPath == BasicSphere || AssetData.ObjectPath == BasicCone || AssetData.ObjectPath == BasicCylinder || AssetData.ObjectPath == BasicPlane) )
	{
		return true;
	}

	return false;
}

void UActorFactoryBasicShape::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	// Change properties
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(Asset);

	AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>(NewActor);
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();

	if( StaticMeshComponent )
	{
		StaticMeshComponent->UnregisterComponent();

		StaticMeshComponent->SetStaticMesh(StaticMesh);
		StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
		StaticMeshComponent->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
		// Init Component
		StaticMeshComponent->RegisterComponent();
	}
}


/*-----------------------------------------------------------------------------
UActorFactoryDeferredDecal
-----------------------------------------------------------------------------*/
UActorFactoryDeferredDecal::UActorFactoryDeferredDecal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ 
	DisplayName = LOCTEXT("DeferredDecalDisplayName", "Deferred Decal");
	NewActorClass = ADecalActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactoryDeferredDecal::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	//We can create a DecalActor without an existing asset
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	//But if an asset is specified it must be based-on a deferred decal umaterial
	if ( !AssetData.GetClass()->IsChildOf( UMaterialInterface::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoMaterial", "A valid material must be specified.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	uint32 SanityCheck = 0;
	FAssetData CurrentAssetData = AssetData;
	while( SanityCheck < 1000 && !CurrentAssetData.GetClass()->IsChildOf( UMaterial::StaticClass() ) )
	{
		const FString ObjectPath = CurrentAssetData.GetTagValueRef<FString>( "Parent" );
		if ( ObjectPath.IsEmpty() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoMaterial", "A valid material must be specified.");
			return false;
		}

		CurrentAssetData = AssetRegistry.GetAssetByObjectPath( *ObjectPath );
		if ( !CurrentAssetData.IsValid() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoMaterial", "A valid material must be specified.");
			return false;
		}

		++SanityCheck;
	}

	if ( SanityCheck >= 1000 )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "RecursiveParentMaterial", "The specified material must not have a recursive parent.");
		return false;
	}

	if ( !CurrentAssetData.GetClass()->IsChildOf( UMaterial::StaticClass() ) )
	{
		return false;
	}

	const FString MaterialDomain = CurrentAssetData.GetTagValueRef<FString>( "MaterialDomain" );
	if ( MaterialDomain != TEXT("MD_DeferredDecal") )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NotDecalMaterial", "Only materials with a material domain of DeferredDecal can be specified.");
		return false;
	}

	return true;
}

void UActorFactoryDeferredDecal::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UMaterialInterface* Material = GetMaterial( Asset );

	if ( Material != NULL )
	{
		// Change properties
		TInlineComponentArray<UDecalComponent*> DecalComponents;
		NewActor->GetComponents(DecalComponents);

		UDecalComponent* DecalComponent = NULL;
		for (int32 Idx = 0; Idx < DecalComponents.Num() && DecalComponent == NULL; Idx++)
		{
			DecalComponent = DecalComponents[Idx];
		}

		check(DecalComponent);

		DecalComponent->UnregisterComponent();

		DecalComponent->DecalMaterial = Material;

		// Init Component
		DecalComponent->RegisterComponent();
	}
}

void UActorFactoryDeferredDecal::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		UMaterialInterface* Material = GetMaterial(Asset);

		if (Material != NULL)
		{
			TInlineComponentArray<UDecalComponent*> DecalComponents;
			CDO->GetComponents(DecalComponents);

			UDecalComponent* DecalComponent = NULL;
			for (int32 Idx = 0; Idx < DecalComponents.Num() && DecalComponent == NULL; Idx++)
			{
				DecalComponent = DecalComponents[Idx];
			}

			check(DecalComponent);
			DecalComponent->DecalMaterial = Material;
		}
	}
}

UMaterialInterface* UActorFactoryDeferredDecal::GetMaterial( UObject* Asset ) const
{
	UMaterialInterface* TargetMaterial = Cast<UMaterialInterface>( Asset );

	return TargetMaterial 
		&& TargetMaterial->GetMaterial() 
		&& TargetMaterial->GetMaterial()->MaterialDomain == MD_DeferredDecal ? 
TargetMaterial : 
	NULL;
}

/*-----------------------------------------------------------------------------
UActorFactoryTextRender
-----------------------------------------------------------------------------*/
UActorFactoryTextRender::UActorFactoryTextRender(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Property initialization
	DisplayName = LOCTEXT("TextRenderDisplayName", "Text Render");
	NewActorClass = ATextRenderActor::StaticClass();
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactoryEmitter
-----------------------------------------------------------------------------*/
UActorFactoryEmitter::UActorFactoryEmitter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("EmitterDisplayName", "Emitter");
	NewActorClass = AEmitter::StaticClass();
}

bool UActorFactoryEmitter::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UParticleSystem::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoParticleSystem", "A valid particle system must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryEmitter::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UParticleSystem* ParticleSystem = CastChecked<UParticleSystem>(Asset);
	AEmitter* NewEmitter = CastChecked<AEmitter>(NewActor);

	// Term Component
	NewEmitter->GetParticleSystemComponent()->UnregisterComponent();

	// Change properties
	NewEmitter->SetTemplate(ParticleSystem);

	// if we're created by Kismet on the server during gameplay, we need to replicate the emitter
	if (NewEmitter->GetWorld()->HasBegunPlay() && NewEmitter->GetWorld()->GetNetMode() != NM_Client)
	{
		NewEmitter->SetReplicates(true);
		NewEmitter->bAlwaysRelevant = true;
		NewEmitter->NetUpdateFrequency = 0.1f; // could also set bNetTemporary but LD might further trigger it or something
		// call into gameplay code with template so it can set up replication
		NewEmitter->SetTemplate(ParticleSystem);
	}

	// Init Component
	NewEmitter->GetParticleSystemComponent()->RegisterComponent();
}

UObject* UActorFactoryEmitter::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AEmitter* Emitter = CastChecked<AEmitter>(Instance);
	if (Emitter->GetParticleSystemComponent())
	{
		return Emitter->GetParticleSystemComponent()->Template;
	}
	else
	{
		return NULL;
	}
}

void UActorFactoryEmitter::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		UParticleSystem* ParticleSystem = CastChecked<UParticleSystem>(Asset);
		AEmitter* Emitter = CastChecked<AEmitter>(CDO);
		Emitter->SetTemplate(ParticleSystem);
	}
}



/*-----------------------------------------------------------------------------
UActorFactoryPlayerStart
-----------------------------------------------------------------------------*/
UActorFactoryPlayerStart::UActorFactoryPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("PlayerStartDisplayName", "Player Start");
	NewActorClass = APlayerStart::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTargetPoint
-----------------------------------------------------------------------------*/
UActorFactoryTargetPoint::UActorFactoryTargetPoint( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
{
	DisplayName = LOCTEXT( "TargetPointDisplayName", "Target Point" );
	NewActorClass = ATargetPoint::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryNote
-----------------------------------------------------------------------------*/
UActorFactoryNote::UActorFactoryNote( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
{
	DisplayName = LOCTEXT( "NoteDisplayName", "Note" );
	NewActorClass = ANote::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryPhysicsAsset
-----------------------------------------------------------------------------*/
UActorFactoryPhysicsAsset::UActorFactoryPhysicsAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("PhysicsAssetDisplayName", "Skeletal Physics");
	NewActorClass = ASkeletalMeshActor::StaticClass();
}

bool UActorFactoryPhysicsAsset::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UPhysicsAsset::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoPhysicsAsset", "A valid physics asset must be specified.");
		return false;
	}

	return true;
}

bool UActorFactoryPhysicsAsset::PreSpawnActor(UObject* Asset, FTransform& InOutLocation)
{
	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);
	USkeletalMesh* UseSkelMesh = PhysicsAsset->PreviewSkeletalMesh.LoadSynchronous();

	if(!UseSkelMesh)
	{
		return false;
	}

	return true;
}

void UActorFactoryPhysicsAsset::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);
	USkeletalMesh* UseSkelMesh = PhysicsAsset->PreviewSkeletalMesh.Get();

	ASkeletalMeshActor* NewSkelActor = CastChecked<ASkeletalMeshActor>(NewActor);

	// Term Component
	NewSkelActor->GetSkeletalMeshComponent()->UnregisterComponent();

	// Change properties
	NewSkelActor->GetSkeletalMeshComponent()->SkeletalMesh = UseSkelMesh;
	if (NewSkelActor->GetWorld()->IsPlayInEditor())
	{
		NewSkelActor->ReplicatedMesh = UseSkelMesh;
		NewSkelActor->ReplicatedPhysAsset = PhysicsAsset;
	}
	NewSkelActor->GetSkeletalMeshComponent()->PhysicsAssetOverride = PhysicsAsset;

	// set physics setup
	NewSkelActor->GetSkeletalMeshComponent()->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
	NewSkelActor->GetSkeletalMeshComponent()->BodyInstance.bSimulatePhysics = true;
	NewSkelActor->GetSkeletalMeshComponent()->bBlendPhysics = true;

	NewSkelActor->bAlwaysRelevant = true;
	NewSkelActor->bReplicateMovement = true;
	NewSkelActor->SetReplicates(true);

	// Init Component
	NewSkelActor->GetSkeletalMeshComponent()->RegisterComponent();
}

void UActorFactoryPhysicsAsset::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (CDO != NULL)
	{
		ASkeletalMeshActor* SkeletalPhysicsActor = CastChecked<ASkeletalMeshActor>(CDO);

		if (Asset != NULL)
		{
			UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);

			USkeletalMesh* UseSkelMesh = PhysicsAsset->PreviewSkeletalMesh.Get();

			SkeletalPhysicsActor->GetSkeletalMeshComponent()->SkeletalMesh = UseSkelMesh;
			SkeletalPhysicsActor->GetSkeletalMeshComponent()->PhysicsAssetOverride = PhysicsAsset;
		}

		// set physics setup
		SkeletalPhysicsActor->GetSkeletalMeshComponent()->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
		SkeletalPhysicsActor->GetSkeletalMeshComponent()->BodyInstance.bSimulatePhysics = true;
		SkeletalPhysicsActor->GetSkeletalMeshComponent()->bBlendPhysics = true;

		SkeletalPhysicsActor->bAlwaysRelevant = true;
		SkeletalPhysicsActor->bReplicateMovement = true;
		SkeletalPhysicsActor->SetReplicates(true);
	}
}


/*-----------------------------------------------------------------------------
UActorFactoryAnimationAsset
-----------------------------------------------------------------------------*/
UActorFactoryAnimationAsset::UActorFactoryAnimationAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("SingleAnimSkeletalDisplayName", "Single Animation Skeletal");
	NewActorClass = ASkeletalMeshActor::StaticClass();
}

bool UActorFactoryAnimationAsset::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{ 
	if ( !AssetData.IsValid() || 
		( !AssetData.GetClass()->IsChildOf( UAnimSequenceBase::StaticClass() ) )) 
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimData", "A valid anim data must be specified.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if ( AssetData.GetClass()->IsChildOf( UAnimSequenceBase::StaticClass() ) )
	{
		const FString SkeletonPath = AssetData.GetTagValueRef<FString>("Skeleton");
		if ( SkeletonPath.IsEmpty() ) 
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeleton", "UAnimationAssets must have a valid Skeleton.");
			return false;
		}

		FAssetData SkeletonData = AssetRegistry.GetAssetByObjectPath( *SkeletonPath );

		if ( !SkeletonData.IsValid() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeleton", "UAnimationAssets must have a valid Skeleton.");
			return false;
		}

		// skeleton should be loaded by this time. If not, we have problem
		// so I'm changing this to load directly not using tags and values
		USkeleton* Skeleton = Cast<USkeleton>(SkeletonData.GetAsset());
		if (Skeleton)
		{
			USkeletalMesh * PreviewMesh = Skeleton->GetPreviewMesh(true);
			if (PreviewMesh)
			{ 
				return true;
			}
			else
			{
				OutErrorMsg = NSLOCTEXT("CanCreateActor", "UAnimationAssetNoSkeleton", "UAnimationAssets must have a valid Skeleton with a valid preview skeletal mesh.");
				return false;
			}			
		}
		else
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeleton", "UAnimationAssets must have a valid Skeleton.");
			return false;
		}
	}

	return true;
}

USkeletalMesh* UActorFactoryAnimationAsset::GetSkeletalMeshFromAsset( UObject* Asset ) const
{
	USkeletalMesh* SkeletalMesh = NULL;
	
	if(UAnimSequenceBase* AnimationAsset = Cast<UAnimSequenceBase>(Asset))
	{
		// base it on preview skeletal mesh, just to have something
		SkeletalMesh = AnimationAsset->GetSkeleton() ? AnimationAsset->GetSkeleton()->GetAssetPreviewMesh(AnimationAsset) : nullptr;
	}
	else if(UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset))
	{
		SkeletalMesh = AnimBlueprint->TargetSkeleton ? AnimBlueprint->TargetSkeleton->GetAssetPreviewMesh(AnimBlueprint) : nullptr;
	}

	// Check to see if we are using a custom factory in which case this should probably be ignored. This seems kind of wrong...
	if( SkeletalMesh && SkeletalMesh->HasCustomActorFactory())
	{
		SkeletalMesh = NULL;
	}

	check( SkeletalMesh != NULL );
	return SkeletalMesh;
}

void UActorFactoryAnimationAsset::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor( Asset, NewActor );
	UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(Asset);

	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);
	USkeletalMeshComponent* NewSASComponent = (NewSMActor->GetSkeletalMeshComponent());

	if( NewSASComponent )
	{
		if( AnimationAsset )
		{
			NewSASComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
			NewSASComponent->AnimationData.AnimToPlay = AnimationAsset;
			
			// set runtime data
			NewSASComponent->SetAnimation(AnimationAsset);

			if (UAnimSequenceBase* AnimSeq = Cast<UAnimSequenceBase>(AnimationAsset))
			{
				//If we have a negative play rate, default initial position to sequence end
				if (AnimSeq->RateScale < 0.f)
				{
					NewSASComponent->AnimationData.SavedPosition = AnimSeq->SequenceLength;
					NewSASComponent->SetPosition(AnimSeq->SequenceLength, false);
				}
			}
			
		}
	}
}

void UActorFactoryAnimationAsset::PostCreateBlueprint( UObject* Asset,  AActor* CDO )
{
	Super::PostCreateBlueprint( Asset, CDO );
	
	if (Asset != NULL && CDO != NULL)
	{
		UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(Asset);

		ASkeletalMeshActor* SkeletalMeshActor = CastChecked<ASkeletalMeshActor>(CDO);
		USkeletalMeshComponent* SkeletalComponent = (SkeletalMeshActor->GetSkeletalMeshComponent());
		if (AnimationAsset)
		{
			SkeletalComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
			SkeletalComponent->SetAnimation(AnimationAsset);
		}
	}
}


/*-----------------------------------------------------------------------------
UActorFactorySkeletalMesh
-----------------------------------------------------------------------------*/
UActorFactorySkeletalMesh::UActorFactorySkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ 
	DisplayName = LOCTEXT("SkeletalMeshDisplayName", "Skeletal Mesh");
	NewActorClass = ASkeletalMeshActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactorySkeletalMesh::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{	
	if ( !AssetData.IsValid() || 
		( !AssetData.GetClass()->IsChildOf( USkeletalMesh::StaticClass() ) && 
		  !AssetData.GetClass()->IsChildOf( UAnimBlueprint::StaticClass() ) && 
		  !AssetData.GetClass()->IsChildOf( USkeleton::StaticClass() ) ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimSeq", "A valid anim sequence must be specified.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	FAssetData SkeletalMeshData;

	if ( AssetData.GetClass()->IsChildOf( USkeletalMesh::StaticClass() ) )
	{
		SkeletalMeshData = AssetData;
	}

	if ( !SkeletalMeshData.IsValid() && AssetData.GetClass()->IsChildOf( UAnimBlueprint::StaticClass() ) )
	{
		const FString TargetSkeletonPath = AssetData.GetTagValueRef<FString>( "TargetSkeleton" );
		if ( TargetSkeletonPath.IsEmpty() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimBPTargetSkeleton", "UAnimBlueprints must have a valid Target Skeleton.");
			return false;
		}

		FAssetData TargetSkeleton = AssetRegistry.GetAssetByObjectPath( *TargetSkeletonPath );
		if ( !TargetSkeleton.IsValid() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimBPTargetSkeleton", "UAnimBlueprints must have a valid Target Skeleton.");
			return false;
		}

		// skeleton should be loaded by this time. If not, we have problem
		// so I'm changing this to load directly not using tags and values
		USkeleton* Skeleton = Cast<USkeleton>(TargetSkeleton.GetAsset());
		if(Skeleton)
		{
			USkeletalMesh * PreviewMesh = Skeleton->GetPreviewMesh(true);
			if(PreviewMesh)
			{
				return true;
			}
			else
			{
				OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoPreviewSkeletalMesh", "The Target Skeleton of the UAnimBlueprint must have a valid Preview Skeletal Mesh.");
				return false;
			}
		}
		else
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimBPTargetSkeleton", "UAnimBlueprints must have a valid Target Skeleton.");
		}
	}

	if ( !SkeletalMeshData.IsValid() && AssetData.GetClass()->IsChildOf( USkeleton::StaticClass() ) )
	{
		// so I'm changing this to load directly not using tags and values
		USkeleton* Skeleton = Cast<USkeleton>(AssetData.GetAsset());
		if(Skeleton)
		{
			USkeletalMesh * PreviewMesh = Skeleton->GetPreviewMesh(true);
			if(PreviewMesh)
			{
				return true;
			}
			else
			{
				OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoPreviewSkeletalMesh", "The Target Skeleton of the UAnimBlueprint must have a valid Preview Skeletal Mesh.");
				return false;
			}
		}
		else
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkelMeshTargetSkeleton", "SkeletalMesh must have a valid Target Skeleton.");
		}
	}

	if ( !SkeletalMeshData.IsValid() )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeletalMeshAss", "No valid skeletal mesh was found associated with the animation sequence.");
		return false;
	}

	if(USkeletalMesh* SkeletalMeshCDO = Cast<USkeletalMesh>(AssetData.GetClass()->GetDefaultObject()))
	{
		if(SkeletalMeshCDO->HasCustomActorFactory())
		{
		return false;
	}
	}

	return true;
}

USkeletalMesh* UActorFactorySkeletalMesh::GetSkeletalMeshFromAsset( UObject* Asset ) const
{
	USkeletalMesh*SkeletalMesh = Cast<USkeletalMesh>( Asset );
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>( Asset );
	USkeleton* Skeleton = Cast<USkeleton>( Asset );

	if( SkeletalMesh == NULL && AnimBlueprint != NULL && AnimBlueprint->TargetSkeleton )
	{
		// base it on preview skeletal mesh, just to have something
		SkeletalMesh = AnimBlueprint->TargetSkeleton->GetPreviewMesh(true);
	}

	if( SkeletalMesh == NULL && Skeleton != NULL )
	{
		SkeletalMesh = Skeleton->GetPreviewMesh(true);
	}

	check( SkeletalMesh != NULL );
	return SkeletalMesh;
}

void UActorFactorySkeletalMesh::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMeshFromAsset(Asset);
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>( Asset );
	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);

	Super::PostSpawnActor(SkeletalMesh, NewActor);

	// Term Component
	NewSMActor->GetSkeletalMeshComponent()->UnregisterComponent();

	// Change properties
	NewSMActor->GetSkeletalMeshComponent()->SkeletalMesh = SkeletalMesh;
	if (NewSMActor->GetWorld()->IsGameWorld())
	{
		NewSMActor->ReplicatedMesh = SkeletalMesh;
	}

	// Init Component
	NewSMActor->GetSkeletalMeshComponent()->RegisterComponent();
	if( AnimBlueprint )
	{
		NewSMActor->GetSkeletalMeshComponent()->SetAnimInstanceClass(AnimBlueprint->GeneratedClass);
	}
}

void UActorFactorySkeletalMesh::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		USkeletalMesh* SkeletalMesh = GetSkeletalMeshFromAsset(Asset);
		UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset);

		ASkeletalMeshActor* SkeletalMeshActor = CastChecked<ASkeletalMeshActor>(CDO);
		SkeletalMeshActor->GetSkeletalMeshComponent()->SkeletalMesh = SkeletalMesh;
		SkeletalMeshActor->GetSkeletalMeshComponent()->AnimClass = AnimBlueprint ? Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint->GeneratedClass) : NULL;
	}
}

FQuat UActorFactorySkeletalMesh::AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const
{
	// Meshes align the Z (up) axis with the surface normal
	return FindActorAlignmentRotation(ActorRotation, FVector(0.f, 0.f, 1.f), InSurfaceNormal);
}

/*-----------------------------------------------------------------------------
UActorFactoryCameraActor
-----------------------------------------------------------------------------*/
UActorFactoryCameraActor::UActorFactoryCameraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("CameraDisplayName", "Camera");
	NewActorClass = ACameraActor::StaticClass();
}

static UBillboardComponent* CreateEditorOnlyBillboardComponent(AActor* ActorOwner, USceneComponent* AttachParent)
{
	// Create a new billboard component to serve as a visualization of the actor until there is another primitive component
	UBillboardComponent* BillboardComponent = NewObject<UBillboardComponent>(ActorOwner, NAME_None, RF_Transactional);

	BillboardComponent->Sprite = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/EmptyActor.EmptyActor"));
	BillboardComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
	BillboardComponent->Mobility = EComponentMobility::Movable;
	BillboardComponent->bIsEditorOnly = true;

	BillboardComponent->SetupAttachment(AttachParent);

	return BillboardComponent;
}

/*-----------------------------------------------------------------------------
UActorFactoryEmptyActor
-----------------------------------------------------------------------------*/
UActorFactoryEmptyActor::UActorFactoryEmptyActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ActorFactoryEmptyActorDisplayName", "Empty Actor");
	NewActorClass = AActor::StaticClass();
	bVisualizeActor = true;
}

bool UActorFactoryEmptyActor::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	return AssetData.ObjectPath == FName(*AActor::StaticClass()->GetPathName());
}

AActor* UActorFactoryEmptyActor::SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName Name )
{
	AActor* NewActor = nullptr;
	{
		// Spawn a temporary actor for dragging around
		NewActor = Super::SpawnActor(Asset, InLevel, Transform, InObjectFlags, Name);

		USceneComponent* RootComponent = NewObject<USceneComponent>(NewActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
		RootComponent->Mobility = EComponentMobility::Movable;
		RootComponent->bVisualizeComponent = bVisualizeActor;
		RootComponent->SetWorldTransform(Transform);

		NewActor->SetRootComponent(RootComponent);
		NewActor->AddInstanceComponent(RootComponent);

		RootComponent->RegisterComponent();
	}

	return NewActor;
}



/*-----------------------------------------------------------------------------
UActorFactoryCharacter
-----------------------------------------------------------------------------*/
UActorFactoryCharacter::UActorFactoryCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ActorFactoryCharacterDisplayName", "Empty Character");
	NewActorClass = ACharacter::StaticClass();
}

bool UActorFactoryCharacter::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	return AssetData.ObjectPath == FName(*ACharacter::StaticClass()->GetPathName());
}

/*-----------------------------------------------------------------------------
UActorFactoryPawn
-----------------------------------------------------------------------------*/
UActorFactoryPawn::UActorFactoryPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ActorFactoryPawnDisplayName", "Empty Pawn");
	NewActorClass = APawn::StaticClass();
}

bool UActorFactoryPawn::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	return AssetData.ObjectPath == FName(*APawn::StaticClass()->GetPathName());
}

/*-----------------------------------------------------------------------------
UActorFactoryAmbientSound
-----------------------------------------------------------------------------*/
UActorFactoryAmbientSound::UActorFactoryAmbientSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("AmbientSoundDisplayName", "Ambient Sound");
	NewActorClass = AAmbientSound::StaticClass();
}

bool UActorFactoryAmbientSound::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	//We allow creating AAmbientSounds without an existing sound asset
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( USoundBase::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSoundAsset", "A valid sound asset must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryAmbientSound::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	USoundBase* AmbientSound = Cast<USoundBase>(Asset);

	if ( AmbientSound != NULL )
	{
		AAmbientSound* NewSound = CastChecked<AAmbientSound>( NewActor );
		NewSound->GetAudioComponent()->SetSound(AmbientSound);
	}
}

UObject* UActorFactoryAmbientSound::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AAmbientSound* SoundActor = CastChecked<AAmbientSound>(Instance);

	check(SoundActor->GetAudioComponent());
	return SoundActor->GetAudioComponent()->Sound;
}

void UActorFactoryAmbientSound::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		USoundBase* AmbientSound = Cast<USoundBase>(Asset);

		if (AmbientSound != NULL)
		{
			AAmbientSound* NewSound = CastChecked<AAmbientSound>(CDO);
			NewSound->GetAudioComponent()->SetSound(AmbientSound);
		}
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryClass
-----------------------------------------------------------------------------*/
UActorFactoryClass::UActorFactoryClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ClassDisplayName", "Class");
}

bool UActorFactoryClass::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( AssetData.IsValid() && AssetData.GetClass()->IsChildOf( UClass::StaticClass() ) )
	{
		UClass* ActualClass = Cast<UClass>(AssetData.GetAsset());
		if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
		{
			return true;
		}
	}

	OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoClass", "The specified Blueprint must be Actor based.");
	return false;
}

AActor* UActorFactoryClass::GetDefaultActor( const FAssetData& AssetData )
{
	if ( AssetData.IsValid() && AssetData.GetClass()->IsChildOf( UClass::StaticClass() ) )
	{
		UClass* ActualClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *AssetData.ObjectPath.ToString(), NULL, LOAD_NoWarn, NULL));
			
		//Cast<UClass>(AssetData.GetAsset());
		if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
		{
			return ActualClass->GetDefaultObject<AActor>();
		}
	}

	return NULL;
}

bool UActorFactoryClass::PreSpawnActor( UObject* Asset, FTransform& InOutLocation)
{
	UClass* ActualClass = Cast<UClass>(Asset);

	if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
	{
		return true;
	}

	return false;
}

AActor* UActorFactoryClass::SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName Name )
{
	UClass* ActualClass = Cast<UClass>(Asset);

	if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = InLevel;
		SpawnInfo.ObjectFlags = InObjectFlags;
		SpawnInfo.Name = Name;
		return InLevel->OwningWorld->SpawnActor( ActualClass, &Transform, SpawnInfo );
	}

	return NULL;
}


/*-----------------------------------------------------------------------------
UActorFactoryBlueprint
-----------------------------------------------------------------------------*/
UActorFactoryBlueprint::UActorFactoryBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("BlueprintDisplayName", "Blueprint");
}

bool UActorFactoryBlueprint::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UBlueprint::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoBlueprint", "No Blueprint was specified, or the specified Blueprint needs to be compiled.");
		return false;
	}

	const FString ParentClassPath = AssetData.GetTagValueRef<FString>( "ParentClass" );
	if ( ParentClassPath.IsEmpty() )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoBlueprint", "No Blueprint was specified, or the specified Blueprint needs to be compiled.");
		return false;
	}

	UClass* ParentClass = FindObject<UClass>(NULL, *ParentClassPath);

	bool bIsActorBased = false;
	if ( ParentClass != NULL )
	{
		// The parent class is loaded. Make sure it is derived from AActor
		bIsActorBased = ParentClass->IsChildOf(AActor::StaticClass());
	}
	else
	{
		// The parent class does not exist or is not loaded.
		// Ask the asset registry for the ancestors of this class to see if it is an unloaded blueprint generated class.
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(*ParentClassPath);
		const FName ParentClassPathFName = FName( *FPackageName::ObjectPathToObjectName(ObjectPath) );
		TArray<FName> AncestorClassNames;
		AssetRegistry.GetAncestorClassNames(ParentClassPathFName, AncestorClassNames);

		bIsActorBased = AncestorClassNames.Contains(AActor::StaticClass()->GetFName());
	}

	if ( !bIsActorBased )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NotActor", "The specified Blueprint must be Actor based.");
		return false;
	}

	return true;
}

AActor* UActorFactoryBlueprint::GetDefaultActor( const FAssetData& AssetData )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UBlueprint::StaticClass() ) )
	{
		return NULL;
	}

	const FString GeneratedClassPath = AssetData.GetTagValueRef<FString>("GeneratedClass");
	if ( GeneratedClassPath.IsEmpty() )
	{
		return NULL;
	}

	UClass* GeneratedClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GeneratedClassPath, NULL, LOAD_NoWarn, NULL));

	if ( GeneratedClass == NULL )
	{
		return NULL;
	}

	return GeneratedClass->GetDefaultObject<AActor>();
}

bool UActorFactoryBlueprint::PreSpawnActor( UObject* Asset, FTransform& InOutLocation)
{
	UBlueprint* Blueprint = CastChecked<UBlueprint>(Asset);

	// Invalid if there is no generated class, or this is not actor based
	if (Blueprint == NULL || Blueprint->GeneratedClass == NULL || !FBlueprintEditorUtils::IsActorBased(Blueprint))
	{
		return false;
	}

	return true;
}



/*-----------------------------------------------------------------------------
UActorFactoryMatineeActor
-----------------------------------------------------------------------------*/
UActorFactoryMatineeActor::UActorFactoryMatineeActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("MatineeDisplayName", "Matinee");
	NewActorClass = AMatineeActor::StaticClass();
}

bool UActorFactoryMatineeActor::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	//We allow creating AMatineeActors without an existing asset
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( UInterpData::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoInterpData", "A valid InterpData must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryMatineeActor::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor(Asset, NewActor);

	UInterpData* MatineeData = Cast<UInterpData>( Asset );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( NewActor );

	if( MatineeData )
	{
		MatineeActor->MatineeData = MatineeData;
	}
	else
	{
		// if MatineeData isn't set yet, create default one
		UInterpData* NewMatineeData = NewObject<UInterpData>(NewActor);
		MatineeActor->MatineeData = NewMatineeData;
	}
}

void UActorFactoryMatineeActor::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	if (Asset != NULL && CDO != NULL)
	{
		UInterpData* MatineeData = Cast<UInterpData>(Asset);
		AMatineeActor* MatineeActor = CastChecked<AMatineeActor>(CDO);

		// @todo sequencer: Don't ever need or want InterpData for Sequencer.  We will probably get rid of this after old Matinee goes away.
		// also note the PostSpawnActor() code above creates an UInterpData and puts it in the actor's outermost package.  Definitely do not
		// want that for Sequencer.
		MatineeActor->MatineeData = MatineeData;
	}
}


/*-----------------------------------------------------------------------------
UActorFactoryDirectionalLight
-----------------------------------------------------------------------------*/
UActorFactoryDirectionalLight::UActorFactoryDirectionalLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DirectionalLightDisplayName", "Directional Light");
	NewActorClass = ADirectionalLight::StaticClass();
	SpawnPositionOffset = FVector(50, 0, 0);
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactorySpotLight
-----------------------------------------------------------------------------*/
UActorFactorySpotLight::UActorFactorySpotLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("SpotLightDisplayName", "Spot Light");
	NewActorClass = ASpotLight::StaticClass();
	SpawnPositionOffset = FVector(50, 0, 0);
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactoryPointLight
-----------------------------------------------------------------------------*/
UActorFactoryPointLight::UActorFactoryPointLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("PointLightDisplayName", "Point Light");
	NewActorClass = APointLight::StaticClass();
	SpawnPositionOffset = FVector(50, 0, 0);
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactorySkyLight
-----------------------------------------------------------------------------*/
UActorFactorySkyLight::UActorFactorySkyLight( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
{
	DisplayName = LOCTEXT( "SkyLightDisplayName", "Sky Light" );
	NewActorClass = ASkyLight::StaticClass();
}

// NVCHANGE_BEGIN: Add VXGI
/*-----------------------------------------------------------------------------
UActorFactoryVxgiAnchor
-----------------------------------------------------------------------------*/
UActorFactoryVxgiAnchor::UActorFactoryVxgiAnchor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString("VXGI Anchor");
	NewActorClass = AVxgiAnchor::StaticClass();
}
// NVCHANGE_END: Add VXGI

/*-----------------------------------------------------------------------------
UActorFactorySphereReflectionCapture
-----------------------------------------------------------------------------*/
UActorFactorySphereReflectionCapture::UActorFactorySphereReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ReflectionCaptureSphereDisplayName", "Sphere Reflection Capture");
	NewActorClass = ASphereReflectionCapture::StaticClass();
	SpawnPositionOffset = FVector(50, 0, 0);
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactoryBoxReflectionCapture
-----------------------------------------------------------------------------*/
UActorFactoryBoxReflectionCapture::UActorFactoryBoxReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ReflectionCaptureBoxDisplayName", "Box Reflection Capture");
	NewActorClass = ABoxReflectionCapture::StaticClass();
	SpawnPositionOffset = FVector(50, 0, 0);
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactoryPlanarReflection
-----------------------------------------------------------------------------*/
UActorFactoryPlanarReflection::UActorFactoryPlanarReflection(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("PlanarReflectionDisplayName", "Planar Reflection");
	NewActorClass = APlanarReflection::StaticClass();
	SpawnPositionOffset = FVector(0, 0, 0);
	bUseSurfaceOrientation = false;
}

/*-----------------------------------------------------------------------------
UActorFactoryPlaneReflectionCapture
-----------------------------------------------------------------------------*/
UActorFactoryPlaneReflectionCapture::UActorFactoryPlaneReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ReflectionCapturePlaneDisplayName", "Plane Reflection Capture");
	NewActorClass = APlaneReflectionCapture::StaticClass();
	SpawnPositionOffset = FVector(50, 0, 0);
	bUseSurfaceOrientation = true;
}

/*-----------------------------------------------------------------------------
UActorFactoryAtmosphericFog
-----------------------------------------------------------------------------*/
UActorFactoryAtmosphericFog::UActorFactoryAtmosphericFog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("AtmosphericFogDisplayName", "Atmospheric Fog");
	NewActorClass = AAtmosphericFog::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryExponentialHeightFog
-----------------------------------------------------------------------------*/
UActorFactoryExponentialHeightFog::UActorFactoryExponentialHeightFog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ExponentialHeightFogDisplayName", "Exponential Height Fog");
	NewActorClass = AExponentialHeightFog::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryInteractiveFoliage
-----------------------------------------------------------------------------*/
UActorFactoryInteractiveFoliage::UActorFactoryInteractiveFoliage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("InteractiveFoliageDisplayName", "Interactive Foliage");
	NewActorClass = AInteractiveFoliageActor::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryFlex
-----------------------------------------------------------------------------*/
UActorFactoryFlex::UActorFactoryFlex(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("FlexDisplayName", "Flex Actor");
	NewActorClass = AFlexActor::StaticClass();
}

bool UActorFactoryFlex::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UStaticMesh::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoStaticMesh", "A valid static mesh must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryFlex::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);

	if (StaticMesh)
	{
		UE_LOG(LogActorFactory, Log, TEXT("Actor Factory created %s"), *StaticMesh->GetName());

		// Change properties
		AFlexActor* FlexActor = CastChecked<AFlexActor>(NewActor);
		UStaticMeshComponent* StaticMeshComponent = FlexActor->GetStaticMeshComponent();
		check(StaticMeshComponent);

		StaticMeshComponent->UnregisterComponent();

		StaticMeshComponent->SetStaticMesh(StaticMesh);
		StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;

		// Init Component
		StaticMeshComponent->RegisterComponent();
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryTriggerBox
-----------------------------------------------------------------------------*/
UActorFactoryTriggerBox::UActorFactoryTriggerBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("TriggerBoxDisplayName", "Box Trigger");
	NewActorClass = ATriggerBox::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTriggerCapsule
-----------------------------------------------------------------------------*/
UActorFactoryTriggerCapsule::UActorFactoryTriggerCapsule(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("TriggerCapsuleDisplayName", "Capsule Trigger");
	NewActorClass = ATriggerCapsule::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTriggerSphere
-----------------------------------------------------------------------------*/
UActorFactoryTriggerSphere::UActorFactoryTriggerSphere(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("TriggerSphereDisplayName", "Sphere Trigger");
	NewActorClass = ATriggerSphere::StaticClass();
}


/*-----------------------------------------------------------------------------
UActorFactoryVectorField
-----------------------------------------------------------------------------*/
UActorFactoryVectorFieldVolume::UActorFactoryVectorFieldVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("VectorFieldVolumeDisplayName", "Vector Field Volume");
	NewActorClass = AVectorFieldVolume::StaticClass();
}

bool UActorFactoryVectorFieldVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UVectorField::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoVectorField", "No vector field was specified.");
		return false;
	}

	return true;
}

void UActorFactoryVectorFieldVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor(Asset, NewActor);

	UVectorField* VectorField = CastChecked<UVectorField>(Asset);
	AVectorFieldVolume* VectorFieldVolumeActor = CastChecked<AVectorFieldVolume>(NewActor);

	if ( VectorFieldVolumeActor && VectorFieldVolumeActor->GetVectorFieldComponent() )
	{
		VectorFieldVolumeActor->GetVectorFieldComponent()->VectorField = VectorField;
		VectorFieldVolumeActor->PostEditChange();
	}
}

/*-----------------------------------------------------------------------------
CreateBrushForVolumeActor
-----------------------------------------------------------------------------*/
// Helper function for the volume actor factories, not sure where it should live
void CreateBrushForVolumeActor( AVolume* NewActor, UBrushBuilder* BrushBuilder )
{
	if ( NewActor != NULL )
	{
		// this code builds a brush for the new actor
		NewActor->PreEditChange(NULL);

		NewActor->PolyFlags = 0;
		NewActor->Brush = NewObject<UModel>(NewActor, NAME_None, RF_Transactional);
		NewActor->Brush->Initialize(nullptr, true);
		NewActor->Brush->Polys = NewObject<UPolys>(NewActor->Brush, NAME_None, RF_Transactional);
		NewActor->GetBrushComponent()->Brush = NewActor->Brush;
		if(BrushBuilder != nullptr)
		{
			NewActor->BrushBuilder = DuplicateObject<UBrushBuilder>(BrushBuilder, NewActor);
		}

		BrushBuilder->Build( NewActor->GetWorld(), NewActor );

		FBSPOps::csgPrepMovingBrush( NewActor );

		// Set the texture on all polys to NULL.  This stops invisible textures
		// dependencies from being formed on volumes.
		if ( NewActor->Brush )
		{
			for ( int32 poly = 0 ; poly < NewActor->Brush->Polys->Element.Num() ; ++poly )
			{
				FPoly* Poly = &(NewActor->Brush->Polys->Element[poly]);
				Poly->Material = NULL;
			}
		}

		NewActor->PostEditChange();
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryBoxVolume
-----------------------------------------------------------------------------*/
UActorFactoryBoxVolume::UActorFactoryBoxVolume( const FObjectInitializer& ObjectInitializer )
: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT( "BoxVolumeDisplayName", "Box Volume" );
	NewActorClass = AVolume::StaticClass();
}

bool UActorFactoryBoxVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( AVolume::StaticClass() ) )
	{
		return false;
	}

	return true;
}

void UActorFactoryBoxVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor(Asset, NewActor);

	AVolume* VolumeActor = CastChecked<AVolume>(NewActor);
	if ( VolumeActor != NULL )
	{
		UCubeBuilder* Builder = NewObject<UCubeBuilder>();
		CreateBrushForVolumeActor( VolumeActor, Builder );
	}
}

/*-----------------------------------------------------------------------------
UActorFactorySphereVolume
-----------------------------------------------------------------------------*/
UActorFactorySphereVolume::UActorFactorySphereVolume( const FObjectInitializer& ObjectInitializer )
: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT( "SphereVolumeDisplayName", "Sphere Volume" );
	NewActorClass = AVolume::StaticClass();
}

bool UActorFactorySphereVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( AVolume::StaticClass() ) )
	{
		return false;
	}

	return true;
}

void UActorFactorySphereVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor(Asset, NewActor);

	AVolume* VolumeActor = CastChecked<AVolume>(NewActor);
	if ( VolumeActor != NULL )
	{
		UTetrahedronBuilder* Builder = NewObject<UTetrahedronBuilder>();
		Builder->SphereExtrapolation = 2;
		Builder->Radius = 192.0f;
		CreateBrushForVolumeActor( VolumeActor, Builder );
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryCylinderVolume
-----------------------------------------------------------------------------*/
UActorFactoryCylinderVolume::UActorFactoryCylinderVolume( const FObjectInitializer& ObjectInitializer )
: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT( "CylinderVolumeDisplayName", "Cylinder Volume" );
	NewActorClass = AVolume::StaticClass();
}
bool UActorFactoryCylinderVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( AVolume::StaticClass() ) )
	{
		return false;
	}

	return true;

}
void UActorFactoryCylinderVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor(Asset, NewActor);

	AVolume* VolumeActor = CastChecked<AVolume>(NewActor);
	if ( VolumeActor != NULL )
	{
		UCylinderBuilder* Builder = NewObject<UCylinderBuilder>();
		Builder->OuterRadius = 128.0f;
		CreateBrushForVolumeActor( VolumeActor, Builder );
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryMovieScene
-----------------------------------------------------------------------------*/
UActorFactoryMovieScene::UActorFactoryMovieScene(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("MovieSceneDisplayName", "MovieScene");
	NewActorClass = ALevelSequenceActor::StaticClass();
}

bool UActorFactoryMovieScene::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( ULevelSequence::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoLevelSequenceAsset", "A valid sequencer asset must be specified.");
		return false;
	}

	return true;
}

AActor* UActorFactoryMovieScene::SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName Name )
{
	ALevelSequenceActor* NewActor = Cast<ALevelSequenceActor>(Super::SpawnActor(Asset, InLevel, Transform, InObjectFlags, Name));

	if (NewActor)
	{
		if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Asset))
		{
			NewActor->SetSequence(LevelSequence);
		}
	}

	return NewActor;
}

UObject* UActorFactoryMovieScene::GetAssetFromActorInstance(AActor* Instance)
{
	if (ALevelSequenceActor* LevelSequenceActor = Cast<ALevelSequenceActor>(Instance))
	{
		return LevelSequenceActor->LevelSequence.TryLoad();
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
