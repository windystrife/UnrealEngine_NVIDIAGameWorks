// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "LevelEditorViewport.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Animation/AnimSequenceBase.h"
#include "CanvasItem.h"
#include "Engine/BrushBuilder.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Engine/Brush.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AssetData.h"
#include "Editor/UnrealEdEngine.h"
#include "Animation/AnimBlueprint.h"
#include "Exporters/ExportTextContainer.h"
#include "Factories/MaterialFactoryNew.h"
#include "Editor/GroupActor.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ModelComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Engine/Selection.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "EditorModeRegistry.h"
#include "EditorModes.h"
#include "EditorModeInterpolation.h"
#include "PhysicsManipulationMode.h"
#include "UnrealEdGlobals.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "EditorSupportDelegates.h"
#include "AudioDevice.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "HModel.h"
#include "Layers/ILayers.h"
#include "StaticLightingSystem/StaticLightingPrivate.h"
#include "SEditorViewport.h"
#include "LevelEditor.h"
#include "LevelViewportActions.h"
#include "SLevelViewport.h"
#include "AssetSelection.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistryModule.h"
#include "IPlacementModeModule.h"
#include "Engine/Polys.h"
#include "Editor/GeometryMode/Public/EditorGeometry.h"
#include "ActorEditorUtils.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "SnappingUtils.h"
#include "LevelViewportClickHandlers.h"
#include "DragTool_BoxSelect.h"
#include "DragTool_FrustumSelect.h"
#include "DragTool_Measure.h"
#include "DragTool_ViewportChange.h"
#include "DragAndDrop/BrushBuilderDragDropOp.h"
#include "DynamicMeshBuilder.h"
#include "Editor/ActorPositioning.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Settings/EditorProjectSettings.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentStreaming.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "ActorGroupingUtils.h"

DEFINE_LOG_CATEGORY(LogEditorViewport);

#define LOCTEXT_NAMESPACE "LevelEditorViewportClient"

static const float MIN_ACTOR_BOUNDS_EXTENT	= 1.0f;

TArray< TWeakObjectPtr< AActor > > FLevelEditorViewportClient::DropPreviewActors;
bool FLevelEditorViewportClient::bIsDroppingPreviewActor;

/** Static: List of objects we're hovering over */
TSet<FViewportHoverTarget> FLevelEditorViewportClient::HoveredObjects;

IMPLEMENT_HIT_PROXY( HLevelSocketProxy, HHitProxy );


///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FViewportCursorLocation
//	Contains information about a mouse cursor position within a viewport, transformed into the correct
//	coordinate system for the viewport.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
FViewportCursorLocation::FViewportCursorLocation( const FSceneView* View, FEditorViewportClient* InViewportClient, int32 X, int32 Y )
	:	Origin(ForceInit), Direction(ForceInit), CursorPos(X, Y)
{

	FVector4 ScreenPos = View->PixelToScreen(X, Y, 0);

	const FMatrix InvViewMatrix = View->ViewMatrices.GetInvViewMatrix();
	const FMatrix InvProjMatrix = View->ViewMatrices.GetInvProjectionMatrix();

	const float ScreenX = ScreenPos.X;
	const float ScreenY = ScreenPos.Y;

	ViewportClient = InViewportClient;

	if ( ViewportClient->IsPerspective() )
	{
		Origin = View->ViewMatrices.GetViewOrigin();
		Direction = InvViewMatrix.TransformVector(FVector(InvProjMatrix.TransformFVector4(FVector4(ScreenX * GNearClippingPlane,ScreenY * GNearClippingPlane,0.0f,GNearClippingPlane)))).GetSafeNormal();
	}
	else
	{
		Origin = InvViewMatrix.TransformFVector4(InvProjMatrix.TransformFVector4(FVector4(ScreenX,ScreenY,0.5f,1.0f)));
		Direction = InvViewMatrix.TransformVector(FVector(0,0,1)).GetSafeNormal();
	}
}

FViewportCursorLocation::~FViewportCursorLocation()
{
}

ELevelViewportType FViewportCursorLocation::GetViewportType() const
{
	return ViewportClient->GetViewportType();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FViewportClick::FViewportClick - Calculates useful information about a click for the below ClickXXX functions to use.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
FViewportClick::FViewportClick(const FSceneView* View,FEditorViewportClient* ViewportClient,FKey InKey,EInputEvent InEvent,int32 X,int32 Y)
	:	FViewportCursorLocation(View, ViewportClient, X, Y)
	,	Key(InKey), Event(InEvent)
{
	ControlDown = ViewportClient->IsCtrlPressed();
	ShiftDown = ViewportClient->IsShiftPressed();
	AltDown = ViewportClient->IsAltPressed();
}

FViewportClick::~FViewportClick()
{
}

/** Helper function to compute a new location that is snapped to the origin plane given the users cursor location and camera angle */
static FVector4 AttemptToSnapLocationToOriginPlane( const FViewportCursorLocation& Cursor, FVector4 Location )
{
	ELevelViewportType ViewportType = Cursor.GetViewportType();
	if ( ViewportType == LVT_Perspective )
	{
		FVector CamPos = Cursor.GetViewportClient()->GetViewLocation();

		FVector NewLocFloor( Location.X, Location.Y, 0 );

		bool CamBelowOrigin = CamPos.Z < 0;

		FPlane CamPlane( CamPos, FVector::UpVector );
		// If the camera is looking at the floor place the brush on the floor
		if ( !CamBelowOrigin && CamPlane.PlaneDot( Location ) < 0 )
		{
			Location = NewLocFloor;
		}
		else if ( CamBelowOrigin && CamPlane.PlaneDot( Location ) > 0 )
		{
			Location = NewLocFloor;
		}
	}
	else if ( ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoNegativeXY )
	{
		// In ortho place the brush at the origin of the hidden axis
		Location.Z = 0;
	}
	else if ( ViewportType == LVT_OrthoXZ || ViewportType == LVT_OrthoNegativeXZ )
	{
		// In ortho place the brush at the origin of the hidden axis
		Location.Y = 0;
	}
	else if ( ViewportType == LVT_OrthoYZ || ViewportType == LVT_OrthoNegativeYZ )
	{
		// In ortho place the brush at the origin of the hidden axis
		Location.X = 0;
	}

	return Location;
}

TArray<AActor*> FLevelEditorViewportClient::TryPlacingActorFromObject( ULevel* InLevel, UObject* ObjToUse, bool bSelectActors, EObjectFlags ObjectFlags, UActorFactory* FactoryToUse, const FName Name )
{
	TArray<AActor*> PlacedActors;

	UClass* ObjectClass = Cast<UClass>(ObjToUse);

	if ( ObjectClass == NULL )
	{
		ObjectClass = ObjToUse->GetClass();
		check(ObjectClass);
	}

	AActor* PlacedActor = NULL;
	if ( ObjectClass->IsChildOf( AActor::StaticClass() ) )
	{
		//Attempting to drop a UClass object
		UActorFactory* ActorFactory = FactoryToUse;
		if ( ActorFactory == NULL )
		{
			ActorFactory = GEditor->FindActorFactoryForActorClass( ObjectClass );
		}

		if ( ActorFactory != NULL )
		{
			PlacedActor = FActorFactoryAssetProxy::AddActorFromSelection( ObjectClass, NULL, bSelectActors, ObjectFlags, ActorFactory, Name );
		}

		if ( PlacedActor == NULL && ActorFactory != NULL )
		{
			PlacedActor = FActorFactoryAssetProxy::AddActorForAsset( ObjToUse, bSelectActors, ObjectFlags, ActorFactory, Name );
		}
		
		if ( PlacedActor == NULL && !ObjectClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Abstract) )
		{
			// If no actor factory was found or failed, add the actor directly.
			const FTransform ActorTransform = FActorPositioning::GetCurrentViewportPlacementTransform(*ObjectClass->GetDefaultObject<AActor>());
			PlacedActor = GEditor->AddActor( InLevel, ObjectClass, ActorTransform, /*bSilent=*/false, ObjectFlags );
		}

		if ( PlacedActor != NULL )
		{
			FVector Collision = ObjectClass->GetDefaultObject<AActor>()->GetPlacementExtent();
			PlacedActors.Add(PlacedActor);
		}
	}
	
	if ( (NULL == PlacedActor) && ObjToUse->IsA( UExportTextContainer::StaticClass() ) )
	{
		UExportTextContainer* ExportContainer = CastChecked<UExportTextContainer>(ObjToUse);
		const TArray<AActor*> NewActors = GEditor->AddExportTextActors( ExportContainer->ExportText, /*bSilent*/false, ObjectFlags);
		PlacedActors.Append(NewActors);
	}
	else if ( (NULL == PlacedActor) && ObjToUse->IsA( UBrushBuilder::StaticClass() ) )
	{
		UBrushBuilder* BrushBuilder = CastChecked<UBrushBuilder>(ObjToUse);
		UWorld* World = InLevel->OwningWorld;
		BrushBuilder->Build(World);

		ABrush* DefaultBrush = World->GetDefaultBrush();
		if (DefaultBrush != NULL)
		{
			FVector ActorLoc = GEditor->ClickLocation + GEditor->ClickPlane * (FVector::BoxPushOut(GEditor->ClickPlane, DefaultBrush->GetPlacementExtent()));
			FSnappingUtils::SnapPointToGrid(ActorLoc, FVector::ZeroVector);

			DefaultBrush->SetActorLocation(ActorLoc);
			PlacedActor = DefaultBrush;
			PlacedActors.Add(DefaultBrush);
		}
	}
	else if (NULL == PlacedActor)
	{
		bool bPlace = true;
		if (ObjectClass->IsChildOf(UBlueprint::StaticClass()))
		{
			UBlueprint* BlueprintObj = StaticCast<UBlueprint*>(ObjToUse);
			bPlace = BlueprintObj->GeneratedClass != NULL;
			if(bPlace)
			{
				check(BlueprintObj->ParentClass == BlueprintObj->GeneratedClass->GetSuperClass());
				if (BlueprintObj->GeneratedClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Abstract))
				{
					bPlace = false;
				}
			}
		}

		if (bPlace)
		{
			PlacedActor = FActorFactoryAssetProxy::AddActorForAsset( ObjToUse, bSelectActors, ObjectFlags, FactoryToUse, Name );
			if ( PlacedActor != NULL )
			{
				PlacedActors.Add(PlacedActor);
				PlacedActor->PostEditMove(true);
			}
		}
	}

	return PlacedActors;
}

namespace EMaterialKind
{
	enum Type
	{
		Unknown = 0,
		Base,
		Normal,
		Specular,
		Emissive,
	};
}

static FString GetSharedTextureNameAndKind( FString TextureName, EMaterialKind::Type& Kind)
{
	// Try and strip the suffix from the texture name, if we're successful it must be of that type.
	bool hasBaseSuffix = TextureName.RemoveFromEnd( "_D" ) || TextureName.RemoveFromEnd( "_Diff" ) || TextureName.RemoveFromEnd( "_Diffuse" ) || TextureName.RemoveFromEnd( "_Detail" ) || TextureName.RemoveFromEnd( "_Base" );
	if ( hasBaseSuffix )
	{
		Kind = EMaterialKind::Base;
		return TextureName;
	}

	bool hasNormalSuffix = TextureName.RemoveFromEnd( "_N" ) || TextureName.RemoveFromEnd( "_Norm" ) || TextureName.RemoveFromEnd( "_Normal" );
	if ( hasNormalSuffix )
	{
		Kind = EMaterialKind::Normal;
		return TextureName;
	}
	
	bool hasSpecularSuffix = TextureName.RemoveFromEnd( "_S" ) || TextureName.RemoveFromEnd( "_Spec" ) || TextureName.RemoveFromEnd( "_Specular" );
	if ( hasSpecularSuffix )
	{
		Kind = EMaterialKind::Specular;
		return TextureName;
	}

	bool hasEmissiveSuffix = TextureName.RemoveFromEnd( "_E" ) || TextureName.RemoveFromEnd( "_Emissive" );
	if ( hasEmissiveSuffix )
	{
		Kind = EMaterialKind::Emissive;
		return TextureName;
	}

	Kind = EMaterialKind::Unknown;
	return TextureName;
}

static UTexture* GetTextureWithNameVariations( const FString& BasePackageName, const TArray<FString>& Suffixes )
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( TEXT( "AssetRegistry" ) );

	// Try all the variations of suffixes, if we find a package matching the suffix, return it.
	for ( int i = 0; i < Suffixes.Num(); i++ )
	{
		TArray<FAssetData> OutAssetData;
		if ( AssetRegistryModule.Get().GetAssetsByPackageName( *( BasePackageName + Suffixes[i] ), OutAssetData ) && OutAssetData.Num() > 0 )
		{
			if ( OutAssetData[0].AssetClass == "Texture2D" )
			{
				return Cast<UTexture>(OutAssetData[0].GetAsset());
			}
		}
	}

	return nullptr;
}

static bool TryAndCreateMaterialInput( UMaterial* UnrealMaterial, EMaterialKind::Type TextureKind, UTexture* UnrealTexture, FExpressionInput& MaterialInput, int X, int Y )
{
	// Ignore null textures.
	if ( UnrealTexture == nullptr )
	{
		return false;
	}

	bool bSetupAsNormalMap = UnrealTexture->IsNormalMap();

	// Create a new texture sample expression, this is our texture input node into the material output.
	UMaterialExpressionTextureSample* UnrealTextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
	UnrealMaterial->Expressions.Add( UnrealTextureExpression );
	MaterialInput.Expression = UnrealTextureExpression;
	UnrealTextureExpression->Texture = UnrealTexture;
	UnrealTextureExpression->AutoSetSampleType();
	UnrealTextureExpression->MaterialExpressionEditorX += X;
	UnrealTextureExpression->MaterialExpressionEditorY += Y;

	// If we know for a fact this is a normal map, it can only legally be placed in the normal map slot.
	// Ignore the Material kind for, but for everything else try and match it to the right slot, fallback
	// to the BaseColor if we don't know.
	if ( !bSetupAsNormalMap )
	{
		if ( TextureKind == EMaterialKind::Base )
		{
			UnrealMaterial->BaseColor.Expression = UnrealTextureExpression;
		}
		else if ( TextureKind == EMaterialKind::Specular )
		{
			UnrealMaterial->Specular.Expression = UnrealTextureExpression;
		}
		else if ( TextureKind == EMaterialKind::Emissive )
		{
			UnrealMaterial->EmissiveColor.Expression = UnrealTextureExpression;
		}
		else
		{
			UnrealMaterial->BaseColor.Expression = UnrealTextureExpression;
		}
	}
	else
	{
		UnrealMaterial->Normal.Expression = UnrealTextureExpression;
	}

	return true;
}

UObject* FLevelEditorViewportClient::GetOrCreateMaterialFromTexture( UTexture* UnrealTexture )
{
	FString TextureShortName = FPackageName::GetShortName( UnrealTexture->GetOutermost()->GetName() );

	// See if we can figure out what kind of material it is, based on a suffix, like _S for Specular, _D for Base/Detail/Diffuse.
	// if it can determine which type of texture it was, it will return the base name of the texture minus the suffix.
	EMaterialKind::Type MaterialKind;
	TextureShortName = GetSharedTextureNameAndKind( TextureShortName, MaterialKind );
	
	FString MaterialFullName = TextureShortName + "_Mat";
	FString NewPackageName = FPackageName::GetLongPackagePath( UnrealTexture->GetOutermost()->GetName() ) + TEXT( "/" ) + MaterialFullName;
	NewPackageName = PackageTools::SanitizePackageName( NewPackageName );
	UPackage* Package = CreatePackage( NULL, *NewPackageName );

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( TEXT( "AssetRegistry" ) );

	// See if the material asset already exists with the expected name, if it does, just return
	// an instance of it.
	TArray<FAssetData> OutAssetData;
	if ( AssetRegistryModule.Get().GetAssetsByPackageName( *NewPackageName, OutAssetData ) && OutAssetData.Num() > 0 )
	{
		// TODO Check if is material?
		return OutAssetData[0].GetAsset();
	}

	// create an unreal material asset
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();

	UMaterial* UnrealMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, *MaterialFullName, RF_Standalone | RF_Public, NULL, GWarn );

	if (UnrealMaterial == nullptr)
	{
		return nullptr;
	}

	const int HSpace = -300;

	// If we were able to figure out the material kind, we need to try and build a complex material
	// involving multiple textures.  If not, just try and connect what we found to the base map.
	if ( MaterialKind == EMaterialKind::Unknown )
	{
		TryAndCreateMaterialInput( UnrealMaterial, EMaterialKind::Base, UnrealTexture, UnrealMaterial->BaseColor, HSpace, 0 );
	}
	else
	{
		// Variations for Base Maps.
		TArray<FString> BaseSuffixes;
		BaseSuffixes.Add( "_D" );
		BaseSuffixes.Add( "_Diff" );
		BaseSuffixes.Add( "_Diffuse" );
		BaseSuffixes.Add( "_Detail" );
		BaseSuffixes.Add( "_Base" );

		// Variations for Normal Maps.
		TArray<FString> NormalSuffixes;
		NormalSuffixes.Add( "_N" );
		NormalSuffixes.Add( "_Norm" );
		NormalSuffixes.Add( "_Normal" );

		// Variations for Specular Maps.
		TArray<FString> SpecularSuffixes;
		SpecularSuffixes.Add( "_S" );
		SpecularSuffixes.Add( "_Spec" );
		SpecularSuffixes.Add( "_Specular" );

		// Variations for Emissive Maps.
		TArray<FString> EmissiveSuffixes;
		EmissiveSuffixes.Add( "_E" );
		EmissiveSuffixes.Add( "_Emissive" );

		// The asset path for the base texture, we need this to try and append different suffixes to to find
		// other textures we can use.
		FString BaseTexturePackage = FPackageName::GetLongPackagePath( UnrealTexture->GetOutermost()->GetName() ) + TEXT( "/" ) + TextureShortName;

		// Try and find different variations
		UTexture* BaseTexture = GetTextureWithNameVariations( BaseTexturePackage, BaseSuffixes );
		UTexture* NormalTexture = GetTextureWithNameVariations( BaseTexturePackage, NormalSuffixes );
		UTexture* SpecularTexture = GetTextureWithNameVariations( BaseTexturePackage, SpecularSuffixes );
		UTexture* EmissiveTexture = GetTextureWithNameVariations( BaseTexturePackage, EmissiveSuffixes );

		// Connect and layout any textures we find into their respective inputs in the material.
		const int VSpace = 170;
		TryAndCreateMaterialInput( UnrealMaterial, EMaterialKind::Base, BaseTexture, UnrealMaterial->BaseColor, HSpace, VSpace * -1 );
		TryAndCreateMaterialInput( UnrealMaterial, EMaterialKind::Specular, SpecularTexture, UnrealMaterial->Specular, HSpace, VSpace * 0 );
		TryAndCreateMaterialInput( UnrealMaterial, EMaterialKind::Emissive, EmissiveTexture, UnrealMaterial->EmissiveColor, HSpace, VSpace * 1 );
		TryAndCreateMaterialInput( UnrealMaterial, EMaterialKind::Normal, NormalTexture, UnrealMaterial->Normal, HSpace, VSpace * 2 );
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated( UnrealMaterial );

	// Set the dirty flag so this package will get saved later
	Package->SetDirtyFlag( true );

	UnrealMaterial->ForceRecompileForRendering();

	// Warn users that a new material has been created
	FNotificationInfo Info( FText::Format( LOCTEXT( "DropTextureMaterialCreated", "Material '{0}' Created" ), FText::FromString(MaterialFullName)) );
	Info.ExpireDuration = 4.0f;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = false;
	Info.Image = FEditorStyle::GetBrush( "ClassThumbnail.Material" );
	FSlateNotificationManager::Get().AddNotification( Info );

	return UnrealMaterial;
}

/**
* Helper function that attempts to apply the supplied object to the supplied actor.
*
* @param	ObjToUse				Object to attempt to apply as specific asset
* @param	ComponentToApplyTo		Component to whom the asset should be applied
* @param	TargetMaterialSlot      When dealing with submeshes this will represent the target section/slot to apply materials to.
* @param	bTest					Whether to test if the object would be successfully applied without actually doing it.
*
* @return	true if the provided object was successfully applied to the provided actor
*/
static bool AttemptApplyObjToComponent(UObject* ObjToUse, USceneComponent* ComponentToApplyTo, int32 TargetMaterialSlot = -1, bool bTest = false)
{
	bool bResult = false;

	if (ComponentToApplyTo && !ComponentToApplyTo->IsCreatedByConstructionScript())
	{
		// MESH/DECAL
		UMeshComponent* MeshComponent = Cast<UMeshComponent>(ComponentToApplyTo);
		UDecalComponent* DecalComponent = Cast<UDecalComponent>(ComponentToApplyTo);
		if (MeshComponent || DecalComponent)
		{
			// Dropping a texture?
			UTexture* DroppedObjAsTexture = Cast<UTexture>(ObjToUse);
			if (DroppedObjAsTexture != NULL)
			{
				if (bTest)
				{
					bResult = false;
				}
				else
				{
					// Turn dropped textures into materials
					ObjToUse = FLevelEditorViewportClient::GetOrCreateMaterialFromTexture(DroppedObjAsTexture);
				}
			}

			// Dropping a material?
			UMaterialInterface* DroppedObjAsMaterial = Cast<UMaterialInterface>(ObjToUse);
			if (DroppedObjAsMaterial)
			{
				if (bTest)
				{
					bResult = false;
				}
				else
				{
					bResult = FComponentEditorUtils::AttemptApplyMaterialToComponent(ComponentToApplyTo, DroppedObjAsMaterial, TargetMaterialSlot);
				}
			}
		}

		// SKELETAL MESH COMPONENT
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ComponentToApplyTo);
		if (SkeletalMeshComponent)
		{
			// Dropping an Anim Blueprint?
			UAnimBlueprint* DroppedObjAsAnimBlueprint = Cast<UAnimBlueprint>(ObjToUse);
			if (DroppedObjAsAnimBlueprint)
			{
				USkeleton* AnimBPSkeleton = DroppedObjAsAnimBlueprint->TargetSkeleton;
				if (AnimBPSkeleton)
				{
					if (bTest)
					{
						bResult = true;
					}
					else
					{
						const FScopedTransaction Transaction(LOCTEXT("DropAnimBlueprintOnObject", "Drop Anim Blueprint On Object"));
						SkeletalMeshComponent->Modify();

						// If the component doesn't have a mesh or the anim blueprint's skeleton isn't compatible with the existing mesh's skeleton, the mesh should change
						const bool bShouldChangeMesh = !SkeletalMeshComponent->SkeletalMesh || !AnimBPSkeleton->IsCompatible(SkeletalMeshComponent->SkeletalMesh->Skeleton);

						if (bShouldChangeMesh)
						{
							SkeletalMeshComponent->SetSkeletalMesh(AnimBPSkeleton->GetPreviewMesh(true));
						}

						// Verify that the skeletons are compatible before changing the anim BP
						if (SkeletalMeshComponent->SkeletalMesh && AnimBPSkeleton->IsCompatible(SkeletalMeshComponent->SkeletalMesh->Skeleton))
						{
							SkeletalMeshComponent->SetAnimInstanceClass(DroppedObjAsAnimBlueprint->GeneratedClass);
							bResult = true;
						}
					}
				}
			}

			// Dropping an Anim Sequence or Vertex Animation?
			UAnimSequenceBase* DroppedObjAsAnimSequence = Cast<UAnimSequenceBase>(ObjToUse);
			if (DroppedObjAsAnimSequence)
			{
				USkeleton* AnimSkeleton = DroppedObjAsAnimSequence->GetSkeleton();

				if (AnimSkeleton)
				{
					if (bTest)
					{
						bResult = true;
					}
					else
					{
						const FScopedTransaction Transaction(LOCTEXT("DropAnimationOnObject", "Drop Animation On Object"));
						SkeletalMeshComponent->Modify();

						// If the component doesn't have a mesh or the anim blueprint's skeleton isn't compatible with the existing mesh's skeleton, the mesh should change
						const bool bShouldChangeMesh = !SkeletalMeshComponent->SkeletalMesh || !AnimSkeleton->IsCompatible(SkeletalMeshComponent->SkeletalMesh->Skeleton);

						if (bShouldChangeMesh)
						{
							SkeletalMeshComponent->SetSkeletalMesh(AnimSkeleton->GetAssetPreviewMesh(DroppedObjAsAnimSequence));
						}

						SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
						SkeletalMeshComponent->AnimationData.AnimToPlay = DroppedObjAsAnimSequence;

						// set runtime data
						SkeletalMeshComponent->SetAnimation(DroppedObjAsAnimSequence);

						if (SkeletalMeshComponent->SkeletalMesh)
						{
							bResult = true;
							SkeletalMeshComponent->InitAnim(true);
						}
					}
				}
			}
		}
	}

	return bResult;
}

/**
 * Helper function that attempts to apply the supplied object to the supplied actor.
 *
 * @param	ObjToUse				Object to attempt to apply as specific asset
 * @param	ActorToApplyTo			Actor to whom the asset should be applied
 * @param   TargetMaterialSlot      When dealing with submeshes this will represent the target section/slot to apply materials to.
 * @param	bTest					Whether to test if the object would be successfully applied without actually doing it.
 *
 * @return	true if the provided object was successfully applied to the provided actor
 */
static bool AttemptApplyObjToActor( UObject* ObjToUse, AActor* ActorToApplyTo, int32 TargetMaterialSlot = -1, bool bTest = false )
{
	bool bResult = false;

	if ( ActorToApplyTo )
	{
		bResult = false;

		TInlineComponentArray<USceneComponent*> SceneComponents;
		ActorToApplyTo->GetComponents(SceneComponents);
		for (USceneComponent* SceneComp : SceneComponents)
		{
			bResult |= AttemptApplyObjToComponent(ObjToUse, SceneComp, TargetMaterialSlot, bTest);
		}

		// Notification hook for dropping asset onto actor
		if(!bTest)
		{
			FEditorDelegates::OnApplyObjectToActor.Broadcast(ObjToUse, ActorToApplyTo);
		}
	}

	return bResult;
}

/**
 * Helper function that attempts to apply the supplied object as a material to the BSP surface specified by the
 * provided model and index.
 *
 * @param	ObjToUse				Object to attempt to apply as a material
 * @param	ModelHitProxy			Hit proxy containing the relevant model
 * @param	SurfaceIndex			The index in the model's surface array of the relevant
 *
 * @return	true if the supplied object was successfully applied to the specified BSP surface
 */
bool FLevelEditorViewportClient::AttemptApplyObjAsMaterialToSurface( UObject* ObjToUse, HModel* ModelHitProxy, FViewportCursorLocation& Cursor )
{
	bool bResult = false;

	UTexture* DroppedObjAsTexture = Cast<UTexture>( ObjToUse );
	if ( DroppedObjAsTexture != NULL )
	{
		ObjToUse = GetOrCreateMaterialFromTexture( DroppedObjAsTexture );
	}

	// Ensure the dropped object is a material
	UMaterialInterface* DroppedObjAsMaterial = Cast<UMaterialInterface>( ObjToUse );

	if ( DroppedObjAsMaterial && ModelHitProxy )
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport, 
			GetScene(),
			EngineShowFlags)
			.SetRealtimeUpdate( IsRealtime() ));
		FSceneView* View = CalcSceneView( &ViewFamily );


		UModel* Model = ModelHitProxy->GetModel();
	
		TArray<uint32> SelectedSurfaces;

		bool bDropedOntoSelectedSurface = false;
		const int32 DropX = Cursor.GetCursorPos().X;
		const int32 DropY = Cursor.GetCursorPos().Y;

		{
		uint32 SurfaceIndex;
		ModelHitProxy->ResolveSurface(View, DropX, DropY, SurfaceIndex);
		if (SurfaceIndex != INDEX_NONE)
		{
			if ((Model->Surfs[SurfaceIndex].PolyFlags & PF_Selected) == 0)
			{
				// Surface was not selected so only apply to this surface
				SelectedSurfaces.Add(SurfaceIndex);
			}
			else
			{
				bDropedOntoSelectedSurface = true;
			}
		}
		}

		if( bDropedOntoSelectedSurface )
		{
			for (int32 SurfaceIndex = 0; SurfaceIndex < Model->Surfs.Num(); ++SurfaceIndex)
			{
				FBspSurf& Surf = Model->Surfs[SurfaceIndex];

				if (Surf.PolyFlags & PF_Selected)
				{
					SelectedSurfaces.Add(SurfaceIndex);
				}
			}
		}

		if( SelectedSurfaces.Num() )
		{
			
			// Apply the material to the specified surface
			FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DragDrop_Transaction_ApplyMaterialToSurface", "Apply Material to Surface"));

			// Modify the component so that PostEditUndo can reregister the model after undo
			ModelHitProxy->GetModelComponent()->Modify();

			for( int32 SurfListIndex = 0; SurfListIndex < SelectedSurfaces.Num(); ++SurfListIndex )
			{
				uint32 SelectedSurfIndex = SelectedSurfaces[SurfListIndex];

				check(Model->Surfs.IsValidIndex(SelectedSurfIndex));

				Model->ModifySurf(SelectedSurfIndex, true);
				Model->Surfs[SelectedSurfIndex].Material = DroppedObjAsMaterial;
				const bool bUpdateTexCoords = false;
				const bool bOnlyRefreshSurfaceMaterials = true;
				GEditor->polyUpdateMaster(Model, SelectedSurfIndex, bUpdateTexCoords, bOnlyRefreshSurfaceMaterials);
			}

			bResult = true;
		}
	}

	return bResult;
}


static bool AreAllDroppedObjectsBrushBuilders(const TArray<UObject*>& DroppedObjects)
{
	for (UObject* DroppedObject : DroppedObjects)
	{
		if (!DroppedObject->IsA(UBrushBuilder::StaticClass()))
		{
			return false;
		}
	}

	return true;
}

bool FLevelEditorViewportClient::DropObjectsOnBackground(FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview, bool bSelectActors, UActorFactory* FactoryToUse)
{
	if (DroppedObjects.Num() == 0)
	{
		return false;
	}

	bool bSuccess = false;

	const bool bTransacted = !bCreateDropPreview && !AreAllDroppedObjectsBrushBuilders(DroppedObjects);

	// Create a transaction if not a preview drop
	if (bTransacted)
	{
		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "CreateActors", "Create Actors"));
	}

	for ( int32 DroppedObjectsIdx = 0; DroppedObjectsIdx < DroppedObjects.Num(); ++DroppedObjectsIdx )
	{
		UObject* AssetObj = DroppedObjects[DroppedObjectsIdx];
		ensure( AssetObj );

		// Attempt to create actors from the dropped object
		TArray<AActor*> NewActors = TryPlacingActorFromObject(GetWorld()->GetCurrentLevel(), AssetObj, bSelectActors, ObjectFlags, FactoryToUse);

		if ( NewActors.Num() > 0 )
		{
			OutNewActors.Append(NewActors);
			bSuccess = true;
		}
	}

	if (bTransacted)
	{
		GEditor->EndTransaction();
	}

	return bSuccess;
}

bool FLevelEditorViewportClient::DropObjectsOnActor(FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, AActor* DroppedUponActor, int32 DroppedUponSlot, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview, bool bSelectActors, UActorFactory* FactoryToUse)
{
	if ( !DroppedUponActor || DroppedObjects.Num() == 0 )
	{
		return false;
	}

	bool bSuccess = false;

	const bool bTransacted = !bCreateDropPreview && !AreAllDroppedObjectsBrushBuilders(DroppedObjects);

	// Create a transaction if not a preview drop
	if (bTransacted)
	{
		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "CreateActors", "Create Actors"));
	}

	for ( UObject* DroppedObject : DroppedObjects )
	{
		const bool bIsTestApplication = bCreateDropPreview;
		const bool bAppliedToActor = ( FactoryToUse == nullptr ) ? AttemptApplyObjToActor( DroppedObject, DroppedUponActor, DroppedUponSlot, bIsTestApplication ) : false;

		if (!bAppliedToActor)
		{
			// Attempt to create actors from the dropped object
			TArray<AActor*> NewActors = TryPlacingActorFromObject(GetWorld()->GetCurrentLevel(), DroppedObject, bSelectActors, ObjectFlags, FactoryToUse);

			if ( NewActors.Num() > 0 )
			{
				OutNewActors.Append( NewActors );
				bSuccess = true;
			}
		}
		else
		{
			bSuccess = true;
		}
	}

	if (bTransacted)
	{
		GEditor->EndTransaction();
	}

	return bSuccess;
}

bool FLevelEditorViewportClient::DropObjectsOnBSPSurface(FSceneView* View, FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, HModel* TargetProxy, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview, bool bSelectActors, UActorFactory* FactoryToUse)
{
	if (DroppedObjects.Num() == 0)
	{
		return false;
	}

	bool bSuccess = false;

	const bool bTransacted = !bCreateDropPreview && !AreAllDroppedObjectsBrushBuilders(DroppedObjects);

	// Create a transaction if not a preview drop
	if (bTransacted)
	{
		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "CreateActors", "Create Actors"));
	}

	for (UObject* DroppedObject : DroppedObjects)
	{
		const bool bAppliedToActor = (!bCreateDropPreview && FactoryToUse == NULL) ? AttemptApplyObjAsMaterialToSurface(DroppedObject, TargetProxy, Cursor) : false;

		if (!bAppliedToActor)
		{
			// Attempt to create actors from the dropped object
			TArray<AActor*> NewActors = TryPlacingActorFromObject(GetWorld()->GetCurrentLevel(), DroppedObject, bSelectActors, ObjectFlags, FactoryToUse);

			if (NewActors.Num() > 0)
			{
				OutNewActors.Append(NewActors);
				bSuccess = true;
			}
		}
		else
		{
			bSuccess = true;
		}
	}

	if (bTransacted)
	{
		GEditor->EndTransaction();
	}

	return bSuccess;
}

/**
 * Called when an asset is dropped upon a manipulation widget.
 *
 * @param	View				The SceneView for the dropped-in viewport
 * @param	Cursor				Mouse cursor location
 * @param	DroppedObjects		Array of objects dropped into the viewport
 *
 * @return	true if the drop operation was successfully handled; false otherwise
 */
bool FLevelEditorViewportClient::DropObjectsOnWidget(FSceneView* View, FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, bool bCreateDropPreview)
{
	bool bResult = false;

	// Axis translation/rotation/scale widget - find out what's underneath the axis widget

	// Modify the ShowFlags for the scene so we can re-render the hit proxies without any axis widgets. 
	// Store original ShowFlags and assign them back when we're done
	const bool bOldModeWidgets1 = EngineShowFlags.ModeWidgets;
	const bool bOldModeWidgets2 = View->Family->EngineShowFlags.ModeWidgets;

	EngineShowFlags.SetModeWidgets(false);
	FSceneViewFamily* SceneViewFamily = const_cast< FSceneViewFamily* >( View->Family );
	SceneViewFamily->EngineShowFlags.SetModeWidgets(false);

	// Invalidate the hit proxy map so it will be rendered out again when GetHitProxy is called
	Viewport->InvalidateHitProxy();

	// This will actually re-render the viewport's hit proxies!
	FIntPoint DropPos = Cursor.GetCursorPos();

	HHitProxy* HitProxy = Viewport->GetHitProxy(DropPos.X, DropPos.Y);

	// We should never encounter a widget axis.  If we do, then something's wrong
	// with our ShowFlags (or the widget drawing code)
	check( !HitProxy || ( HitProxy && !HitProxy->IsA( HWidgetAxis::StaticGetType() ) ) );

	// Try this again, but without the widgets this time!
	TArray< AActor* > TemporaryActors;
	const FIntPoint& CursorPos = Cursor.GetCursorPos();
	const bool bOnlyDropOnTarget = false;
	bResult = DropObjectsAtCoordinates(CursorPos.X, CursorPos.Y, DroppedObjects, TemporaryActors, bOnlyDropOnTarget, bCreateDropPreview);

	// Restore the original flags
	EngineShowFlags.SetModeWidgets(bOldModeWidgets1);
	SceneViewFamily->EngineShowFlags.SetModeWidgets(bOldModeWidgets2);

	return bResult;
}

bool FLevelEditorViewportClient::HasDropPreviewActors() const
{
	return DropPreviewActors.Num() > 0;
}

/* Helper functions to find a dropped position on a 2D layer */

static bool IsDroppingOn2DLayer()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	return ViewportSettings->bEnableLayerSnap && Settings2D->SnapLayers.IsValidIndex(ViewportSettings->ActiveSnapLayerIndex);
}

static FActorPositionTraceResult TraceForPositionOn2DLayer(const FViewportCursorLocation& Cursor)
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const ULevelEditor2DSettings* Settings2D = GetDefault<ULevelEditor2DSettings>();
	check(Settings2D->SnapLayers.IsValidIndex(ViewportSettings->ActiveSnapLayerIndex));

	const float Offset = Settings2D->SnapLayers[ViewportSettings->ActiveSnapLayerIndex].Depth;
	FVector PlaneCenter(0, 0, 0);
	FVector PlaneNormal(0, 0, 0);

	switch (Settings2D->SnapAxis)
	{
	case ELevelEditor2DAxis::X: PlaneCenter.X = Offset; PlaneNormal.X = -1; break;
	case ELevelEditor2DAxis::Y: PlaneCenter.Y = Offset; PlaneNormal.Y = -1; break;
	case ELevelEditor2DAxis::Z: PlaneCenter.Z = Offset; PlaneNormal.Z = -1; break;
	}

	FActorPositionTraceResult Result;
	const float Numerator = FVector::DotProduct(PlaneCenter - Cursor.GetOrigin(), PlaneNormal);
	const float Denominator = FVector::DotProduct(PlaneNormal, Cursor.GetDirection());
	if (FMath::Abs(Denominator) < SMALL_NUMBER)
	{
		Result.State = FActorPositionTraceResult::Failed;
	}
	else
	{
		Result.State = FActorPositionTraceResult::HitSuccess;
		Result.SurfaceNormal = PlaneNormal;
		float D = Numerator / Denominator;
		Result.Location = Cursor.GetOrigin() + D * Cursor.GetDirection();
	}

	return Result;
}

bool FLevelEditorViewportClient::UpdateDropPreviewActors(int32 MouseX, int32 MouseY, const TArray<UObject*>& DroppedObjects, bool& out_bDroppedObjectsVisible, UActorFactory* FactoryToUse)
{
	out_bDroppedObjectsVisible = false;
	if( !HasDropPreviewActors() )
	{
		return false;
	}

	// While dragging actors, allow viewport updates
	bNeedsRedraw = true;

	// If the mouse did not move, there is no need to update anything
	if ( MouseX == DropPreviewMouseX && MouseY == DropPreviewMouseY )
	{
		return false;
	}

	// Update the cached mouse position
	DropPreviewMouseX = MouseX;
	DropPreviewMouseY = MouseY;

	// Get the center point between all the drop preview actors for use in calculations below
	// Also, build a list of valid AActor* pointers
	FVector ActorOrigin = FVector::ZeroVector;
	TArray<AActor*> DraggingActors;
	TArray<AActor*> IgnoreActors;
	for ( auto ActorIt = DropPreviewActors.CreateConstIterator(); ActorIt; ++ActorIt )
	{
		AActor* DraggingActor = (*ActorIt).Get();

		if ( DraggingActor )
		{
			DraggingActors.Add(DraggingActor);
			IgnoreActors.Add(DraggingActor);
			DraggingActor->GetAllChildActors(IgnoreActors);
			ActorOrigin += DraggingActor->GetActorLocation();
		}
	}

	// If there were not valid actors after all, there is nothing to update
	if ( DraggingActors.Num() == 0 )
	{
		return false;
	}

	// Finish the calculation of the actors origin now that we know we are not dividing by zero
	ActorOrigin /= DraggingActors.Num();

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Viewport, 
		GetScene(),
		EngineShowFlags)
		.SetRealtimeUpdate( IsRealtime() ));
	FSceneView* View = CalcSceneView( &ViewFamily );
	FViewportCursorLocation Cursor(View, this, MouseX, MouseY);

	const FActorPositionTraceResult TraceResult = IsDroppingOn2DLayer() ? TraceForPositionOn2DLayer(Cursor) : FActorPositioning::TraceWorldForPositionWithDefault(Cursor, *View, &IgnoreActors);

	GEditor->UnsnappedClickLocation = TraceResult.Location;
	GEditor->ClickLocation = TraceResult.Location;
	GEditor->ClickPlane = FPlane(TraceResult.Location, TraceResult.SurfaceNormal);

	// Snap the new location if snapping is enabled
	FSnappingUtils::SnapPointToGrid(GEditor->ClickLocation, FVector::ZeroVector);

	AActor* DroppedOnActor = TraceResult.HitActor.Get();
	
	if (DroppedOnActor)
	{
		// We indicate that the dropped objects are visible if *any* of them are not applicable to other actors
		out_bDroppedObjectsVisible = DroppedObjects.ContainsByPredicate([&](UObject* AssetObj){
			return !AttemptApplyObjToActor(AssetObj, DroppedOnActor, -1, true);
		});
	}
	else
	{
		// All dropped objects are visible if we're not dropping on an actor
		out_bDroppedObjectsVisible = true;
	}

	for (AActor* Actor : DraggingActors)
	{
		const UActorFactory* ActorFactory = FactoryToUse ? FactoryToUse : GEditor->FindActorFactoryForActorClass(Actor->GetClass());

		const FSnappedPositioningData PositioningData = FSnappedPositioningData(this, TraceResult.Location, TraceResult.SurfaceNormal)
			.DrawSnapHelpers(true)
			.UseFactory(ActorFactory)
			.UseStartTransform(PreDragActorTransforms.FindRef(Actor))
			.UsePlacementExtent(Actor->GetPlacementExtent());

		FTransform ActorTransform = FActorPositioning::GetSnappedSurfaceAlignedTransform(PositioningData);
		ActorTransform.SetScale3D(Actor->GetActorScale3D());		// preserve scaling

		Actor->SetActorTransform(ActorTransform);
		Actor->SetIsTemporarilyHiddenInEditor(false);
		Actor->PostEditMove(false);
	}

	return true;
}

void FLevelEditorViewportClient::DestroyDropPreviewActors()
{
	if ( HasDropPreviewActors() )
	{
		for ( auto ActorIt = DropPreviewActors.CreateConstIterator(); ActorIt; ++ActorIt )
		{
			AActor* PreviewActor = (*ActorIt).Get();
			if (PreviewActor && PreviewActor != GetWorld()->GetDefaultBrush())
			{
				GetWorld()->DestroyActor(PreviewActor);
			}
		}
		DropPreviewActors.Empty();
	}
}


/**
* Checks the viewport to see if the given object can be dropped using the given mouse coordinates local to this viewport
*
* @param MouseX			The position of the mouse's X coordinate
* @param MouseY			The position of the mouse's Y coordinate
* @param AssetData			Asset in question to be dropped
*/
FDropQuery FLevelEditorViewportClient::CanDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, const FAssetData& AssetData)
{
	FDropQuery Result;

	if ( !ObjectTools::IsAssetValidForPlacing( GetWorld(), AssetData.ObjectPath.ToString() ) )
	{
		return Result;
	}

	UObject* AssetObj = AssetData.GetAsset();
	UClass* ClassObj = Cast<UClass>( AssetObj );

	if ( ClassObj )
	{
		if ( !ObjectTools::IsClassValidForPlacing(ClassObj) )
		{
			Result.bCanDrop = false;
			Result.HintText = FText::Format(LOCTEXT("DragAndDrop_CannotDropAssetClassFmt", "The class '{0}' cannot be placed in a level"), FText::FromString(ClassObj->GetName()));
			return Result;
		}

		AssetObj = ClassObj->GetDefaultObject();
	}

	if( ensureMsgf( AssetObj != NULL, TEXT("AssetObj was null (%s)"), *AssetData.GetFullName() ) )
	{
		// Check if the asset has an actor factory
		bool bHasActorFactory = FActorFactoryAssetProxy::GetFactoryForAsset( AssetData ) != NULL;

		if ( AssetObj->IsA( AActor::StaticClass() ) || bHasActorFactory )
		{
			Result.bCanDrop = true;
			GUnrealEd->SetPivotMovedIndependently(false);
		}
		else if( AssetObj->IsA( UBrushBuilder::StaticClass()) )
		{
			Result.bCanDrop = true;
			GUnrealEd->SetPivotMovedIndependently(false);
		}
		else
		{
			HHitProxy* HitProxy = Viewport->GetHitProxy(MouseX, MouseY);
			if( HitProxy != NULL && CanApplyMaterialToHitProxy(HitProxy))
			{
				if ( AssetObj->IsA( UMaterialInterface::StaticClass() ) || AssetObj->IsA( UTexture::StaticClass() ) )
				{
					// If our asset is a material and the target is a valid recipient
					Result.bCanDrop = true;
					GUnrealEd->SetPivotMovedIndependently(false);

					//if ( HitProxy->IsA(HActor::StaticGetType()) )
					//{
					//	Result.HintText = LOCTEXT("Material_Shift_Hint", "Hold [Shift] to apply material to every slot");
					//}
				}
			}
		}
	}

	return Result;
}

bool FLevelEditorViewportClient::DropObjectsAtCoordinates(int32 MouseX, int32 MouseY, const TArray<UObject*>& DroppedObjects, TArray<AActor*>& OutNewActors, bool bOnlyDropOnTarget/* = false*/, bool bCreateDropPreview/* = false*/, bool SelectActors, UActorFactory* FactoryToUse )
{
	bool bResult = false;

	// Make sure the placement dragging actor is cleaned up.
	DestroyDropPreviewActors();

	if(DroppedObjects.Num() > 0)
	{
		bIsDroppingPreviewActor = bCreateDropPreview;
		Viewport->InvalidateHitProxy();

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport, 
			GetScene(),
			EngineShowFlags)
			.SetRealtimeUpdate( IsRealtime() ));
		FSceneView* View = CalcSceneView( &ViewFamily );
		FViewportCursorLocation Cursor(View, this, MouseX, MouseY);

		HHitProxy* HitProxy = Viewport->GetHitProxy(Cursor.GetCursorPos().X, Cursor.GetCursorPos().Y);

		const FActorPositionTraceResult TraceResult = IsDroppingOn2DLayer() ? TraceForPositionOn2DLayer(Cursor) : FActorPositioning::TraceWorldForPositionWithDefault(Cursor, *View);
		
		GEditor->UnsnappedClickLocation = TraceResult.Location;
		GEditor->ClickLocation = TraceResult.Location;
		GEditor->ClickPlane = FPlane(TraceResult.Location, TraceResult.SurfaceNormal);

		// Snap the new location if snapping is enabled
		FSnappingUtils::SnapPointToGrid(GEditor->ClickLocation, FVector::ZeroVector);

		EObjectFlags ObjectFlags = bCreateDropPreview ? RF_Transient : RF_Transactional;
		if (HitProxy == nullptr || HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			bResult = DropObjectsOnBackground(Cursor, DroppedObjects, ObjectFlags, OutNewActors, bCreateDropPreview, SelectActors, FactoryToUse);
		}
		else if (HitProxy->IsA(HActor::StaticGetType()) || HitProxy->IsA(HBSPBrushVert::StaticGetType()))
		{
			AActor* TargetActor = NULL;
			UPrimitiveComponent* TargetComponent = nullptr;
			int32 TargetMaterialSlot = -1;

			if (HitProxy->IsA(HActor::StaticGetType()))
			{
				HActor* TargetProxy = static_cast<HActor*>(HitProxy);
				TargetActor = TargetProxy->Actor;
				TargetComponent = const_cast<UPrimitiveComponent*>(TargetProxy->PrimComponent);
				TargetMaterialSlot = TargetProxy->MaterialIndex;
			}
			else if (HitProxy->IsA(HBSPBrushVert::StaticGetType()))
			{
				HBSPBrushVert* TargetProxy = static_cast<HBSPBrushVert*>(HitProxy);
				TargetActor = TargetProxy->Brush.Get();
			}

			// If shift is pressed set the material slot to -1, so that it's applied to every slot.
			// We have to request it from the platform application directly because IsShiftPressed gets 
			// the cached state, when the viewport had focus
			if ( FSlateApplication::Get().GetPlatformApplication()->GetModifierKeys().IsShiftDown() )
			{
				TargetMaterialSlot = -1;
			}

			if (TargetActor != NULL)
			{
				FNavigationLockContext LockNavigationUpdates(TargetActor->GetWorld(), ENavigationLockReason::SpawnOnDragEnter, bCreateDropPreview);

				// If the target actor is selected, we should drop onto all selected actors
				// otherwise, we should drop only onto the target object
				const bool bDropOntoSelectedActors = TargetActor->IsSelected();
				const bool bCanApplyToComponent = AttemptApplyObjToComponent(DroppedObjects[0], TargetComponent, TargetMaterialSlot, true);
				if (bOnlyDropOnTarget || !bDropOntoSelectedActors || !bCanApplyToComponent)
				{
					if (bCanApplyToComponent)
					{
						const bool bIsTestAttempt = bCreateDropPreview;
						bResult = AttemptApplyObjToComponent(DroppedObjects[0], TargetComponent, TargetMaterialSlot, bIsTestAttempt);
					}
					else
					{
						// Couldn't apply to a component, so try dropping the objects on the hit actor
						bResult = DropObjectsOnActor(Cursor, DroppedObjects, TargetActor, TargetMaterialSlot, ObjectFlags, OutNewActors, bCreateDropPreview, SelectActors, FactoryToUse);
					}
				}
				else
				{
					// Are any components selected?
					if (GEditor->GetSelectedComponentCount() > 0)
					{
						// Is the target component selected?
						USelection* ComponentSelection = GEditor->GetSelectedComponents();
						if (ComponentSelection->IsSelected(TargetComponent))
						{
							// The target component is selected, so try applying the object to every selected component
							for (FSelectedEditableComponentIterator It(GEditor->GetSelectedEditableComponentIterator()); It; ++It)
							{
								USceneComponent* SceneComponent = Cast<USceneComponent>(*It);
								AttemptApplyObjToComponent(DroppedObjects[0], SceneComponent, TargetMaterialSlot, bCreateDropPreview);
								bResult = true;
							}
						}
						else
						{
							// The target component is not selected, so apply the object exclusively to it
							bResult = AttemptApplyObjToComponent(DroppedObjects[0], TargetComponent, TargetMaterialSlot, bCreateDropPreview);
						}
					}
					
					if (!bResult)
					{
						const FScopedTransaction Transaction(LOCTEXT("DropObjectsOnSelectedActors", "Drop Objects on Selected Actors"));
						for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
						{
							TargetActor = static_cast<AActor*>( *It );
							if (TargetActor)
							{
								DropObjectsOnActor(Cursor, DroppedObjects, TargetActor, TargetMaterialSlot, ObjectFlags, OutNewActors, bCreateDropPreview, SelectActors, FactoryToUse);
								bResult = true;
							}
						}
					}
				}
			}
		}
		else if (HitProxy->IsA(HModel::StaticGetType()))
		{
			// BSP surface
			bResult = DropObjectsOnBSPSurface(View, Cursor, DroppedObjects, static_cast<HModel*>(HitProxy), ObjectFlags, OutNewActors, bCreateDropPreview, SelectActors, FactoryToUse);
		}
		else if( HitProxy->IsA( HWidgetAxis::StaticGetType() ) )
		{
			// Axis translation/rotation/scale widget - find out what's underneath the axis widget
			bResult = DropObjectsOnWidget(View, Cursor, DroppedObjects);
		}

		if ( bResult )
		{
			// If we are creating a drop preview actor instead of a normal actor, we need to disable collision, selection, and make sure it is never saved.
			if ( bCreateDropPreview && OutNewActors.Num() > 0 )
			{
				DropPreviewActors.Empty();

				for ( auto ActorIt = OutNewActors.CreateConstIterator(); ActorIt; ++ActorIt )
				{
					AActor* NewActor = *ActorIt;
					DropPreviewActors.Add(NewActor);
					
					PreDragActorTransforms.Add(NewActor, NewActor->GetTransform());

					NewActor->SetActorEnableCollision(false);

					// Deselect if selected
					if( NewActor->IsSelected() )
					{
						GEditor->SelectActor( NewActor, /*InSelected=*/false, /*bNotify=*/true );
					}

					// Prevent future selection. This also prevents the hit proxy from interfering with placement logic.
					TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
					NewActor->GetComponents(PrimitiveComponents);

					for ( auto CompIt = PrimitiveComponents.CreateConstIterator(); CompIt; ++CompIt )
					{
						(*CompIt)->bSelectable = false;
					}
				}

				// Set the current MouseX/Y to prime the preview update
				DropPreviewMouseX = MouseX;
				DropPreviewMouseY = MouseY;

				// Invalidate the hit proxy now so the drop preview will be accurate.
				// We don't invalidate the hit proxy in the drop preview update itself because it is slow.
				//Viewport->InvalidateHitProxy();
			}
			// Dropping the actors rather than a preview? Probably want to select them all then. 
			else if(!bCreateDropPreview && SelectActors && OutNewActors.Num() > 0)
			{
				for (auto It = OutNewActors.CreateConstIterator(); It; ++It)
				{
					GEditor->SelectActor( (*It), true, true );
				}
			}

			// Give the viewport focus
			//SetFocus( static_cast<HWND>( Viewport->GetWindow() ) );

			SetCurrentViewport();
		}
	}

	if (bResult)
	{
		if ( !bCreateDropPreview && IPlacementModeModule::IsAvailable() )
		{
			IPlacementModeModule::Get().AddToRecentlyPlaced( DroppedObjects, FactoryToUse );
		}

		if (!bCreateDropPreview)
		{
			FEditorDelegates::OnNewActorsDropped.Broadcast(DroppedObjects, OutNewActors);
		}
	}

	// Reset if creating a preview actor.
	bIsDroppingPreviewActor = false;

	return bResult;
}

/**
 *	Called to check if a material can be applied to an object, given the hit proxy
 */
bool FLevelEditorViewportClient::CanApplyMaterialToHitProxy( const HHitProxy* HitProxy ) const
{
	// The check for HWidgetAxis is made to prevent the transform widget from blocking an attempt at applying a material to a mesh.
	return ( HitProxy->IsA(HModel::StaticGetType()) || HitProxy->IsA(HActor::StaticGetType()) || HitProxy->IsA(HWidgetAxis::StaticGetType()) );
}

FTrackingTransaction::FTrackingTransaction()
	: TransCount(0)
	, ScopedTransaction(NULL)
	, TrackingTransactionState(ETransactionState::Inactive)
{}

FTrackingTransaction::~FTrackingTransaction()
{
	End();
}

void FTrackingTransaction::Begin(const FText& Description)
{
	End();
	ScopedTransaction = new FScopedTransaction( Description );

	TrackingTransactionState = ETransactionState::Active;

	TSet<AGroupActor*> GroupActors;

	// Modify selected actors to record their state at the start of the transaction
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* Actor = CastChecked<AActor>(*It);

		Actor->Modify();

		if (UActorGroupingUtils::IsGroupingActive())
		{
			// if this actor is in a group, add the GroupActor into a list to be modified shortly
			AGroupActor* ActorLockedRootGroup = AGroupActor::GetRootForActor(Actor, true);
			if (ActorLockedRootGroup != nullptr)
			{
				GroupActors.Add(ActorLockedRootGroup);
			}
		}
	}

	// Modify unique group actors
	for (AGroupActor* GroupActor : GroupActors)
	{
		GroupActor->Modify();
	}

	// Modify selected components
	for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
	{
		CastChecked<UActorComponent>(*It)->Modify();
	}
}

void FTrackingTransaction::End()
{
	if( ScopedTransaction )
	{
		delete ScopedTransaction;
		ScopedTransaction = NULL;
	}
	TrackingTransactionState = ETransactionState::Inactive;
}

void FTrackingTransaction::Cancel()
{
	if(ScopedTransaction)
	{
		ScopedTransaction->Cancel();
	}
	End();
}

void FTrackingTransaction::BeginPending(const FText& Description)
{
	End();

	PendingDescription = Description;
	TrackingTransactionState = ETransactionState::Pending;
}

void FTrackingTransaction::PromotePendingToActive()
{
	if(IsPending())
	{
		Begin(PendingDescription);
		PendingDescription = FText();
	}
}

FLevelEditorViewportClient::FLevelEditorViewportClient(const TSharedPtr<SLevelViewport>& InLevelViewport)
	: FEditorViewportClient(&GLevelEditorModeTools(), nullptr, StaticCastSharedPtr<SEditorViewport>(InLevelViewport))
	, ViewHiddenLayers()
	, VolumeActorVisibility()
	, LastEditorViewLocation( FVector::ZeroVector )
	, LastEditorViewRotation( FRotator::ZeroRotator )
	, ColorScale( FVector(1,1,1) )
	, FadeColor( FColor(0,0,0) )
	, FadeAmount(0.f)	
	, bEnableFading(false)
	, bEnableColorScaling(false)
	, bDrawBaseInfo(false)
	, bDuplicateOnNextDrag( false )
	, bDuplicateActorsInProgress( false )
	, bIsTrackingBrushModification( false )
	, bOnlyMovedPivot(false)
	, bLockedCameraView(true)
	, bReceivedFocusRecently(false)
	, bAlwaysShowModeWidgetAfterSelectionChanges(true)
	, SpriteCategoryVisibility()
	, World(nullptr)
	, TrackingTransaction()
	, DropPreviewMouseX(0)
	, DropPreviewMouseY(0)
	, bWasControlledByOtherViewport(false)
	, ActorLockedByMatinee(nullptr)
	, ActorLockedToCamera(nullptr)
	, SoundShowFlags(ESoundShowFlags::Disabled)
	, bEditorCameraCut(false)
	, bWasEditorCameraCut(false)
{
	// By default a level editor viewport is pointed to the editor world
	SetReferenceToWorldContext(GEditor->GetEditorWorldContext());

	GEditor->LevelViewportClients.Add(this);

	// The level editor fully supports mode tools and isn't doing any incompatible stuff with the Widget
	ModeTools->SetWidgetMode(FWidget::WM_Translate);
	Widget->SetUsesEditorModeTools(ModeTools);

	// Register for editor cleanse events so we can release references to hovered actors
	FEditorSupportDelegates::CleanseEditor.AddRaw(this, &FLevelEditorViewportClient::OnEditorCleanse);

	// Register for editor PIE event that allows you to clean up states that might block PIE
	FEditorDelegates::PreBeginPIE.AddRaw(this, &FLevelEditorViewportClient::OnPreBeginPIE);

	// Add a delegate so we get informed when an actor has moved.
	GEngine->OnActorMoved().AddRaw(this, &FLevelEditorViewportClient::OnActorMoved);

	// GEditorModeTools serves as our draw helper
	bUsesDrawHelper = false;

	InitializeVisibilityFlags();

	// Sign up for notifications about users changing settings.
	GetMutableDefault<ULevelEditorViewportSettings>()->OnSettingChanged().AddRaw(this, &FLevelEditorViewportClient::HandleViewportSettingChanged);
}

//
//	FLevelEditorViewportClient::~FLevelEditorViewportClient
//

FLevelEditorViewportClient::~FLevelEditorViewportClient()
{
	// Unregister for all global callbacks to this object
	FEditorSupportDelegates::CleanseEditor.RemoveAll(this);
	FEditorDelegates::PreBeginPIE.RemoveAll(this);

	// Remove our move delegate
	GEngine->OnActorMoved().RemoveAll(this);

	// make sure all actors have this view removed from their visibility bits
	GEditor->Layers->RemoveViewFromActorViewVisibility( this );

	//make to clean up the global "current" & "last" clients when we delete the active one.
	if (GCurrentLevelEditingViewportClient == this)
	{
		GCurrentLevelEditingViewportClient = NULL;
	}
	if (GLastKeyLevelEditingViewportClient == this)
	{
		GLastKeyLevelEditingViewportClient = NULL;
	}

	GetMutableDefault<ULevelEditorViewportSettings>()->OnSettingChanged().RemoveAll(this);

	GEditor->LevelViewportClients.Remove(this);

	RemoveReferenceToWorldContext(GEditor->GetEditorWorldContext());
}

void FLevelEditorViewportClient::InitializeVisibilityFlags()
{
	// make sure all actors know about this view for per-view layer vis
	GEditor->Layers->UpdatePerViewVisibility(this);

	// Get the number of volume classes so we can initialize our bit array
	TArray<UClass*> VolumeClasses;
	UUnrealEdEngine::GetSortedVolumeClasses(&VolumeClasses);
	VolumeActorVisibility.Init(true, VolumeClasses.Num());

	// Initialize all sprite categories to visible
	SpriteCategoryVisibility.Init(true, GUnrealEd->SpriteIDToIndexMap.Num());
}

FSceneView* FLevelEditorViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const EStereoscopicPass StereoPass)
{
	bWasControlledByOtherViewport = false;

	UpdateViewForLockedActor();

	// set all other matching viewports to my location, if the LOD locking is enabled,
	// unless another viewport already set me this frame (otherwise they fight)
	if (GEditor->bEnableLODLocking)
	{
		for (int32 ViewportIndex = 0; ViewportIndex < GEditor->LevelViewportClients.Num(); ViewportIndex++)
		{
			FLevelEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[ViewportIndex];

			//only change camera for a viewport that is looking at the same scene
			if (ViewportClient == NULL || GetScene() != ViewportClient->GetScene())
			{
				continue;
			}

			// go over all other level viewports
			if (ViewportClient->Viewport && ViewportClient != this)
			{
				// force camera of same-typed viewports
				if (ViewportClient->GetViewportType() == GetViewportType())
				{
					ViewportClient->SetViewLocation( GetViewLocation() );
					ViewportClient->SetViewRotation( GetViewRotation() );
					ViewportClient->SetOrthoZoom( GetOrthoZoom() );

					// don't let this other viewport update itself in its own CalcSceneView
					ViewportClient->bWasControlledByOtherViewport = true;
				}
				// when we are LOD locking, ortho views get their camera position from this view, so make sure it redraws
				else if (IsPerspective() && !ViewportClient->IsPerspective())
				{
					// don't let this other viewport update itself in its own CalcSceneView
					ViewportClient->bWasControlledByOtherViewport = true;
				}
			}

			// if the above code determined that this viewport has changed, delay the update unless
			// an update is already in the pipe
			if (ViewportClient->bWasControlledByOtherViewport && ViewportClient->TimeForForceRedraw == 0.0)
			{
				ViewportClient->TimeForForceRedraw = FPlatformTime::Seconds() + 0.9 + FMath::FRand() * 0.2;
			}
		}
	}

	FSceneView* View = FEditorViewportClient::CalcSceneView(ViewFamily, StereoPass);

	View->ViewActor = ActorLockedByMatinee.IsValid() ? ActorLockedByMatinee.Get() : ActorLockedToCamera.Get();
	View->SpriteCategoryVisibility = SpriteCategoryVisibility;
	View->bCameraCut = bEditorCameraCut;
	View->bHasSelectedComponents = GEditor->GetSelectedComponentCount() > 0;
	return View;

}

ELevelViewportType FLevelEditorViewportClient::GetViewportType() const
{
	const UCameraComponent* ActiveCameraComponent = GetCameraComponentForView();
	
	if (ActiveCameraComponent != NULL)
	{
		return (ActiveCameraComponent->ProjectionMode == ECameraProjectionMode::Perspective) ? LVT_Perspective : LVT_OrthoFreelook;
	}
	else
	{
		return FEditorViewportClient::GetViewportType();
	}
}

void FLevelEditorViewportClient::SetViewportTypeFromTool( ELevelViewportType InViewportType )
{
	SetViewportType(InViewportType);
}

void FLevelEditorViewportClient::SetViewportType( ELevelViewportType InViewportType )
{
	if (InViewportType != LVT_Perspective)
	{
		SetActorLock(nullptr);
		UpdateViewForLockedActor();
	}

	FEditorViewportClient::SetViewportType(InViewportType);
}

void FLevelEditorViewportClient::RotateViewportType()
{
	SetActorLock(nullptr);
	UpdateViewForLockedActor();

	FEditorViewportClient::RotateViewportType();
}

void FLevelEditorViewportClient::OverridePostProcessSettings( FSceneView& View )
{
	const UCameraComponent* CameraComponent = GetCameraComponentForView();
	if (CameraComponent)
	{
		View.OverridePostProcessSettings(CameraComponent->PostProcessSettings, CameraComponent->PostProcessBlendWeight);
	}
}

bool FLevelEditorViewportClient::ShouldLockPitch() const 
{
	return FEditorViewportClient::ShouldLockPitch() || !ModeTools->GetActiveMode(FBuiltinEditorModes::EM_InterpEdit) ;
}

void FLevelEditorViewportClient::BeginCameraMovement(bool bHasMovement)
{
	// If there's new movement broadcast it
	if (bHasMovement)
	{
		if (!bIsCameraMoving)
		{
			AActor* ActorLock = GetActiveActorLock().Get();
			if (!bIsCameraMovingOnTick && ActorLock)
			{
				GEditor->BroadcastBeginCameraMovement(*ActorLock);
			}
			bIsCameraMoving = true;
		}
	}
	else
	{
		bIsCameraMoving = false;
	}
}

void FLevelEditorViewportClient::EndCameraMovement()
{
	// If there was movement and it has now stopped, broadcast it
	if (bIsCameraMovingOnTick && !bIsCameraMoving)
	{
		if (AActor* ActorLock = GetActiveActorLock().Get())
		{
			GEditor->BroadcastEndCameraMovement(*ActorLock);
		}
	}
}

void FLevelEditorViewportClient::PerspectiveCameraMoved()
{
	// Update the locked actor (if any) from the camera
	MoveLockedActorToCamera();

	// If any other viewports have this actor locked too, we need to update them
	if( GetActiveActorLock().IsValid() )
	{
		UpdateLockedActorViewports(GetActiveActorLock().Get(), false);
	}

	// Tell the editing mode that the camera moved, in case its interested.
	FEdMode* Mode = ModeTools->GetActiveMode(FBuiltinEditorModes::EM_InterpEdit);
	if( Mode )
	{
		((FEdModeInterpEdit*)Mode)->CamMoveNotify(this);
	}

	// Broadcast 'camera moved' delegate
	FEditorDelegates::OnEditorCameraMoved.Broadcast(GetViewLocation(), GetViewRotation(), ViewportType, ViewIndex);
}

/**
 * Reset the camera position and rotation.  Used when creating a new level.
 */
void FLevelEditorViewportClient::ResetCamera()
{
	// Initialize perspective view transform
	ViewTransformPerspective.SetLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	ViewTransformPerspective.SetRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);
	ViewTransformPerspective.SetLookAt(FVector::ZeroVector);

	FMatrix OrbitMatrix = ViewTransformPerspective.ComputeOrbitMatrix();
	OrbitMatrix = OrbitMatrix.InverseFast();

	ViewTransformPerspective.SetRotation(OrbitMatrix.Rotator());
	ViewTransformPerspective.SetLocation(OrbitMatrix.GetOrigin());

	ViewTransformPerspective.SetOrthoZoom(DEFAULT_ORTHOZOOM);

	// Initialize orthographic view transform
	ViewTransformOrthographic.SetLocation(FVector::ZeroVector);
	ViewTransformOrthographic.SetRotation(FRotator::ZeroRotator);
	ViewTransformOrthographic.SetOrthoZoom(DEFAULT_ORTHOZOOM);

	ViewFOV = FOVAngle;

	// If interp mode is active, tell it about the camera movement.
	FEdMode* Mode = ModeTools->GetActiveMode(FBuiltinEditorModes::EM_InterpEdit);
	if( Mode )
	{
		((FEdModeInterpEdit*)Mode)->CamMoveNotify(this);
	}

	// Broadcast 'camera moved' delegate
	FEditorDelegates::OnEditorCameraMoved.Broadcast(GetViewLocation(), GetViewRotation(), ViewportType, ViewIndex);
}

void FLevelEditorViewportClient::ResetViewForNewMap()
{
	ResetCamera();
	bForcingUnlitForNewMap = true;
}

void FLevelEditorViewportClient::PrepareCameraForPIE()
{
	LastEditorViewLocation = GetViewLocation();
	LastEditorViewRotation = GetViewRotation();
}

void FLevelEditorViewportClient::RestoreCameraFromPIE()
{
	const bool bRestoreEditorCamera = GEditor != NULL && !GetDefault<ULevelEditorViewportSettings>()->bEnableViewportCameraToUpdateFromPIV;

	//restore the camera position if this is an ortho viewport OR if PIV camera dropping is undesired
	if ( IsOrtho() || bRestoreEditorCamera )
	{
		SetViewLocation( LastEditorViewLocation );
		SetViewRotation( LastEditorViewRotation );
	}

	if( IsPerspective() )
	{
		ViewFOV = FOVAngle;
		RemoveCameraRoll();
	}
}

void FLevelEditorViewportClient::ReceivedFocus(FViewport* InViewport)
{
	if (!bReceivedFocusRecently)
	{
		bReceivedFocusRecently = true;

		// A few frames can pass between receiving focus and processing a click, so we use a timer to track whether we have recently received focus.
		FTimerHandle DummyHandle;
		FTimerDelegate ResetFocusReceivedTimer;
		ResetFocusReceivedTimer.BindLambda([&] ()
		{
			bReceivedFocusRecently = false;
		});
		GEditor->GetTimerManager()->SetTimer(DummyHandle, ResetFocusReceivedTimer, 0.1f, false);
	}

	FEditorViewportClient::ReceivedFocus(InViewport);
}

//
//	FLevelEditorViewportClient::ProcessClick
//
void FLevelEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{

	const FViewportClick Click(&View,this,Key,Event,HitX,HitY);
	if (Click.GetKey() == EKeys::MiddleMouseButton && !Click.IsAltDown() && !Click.IsShiftDown())
	{
		ClickHandlers::ClickViewport(this, Click);
		return;
	}
	if (!ModeTools->HandleClick(this, HitProxy,Click))
	{
		if (HitProxy == NULL)
		{
			ClickHandlers::ClickBackdrop(this,Click);
		}
		else if (HitProxy->IsA(HWidgetAxis::StaticGetType()))
		{
			// The user clicked on an axis translation/rotation hit proxy.  However, we want
			// to find out what's underneath the axis widget.  To do this, we'll need to render
			// the viewport's hit proxies again, this time *without* the axis widgets!

			// OK, we need to be a bit evil right here.  Basically we want to hijack the ShowFlags
			// for the scene so we can re-render the hit proxies without any axis widgets.  We'll
			// store the original ShowFlags and modify them appropriately
			const bool bOldModeWidgets1 = EngineShowFlags.ModeWidgets;
			const bool bOldModeWidgets2 = View.Family->EngineShowFlags.ModeWidgets;

			EngineShowFlags.SetModeWidgets(false);
			FSceneViewFamily* SceneViewFamily = const_cast<FSceneViewFamily*>(View.Family);
			SceneViewFamily->EngineShowFlags.SetModeWidgets(false);
			bool bWasWidgetDragging = Widget->IsDragging();
			Widget->SetDragging(false);

			// Invalidate the hit proxy map so it will be rendered out again when GetHitProxy
			// is called
			Viewport->InvalidateHitProxy();

			// This will actually re-render the viewport's hit proxies!
			HHitProxy* HitProxyWithoutAxisWidgets = Viewport->GetHitProxy(HitX, HitY);
			if (HitProxyWithoutAxisWidgets != NULL && !HitProxyWithoutAxisWidgets->IsA(HWidgetAxis::StaticGetType()))
			{
				// Try this again, but without the widget this time!
				ProcessClick(View, HitProxyWithoutAxisWidgets, Key, Event, HitX, HitY);
			}

			// Undo the evil
			EngineShowFlags.SetModeWidgets(bOldModeWidgets1);
			SceneViewFamily->EngineShowFlags.SetModeWidgets(bOldModeWidgets2);

			Widget->SetDragging(bWasWidgetDragging);

			// Invalidate the hit proxy map again so that it'll be refreshed with the original
			// scene contents if we need it again later.
			Viewport->InvalidateHitProxy();
		}
		else if (GUnrealEd->ComponentVisManager.HandleClick(this, HitProxy, Click))
		{
			// Component vis manager handled the click
		}
		else if (HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorHitProxy = (HActor*)HitProxy;
			AActor* ConsideredActor = ActorHitProxy->Actor;
			while (ConsideredActor->IsChildActor())
			{
				ConsideredActor = ConsideredActor->GetParentActor();
			}

			// We want to process the click on the component only if:
			// 1. The actor clicked is already selected
			// 2. The actor selected is the only actor selected
			// 3. The actor selected is blueprintable
			// 4. No components are already selected and the click was a double click
			// 5. OR, a component is already selected and the click was NOT a double click
			const bool bActorAlreadySelectedExclusively = GEditor->GetSelectedActors()->IsSelected(ConsideredActor) && ( GEditor->GetSelectedActorCount() == 1 );
			const bool bActorIsBlueprintable = FKismetEditorUtilities::CanCreateBlueprintOfClass(ConsideredActor->GetClass());
			const bool bComponentAlreadySelected = GEditor->GetSelectedComponentCount() > 0;
			const bool bWasDoubleClick = ( Click.GetEvent() == IE_DoubleClick );

			const bool bSelectComponent = bActorAlreadySelectedExclusively && bActorIsBlueprintable && (bComponentAlreadySelected != bWasDoubleClick);

			if (bSelectComponent)
			{
				ClickHandlers::ClickComponent(this, ActorHitProxy, Click);
			}
			else
			{
				ClickHandlers::ClickActor(this, ConsideredActor, Click, true);
			}

			// We clicked an actor, allow the pivot to reposition itself.
			// GUnrealEd->SetPivotMovedIndependently(false);
		}
		else if (HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			ClickHandlers::ClickActor(this, ((HInstancedStaticMeshInstance*)HitProxy)->Component->GetOwner(), Click, true);
		}
		else if (HitProxy->IsA(HBSPBrushVert::StaticGetType()) && ((HBSPBrushVert*)HitProxy)->Brush.IsValid())
		{
			ClickHandlers::ClickBrushVertex(this,((HBSPBrushVert*)HitProxy)->Brush.Get(),((HBSPBrushVert*)HitProxy)->Vertex,Click);
		}
		else if (HitProxy->IsA(HStaticMeshVert::StaticGetType()))
		{
			ClickHandlers::ClickStaticMeshVertex(this,((HStaticMeshVert*)HitProxy)->Actor,((HStaticMeshVert*)HitProxy)->Vertex,Click);
		}
		else if (HitProxy->IsA(HGeomPolyProxy::StaticGetType()))
		{
			HGeomPolyProxy* GeomHitProxy = (HGeomPolyProxy*)HitProxy;

			if( GeomHitProxy->GetGeomObject() )
			{
				FHitResult CheckResult(ForceInit);
				FCollisionQueryParams BoxParams(SCENE_QUERY_STAT(ProcessClickTrace), false, GeomHitProxy->GetGeomObject()->ActualBrush);
				bool bHit = GWorld->SweepSingleByObjectType(CheckResult, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(FVector(1.f)), BoxParams);

				if(bHit)
				{
					GEditor->UnsnappedClickLocation = CheckResult.Location;
					GEditor->ClickLocation = CheckResult.Location;
					GEditor->ClickPlane = FPlane(CheckResult.Location, CheckResult.Normal);
				}

				if(!ClickHandlers::ClickActor(this, GeomHitProxy->GetGeomObject()->ActualBrush, Click, false))
				{
					ClickHandlers::ClickGeomPoly(this, GeomHitProxy, Click);
				}

				Invalidate(true, true);
			}
		}
		else if (HitProxy->IsA(HGeomEdgeProxy::StaticGetType()))
		{
			HGeomEdgeProxy* GeomHitProxy = (HGeomEdgeProxy*)HitProxy;

			if( GeomHitProxy->GetGeomObject() !=nullptr )
			{
				if(!ClickHandlers::ClickGeomEdge(this, GeomHitProxy, Click))
				{
					ClickHandlers::ClickActor(this, GeomHitProxy->GetGeomObject()->ActualBrush, Click, true);
				}
			}
		}
		else if (HitProxy->IsA(HGeomVertexProxy::StaticGetType()))
		{
			ClickHandlers::ClickGeomVertex(this,(HGeomVertexProxy*)HitProxy,Click);
		}
		else if (HitProxy->IsA(HModel::StaticGetType()))
		{
			HModel* ModelHit = (HModel*)HitProxy;

			// Compute the viewport's current view family.
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues( Viewport, GetScene(), EngineShowFlags ));
			FSceneView* SceneView = CalcSceneView( &ViewFamily );

			uint32 SurfaceIndex = INDEX_NONE;
			if(ModelHit->ResolveSurface(SceneView,HitX,HitY,SurfaceIndex))
			{
				ClickHandlers::ClickSurface(this,ModelHit->GetModel(),SurfaceIndex,Click);
			}
		}
		else if (HitProxy->IsA(HLevelSocketProxy::StaticGetType()))
		{
			ClickHandlers::ClickLevelSocket(this, HitProxy, Click);
		}
	}
}

// Frustum parameters for the perspective view.
static float GPerspFrustumAngle=90.f;
static float GPerspFrustumAspectRatio=1.77777f;
static float GPerspFrustumStartDist=GNearClippingPlane;
static float GPerspFrustumEndDist=HALF_WORLD_MAX;
static FMatrix GPerspViewMatrix;


void FLevelEditorViewportClient::Tick(float DeltaTime)
{
	if (bWasEditorCameraCut && bEditorCameraCut)
	{
		bEditorCameraCut = false;
	}
	bWasEditorCameraCut = bEditorCameraCut;

	FEditorViewportClient::Tick(DeltaTime);

	// Update the preview mesh for the preview mesh mode. 
	GEditor->UpdatePreviewMesh();

	// Copy perspective views to the global if this viewport is a view parent or has streaming volume previs enabled
	if ( ViewState.GetReference()->IsViewParent() || (IsPerspective() && GetDefault<ULevelEditorViewportSettings>()->bLevelStreamingVolumePrevis && Viewport->GetSizeXY().X > 0) )
	{
		GPerspFrustumAngle=ViewFOV;
		GPerspFrustumAspectRatio=AspectRatio;
		GPerspFrustumStartDist=GetNearClipPlane();

		GPerspFrustumEndDist= HALF_WORLD_MAX;

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport,
			GetScene(),
			EngineShowFlags)
			.SetRealtimeUpdate( IsRealtime() ) );
		FSceneView* View = CalcSceneView(&ViewFamily);
		GPerspViewMatrix = View->ViewMatrices.GetViewMatrix();
	}

	UpdateViewForLockedActor(DeltaTime);
}

void FLevelEditorViewportClient::UpdateViewForLockedActor(float DeltaTime)
{
	// We can't be locked to a matinee actor if this viewport doesn't allow matinee control
	if ( !bAllowCinematicPreview && ActorLockedByMatinee.IsValid() )
	{
		ActorLockedByMatinee = nullptr;
	}

	bUseControllingActorViewInfo = false;
	ControllingActorViewInfo = FMinimalViewInfo();
	ControllingActorExtraPostProcessBlends.Empty();
	ControllingActorExtraPostProcessBlendWeights.Empty();

	AActor* Actor = ActorLockedByMatinee.IsValid() ? ActorLockedByMatinee.Get() : ActorLockedToCamera.Get();
	if( Actor != NULL )
	{
		// Check if the viewport is transitioning
		FViewportCameraTransform& ViewTransform = GetViewTransform();
		if (!ViewTransform.IsPlaying())
		{
			// Update transform
			if (Actor->GetAttachParentActor() != NULL)
			{
				// Actor is parented, so use the actor to world matrix for translation and rotation information.
				SetViewLocation(Actor->GetActorLocation());
				SetViewRotation(Actor->GetActorRotation());
			}
			else if (Actor->GetRootComponent() != NULL)
			{
				// No attachment, so just use the relative location, so that we don't need to
				// convert from a quaternion, which loses winding information.
				SetViewLocation(Actor->GetRootComponent()->RelativeLocation);
				SetViewRotation(Actor->GetRootComponent()->RelativeRotation);
			}

			if (bLockedCameraView)
			{
				// If this is a camera actor, then inherit some other settings
				USceneComponent* const ViewComponent = FindViewComponentForActor(Actor);
				if (ViewComponent != nullptr)
				{
					if ( ensure(ViewComponent->GetEditorPreviewInfo(DeltaTime, ControllingActorViewInfo)) )
					{
						bUseControllingActorViewInfo = true;
						if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(ViewComponent))
						{
							CameraComponent->GetExtraPostProcessBlends(ControllingActorExtraPostProcessBlends, ControllingActorExtraPostProcessBlendWeights);
						}
						
						// Post processing is handled by OverridePostProcessingSettings
						ViewFOV = ControllingActorViewInfo.FOV;
						AspectRatio = ControllingActorViewInfo.AspectRatio;
						SetViewLocation(ControllingActorViewInfo.Location);
						SetViewRotation(ControllingActorViewInfo.Rotation);
					}
				}
			}
		}
	}
}

/*namespace ViewportDeadZoneConstants
{
	enum
	{
		NO_DEAD_ZONE,
		STANDARD_DEAD_ZONE
	};
};*/

/** Trim the specified line to the planes of the frustum */
void TrimLineToFrustum(const FConvexVolume& Frustum, FVector& Start, FVector& End)
{
	FVector Intersection;
	for (const FPlane& Plane : Frustum.Planes)
	{
		if (FMath::SegmentPlaneIntersection(Start, End, Plane, Intersection))
		{
			// Chop the line inside the frustum
			if ((static_cast<const FVector&>(Plane) | (Intersection - End)) > 0.0f)
			{
				Start = Intersection;
			}
			else
			{
				End = Intersection;
			}
		}
	}
}

void FLevelEditorViewportClient::ProjectActorsIntoWorld(const TArray<AActor*>& Actors, FViewport* InViewport, const FVector& Drag, const FRotator& Rot)
{
	// Compile an array of selected actors
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewport, 
		GetScene(),
		EngineShowFlags)
		.SetRealtimeUpdate( IsRealtime() ));
	// SceneView is deleted with the ViewFamily
	FSceneView* SceneView = CalcSceneView( &ViewFamily );

	// Calculate the frustum so we can trim rays to it
	const FConvexVolume Frustum;
	GetViewFrustumBounds(const_cast<FConvexVolume&>(Frustum), SceneView->ViewMatrices.GetViewProjectionMatrix(), true);

	const FMatrix InputCoordSystem = GetWidgetCoordSystem();
	const EAxisList::Type CurrentAxis = GetCurrentWidgetAxis();
	
	const FVector DeltaTranslation = (ModeTools->PivotLocation - ModeTools->CachedLocation) + Drag;


	// Loop over all the actors and attempt to snap them along the drag axis normal
	for (AActor* Actor : Actors)
	{
		// Use the Delta of the Mode tool with the actor pre drag location to avoid accumulating snapping offsets
		FVector NewActorPosition;
		if (const FTransform* PreDragTransform = PreDragActorTransforms.Find(Actor))
		{
			NewActorPosition = PreDragTransform->GetLocation() + DeltaTranslation;
		}
		else
		{
			const FTransform& ActorTransform = PreDragActorTransforms.Add(Actor, Actor->GetTransform());
			NewActorPosition = ActorTransform.GetLocation() + DeltaTranslation;
		}

		FViewportCursorLocation Cursor(SceneView, this, 0, 0);

		FActorPositionTraceResult TraceResult;
		bool bSnapped = false;

		bool bIsOnScreen = false;
		{
			// We only snap things that are on screen
			FVector2D ScreenPos;
			FIntPoint ViewportSize = InViewport->GetSizeXY();
			if (SceneView->WorldToPixel(NewActorPosition, ScreenPos) && FMath::IsWithin<float>(ScreenPos.X, 0, ViewportSize.X) && FMath::IsWithin<float>(ScreenPos.Y, 0, ViewportSize.Y))
			{
				bIsOnScreen = true;
				Cursor = FViewportCursorLocation(SceneView, this, ScreenPos.X, ScreenPos.Y);
			}
		}

		if (bIsOnScreen)
		{
			// Determine how we're going to attempt to project the object onto the world
			if (CurrentAxis == EAxisList::XY || CurrentAxis == EAxisList::XZ || CurrentAxis == EAxisList::YZ)
			{
				// Snap along the perpendicular axis
				const FVector PlaneNormal = CurrentAxis == EAxisList::XY ? FVector(0, 0, 1) : CurrentAxis == EAxisList::XZ ? FVector(0, 1, 0) : FVector(1, 0, 0);
				FVector TraceDirection = InputCoordSystem.TransformVector(PlaneNormal);

				// Make sure the trace normal points along the view direction
				if (FVector::DotProduct(SceneView->GetViewDirection(), TraceDirection) < 0.f)
				{
					TraceDirection = -TraceDirection;
				}

				FVector RayStart	= NewActorPosition - (TraceDirection * HALF_WORLD_MAX/2);
				FVector RayEnd		= NewActorPosition + (TraceDirection * HALF_WORLD_MAX/2);

				TrimLineToFrustum(Frustum, RayStart, RayEnd);

				TraceResult = FActorPositioning::TraceWorldForPosition(*GetWorld(), *SceneView, RayStart, RayEnd, &Actors);
			}
			else
			{
				TraceResult = FActorPositioning::TraceWorldForPosition(Cursor, *SceneView, &Actors);
			}
		}
				
		if (TraceResult.State == FActorPositionTraceResult::HitSuccess)
		{
			// Move the actor to the position of the trace hit using the spawn offset rules
			// We only do this if we found a valid hit (we don't want to move the actor in front of the camera by default)

			const UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(Actor->GetClass());
			
			const FTransform* PreDragActorTransform = PreDragActorTransforms.Find(Actor);
			check(PreDragActorTransform);

			// Compute the surface aligned transform. Note we do not use the snapped version here as our DragDelta is already snapped

			const FPositioningData PositioningData = FPositioningData(TraceResult.Location, TraceResult.SurfaceNormal)
				.UseStartTransform(*PreDragActorTransform)
				.UsePlacementExtent(Actor->GetPlacementExtent())
				.UseFactory(Factory);

			FTransform ActorTransform = FActorPositioning::GetSurfaceAlignedTransform(PositioningData);
			
			ActorTransform.SetScale3D(Actor->GetActorScale3D());
			if (USceneComponent* RootComponent = Actor->GetRootComponent())
			{
				RootComponent->SetWorldTransform(ActorTransform);
			}
		}
		else
		{
			// Didn't find a valid surface snapping candidate, just apply the deltas directly
			ApplyDeltaToActor(Actor, NewActorPosition - Actor->GetActorLocation(), Rot, FVector(0.f, 0.f, 0.f));
		}
	}
}

bool FLevelEditorViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type InCurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	if (GUnrealEd->ComponentVisManager.HandleInputDelta(this, InViewport, Drag, Rot, Scale))
	{
		return true;
	}

	bool bHandled = false;

	// Give the current editor mode a chance to use the input first.  If it does, don't apply it to anything else.
	if (FEditorViewportClient::InputWidgetDelta(InViewport, InCurrentAxis, Drag, Rot, Scale))
	{
		bHandled = true;
	}
	else
	{
		//@TODO: MODETOOLS: Much of this needs to get pushed to Super, but not all of it can be...
		if( InCurrentAxis != EAxisList::None )
		{
			// Skip actors transformation routine in case if any of the selected actors locked
			// but still pretend that we have handled the input
			if (!GEditor->HasLockedActors())
			{
				const bool LeftMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);
				const bool RightMouseButtonDown = InViewport->KeyState(EKeys::RightMouseButton);
				const bool MiddleMouseButtonDown = InViewport->KeyState(EKeys::MiddleMouseButton);

				// If duplicate dragging . . .
				if ( IsAltPressed() && (LeftMouseButtonDown || RightMouseButtonDown) )
				{
					// The widget has been offset, so check if we should duplicate the selection.
					if ( bDuplicateOnNextDrag )
					{
						// Only duplicate if we're translating or rotating.
						if ( !Drag.IsNearlyZero() || !Rot.IsZero() )
						{
							// Widget hasn't been dragged since ALT+LMB went down.
							bDuplicateOnNextDrag = false;
							ABrush::SetSuppressBSPRegeneration(true);
							GEditor->edactDuplicateSelected(GetWorld()->GetCurrentLevel(), false);
							ABrush::SetSuppressBSPRegeneration(false);
						}
					}
				}

				// We do not want actors updated if we are holding down the middle mouse button.
				if(!MiddleMouseButtonDown)
				{
					bool bSnapped = FSnappingUtils::SnapActorsToNearestActor( Drag, this );
					bSnapped = bSnapped || FSnappingUtils::SnapDraggedActorsToNearestVertex( Drag, this );

					// If we are only changing position, project the actors onto the world
					const bool bOnlyTranslation = !Drag.IsZero() && Rot.IsZero() && Scale.IsZero();

					const EAxisList::Type CurrentAxis = GetCurrentWidgetAxis();
					const bool bSingleAxisDrag = CurrentAxis == EAxisList::X || CurrentAxis == EAxisList::Y || CurrentAxis == EAxisList::Z;
					if (!bSnapped && !bSingleAxisDrag && GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled && bOnlyTranslation)
					{
						TArray<AActor*> SelectedActors;
						for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
						{
							if (AActor* Actor = Cast<AActor>(*It))
							{
								SelectedActors.Add(Actor);
							}
						}

						ProjectActorsIntoWorld(SelectedActors, InViewport, Drag, Rot);
					}
					else
					{
						ApplyDeltaToActors( Drag, Rot, Scale );
					}

					ApplyDeltaToRotateWidget( Rot );
				}
				else
				{
					FSnappingUtils::SnapDragLocationToNearestVertex( ModeTools->PivotLocation, Drag, this );
					GUnrealEd->SetPivotMovedIndependently(true);
					bOnlyMovedPivot = true;
				}

				ModeTools->PivotLocation += Drag;
				ModeTools->SnappedLocation += Drag;

				if( IsShiftPressed() )
				{
					FVector CameraDelta( Drag );
					MoveViewportCamera( CameraDelta, FRotator::ZeroRotator );
				}

				TArray<FEdMode*> ActiveModes; 
				ModeTools->GetActiveModes(ActiveModes);

				for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
				{
					ActiveModes[ModeIndex]->UpdateInternalData();
				}
			}

			bHandled = true;
		}

	}

	return bHandled;
}

TSharedPtr<FDragTool> FLevelEditorViewportClient::MakeDragTool( EDragTool::Type DragToolType )
{
	// Let the drag tool handle the transaction
	TrackingTransaction.Cancel();

	TSharedPtr<FDragTool> DragTool;
	switch( DragToolType )
	{
	case EDragTool::BoxSelect:
		DragTool = MakeShareable( new FDragTool_ActorBoxSelect(this) );
		break;
	case EDragTool::FrustumSelect:
		DragTool = MakeShareable( new FDragTool_ActorFrustumSelect(this) );
		break;	
	case EDragTool::Measure:
		DragTool = MakeShareable( new FDragTool_Measure(this) );
		break;
	case EDragTool::ViewportChange:
		DragTool = MakeShareable( new FDragTool_ViewportChange(this) );
		break;
	};

	return DragTool;
}

static bool CommandAcceptsInput( FLevelEditorViewportClient& ViewportClient, FKey Key, const TSharedPtr<FUICommandInfo> Command )
{
	bool bAccepted = false;
	for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
	{
		// check each bound chord
		EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex>(i);
		const FInputChord& Chord = *Command->GetActiveChord(ChordIndex);

		bAccepted |= Chord.IsValidChord()
			&& (!Chord.NeedsControl() || ViewportClient.IsCtrlPressed())
			&& (!Chord.NeedsAlt() || ViewportClient.IsAltPressed())
			&& (!Chord.NeedsShift() || ViewportClient.IsShiftPressed())
			&& (!Chord.NeedsCommand() || ViewportClient.IsCmdPressed())
			&& Chord.Key == Key;
	}
	return bAccepted;
}

static const FLevelViewportCommands& GetLevelViewportCommands()
{
	static FName LevelEditorName("LevelEditor");
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>( LevelEditorName );
	return LevelEditor.GetLevelViewportCommands();
}

void FLevelEditorViewportClient::SetCurrentViewport()
{
	// Set the current level editing viewport client to the dropped-in viewport client
	if (GCurrentLevelEditingViewportClient != this)
	{
		// Invalidate the old vp client to remove its special selection box
		if (GCurrentLevelEditingViewportClient)
		{
			GCurrentLevelEditingViewportClient->Invalidate();
		}
		GCurrentLevelEditingViewportClient = this;
	}
	Invalidate();
}

void FLevelEditorViewportClient::SetLastKeyViewport()
{
	// Store a reference to the last viewport that received a key press.
	GLastKeyLevelEditingViewportClient = this;

	if (GCurrentLevelEditingViewportClient != this)
	{
		if (GCurrentLevelEditingViewportClient)
		{
			//redraw without yellow selection box
			GCurrentLevelEditingViewportClient->Invalidate();
		}
		//cause this viewport to redraw WITH yellow selection box
		Invalidate();
		GCurrentLevelEditingViewportClient = this;
	}
}

bool FLevelEditorViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	if (bDisableInput)
	{
		return true;
	}

	
	const int32	HitX = InViewport->GetMouseX();
	const int32	HitY = InViewport->GetMouseY();

	FInputEventState InputState( InViewport, Key, Event );

	SetLastKeyViewport();

	// Compute a view.
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewport,
		GetScene(),
		EngineShowFlags )
		.SetRealtimeUpdate( IsRealtime() ) );
	FSceneView* View = CalcSceneView( &ViewFamily );

	// Compute the click location.
	if ( InputState.IsAnyMouseButtonDown() )
	{
		const FViewportCursorLocation Cursor(View, this, HitX, HitY);
		const FActorPositionTraceResult TraceResult = FActorPositioning::TraceWorldForPositionWithDefault(Cursor, *View);
		GEditor->UnsnappedClickLocation = TraceResult.Location;
		GEditor->ClickLocation = TraceResult.Location;
		GEditor->ClickPlane = FPlane(TraceResult.Location, TraceResult.SurfaceNormal);

		// Snap the new location if snapping is enabled
		FSnappingUtils::SnapPointToGrid(GEditor->ClickLocation, FVector::ZeroVector);
	}

	if (GUnrealEd->ComponentVisManager.HandleInputKey(this, InViewport, Key, Event))
	{
		return true;
	}

	bool bHandled = FEditorViewportClient::InputKey(InViewport,ControllerId,Key,Event,AmountDepressed,bGamepad);

	// Handle input for the player height preview mode. 
	if (!InputState.IsMouseButtonEvent() && CommandAcceptsInput(*this, Key, GetLevelViewportCommands().EnablePreviewMesh))
	{
		// Holding down the backslash buttons turns on the mode. 
		if (Event == IE_Pressed)
		{
			GEditor->SetPreviewMeshMode(true);

			// If shift down, cycle between the preview meshes
			if (CommandAcceptsInput(*this, Key, GetLevelViewportCommands().CyclePreviewMesh))
			{
				GEditor->CyclePreviewMesh();
			}
		}
		// Releasing backslash turns off the mode. 
		else if (Event == IE_Released)
		{
			GEditor->SetPreviewMeshMode(false);
		}

		bHandled = true;
	}

	// Clear Duplicate Actors mode when ALT and all mouse buttons are released
	if ( !InputState.IsAltButtonPressed() && !InputState.IsAnyMouseButtonDown() )
	{
		bDuplicateActorsInProgress = false;
	}
	
	return bHandled;
}

void FLevelEditorViewportClient::TrackingStarted( const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge )
{
	// Begin transacting.  Give the current editor mode an opportunity to do the transacting.
	const bool bTrackingHandledExternally = ModeTools->StartTracking(this, Viewport);

	TrackingTransaction.End();

	// Re-initialize new tracking only if a new button was pressed, otherwise we continue the previous one.
	if ( InInputState.GetInputEvent() == IE_Pressed )
	{
		EInputEvent Event = InInputState.GetInputEvent();
		FKey Key = InInputState.GetKey();

		if ( InInputState.IsAltButtonPressed() && bDraggingByHandle )
		{
			if(Event == IE_Pressed && (Key == EKeys::LeftMouseButton || Key == EKeys::RightMouseButton) && !bDuplicateActorsInProgress)
			{
				// Set the flag so that the actors actors will be duplicated as soon as the widget is displaced.
				bDuplicateOnNextDrag = true;
				bDuplicateActorsInProgress = true;
			}
		}
		else
		{
			bDuplicateOnNextDrag = false;
		}
	}

	bOnlyMovedPivot = false;

	const bool bIsDraggingComponents = GEditor->GetSelectedComponentCount() > 0;
	PreDragActorTransforms.Empty();
	if (bIsDraggingComponents)
	{
		if (bIsDraggingWidget)
		{
			Widget->SetSnapEnabled(true);

			for (FSelectedEditableComponentIterator It(GEditor->GetSelectedEditableComponentIterator()); It; ++It)
			{
				USceneComponent* SceneComponent = Cast<USceneComponent>(*It);
				if (SceneComponent)
				{
					// Notify that this component is beginning to move
					GEditor->BroadcastBeginObjectMovement(*SceneComponent);
				}
			}
		}
	}
	else
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It && !bIsTrackingBrushModification; ++It)
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow(Actor->IsA(AActor::StaticClass()));

			if (bIsDraggingWidget)
			{
				// Notify that this actor is beginning to move
				GEditor->BroadcastBeginObjectMovement(*Actor);
			}

			Widget->SetSnapEnabled(true);

			// See if any brushes are about to be transformed via their Widget
			TArray<AActor*> AttachedActors;
			Actor->GetAttachedActors(AttachedActors);
			const bool bExactClass = true;
			// First, check for selected brush actors, check the actors attached actors for brush actors as well.  If a parent actor moves, the bsp needs to be rebuilt
			ABrush* Brush = Cast< ABrush >(Actor);
			if (Brush && ( !Brush->IsVolumeBrush() && !FActorEditorUtils::IsABuilderBrush(Actor) ))
			{
				bIsTrackingBrushModification = true;
			}
			else // Next, check for selected groups actors that contain brushes
			{
				AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
				if (GroupActor)
				{
					TArray<AActor*> GroupMembers;
					GroupActor->GetAllChildren(GroupMembers, true);
					for (int32 GroupMemberIdx = 0; GroupMemberIdx < GroupMembers.Num(); ++GroupMemberIdx)
					{
						Brush = Cast< ABrush >(GroupMembers[GroupMemberIdx]);
						if (Brush && ( !Brush->IsVolumeBrush() && !FActorEditorUtils::IsABuilderBrush(Actor) ))
						{
							bIsTrackingBrushModification = true;
						}
					}
				}
			}
		}
	}

	// Start a transformation transaction if required
	if( !bTrackingHandledExternally )
	{
		if( bIsDraggingWidget )
		{
			TrackingTransaction.TransCount++;

			FText ObjectTypeBeingTracked = bIsDraggingComponents ? LOCTEXT("TransactionFocus_Components", "Components") : LOCTEXT("TransactionFocus_Actors", "Actors");
			FText TrackingDescription;

			switch( GetWidgetMode() )
			{
			case FWidget::WM_Translate:
				TrackingDescription = FText::Format(LOCTEXT("MoveTransaction", "Move {0}"), ObjectTypeBeingTracked);
				break;
			case FWidget::WM_Rotate:
				TrackingDescription = FText::Format(LOCTEXT("RotateTransaction", "Rotate {0}"), ObjectTypeBeingTracked);
				break;
			case FWidget::WM_Scale:
				TrackingDescription = FText::Format(LOCTEXT("ScaleTransaction", "Scale {0}"), ObjectTypeBeingTracked);
				break;
			case FWidget::WM_TranslateRotateZ:
				TrackingDescription = FText::Format(LOCTEXT("TranslateRotateZTransaction", "Translate/RotateZ {0}"), ObjectTypeBeingTracked);
				break;
			case FWidget::WM_2D:
				TrackingDescription = FText::Format(LOCTEXT("TranslateRotate2D", "Translate/Rotate2D {0}"), ObjectTypeBeingTracked);
				break;
			default:
				if( bNudge )
				{
					TrackingDescription = FText::Format(LOCTEXT("NudgeTransaction", "Nudge {0}"), ObjectTypeBeingTracked);
				}
			}

			if(!TrackingDescription.IsEmpty())
			{
				if(bNudge)
				{
					TrackingTransaction.Begin(TrackingDescription);
				}
				else
				{
					// If this hasn't begun due to a nudge, start it as a pending transaction so that it only really begins when the mouse is moved
					TrackingTransaction.BeginPending(TrackingDescription);
				}
			}

			if(TrackingTransaction.IsActive() || TrackingTransaction.IsPending())
			{
				// Suspend actor/component modification during each delta step to avoid recording unnecessary overhead into the transaction buffer
				GEditor->DisableDeltaModification(true);
			}
		}
	}

}

void FLevelEditorViewportClient::TrackingStopped()
{
	const bool AltDown = IsAltPressed();
	const bool ShiftDown = IsShiftPressed();
	const bool ControlDown = IsCtrlPressed();
	const bool LeftMouseButtonDown = Viewport->KeyState(EKeys::LeftMouseButton);
	const bool RightMouseButtonDown = Viewport->KeyState(EKeys::RightMouseButton);
	const bool MiddleMouseButtonDown = Viewport->KeyState(EKeys::MiddleMouseButton);

	// Only disable the duplicate on next drag flag if we actually dragged the mouse.
	bDuplicateOnNextDrag = false;

	// here we check to see if anything of worth actually changed when ending our MouseMovement
	// If the TransCount > 0 (we changed something of value) so we need to call PostEditMove() on stuff
	// if we didn't change anything then don't call PostEditMove()
	bool bDidAnythingActuallyChange = false;

	// Stop transacting.  Give the current editor mode an opportunity to do the transacting.
	const bool bTransactingHandledByEditorMode = ModeTools->EndTracking(this, Viewport);
	if( !bTransactingHandledByEditorMode )
	{
		if( TrackingTransaction.TransCount > 0 )
		{
			bDidAnythingActuallyChange = true;
			TrackingTransaction.TransCount--;
		}
	}

	// Finish tracking a brush transform and update the Bsp
	if (bIsTrackingBrushModification)
	{
		bDidAnythingActuallyChange = HaveSelectedObjectsBeenChanged() && !bOnlyMovedPivot;

		bIsTrackingBrushModification = false;
		if ( bDidAnythingActuallyChange && bWidgetAxisControlledByDrag )
		{
			GEditor->RebuildAlteredBSP();
		}
	}

	// Notify the selected actors that they have been moved.
	// Don't do this if AddDelta was never called.
	if( bDidAnythingActuallyChange && MouseDeltaTracker->HasReceivedDelta() )
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow(Actor->IsA(AActor::StaticClass()));

			// Verify that the actor is in the same world as the viewport before moving it.
			if (GEditor->PlayWorld)
			{
				if (bIsSimulateInEditorViewport)
				{
					// If the Actor's outer (level) outer (world) is not the PlayWorld then it cannot be moved in this viewport.
					if (!( GEditor->PlayWorld == Actor->GetOuter()->GetOuter() ))
					{
						continue;
					}
				}
				else if (!( GEditor->EditorWorld == Actor->GetOuter()->GetOuter() ))
				{
					continue;
				}
			}

			bool bComponentsMoved = false;
			if (GEditor->GetSelectedComponentCount() > 0)
			{
				USelection* ComponentSelection = GEditor->GetSelectedComponents();

				// Only move the parent-most component(s) that are selected 
				// Otherwise, if both a parent and child are selected and the delta is applied to both, the child will actually move 2x delta
				TInlineComponentArray<USceneComponent*> ComponentsToMove;
				for (FSelectedEditableComponentIterator EditableComponentIt(GEditor->GetSelectedEditableComponentIterator()); EditableComponentIt; ++EditableComponentIt)
				{
					USceneComponent* SceneComponent = CastChecked<USceneComponent>(*EditableComponentIt);
					if (SceneComponent)
					{
						USceneComponent* SelectedComponent = Cast<USceneComponent>(*EditableComponentIt);

						// Check to see if any parent is selected
						bool bParentAlsoSelected = false;
						USceneComponent* Parent = SelectedComponent->GetAttachParent();
						while (Parent != nullptr)
						{
							if (ComponentSelection->IsSelected(Parent))
							{
								bParentAlsoSelected = true;
								break;
							}

							Parent = Parent->GetAttachParent();
						}

						// If no parent of this component is also in the selection set, move it!
						if (!bParentAlsoSelected)
						{
							ComponentsToMove.Add(SelectedComponent);
						}
					}
				}

				// Now actually apply the delta to the appropriate component(s)
				for (USceneComponent* SceneComp : ComponentsToMove)
				{
					SceneComp->PostEditComponentMove(true);

					GEditor->BroadcastEndObjectMovement(*SceneComp);

					bComponentsMoved = true;
				}
			}
				
			if (!bComponentsMoved)
			{
				Actor->PostEditMove(true);
	
				GEditor->BroadcastEndObjectMovement(*Actor);
			}
		}

		if (!GUnrealEd->IsPivotMovedIndependently())
		{
			GUnrealEd->UpdatePivotLocationForSelection();
		}
	}

	// End the transaction here if one was started in StartTransaction()
	if( TrackingTransaction.IsActive() || TrackingTransaction.IsPending() )
	{
		if( !HaveSelectedObjectsBeenChanged())
		{
			TrackingTransaction.Cancel();
		}
		else
		{
			TrackingTransaction.End();
		}
		
		// Restore actor/component delta modification
		GEditor->DisableDeltaModification(false);
	}

	TArray<FEdMode*> ActiveModes; 
	ModeTools->GetActiveModes(ActiveModes);
	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		// Also notify the current editing modes if they are interested.
		ActiveModes[ModeIndex]->ActorMoveNotify();
	}

	if( bDidAnythingActuallyChange )
	{
		FScopedLevelDirtied LevelDirtyCallback;
		LevelDirtyCallback.Request();

		RedrawAllViewportsIntoThisScene();
	}

	PreDragActorTransforms.Empty();
}

void FLevelEditorViewportClient::AbortTracking()
{
	if (TrackingTransaction.IsActive())
	{
		// Applying the global undo here will reset the drag operation
		if (GUndo)
		{
			GUndo->Apply();
		}
		TrackingTransaction.Cancel();
		StopTracking();
	}
}

void FLevelEditorViewportClient::HandleViewportSettingChanged(FName PropertyName)
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelEditorViewportSettings, bUseSelectionOutline))
	{
		EngineShowFlags.SetSelectionOutline(GetDefault<ULevelEditorViewportSettings>()->bUseSelectionOutline);
	}
}

void FLevelEditorViewportClient::OnActorMoved(AActor* InActor)
{
	// Update the cameras from their locked actor (if any)
	UpdateLockedActorViewport(InActor, false);
}

void FLevelEditorViewportClient::NudgeSelectedObjects( const struct FInputEventState& InputState )
{
	FViewport* InViewport = InputState.GetViewport();
	EInputEvent Event = InputState.GetInputEvent();
	FKey Key = InputState.GetKey();

	const int32 MouseX = InViewport->GetMouseX();
	const int32 MouseY = InViewport->GetMouseY();

	if( Event == IE_Pressed || Event == IE_Repeat )
	{
		// If this is a pressed event, start tracking.
		if ( !bIsTracking && Event == IE_Pressed )
		{
			// without the check for !bIsTracking, the following code would cause a new transaction to be created
			// for each "nudge" that occurred while the key was held down.  Disabling this code prevents the transaction
			// from being constantly recreated while as long as the key is held, so that the entire move is considered an atomic action (and
			// doing undo reverts the entire movement, as opposed to just the last nudge that occurred while the key was held down)
			MouseDeltaTracker->StartTracking( this, MouseX, MouseY, InputState, true );
			bIsTracking = true;
		}

		FIntPoint StartMousePos;
		InViewport->GetMousePos( StartMousePos );
		FKey VirtualKey = EKeys::MouseX;
		EAxisList::Type VirtualAxis = GetHorizAxis();
		float VirtualDelta = GEditor->GetGridSize() * (Key == EKeys::Left?-1:1);
		if( Key == EKeys::Up || Key == EKeys::Down )
		{
			VirtualKey = EKeys::MouseY;
			VirtualAxis = GetVertAxis();
			VirtualDelta = GEditor->GetGridSize() * (Key == EKeys::Up?1:-1);
		}

		bWidgetAxisControlledByDrag = false;
		Widget->SetCurrentAxis( VirtualAxis );
		MouseDeltaTracker->AddDelta( this, VirtualKey , VirtualDelta, 1 );
		Widget->SetCurrentAxis( VirtualAxis );
		UpdateMouseDelta();
		InViewport->SetMouse( StartMousePos.X , StartMousePos.Y );
	}
	else if( bIsTracking && Event == IE_Released )
	{
		bWidgetAxisControlledByDrag = false;
		MouseDeltaTracker->EndTracking( this );
		bIsTracking = false;
		Widget->SetCurrentAxis( EAxisList::None );
	}

	RedrawAllViewportsIntoThisScene();
}


/**
 * Returns the horizontal axis for this viewport.
 */

EAxisList::Type FLevelEditorViewportClient::GetHorizAxis() const
{
	switch( GetViewportType() )
	{
	case LVT_OrthoXY:
	case LVT_OrthoNegativeXY:
		return EAxisList::X;
	case LVT_OrthoXZ:
	case LVT_OrthoNegativeXZ:
		return EAxisList::X;
	case LVT_OrthoYZ:
	case LVT_OrthoNegativeYZ:
		return EAxisList::Y;
	case LVT_OrthoFreelook:
	case LVT_Perspective:
		break;
	}

	return EAxisList::X;
}

/**
 * Returns the vertical axis for this viewport.
 */

EAxisList::Type FLevelEditorViewportClient::GetVertAxis() const
{
	switch( GetViewportType() )
	{
	case LVT_OrthoXY:
	case LVT_OrthoNegativeXY:
		return EAxisList::Y;
	case LVT_OrthoXZ:
	case LVT_OrthoNegativeXZ:
		return EAxisList::Z;
	case LVT_OrthoYZ:
	case LVT_OrthoNegativeYZ:
		return EAxisList::Z;
	case LVT_OrthoFreelook:
	case LVT_Perspective:
		break;
	}

	return EAxisList::Y;
}

//
//	FLevelEditorViewportClient::InputAxis
//

/**
 * Sets the current level editing viewport client when created and stores the previous one
 * When destroyed it sets the current viewport client back to the previous one.
 */
struct FScopedSetCurrentViewportClient
{
	FScopedSetCurrentViewportClient( FLevelEditorViewportClient* NewCurrentViewport )
	{
		PrevCurrentLevelEditingViewportClient = GCurrentLevelEditingViewportClient;
		GCurrentLevelEditingViewportClient = NewCurrentViewport;
	}
	~FScopedSetCurrentViewportClient()
	{
		GCurrentLevelEditingViewportClient = PrevCurrentLevelEditingViewportClient;
	}
private:
	FLevelEditorViewportClient* PrevCurrentLevelEditingViewportClient;
};

bool FLevelEditorViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (bDisableInput)
	{
		return true;
	}

	// @todo Slate: GCurrentLevelEditingViewportClient is switched multiple times per frame and since we draw the border in slate this effectively causes border to always draw on the last viewport

	FScopedSetCurrentViewportClient( this );

	return FEditorViewportClient::InputAxis(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
}



static uint32 GetVolumeActorVisibilityId( const AActor& InActor )
{
	UClass* Class = InActor.GetClass();

	static TMap<UClass*, uint32 > ActorToIdMap;
	if( ActorToIdMap.Num() == 0 )
	{
		// Build a mapping of volume classes to ID's.  Do this only once
		TArray< UClass *> VolumeClasses;
		UUnrealEdEngine::GetSortedVolumeClasses(&VolumeClasses);
		for( int32 VolumeIdx = 0; VolumeIdx < VolumeClasses.Num(); ++VolumeIdx )
		{
			// An actors flag is just the index of the actor in the stored volume array shifted left to represent a unique bit.
			ActorToIdMap.Add( VolumeClasses[VolumeIdx], VolumeIdx );
		}
	}

	uint32* ActorID =  ActorToIdMap.Find( Class );

	// return 0 if the actor flag was not found, otherwise return the actual flag.  
	return ActorID ? *ActorID : 0;
}


/** 
 * Returns true if the passed in volume is visible in the viewport (due to volume actor visibility flags)
 *
 * @param Volume	The volume to check
 */
bool FLevelEditorViewportClient::IsVolumeVisibleInViewport( const AActor& VolumeActor ) const
{
	// We pass in the actor class for compatibility but we should make sure 
	// the function is only given volume actors
	//check( VolumeActor.IsA(AVolume::StaticClass()) );

	uint32 VolumeId = GetVolumeActorVisibilityId( VolumeActor );
	return VolumeActorVisibility[ VolumeId ];
}

void FLevelEditorViewportClient::RedrawAllViewportsIntoThisScene()
{
	// Invalidate all viewports, so the new gizmo is rendered in each one
	GEditor->RedrawLevelEditingViewports();
}

FWidget::EWidgetMode FLevelEditorViewportClient::GetWidgetMode() const
{
	if (GUnrealEd->ComponentVisManager.IsActive() && GUnrealEd->ComponentVisManager.IsVisualizingArchetype())
	{
		return FWidget::WM_None;
	}

	return FEditorViewportClient::GetWidgetMode();
}

FVector FLevelEditorViewportClient::GetWidgetLocation() const
{
	FVector ComponentVisWidgetLocation;
	if (GUnrealEd->ComponentVisManager.GetWidgetLocation(this, ComponentVisWidgetLocation))
	{
		return ComponentVisWidgetLocation;
	}

	return FEditorViewportClient::GetWidgetLocation();
}

FMatrix FLevelEditorViewportClient::GetWidgetCoordSystem() const 
{
	FMatrix ComponentVisWidgetCoordSystem;
	if (GUnrealEd->ComponentVisManager.GetCustomInputCoordinateSystem(this, ComponentVisWidgetCoordSystem))
	{
		return ComponentVisWidgetCoordSystem;
	}

	return FEditorViewportClient::GetWidgetCoordSystem();
}

void FLevelEditorViewportClient::MoveLockedActorToCamera()
{
	// If turned on, move any selected actors to the cameras location/rotation
	TWeakObjectPtr<AActor> ActiveActorLock = GetActiveActorLock();
	if( ActiveActorLock.IsValid() )
	{
		if ( !ActiveActorLock->bLockLocation )
		{
			ActiveActorLock->SetActorLocation(GCurrentLevelEditingViewportClient->GetViewLocation(), false);
			ActiveActorLock->SetActorRotation(GCurrentLevelEditingViewportClient->GetViewRotation());
		}

		ABrush* Brush = Cast< ABrush >( ActiveActorLock.Get() );
		if( Brush )
		{
			Brush->SetNeedRebuild( Brush->GetLevel() );
		}

		FScopedLevelDirtied LevelDirtyCallback;
		LevelDirtyCallback.Request();

		RedrawAllViewportsIntoThisScene();
	}
}

bool FLevelEditorViewportClient::HaveSelectedObjectsBeenChanged() const
{
	return ( TrackingTransaction.TransCount > 0 || TrackingTransaction.IsActive() ) && MouseDeltaTracker->HasReceivedDelta();
}

void FLevelEditorViewportClient::MoveCameraToLockedActor()
{
	// If turned on, move cameras location/rotation to the selected actors
	if( GetActiveActorLock().IsValid() )
	{
		SetViewLocation( GetActiveActorLock()->GetActorLocation() );
		SetViewRotation( GetActiveActorLock()->GetActorRotation() );
		Invalidate();
	}
}

USceneComponent* FLevelEditorViewportClient::FindViewComponentForActor(AActor const* Actor)
{
	USceneComponent* PreviewComponent = nullptr;
	if (Actor)
	{
		
		// see if actor has a component with preview capabilities (prioritize camera components)
		TArray<USceneComponent*> SceneComps;
		Actor->GetComponents<USceneComponent>(SceneComps);

		bool bChoseCamComponent = false;
		for (USceneComponent* Comp : SceneComps)
		{
			FMinimalViewInfo DummyViewInfo;
			if (Comp->bIsActive && Comp->GetEditorPreviewInfo(/*DeltaTime =*/0.0f, DummyViewInfo))
			{
				if (Comp->IsSelected())
				{
					PreviewComponent = Comp;
					break;
				}
				else if (PreviewComponent)
				{
					if (bChoseCamComponent)
					{
						continue;
					}

					UCameraComponent* AsCamComp = Cast<UCameraComponent>(Comp);
					if (AsCamComp != nullptr)
					{
						PreviewComponent = AsCamComp;
					}
					continue;
				}
				PreviewComponent = Comp;
			}
		}

		// now see if any actors are attached to us, directly or indirectly, that have an active camera component we might want to use
		// we will just return the first one.
		// #note: assumption here that attachment cannot be circular
		if (PreviewComponent == nullptr)
		{
			TArray<AActor*> AttachedActors;
			Actor->GetAttachedActors(AttachedActors);
			for (AActor* AttachedActor : AttachedActors)
			{
				USceneComponent* const Comp = FindViewComponentForActor(AttachedActor);
				if (Comp)
				{
					PreviewComponent = Comp;
					break;
				}
			}
		}
	}

	return PreviewComponent;
}

void FLevelEditorViewportClient::SetActorLock(AActor* Actor)
{
	if (ActorLockedToCamera != Actor)
	{
		SetIsCameraCut();
	}
	ActorLockedToCamera = Actor;
}

void FLevelEditorViewportClient::SetMatineeActorLock(AActor* Actor)
{
	if (ActorLockedByMatinee != Actor)
	{
		SetIsCameraCut();
	}
	ActorLockedByMatinee = Actor;
}

bool FLevelEditorViewportClient::IsActorLocked(const TWeakObjectPtr<AActor> InActor) const
{
	return (InActor.IsValid() && GetActiveActorLock() == InActor);
}

bool FLevelEditorViewportClient::IsAnyActorLocked() const
{
	return GetActiveActorLock().IsValid();
}

void FLevelEditorViewportClient::UpdateLockedActorViewports(const AActor* InActor, const bool bCheckRealtime)
{
	// Loop through all the other viewports, checking to see if the camera needs updating based on the locked actor
	for( int32 ViewportIndex = 0; ViewportIndex < GEditor->LevelViewportClients.Num(); ++ViewportIndex )
	{
		FLevelEditorViewportClient* Client = GEditor->LevelViewportClients[ViewportIndex];
		if( Client && Client != this )
		{
			Client->UpdateLockedActorViewport(InActor, bCheckRealtime);
		}
	}
}

void FLevelEditorViewportClient::UpdateLockedActorViewport(const AActor* InActor, const bool bCheckRealtime)
{
	// If this viewport has the actor locked and we need to update the camera, then do so
	if( IsActorLocked(InActor) && ( !bCheckRealtime || IsRealtime() ) )
	{
		MoveCameraToLockedActor();
	}
}


void FLevelEditorViewportClient::ApplyDeltaToActors(const FVector& InDrag,
													const FRotator& InRot,
													const FVector& InScale)
{
	if( (InDrag.IsZero() && InRot.IsZero() && InScale.IsZero()) )
	{
		return;
	}

	FVector ModifiedScale = InScale;
	// If we are scaling, we need to change the scaling factor a bit to properly align to grid.

	if( GEditor->UsePercentageBasedScaling() )
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		const bool bScalingActors = !InScale.IsNearlyZero();

		if( bScalingActors )
		{
			/* @todo: May reenable this form of calculating scaling factors later on.
			// Calculate a bounding box for the actors.
			FBox ActorsBoundingBox( 0 );

			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				const FBox ActorsBox = Actor->GetComponentsBoundingBox( true );
				ActorsBoundingBox += ActorsBox;
			}

			const FVector BoxExtent = ActorsBoundingBox.GetExtent();

			for (int32 Idx=0; Idx < 3; Idx++)
			{
				ModifiedScale[Idx] = InScale[Idx] / BoxExtent[Idx];
			}
			*/

			ModifiedScale = InScale * ((GEditor->GetScaleGridSize() / 100.0f) / GEditor->GetGridSize());
		}
	}

	// Transact the actors.
	GEditor->NoteActorMovement();

	TArray<AGroupActor*> ActorGroups;

	// Apply the deltas to any selected actors.
	for ( FSelectionIterator SelectedActorIt( GEditor->GetSelectedActorIterator() ) ; SelectedActorIt ; ++SelectedActorIt )
	{
		AActor* Actor = static_cast<AActor*>( *SelectedActorIt );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Verify that the actor is in the same world as the viewport before moving it.
		if(GEditor->PlayWorld)
		{
			if(bIsSimulateInEditorViewport)
			{
				// If the Actor's outer (level) outer (world) is not the PlayWorld then it cannot be moved in this viewport.
				if( !(GEditor->PlayWorld == Actor->GetWorld()) )
				{
					continue;
				}
			}
			else if( !(GEditor->EditorWorld == Actor->GetWorld()) )
			{
				continue;
			}
		}

		if ( !Actor->bLockLocation )
		{
			if (GEditor->GetSelectedComponentCount() > 0)
			{
				USelection* ComponentSelection = GEditor->GetSelectedComponents();

				// Only move the parent-most component(s) that are selected 
				// Otherwise, if both a parent and child are selected and the delta is applied to both, the child will actually move 2x delta
				TInlineComponentArray<USceneComponent*> ComponentsToMove;
				for (FSelectedEditableComponentIterator EditableComponentIt(GEditor->GetSelectedEditableComponentIterator()); EditableComponentIt; ++EditableComponentIt)
				{
					USceneComponent* SceneComponent = CastChecked<USceneComponent>(*EditableComponentIt);
					if (SceneComponent)
					{
						USceneComponent* SelectedComponent = Cast<USceneComponent>(*EditableComponentIt);

						// Check to see if any parent is selected
						bool bParentAlsoSelected = false;
						USceneComponent* Parent = SelectedComponent->GetAttachParent();
						while (Parent != nullptr)
						{
							if (ComponentSelection->IsSelected(Parent))
							{
								bParentAlsoSelected = true;
								break;
							}

							Parent = Parent->GetAttachParent();
						}

						// If no parent of this component is also in the selection set, move it!
						if (!bParentAlsoSelected)
						{
							ComponentsToMove.Add(SelectedComponent);
						}
					}	
				}

				// Now actually apply the delta to the appropriate component(s)
				for (USceneComponent* SceneComp : ComponentsToMove)
				{
					ApplyDeltaToComponent(SceneComp, InDrag, InRot, ModifiedScale);
				}
			}
			else
			{
				AGroupActor* ParentGroup = AGroupActor::GetRootForActor(Actor, true, true);
				if (ParentGroup && UActorGroupingUtils::IsGroupingActive())
				{
					ActorGroups.AddUnique(ParentGroup);
				}
				else
				{
					// Finally, verify that no actor in the parent hierarchy is also selected
					bool bHasParentInSelection = false;
					AActor* ParentActor = Actor->GetAttachParentActor();
					while (ParentActor != NULL && !bHasParentInSelection)
					{
						if (ParentActor->IsSelected())
						{
							bHasParentInSelection = true;
						}
						ParentActor = ParentActor->GetAttachParentActor();
					}
					if (!bHasParentInSelection)
					{
						ApplyDeltaToActor(Actor, InDrag, InRot, ModifiedScale);
					}
				}
			}
		}
	}
	AGroupActor::RemoveSubGroupsFromArray(ActorGroups);
	for(int32 ActorGroupsIndex=0; ActorGroupsIndex<ActorGroups.Num(); ++ActorGroupsIndex)
	{
		ActorGroups[ActorGroupsIndex]->GroupApplyDelta(this, InDrag, InRot, ModifiedScale);
	}
}

void FLevelEditorViewportClient::ApplyDeltaToComponent(USceneComponent* InComponent, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale)
{
	// If we are scaling, we need to change the scaling factor a bit to properly align to grid.
	FVector ModifiedDeltaScale = InDeltaScale;

	// we don't scale components when we only have a very small scale change
	if (!InDeltaScale.IsNearlyZero())
	{
		if (!GEditor->UsePercentageBasedScaling())
		{
			ModifyScale(InComponent, ModifiedDeltaScale);
		}
	}
	else
	{
		ModifiedDeltaScale = FVector::ZeroVector;
	}

	FVector AdjustedDrag = InDeltaDrag;
	FRotator AdjustedRot = InDeltaRot;
	FComponentEditorUtils::AdjustComponentDelta(InComponent, AdjustedDrag, AdjustedRot);

	FVector EditorWorldPivotLocation = GEditor->GetPivotLocation();

	// If necessary, transform the editor pivot location to be relative to the component's parent
	const bool bIsRootComponent = InComponent->GetOwner()->GetRootComponent() == InComponent;
	FVector RelativePivotLocation = bIsRootComponent || !InComponent->GetAttachParent() ? EditorWorldPivotLocation : InComponent->GetAttachParent()->GetComponentToWorld().Inverse().TransformPosition(EditorWorldPivotLocation);

	GEditor->ApplyDeltaToComponent(
		InComponent,
		true,
		&AdjustedDrag,
		&AdjustedRot,
		&ModifiedDeltaScale,
		RelativePivotLocation);
}

/** Helper function for ModifyScale - Convert the active Dragging Axis to per-axis flags */
static void CheckActiveAxes( EAxisList::Type DraggingAxis, bool bActiveAxes[3] )
{
	bActiveAxes[0] = bActiveAxes[1] = bActiveAxes[2] = false;
	switch(DraggingAxis)
	{
	default:
	case EAxisList::None:
		break;
	case EAxisList::X:
		bActiveAxes[0] = true;
		break;
	case EAxisList::Y:
		bActiveAxes[1] = true;
		break;
	case EAxisList::Z:
		bActiveAxes[2] = true;
		break;
	case EAxisList::XYZ:
	case EAxisList::All:
	case EAxisList::Screen:
		bActiveAxes[0] = bActiveAxes[1] = bActiveAxes[2] = true;
		break;
	case EAxisList::XY:
		bActiveAxes[0] = bActiveAxes[1] = true;
		break;
	case EAxisList::XZ:
		bActiveAxes[0] = bActiveAxes[2] = true;
		break;
	case EAxisList::YZ:
		bActiveAxes[1] = bActiveAxes[2] = true;
		break;
	}
}

/** Helper function for ModifyScale - Check scale criteria to see if this is allowed, returns modified absolute scale*/
static float CheckScaleValue( float ScaleDeltaToCheck, float CurrentScaleFactor, float CurrentExtent, bool bCheckSmallExtent, bool bSnap )
{
	float AbsoluteScaleValue = ScaleDeltaToCheck + CurrentScaleFactor;
	if( bSnap )
	{
		AbsoluteScaleValue = FMath::GridSnap( AbsoluteScaleValue, GEditor->GetScaleGridSize() );
	}
	// In some situations CurrentExtent can be 0 (eg: when scaling a plane in Z), this causes a divide by 0 that we need to avoid.
	if(FMath::Abs(CurrentExtent) < KINDA_SMALL_NUMBER) {
		return AbsoluteScaleValue;
	}
	float UnscaledExtent = CurrentExtent / CurrentScaleFactor;
	float ScaledExtent = UnscaledExtent * AbsoluteScaleValue;

	if( ( FMath::Square( ScaledExtent ) ) > BIG_NUMBER )	// cant get too big...
	{
		return CurrentScaleFactor;
	}
	else if( bCheckSmallExtent && 
			(FMath::Abs( ScaledExtent ) < MIN_ACTOR_BOUNDS_EXTENT * 0.5f ||		// ...or too small (apply sign in this case)...
			( CurrentScaleFactor < 0.0f ) != ( AbsoluteScaleValue < 0.0f )) )	// ...also cant cross the zero boundary
	{
		return ( ( MIN_ACTOR_BOUNDS_EXTENT * 0.5 ) / UnscaledExtent ) * ( CurrentScaleFactor < 0.0f ? -1.0f : 1.0f );
	}

	return AbsoluteScaleValue;
}

/**
* Helper function for ValidateScale().
* If the "PreserveNonUniformScale" setting is enabled, this function will appropriately re-scale the scale delta so that
* proportions are preserved also when snapping.
* This function will modify the scale delta sign so that scaling is apply in the good direction when using multiple axis in same time.
* The function will not transform the scale delta in the case the scale delta is not uniform
* @param    InOriginalPreDragScale		The object's original scale
* @param	bActiveAxes					The axes that are active when scaling interactively.
* @param	InOutScaleDelta				The scale delta we are potentially transforming.
* @return true if the axes should be snapped individually, according to the snap setting (i.e. this function had no effect)
*/
static bool ApplyScalingOptions(const FVector& InOriginalPreDragScale, const bool bActiveAxes[3], FVector& InOutScaleDelta)
{
	int ActiveAxisCount = 0;
	bool CurrentValueSameSign = true;
	bool FirstSignPositive = true;
	float MaxComponentSum = -1.0f;
	int32 MaxAxisIndex = -1;
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	bool SnapScaleAfter = ViewportSettings->SnapScaleEnabled;

	//Found the number of active axis
	//Found if we have to swap some sign
	for (int Axis = 0; Axis < 3; ++Axis)
	{
		if (bActiveAxes[Axis])
		{
			bool CurrentValueIsZero = FMath::IsNearlyZero(InOriginalPreDragScale[Axis], SMALL_NUMBER);
			//when the current value is zero we assume it is positive
			bool IsCurrentValueSignPositive = CurrentValueIsZero ? true : InOriginalPreDragScale[Axis] > 0.0f;
			if (ActiveAxisCount == 0)
			{
				//Set the first value when we find the first active axis
				FirstSignPositive = IsCurrentValueSignPositive;
			}
			else
			{
				if (FirstSignPositive != IsCurrentValueSignPositive)
				{
					CurrentValueSameSign = false;
				}
			}
			ActiveAxisCount++;
		}
	}

	//If we scale more then one axis and
	//we have to swap some sign
	if (ActiveAxisCount > 1 && !CurrentValueSameSign)
	{
		//Change the scale delta to reflect the sign of the value
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			if (bActiveAxes[Axis])
			{
				bool CurrentValueIsZero = FMath::IsNearlyZero(InOriginalPreDragScale[Axis], SMALL_NUMBER);
				//when the current value is zero we assume it is positive
				bool IsCurrentValueSignPositive = CurrentValueIsZero ? true : InOriginalPreDragScale[Axis] > 0.0f;
				InOutScaleDelta[Axis] = IsCurrentValueSignPositive ? InOutScaleDelta[Axis] : -(InOutScaleDelta[Axis]);
			}
		}
	}

	if (ViewportSettings->PreserveNonUniformScale)
	{
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			if (bActiveAxes[Axis])
			{
				const float AbsScale = FMath::Abs(InOutScaleDelta[Axis] + InOriginalPreDragScale[Axis]);
				if (AbsScale > MaxComponentSum)
				{
					MaxAxisIndex = Axis;
					MaxComponentSum = AbsScale;
				}
			}
		}

		check(MaxAxisIndex != -1);

		float AbsoluteScaleValue = InOriginalPreDragScale[MaxAxisIndex] + InOutScaleDelta[MaxAxisIndex];
		if (ViewportSettings->SnapScaleEnabled)
		{
			AbsoluteScaleValue = FMath::GridSnap(InOriginalPreDragScale[MaxAxisIndex] + InOutScaleDelta[MaxAxisIndex], GEditor->GetScaleGridSize());
			SnapScaleAfter = false;
		}

		float ScaleRatioMax = AbsoluteScaleValue / InOriginalPreDragScale[MaxAxisIndex];
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			if (bActiveAxes[Axis])
			{
				InOutScaleDelta[Axis] = (InOriginalPreDragScale[Axis] * ScaleRatioMax) - InOriginalPreDragScale[Axis];
			}
		}
	}

	
	return SnapScaleAfter;
}


/** Helper function for ModifyScale - Check scale criteria to see if this is allowed */
void FLevelEditorViewportClient::ValidateScale(const FVector& InOriginalPreDragScale, const FVector& InCurrentScale, const FVector& InBoxExtent, FVector& InOutScaleDelta, bool bInCheckSmallExtent ) const
{
	// get the axes that are active in this operation
	bool bActiveAxes[3];
	CheckActiveAxes( Widget != NULL ? Widget->GetCurrentAxis() : EAxisList::None, bActiveAxes );

	//When scaling with more then one active axis, We must make sure we apply the correct delta sign to each delta scale axis
	//We also want to support the PreserveNonUniformScale option
	bool bSnapAxes = ApplyScalingOptions(InOriginalPreDragScale, bActiveAxes, InOutScaleDelta);

	// check each axis
	for( int Axis = 0; Axis < 3; ++Axis )
	{
		if( bActiveAxes[Axis] ) 
		{
			float ModifiedScaleAbsolute = CheckScaleValue( InOutScaleDelta[Axis], InCurrentScale[Axis], InBoxExtent[Axis], bInCheckSmallExtent, bSnapAxes );
			InOutScaleDelta[Axis] = ModifiedScaleAbsolute - InCurrentScale[Axis];
		}
		else
		{
			InOutScaleDelta[Axis] = 0.0f;
		}
	}
}

void FLevelEditorViewportClient::ModifyScale( AActor* InActor, FVector& ScaleDelta, bool bCheckSmallExtent ) const
{
	if( InActor->GetRootComponent() )
	{
		const FVector CurrentScale = InActor->GetRootComponent()->RelativeScale3D;

		const FBox LocalBox = InActor->GetComponentsBoundingBox( true );
		const FVector ScaledExtents = LocalBox.GetExtent() * CurrentScale;
		const FTransform* PreDragTransform = PreDragActorTransforms.Find(InActor);
		//In scale mode we need the predrag transform before the first delta calculation
		if (PreDragTransform == nullptr)
		{
			PreDragTransform = &PreDragActorTransforms.Add(InActor, InActor->GetTransform());
		}
		check(PreDragTransform);
		ValidateScale(PreDragTransform->GetScale3D(), CurrentScale, ScaledExtents, ScaleDelta, bCheckSmallExtent);

		if( ScaleDelta.IsNearlyZero() )
		{
			ScaleDelta = FVector::ZeroVector;
		}
	}
}

void FLevelEditorViewportClient::ModifyScale( USceneComponent* InComponent, FVector& ScaleDelta ) const
{
	AActor* Actor = InComponent->GetOwner();
	const FTransform* PreDragTransform = PreDragActorTransforms.Find(Actor);
	//In scale mode we need the predrag transform before the first delta calculation
	if (PreDragTransform == nullptr)
	{
		PreDragTransform = &PreDragActorTransforms.Add(Actor, Actor->GetTransform());
	}
	check(PreDragTransform);
	const FBox LocalBox = Actor->GetComponentsBoundingBox(true);
	const FVector ScaledExtents = LocalBox.GetExtent() * InComponent->RelativeScale3D;
	ValidateScale(PreDragTransform->GetScale3D(), InComponent->RelativeScale3D, ScaledExtents, ScaleDelta);

	if( ScaleDelta.IsNearlyZero() )
	{
		ScaleDelta = FVector::ZeroVector;
	}
}

//
//	FLevelEditorViewportClient::ApplyDeltaToActor
//

void FLevelEditorViewportClient::ApplyDeltaToActor( AActor* InActor, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale )
{
	// If we are scaling, we may need to change the scaling factor a bit to properly align to the grid.
	FVector ModifiedDeltaScale = InDeltaScale;

	// we dont scale actors when we only have a very small scale change
	if( !InDeltaScale.IsNearlyZero() )
	{
		if(!GEditor->UsePercentageBasedScaling())
		{
			ModifyScale( InActor, ModifiedDeltaScale, Cast< ABrush >( InActor ) != NULL );
		}
	}
	else
	{
		ModifiedDeltaScale = FVector::ZeroVector;
	}

	GEditor->ApplyDeltaToActor(
		InActor,
		true,
		&InDeltaDrag,
		&InDeltaRot,
		&ModifiedDeltaScale,
		IsAltPressed(),
		IsShiftPressed(),
		IsCtrlPressed() );

	// Update the cameras from their locked actor (if any) only if the viewport is realtime enabled
	UpdateLockedActorViewports(InActor, true);
}

EMouseCursor::Type FLevelEditorViewportClient::GetCursor(FViewport* InViewport,int32 X,int32 Y)
{
	EMouseCursor::Type CursorType = FEditorViewportClient::GetCursor(InViewport,X,Y);

	HHitProxy* HitProxy = InViewport->GetHitProxy(X,Y);

	// Don't select widget axes by mouse over while they're being controlled by a mouse drag.
	if( InViewport->IsCursorVisible() && !bWidgetAxisControlledByDrag && !HitProxy )
	{
		if( HoveredObjects.Num() > 0 )
		{
			ClearHoverFromObjects();
			Invalidate( false, false );
		}
	}

	return CursorType;

}

FViewportCursorLocation FLevelEditorViewportClient::GetCursorWorldLocationFromMousePos()
{
	// Create the scene view context
	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
		Viewport, 
		GetScene(),
		EngineShowFlags )
		.SetRealtimeUpdate( IsRealtime() ));

	// Calculate the scene view
	FSceneView* View = CalcSceneView( &ViewFamily );

	// Construct an FViewportCursorLocation which calculates world space postion from the scene view and mouse pos.
	return FViewportCursorLocation( View, 
		this, 
		Viewport->GetMouseX(), 
		Viewport->GetMouseY()
		);
}



/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewport	Viewport that captured the mouse input
 * @param	InMouseX	New mouse cursor X coordinate
 * @param	InMouseY	New mouse cursor Y coordinate
 */
void FLevelEditorViewportClient::CapturedMouseMove( FViewport* InViewport, int32 InMouseX, int32 InMouseY )
{
	// Commit to any pending transactions now
	TrackingTransaction.PromotePendingToActive();

	FEditorViewportClient::CapturedMouseMove(InViewport, InMouseX, InMouseY);
}


/**
 * Checks if the mouse is hovered over a hit proxy and decides what to do.
 */
void FLevelEditorViewportClient::CheckHoveredHitProxy( HHitProxy* HoveredHitProxy )
{
	FEditorViewportClient::CheckHoveredHitProxy(HoveredHitProxy);

	// We'll keep track of changes to hovered objects as the cursor moves
	const bool bUseHoverFeedback = GEditor != NULL && GetDefault<ULevelEditorViewportSettings>()->bEnableViewportHoverFeedback;
	TSet< FViewportHoverTarget > NewHoveredObjects;

	// If the cursor is visible over level viewports, then we'll check for new objects to be hovered over
	if( bUseHoverFeedback && HoveredHitProxy )
	{
		// Set mouse hover cue for objects under the cursor
		if (HoveredHitProxy->IsA(HActor::StaticGetType()) || HoveredHitProxy->IsA(HBSPBrushVert::StaticGetType()))
		{
			// Hovered over an actor
			AActor* ActorUnderCursor = NULL;
			if (HoveredHitProxy->IsA(HActor::StaticGetType()))
			{
				HActor* ActorHitProxy = static_cast<HActor*>(HoveredHitProxy);
				ActorUnderCursor = ActorHitProxy->Actor;
			}
			else if (HoveredHitProxy->IsA(HBSPBrushVert::StaticGetType()))
			{
				HBSPBrushVert* ActorHitProxy = static_cast<HBSPBrushVert*>(HoveredHitProxy);
				ActorUnderCursor = ActorHitProxy->Brush.Get();
			}

			if( ActorUnderCursor != NULL  )
			{
				// Check to see if the actor under the cursor is part of a group.  If so, we will how a hover cue the whole group
				AGroupActor* GroupActor = AGroupActor::GetRootForActor( ActorUnderCursor, true, false );

				if(GroupActor && UActorGroupingUtils::IsGroupingActive())
				{
					// Get all the actors in the group and add them to the list of objects to show a hover cue for.
					TArray<AActor*> ActorsInGroup;
					GroupActor->GetGroupActors( ActorsInGroup, true );
					for( int32 ActorIndex = 0; ActorIndex < ActorsInGroup.Num(); ++ActorIndex )
					{
						NewHoveredObjects.Add( FViewportHoverTarget( ActorsInGroup[ActorIndex] ) );
					}
				}
				else
				{
					NewHoveredObjects.Add( FViewportHoverTarget( ActorUnderCursor ) );
				}
			}
		}
		else if( HoveredHitProxy->IsA( HModel::StaticGetType() ) )
		{
			// Hovered over a model (BSP surface)
			HModel* ModelHitProxy = static_cast< HModel* >( HoveredHitProxy );
			UModel* ModelUnderCursor = ModelHitProxy->GetModel();
			if( ModelUnderCursor != NULL )
			{
				FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
					Viewport, 
					GetScene(),
					EngineShowFlags)
					.SetRealtimeUpdate( IsRealtime() ));
				FSceneView* SceneView = CalcSceneView( &ViewFamily );

				uint32 SurfaceIndex = INDEX_NONE;
				if( ModelHitProxy->ResolveSurface( SceneView, CachedMouseX, CachedMouseY, SurfaceIndex ) )
				{
					FBspSurf& Surf = ModelUnderCursor->Surfs[ SurfaceIndex ];
					Surf.PolyFlags |= PF_Hovered;

					NewHoveredObjects.Add( FViewportHoverTarget( ModelUnderCursor, SurfaceIndex ) );
				}
			}
		}
	}

	UpdateHoveredObjects( NewHoveredObjects );
}

void FLevelEditorViewportClient::UpdateHoveredObjects( const TSet<FViewportHoverTarget>& NewHoveredObjects )
{
	// Check to see if there are any hovered objects that need to be updated
	{
		bool bAnyHoverChanges = false;
		if( NewHoveredObjects.Num() > 0 )
		{
			for( TSet<FViewportHoverTarget>::TIterator It( HoveredObjects ); It; ++It )
			{
				FViewportHoverTarget& OldHoverTarget = *It;
				if( !NewHoveredObjects.Contains( OldHoverTarget ) )
				{
					// Remove hover effect from object that no longer needs it
					RemoveHoverEffect( OldHoverTarget );
					HoveredObjects.Remove( OldHoverTarget );

					bAnyHoverChanges = true;
				}
			}
		}

		for( TSet<FViewportHoverTarget>::TConstIterator It( NewHoveredObjects ); It; ++It )
		{
			const FViewportHoverTarget& NewHoverTarget = *It;
			if( !HoveredObjects.Contains( NewHoverTarget ) )
			{
				// Add hover effect to this object
				AddHoverEffect( NewHoverTarget );
				HoveredObjects.Add( NewHoverTarget );

				bAnyHoverChanges = true;
			}
		}


		// Redraw the viewport if we need to
		if( bAnyHoverChanges )
		{
			// NOTE: We're only redrawing the viewport that the mouse is over.  We *could* redraw all viewports
			//		 so the hover effect could be seen in all potential views, but it will be slower.
			RedrawRequested( Viewport );
		}
	}
}

bool FLevelEditorViewportClient::GetActiveSafeFrame(float& OutAspectRatio) const
{
	if (!IsOrtho())
	{
		const UCameraComponent* CameraComponent = GetCameraComponentForView();
		if (CameraComponent && CameraComponent->bConstrainAspectRatio)
		{
			OutAspectRatio = CameraComponent->AspectRatio;
			return true;
		}
	}

	return false;
}

/** 
 * Renders a view frustum specified by the provided frustum parameters
 *
 * @param	PDI					PrimitiveDrawInterface to use to draw the view frustum
 * @param	FrustumColor		Color to draw the view frustum in
 * @param	FrustumAngle		Angle of the frustum
 * @param	FrustumAspectRatio	Aspect ratio of the frustum
 * @param	FrustumStartDist	Start distance of the frustum
 * @param	FrustumEndDist		End distance of the frustum
 * @param	InViewMatrix		View matrix to use to draw the frustum
 */
static void RenderViewFrustum( FPrimitiveDrawInterface* PDI,
								const FLinearColor& FrustumColor,
								float FrustumAngle,
								float FrustumAspectRatio,
								float FrustumStartDist,
								float FrustumEndDist,
								const FMatrix& InViewMatrix)
{
	FVector Direction(0,0,1);
	FVector LeftVector(1,0,0);
	FVector UpVector(0,1,0);

	FVector Verts[8];

	// FOVAngle controls the horizontal angle.
	float HozHalfAngle = (FrustumAngle) * ((float)PI/360.f);
	float HozLength = FrustumStartDist * FMath::Tan(HozHalfAngle);
	float VertLength = HozLength/FrustumAspectRatio;

	// near plane verts
	Verts[0] = (Direction * FrustumStartDist) + (UpVector * VertLength) + (LeftVector * HozLength);
	Verts[1] = (Direction * FrustumStartDist) + (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[2] = (Direction * FrustumStartDist) - (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[3] = (Direction * FrustumStartDist) - (UpVector * VertLength) + (LeftVector * HozLength);

	HozLength = FrustumEndDist * FMath::Tan(HozHalfAngle);
	VertLength = HozLength/FrustumAspectRatio;

	// far plane verts
	Verts[4] = (Direction * FrustumEndDist) + (UpVector * VertLength) + (LeftVector * HozLength);
	Verts[5] = (Direction * FrustumEndDist) + (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[6] = (Direction * FrustumEndDist) - (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[7] = (Direction * FrustumEndDist) - (UpVector * VertLength) + (LeftVector * HozLength);

	for( int32 x = 0 ; x < 8 ; ++x )
	{
		Verts[x] = InViewMatrix.InverseFast().TransformPosition( Verts[x] );
	}

	const uint8 PrimitiveDPG = SDPG_Foreground;
	PDI->DrawLine( Verts[0], Verts[1], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[1], Verts[2], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[2], Verts[3], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[3], Verts[0], FrustumColor, PrimitiveDPG );

	PDI->DrawLine( Verts[4], Verts[5], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[5], Verts[6], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[6], Verts[7], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[7], Verts[4], FrustumColor, PrimitiveDPG );

	PDI->DrawLine( Verts[0], Verts[4], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[1], Verts[5], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[2], Verts[6], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[3], Verts[7], FrustumColor, PrimitiveDPG );
}

void FLevelEditorViewportClient::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	FMemMark Mark(FMemStack::Get());

	FEditorViewportClient::Draw(View,PDI);

	DrawBrushDetails(View, PDI);
	AGroupActor::DrawBracketsForGroups(PDI, Viewport);

	if (EngineShowFlags.StreamingBounds)
	{
		DrawTextureStreamingBounds(View, PDI);
	}

	// Determine if a view frustum should be rendered in the viewport.
	// The frustum should definitely be rendered if the viewport has a view parent.
	bool bRenderViewFrustum = ViewState.GetReference()->HasViewParent();

	// If the viewport doesn't have a view parent, a frustum still should be drawn anyway if the viewport is ortho and level streaming
	// volume previs is enabled in some viewport
	if ( !bRenderViewFrustum && IsOrtho() )
	{
		for ( int32 ViewportClientIndex = 0; ViewportClientIndex < GEditor->LevelViewportClients.Num(); ++ViewportClientIndex )
		{
			const FLevelEditorViewportClient* CurViewportClient = GEditor->LevelViewportClients[ ViewportClientIndex ];
			if ( CurViewportClient && IsPerspective() && GetDefault<ULevelEditorViewportSettings>()->bLevelStreamingVolumePrevis )
			{
				bRenderViewFrustum = true;
				break;
			}
		}
	}

	// Draw the view frustum of the view parent or level streaming volume previs viewport, if necessary
	if ( bRenderViewFrustum )
	{
		RenderViewFrustum( PDI, FLinearColor(1.0,0.0,1.0,1.0),
			GPerspFrustumAngle,
			GPerspFrustumAspectRatio,
			GPerspFrustumStartDist,
			GPerspFrustumEndDist,
			GPerspViewMatrix);
	}


	if (IsPerspective())
	{
		DrawStaticLightingDebugInfo( View, PDI );
	}

	if ( GEditor->bEnableSocketSnapping )
	{
		const bool bGameViewMode = View->Family->EngineShowFlags.Game && !GEditor->bDrawSocketsInGMode;

		for( FActorIterator It(GetWorld()); It; ++It )
		{
			AActor* Actor = *It;

			if (bGameViewMode || Actor->IsHiddenEd())
			{
				// Don't display sockets on hidden actors...
				continue;
			}

			TInlineComponentArray<USceneComponent*> Components;
			Actor->GetComponents(Components);

			for (int32 ComponentIndex = 0 ; ComponentIndex < Components.Num(); ++ComponentIndex)
			{
				USceneComponent* SceneComponent = Components[ComponentIndex];
				if (SceneComponent->HasAnySockets())
				{
					TArray<FComponentSocketDescription> Sockets;
					SceneComponent->QuerySupportedSockets(Sockets);

					for (int32 SocketIndex = 0; SocketIndex < Sockets.Num() ; ++SocketIndex)
					{
						FComponentSocketDescription& Socket = Sockets[SocketIndex];

						if (Socket.Type == EComponentSocketType::Socket)
						{
							const FTransform SocketTransform = SceneComponent->GetSocketTransform(Socket.Name);

							const float DiamondSize = 2.0f;
							const FColor DiamondColor(255,128,128);

							PDI->SetHitProxy( new HLevelSocketProxy( *It, SceneComponent, Socket.Name ) );
							DrawWireDiamond( PDI, SocketTransform.ToMatrixWithScale(), DiamondSize, DiamondColor, SDPG_Foreground );
							PDI->SetHitProxy( NULL );
						}
					}
				}
			}
		}
	}

	if( this == GCurrentLevelEditingViewportClient )
	{
		FSnappingUtils::DrawSnappingHelpers( View, PDI );
	}

	if(GUnrealEd != NULL && !IsInGameView())
	{
		GUnrealEd->DrawComponentVisualizers(View, PDI);
	}

	if (GEditor->bDrawParticleHelpers == true)
	{
		if (View->Family->EngineShowFlags.Game)
		{
			extern ENGINE_API void DrawParticleSystemHelpers(const FSceneView* View,FPrimitiveDrawInterface* PDI);
			DrawParticleSystemHelpers(View, PDI);
		}
	}

	Mark.Pop();
}

void FLevelEditorViewportClient::DrawBrushDetails(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (GEditor->bShowBrushMarkerPolys)
	{
		// Draw translucent polygons on brushes and volumes

		for (TActorIterator<ABrush> It(GetWorld()); It; ++It)
		{
			ABrush* Brush = *It;

			// Brush->Brush is checked to safe from brushes that were created without having their brush members attached.
			if (Brush->Brush && (FActorEditorUtils::IsABuilderBrush(Brush) || Brush->IsVolumeBrush()) && ModeTools->GetSelectedActors()->IsSelected(Brush))
			{
				// Build a mesh by basically drawing the triangles of each 
				FDynamicMeshBuilder MeshBuilder;
				int32 VertexOffset = 0;

				for (int32 PolyIdx = 0; PolyIdx < Brush->Brush->Polys->Element.Num(); ++PolyIdx)
				{
					const FPoly* Poly = &Brush->Brush->Polys->Element[PolyIdx];

					if (Poly->Vertices.Num() > 2)
					{
						const FVector Vertex0 = Poly->Vertices[0];
						FVector Vertex1 = Poly->Vertices[1];

						MeshBuilder.AddVertex(Vertex0, FVector2D::ZeroVector, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
						MeshBuilder.AddVertex(Vertex1, FVector2D::ZeroVector, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);

						for (int32 VertexIdx = 2; VertexIdx < Poly->Vertices.Num(); ++VertexIdx)
						{
							const FVector Vertex2 = Poly->Vertices[VertexIdx];
							MeshBuilder.AddVertex(Vertex2, FVector2D::ZeroVector, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							MeshBuilder.AddTriangle(VertexOffset, VertexOffset + VertexIdx, VertexOffset + VertexIdx - 1);
							Vertex1 = Vertex2;
						}

						// Increment the vertex offset so the next polygon uses the correct vertex indices.
						VertexOffset += Poly->Vertices.Num();
					}
				}

				// Allocate the material proxy and register it so it can be deleted properly once the rendering is done with it.
				FDynamicColoredMaterialRenderProxy* MaterialProxy = new FDynamicColoredMaterialRenderProxy(GEngine->EditorBrushMaterial->GetRenderProxy(false), Brush->GetWireColor());
				PDI->RegisterDynamicResource(MaterialProxy);

				// Flush the mesh triangles.
				MeshBuilder.Draw(PDI, Brush->ActorToWorld().ToMatrixWithScale(), MaterialProxy, SDPG_World, 0.f);
			}
		}
	}
	
	if (ModeTools->ShouldDrawBrushVertices() && !IsInGameView())
	{
		UTexture2D* VertexTexture = GEngine->DefaultBSPVertexTexture;
		const float TextureSizeX = VertexTexture->GetSizeX() * 0.170f;
		const float TextureSizeY = VertexTexture->GetSizeY() * 0.170f;

		USelection* Selection = ModeTools->GetSelectedActors();
		if(Selection->IsClassSelected(ABrush::StaticClass()))
		{
			for (FSelectionIterator It(*ModeTools->GetSelectedActors()); It; ++It)
			{
				ABrush* Brush = Cast<ABrush>(*It);
				if(Brush && Brush->Brush && !FActorEditorUtils::IsABuilderBrush(Brush))
				{
					for(int32 p = 0; p < Brush->Brush->Polys->Element.Num(); ++p)
					{
						FTransform BrushTransform = Brush->ActorToWorld();

						FPoly* poly = &Brush->Brush->Polys->Element[p];
						for(int32 VertexIndex = 0; VertexIndex < poly->Vertices.Num(); ++VertexIndex)
						{
							const FVector& PolyVertex = poly->Vertices[VertexIndex];
							const FVector WorldLocation = BrushTransform.TransformPosition(PolyVertex);

							const float Scale = View->WorldToScreen(WorldLocation).W * (4.0f / View->ViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0]);

							const FColor Color(Brush->GetWireColor());
							PDI->SetHitProxy(new HBSPBrushVert(Brush, &poly->Vertices[VertexIndex]));

							PDI->DrawSprite(WorldLocation, TextureSizeX * Scale, TextureSizeY * Scale, VertexTexture->Resource, Color, SDPG_World, 0.0f, 0.0f, 0.0f, 0.0f, SE_BLEND_Masked);

							PDI->SetHitProxy(NULL);
						}
					}
				}
			}
		}
	}
}

void FLevelEditorViewportClient::UpdateAudioListener(const FSceneView& View)
{
	UWorld* ViewportWorld = GetWorld();

	if (ViewportWorld)
	{
		if (FAudioDevice* AudioDevice = ViewportWorld->GetAudioDevice())
		{
			FVector ViewLocation = GetViewLocation();

			const bool bStereoRendering = GEngine->XRSystem.IsValid() && GEngine->IsStereoscopic3D( Viewport );
			if( bStereoRendering && GEngine->XRSystem->IsHeadTrackingAllowed() )
			{
				FQuat RoomSpaceHeadOrientation;
				FVector RoomSpaceHeadLocation;
				GEngine->XRSystem->GetCurrentPose( IXRTrackingSystem::HMDDeviceId, /* Out */ RoomSpaceHeadOrientation, /* Out */ RoomSpaceHeadLocation );

				// NOTE: The RoomSpaceHeadLocation has already been adjusted for WorldToMetersScale
				const FVector WorldSpaceHeadLocation = GetViewLocation() + GetViewRotation().RotateVector( RoomSpaceHeadLocation );
				ViewLocation = WorldSpaceHeadLocation;
			}

			const FRotator& ViewRotation = GetViewRotation();

			FTransform ListenerTransform(ViewRotation);
			ListenerTransform.SetLocation(ViewLocation);

			AudioDevice->SetListener(ViewportWorld, 0, ListenerTransform, 0.f);
		}
	}
}

void FLevelEditorViewportClient::SetupViewForRendering( FSceneViewFamily& ViewFamily, FSceneView& View )
{
	FEditorViewportClient::SetupViewForRendering( ViewFamily, View );

	ViewFamily.bDrawBaseInfo = bDrawBaseInfo;

	// Don't use fading or color scaling while we're in light complexity mode, since it may change the colors!
	if(!ViewFamily.EngineShowFlags.LightComplexity)
	{
		if(bEnableFading)
		{
			View.OverlayColor = FadeColor;
			View.OverlayColor.A = FMath::Clamp(FadeAmount, 0.f, 1.f);
		}

		if(bEnableColorScaling)
		{
				View.ColorScale = FLinearColor(ColorScale.X,ColorScale.Y,ColorScale.Z);
		}
	}

	TSharedPtr<FDragDropOperation> DragOperation = FSlateApplication::Get().GetDragDroppingContent();
	if (!(DragOperation.IsValid() && DragOperation->IsOfType<FBrushBuilderDragDropOp>()))
	{
		// Hide the builder brush when not in geometry mode
		ViewFamily.EngineShowFlags.SetBuilderBrush(false);
	}

	// Update the listener.
	if (bHasAudioFocus)
	{
		UpdateAudioListener(View);
	}
}

void FLevelEditorViewportClient::DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas )
{
	// HUD for components visualizers
	if (GUnrealEd != NULL)
	{
		GUnrealEd->DrawComponentVisualizersHUD(&InViewport, &View, &Canvas);
	}

	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	// Testbed
	FCanvasItemTestbed TestBed;
	TestBed.Draw( Viewport, &Canvas );

	DrawStaticLightingDebugInfo(&View, &Canvas);
}

/**
 *	Draw the texture streaming bounds.
 */
void FLevelEditorViewportClient::DrawTextureStreamingBounds(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssetData;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssetData);

	TArray<const UTexture2D*> SelectedTextures;
	for (auto AssetIt = SelectedAssetData.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		if (AssetIt->IsAssetLoaded())
		{
			const UTexture2D* Texture = Cast<UTexture2D>(AssetIt->GetAsset());
			if (Texture)
	{
				SelectedTextures.Add(Texture);
			}
		}
	}

	TArray<FBox> AssetBoxes;
	if (IStreamingManager::Get().IsTextureStreamingEnabled())
		{
		for (const UTexture2D* Texture : SelectedTextures)
			{
			IStreamingManager::Get().GetTextureStreamingManager().GetObjectReferenceBounds(Texture, AssetBoxes);
			}
		}

	for (const FBox& Box : AssetBoxes)
	{
		DrawWireBox(PDI, Box, FColorList::Yellow, SDPG_World);
	}
}


/** Serialization. */
void FLevelEditorViewportClient::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEditorViewportClient::AddReferencedObjects( Collector );

	for( TSet<FViewportHoverTarget>::TIterator It( FLevelEditorViewportClient::HoveredObjects ); It; ++It )
	{
		FViewportHoverTarget& CurHoverTarget = *It;
		Collector.AddReferencedObject( CurHoverTarget.HoveredActor );
		Collector.AddReferencedObject( CurHoverTarget.HoveredModel );
	}

	{
		FSceneViewStateInterface* Ref = ViewState.GetReference();

		if(Ref)
		{
			Ref->AddReferencedObjects(Collector);
		}
	}
}

/**
 * Copies layout and camera settings from the specified viewport
 *
 * @param InViewport The viewport to copy settings from
 */
void FLevelEditorViewportClient::CopyLayoutFromViewport( const FLevelEditorViewportClient& InViewport )
{
	SetViewLocation( InViewport.GetViewLocation() );
	SetViewRotation( InViewport.GetViewRotation() );
	ViewFOV = InViewport.ViewFOV;
	ViewportType = InViewport.ViewportType;
	SetOrthoZoom( InViewport.GetOrthoZoom() );
	ActorLockedToCamera = InViewport.ActorLockedToCamera;
	bAllowCinematicPreview = InViewport.bAllowCinematicPreview;
}


UWorld* FLevelEditorViewportClient::ConditionalSetWorld()
{
	// Should set GWorld to the play world if we are simulating in the editor and not already in the play world (reentrant calls to this would cause the world to be the same)
	if( bIsSimulateInEditorViewport && GEditor->PlayWorld != GWorld )
	{
		check( GEditor->PlayWorld != NULL );
		return SetPlayInEditorWorld( GEditor->PlayWorld );
	}

	// Returned world doesn't matter for this case
	return NULL;
}

void FLevelEditorViewportClient::ConditionalRestoreWorld( UWorld* InWorld )
{
	if( bIsSimulateInEditorViewport && InWorld )
	{
		// We should not already be in the world about to switch to an we should not be switching to the play world
		check( GWorld != InWorld && InWorld != GEditor->PlayWorld );
		RestoreEditorWorld( InWorld );
	}
}


/** Updates any orthographic viewport movement to use the same location as this viewport */
void FLevelEditorViewportClient::UpdateLinkedOrthoViewports( bool bInvalidate )
{
	// Only update if linked ortho movement is on, this viewport is orthographic, and is the current viewport being used.
	if (GetDefault<ULevelEditorViewportSettings>()->bUseLinkedOrthographicViewports && IsOrtho() && GCurrentLevelEditingViewportClient == this)
	{
		int32 MaxFrames = -1;
		int32 NextViewportIndexToDraw = INDEX_NONE;

		// Search through all viewports for orthographic ones
		for( int32 ViewportIndex = 0; ViewportIndex < GEditor->LevelViewportClients.Num(); ++ViewportIndex )
		{
			FLevelEditorViewportClient* Client = GEditor->LevelViewportClients[ViewportIndex];
			check(Client);

			// Only update other orthographic viewports viewing the same scene
			if( (Client != this) && Client->IsOrtho() && (Client->GetScene() == this->GetScene()) )
			{
				int32 Frames = Client->FramesSinceLastDraw;
				Client->bNeedsLinkedRedraw = false;
				Client->SetOrthoZoom( GetOrthoZoom() );
				Client->SetViewLocation( GetViewLocation() );
				if( Client->IsVisible() )
				{
					// Find the viewport which has the most number of frames since it was last rendered.  We will render that next.
					if( Frames > MaxFrames )
					{
						MaxFrames = Frames;
						NextViewportIndexToDraw = ViewportIndex;
					}
					if( bInvalidate )
					{
						Client->Invalidate();
					}
				}
			}
		}

		if( bInvalidate )
		{
			Invalidate();
		}

		if( NextViewportIndexToDraw != INDEX_NONE )
		{
			// Force this viewport to redraw.
			GEditor->LevelViewportClients[NextViewportIndexToDraw]->bNeedsLinkedRedraw = true;
		}
	}
}


//
//	FLevelEditorViewportClient::GetScene
//

FLinearColor FLevelEditorViewportClient::GetBackgroundColor() const
{
	return IsPerspective() ? GEditor->C_WireBackground : GEditor->C_OrthoBackground;
}

int32 FLevelEditorViewportClient::GetCameraSpeedSetting() const
{
	return GetDefault<ULevelEditorViewportSettings>()->CameraSpeed;
}

void FLevelEditorViewportClient::SetCameraSpeedSetting(int32 SpeedSetting)
{
	GetMutableDefault<ULevelEditorViewportSettings>()->CameraSpeed = SpeedSetting;
}


bool FLevelEditorViewportClient::OverrideHighResScreenshotCaptureRegion(FIntRect& OutCaptureRegion)
{
	FSlateRect Rect;
	if (CalculateEditorConstrainedViewRect(Rect, Viewport))
	{
		FSlateRect InnerRect = Rect.InsetBy(FMargin(0.5f * SafePadding * Rect.GetSize().Size()));
		OutCaptureRegion = FIntRect((int32)InnerRect.Left, (int32)InnerRect.Top, (int32)(InnerRect.Left + InnerRect.GetSize().X), (int32)(InnerRect.Top + InnerRect.GetSize().Y));
		return true;
	}
	return false;
}

/**
 * Static: Adds a hover effect to the specified object
 *
 * @param	InHoverTarget	The hoverable object to add the effect to
 */
void FLevelEditorViewportClient::AddHoverEffect( const FViewportHoverTarget& InHoverTarget )
{
	AActor* ActorUnderCursor = InHoverTarget.HoveredActor;
	UModel* ModelUnderCursor = InHoverTarget.HoveredModel;

	if( ActorUnderCursor != nullptr )
	{
		TInlineComponentArray<UPrimitiveComponent*> Components;
		ActorUnderCursor->GetComponents(Components);

		for(int32 ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent* PrimitiveComponent = Components[ComponentIndex];
			if (PrimitiveComponent->IsRegistered())
			{
				PrimitiveComponent->PushHoveredToProxy( true );
			}
		}
	}
	else if (ModelUnderCursor != nullptr)
	{
		check( InHoverTarget.ModelSurfaceIndex != INDEX_NONE );
		check( InHoverTarget.ModelSurfaceIndex < (uint32)ModelUnderCursor->Surfs.Num() );
		FBspSurf& Surf = ModelUnderCursor->Surfs[ InHoverTarget.ModelSurfaceIndex ];
		Surf.PolyFlags |= PF_Hovered;
	}
}


/**
 * Static: Removes a hover effect from the specified object
 *
 * @param	InHoverTarget	The hoverable object to remove the effect from
 */
void FLevelEditorViewportClient::RemoveHoverEffect( const FViewportHoverTarget& InHoverTarget )
{
	AActor* CurHoveredActor = InHoverTarget.HoveredActor;
	if( CurHoveredActor != nullptr )
	{
		TInlineComponentArray<UPrimitiveComponent*> Components;
		CurHoveredActor->GetComponents(Components);

		for(int32 ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent* PrimitiveComponent = Components[ComponentIndex];
			if (PrimitiveComponent->IsRegistered())
			{
				check(PrimitiveComponent->IsRegistered());
				PrimitiveComponent->PushHoveredToProxy( false );
			}
		}
	}

	UModel* CurHoveredModel = InHoverTarget.HoveredModel;
	if( CurHoveredModel != nullptr )
	{
		if( InHoverTarget.ModelSurfaceIndex != INDEX_NONE &&
			(uint32)CurHoveredModel->Surfs.Num() >= InHoverTarget.ModelSurfaceIndex )
		{
			FBspSurf& Surf = CurHoveredModel->Surfs[ InHoverTarget.ModelSurfaceIndex ];
			Surf.PolyFlags &= ~PF_Hovered;
		}
	}
}


/**
 * Static: Clears viewport hover effects from any objects that currently have that
 */
void FLevelEditorViewportClient::ClearHoverFromObjects()
{
	// Clear hover feedback for any actor's that were previously drawing a hover cue
	if( HoveredObjects.Num() > 0 )
	{
		for( TSet<FViewportHoverTarget>::TIterator It( HoveredObjects ); It; ++It )
		{
			FViewportHoverTarget& CurHoverTarget = *It;
			RemoveHoverEffect( CurHoverTarget );
		}

		HoveredObjects.Empty();
	}
}

void FLevelEditorViewportClient::OnEditorCleanse()
{
	ClearHoverFromObjects();
}

void FLevelEditorViewportClient::OnPreBeginPIE(const bool bIsSimulating)
{
	// Called before PIE attempts to start, allowing the viewport to cancel processes, like dragging, that will block PIE from beginning
	AbortTracking();
}

bool FLevelEditorViewportClient::GetSpriteCategoryVisibility( const FName& InSpriteCategory ) const
{
	const int32 CategoryIndex = GEngine->GetSpriteCategoryIndex( InSpriteCategory );
	check( CategoryIndex != INDEX_NONE && CategoryIndex < SpriteCategoryVisibility.Num() );

	return SpriteCategoryVisibility[ CategoryIndex ];
}

bool FLevelEditorViewportClient::GetSpriteCategoryVisibility( int32 Index ) const
{
	check( Index >= 0 && Index < SpriteCategoryVisibility.Num() );
	return SpriteCategoryVisibility[ Index ];
}

void FLevelEditorViewportClient::SetSpriteCategoryVisibility( const FName& InSpriteCategory, bool bVisible )
{
	const int32 CategoryIndex = GEngine->GetSpriteCategoryIndex( InSpriteCategory );
	check( CategoryIndex != INDEX_NONE && CategoryIndex < SpriteCategoryVisibility.Num() );

	SpriteCategoryVisibility[ CategoryIndex ] = bVisible;
}

void FLevelEditorViewportClient::SetSpriteCategoryVisibility( int32 Index, bool bVisible )
{
	check( Index >= 0 && Index < SpriteCategoryVisibility.Num() );
	SpriteCategoryVisibility[ Index ] = bVisible;
}

void FLevelEditorViewportClient::SetAllSpriteCategoryVisibility( bool bVisible )
{
	SpriteCategoryVisibility.Init( bVisible, SpriteCategoryVisibility.Num() );
}

UWorld* FLevelEditorViewportClient::GetWorld() const
{
	if (bIsSimulateInEditorViewport)
	{
		return GEditor->PlayWorld;
	}
	else if (World)
	{
		return World;
	}
	return FEditorViewportClient::GetWorld();
}

void FLevelEditorViewportClient::SetReferenceToWorldContext(FWorldContext& WorldContext)
{
	WorldContext.AddRef(World);
}

void FLevelEditorViewportClient::RemoveReferenceToWorldContext(FWorldContext& WorldContext)
{
	WorldContext.RemoveRef(World);
}

void FLevelEditorViewportClient::SetIsSimulateInEditorViewport( bool bInIsSimulateInEditorViewport )
{ 
	bIsSimulateInEditorViewport = bInIsSimulateInEditorViewport; 

	if (bInIsSimulateInEditorViewport)
	{
		TSharedRef<FPhysicsManipulationEdModeFactory> Factory = MakeShareable( new FPhysicsManipulationEdModeFactory );
		FEditorModeRegistry::Get().RegisterMode( FBuiltinEditorModes::EM_Physics, Factory );
	}
	else
	{
		FEditorModeRegistry::Get().UnregisterMode( FBuiltinEditorModes::EM_Physics );
	}
}

#undef LOCTEXT_NAMESPACE

// Doxygen cannot parse these correctly since the declarations are made in Editor, not UnrealEd
#if !UE_BUILD_DOCS
IMPLEMENT_HIT_PROXY(HGeomPolyProxy,HHitProxy);
IMPLEMENT_HIT_PROXY(HGeomEdgeProxy,HHitProxy);
IMPLEMENT_HIT_PROXY(HGeomVertexProxy,HHitProxy);
#endif
