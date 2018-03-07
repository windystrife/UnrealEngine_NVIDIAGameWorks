// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Main implementation of FFbxExporter : export FBX data from Unreal
=============================================================================*/

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Misc/MessageDialog.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "RawIndexBuffer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Model.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "Animation/AnimTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Engine/Brush.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Particles/Emitter.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"
#include "Components/ChildActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Polys.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"

#include "Matinee/InterpData.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackInstFloatProp.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackInstAnimControl.h"

#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "Components/SplineMeshComponent.h"
#include "StaticMeshResources.h"

#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/MatineeActor.h"
#include "FbxExporter.h"
#include "RawMesh.h"
#include "Components/BrushComponent.h"
#include "CineCameraComponent.h"
#include "Math/UnitConversion.h"

#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSequence.h"

#if WITH_PHYSX
#include "DynamicMeshBuilder.h"
#include "PhysXPublic.h"
#include "geometry/PxConvexMesh.h"
#include "PhysicsEngine/BodySetup.h"
#endif // WITH_PHYSX

#include "Exporters/FbxExportOption.h"
#include "FbxExportOptionsWindow.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"

namespace UnFbx
{

TSharedPtr<FFbxExporter> FFbxExporter::StaticInstance;

FFbxExporter::FFbxExporter()
{
	//We use the FGCObject pattern to keep the fbx export option alive during the editor session
	ExportOptions = NewObject<UFbxExportOption>();
	//Load the option from the user save ini file
	ExportOptions->LoadOptions();

	// Create the SdkManager
	SdkManager = FbxManager::Create();

	// create an IOSettings object
	FbxIOSettings * ios = FbxIOSettings::Create(SdkManager, IOSROOT );
	SdkManager->SetIOSettings(ios);

	DefaultCamera = NULL;
}

FFbxExporter::~FFbxExporter()
{
	if (SdkManager)
	{
		SdkManager->Destroy();
		SdkManager = NULL;
	}
}

FFbxExporter* FFbxExporter::GetInstance()
{
	if (!StaticInstance.IsValid())
	{
		StaticInstance = MakeShareable( new FFbxExporter() );
	}
	return StaticInstance.Get();
}

void FFbxExporter::DeleteInstance()
{
	StaticInstance.Reset();
}

void FFbxExporter::FillExportOptions(bool BatchMode, bool bShowOptionDialog, const FString& FullPath, bool& OutOperationCanceled, bool& bOutExportAll)
{
	OutOperationCanceled = false;
	
	//Export option should have been set in the constructor
	check(ExportOptions != nullptr);
	
	//Load the option from the user save ini file
	ExportOptions->LoadOptions();
	
	//Return if we do not show the export options or we are running automation test or we are unattended
	if (!bShowOptionDialog || GIsAutomationTesting || FApp::IsUnattended())
	{
		return;
	}

	bOutExportAll = false;

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "FBXExportOpionsTitle", "FBX Export Options"))
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(EAutoCenter::PrimaryWorkArea)
		.ClientSize(FVector2D(500, 445));

	TSharedPtr<SFbxExportOptionsWindow> FbxOptionWindow;
	Window->SetContent
	(
		SAssignNew(FbxOptionWindow, SFbxExportOptionsWindow)
		.ExportOptions(ExportOptions)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FullPath))
		.BatchMode(BatchMode)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	ExportOptions->SaveOptions();

	if (FbxOptionWindow->ShouldExport())
	{
		bOutExportAll = FbxOptionWindow->ShouldExportAll();
	}
	else
	{
		OutOperationCanceled = true;
	}
}


void FFbxExporter::CreateDocument()
{
	Scene = FbxScene::Create(SdkManager,"");
	
	// create scene info
	FbxDocumentInfo* SceneInfo = FbxDocumentInfo::Create(SdkManager,"SceneInfo");
	SceneInfo->mTitle = "Unreal FBX Exporter";
	SceneInfo->mSubject = "Export FBX meshes from Unreal";
	SceneInfo->Original_ApplicationVendor.Set( "Epic Games" );
	SceneInfo->Original_ApplicationName.Set( "Unreal Engine" );
	SceneInfo->Original_ApplicationVersion.Set( TCHAR_TO_UTF8(*FEngineVersion::Current().ToString()) );
	SceneInfo->LastSaved_ApplicationVendor.Set( "Epic Games" );
	SceneInfo->LastSaved_ApplicationName.Set( "Unreal Engine" );
	SceneInfo->LastSaved_ApplicationVersion.Set( TCHAR_TO_UTF8(*FEngineVersion::Current().ToString()) );

	Scene->SetSceneInfo(SceneInfo);
	
	//FbxScene->GetGlobalSettings().SetOriginalUpAxis(KFbxAxisSystem::Max);
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
	if (ExportOptions->bForceFrontXAxis)
		FrontVector = FbxAxisSystem::eParityEven;

	const FbxAxisSystem UnrealZUp(FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded);
	Scene->GetGlobalSettings().SetAxisSystem(UnrealZUp);
	Scene->GetGlobalSettings().SetOriginalUpAxis(UnrealZUp);
	// Maya use cm by default
	Scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::cm);
	//FbxScene->GetGlobalSettings().SetOriginalSystemUnit( KFbxSystemUnit::m );
	
	// setup anim stack
	AnimStack = FbxAnimStack::Create(Scene, "Unreal Take");
	//KFbxSet<KTime>(AnimStack->LocalStart, KTIME_ONE_SECOND);
	AnimStack->Description.Set("Animation Take for Unreal.");

	// this take contains one base layer. In fact having at least one layer is mandatory.
	AnimLayer = FbxAnimLayer::Create(Scene, "Base Layer");
	AnimStack->AddMember(AnimLayer);
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif

void FFbxExporter::WriteToFile(const TCHAR* Filename)
{
	int32 Major, Minor, Revision;
	bool Status = true;

	int32 FileFormat = -1;
	bool bEmbedMedia = false;

	// Create an exporter.
	FbxExporter* Exporter = FbxExporter::Create(SdkManager, "");

	// set file format
	// Write in fall back format if pEmbedMedia is true
	FileFormat = SdkManager->GetIOPluginRegistry()->GetNativeWriterFormat();

	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.

	IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,        true);
	IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,         true);
	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,        bEmbedMedia);
	IOS_REF.SetBoolProp(EXP_FBX_SHAPE,           true);
	IOS_REF.SetBoolProp(EXP_FBX_GOBO,            true);
	IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,       true);
	IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

	//Get the compatibility from the editor settings
	const char* CompatibilitySetting = FBX_2013_00_COMPATIBLE;
	const EFbxExportCompatibility FbxExportCompatibility = ExportOptions->FbxExportCompatibility;
	switch (FbxExportCompatibility)
	{
		case EFbxExportCompatibility::FBX_2010:
			CompatibilitySetting = FBX_2010_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2011:
			CompatibilitySetting = FBX_2011_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2012:
			CompatibilitySetting = FBX_2012_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2013:
			CompatibilitySetting = FBX_2013_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2014:
			CompatibilitySetting = FBX_2014_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2016:
			CompatibilitySetting = FBX_2016_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2018:
			CompatibilitySetting = FBX_2018_00_COMPATIBLE;
			break;
	}
	
	// We export using FBX 2013 format because many users are still on that version and FBX 2014 files has compatibility issues with
	// normals when importing to an earlier version of the plugin
	if (!Exporter->SetFileExportVersion(CompatibilitySetting, FbxSceneRenamer::eNone))
	{
		UE_LOG(LogFbx, Warning, TEXT("Call to KFbxExporter::SetFileExportVersion(FBX_2013_00_COMPATIBLE) to export 2013 fbx file format failed.\n"));
	}

	// Initialize the exporter by providing a filename.
	if( !Exporter->Initialize(TCHAR_TO_UTF8(Filename), FileFormat, SdkManager->GetIOSettings()) )
	{
		UE_LOG(LogFbx, Warning, TEXT("Call to KFbxExporter::Initialize() failed.\n"));
		UE_LOG(LogFbx, Warning, TEXT("Error returned: %s\n\n"), Exporter->GetStatus().GetErrorString() );
		return;
	}

	FbxManager::GetFileFormatVersion(Major, Minor, Revision);
	UE_LOG(LogFbx, Warning, TEXT("FBX version number for this version of the FBX SDK is %d.%d.%d\n\n"), Major, Minor, Revision);

	// Export the scene.
	Status = Exporter->Export(Scene); 

	// Destroy the exporter.
	Exporter->Destroy();
	
	CloseDocument();
	
	return;
}

/**
 * Release the FBX scene, releasing its memory.
 */
void FFbxExporter::CloseDocument()
{
	FbxActors.Reset();
	FbxSkeletonRoots.Reset();
	FbxMaterials.Reset();
	FbxMeshes.Reset();
	FbxNodeNameToIndexMap.Reset();
	
	if (Scene)
	{
		Scene->Destroy();
		Scene = NULL;
	}
}

void FFbxExporter::CreateAnimatableUserProperty(FbxNode* Node, float Value, const char* Name, const char* Label)
{
	// Add one user property for recording the animation
	FbxProperty IntensityProp = FbxProperty::Create(Node, FbxFloatDT, Name, Label);
	IntensityProp.Set(Value);
	IntensityProp.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	IntensityProp.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
}

/**
*	Sorts actors such that parent actors will appear before children actors in the list
*	Stable sort
*/
static void SortActorsHierarchy(TArray<AActor*>& Actors)
{
	auto CalcAttachDepth = [](AActor* InActor) -> int32 {
		int32 Depth = MAX_int32;
		if (InActor)
		{
			Depth = 0;
			if (InActor->GetRootComponent())
			{
				for (const USceneComponent* Test = InActor->GetRootComponent()->GetAttachParent(); Test != nullptr; Test = Test->GetAttachParent(), Depth++);
			}
		}
		return Depth;
	};

	// Unfortunately TArray.StableSort assumes no null entries in the array
	// So it forces me to use internal unrestricted version
	StableSortInternal(Actors.GetData(), Actors.Num(), [&](AActor* L, AActor* R) {
		return CalcAttachDepth(L) < CalcAttachDepth(R);
	});
}

/**
 * Exports the basic scene information to the FBX document.
 */
void FFbxExporter::ExportLevelMesh( ULevel* InLevel, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter )
{
	if (InLevel == NULL)
	{
		return;
	}

	if( !bSelectedOnly )
	{
		// Exports the level's scene geometry
		// the vertex number of Model must be more than 2 (at least a triangle panel)
		if (InLevel->Model != NULL && InLevel->Model->VertexBuffer.Vertices.Num() > 2 && InLevel->Model->MaterialIndexBuffers.Num() > 0)
		{
			// create a FbxNode
			FbxNode* Node = FbxNode::Create(Scene,"LevelMesh");

			// set the shading mode to view texture
			Node->SetShadingMode(FbxNode::eTextureShading);
			Node->LclScaling.Set(FbxVector4(1.0, 1.0, 1.0));

			Scene->GetRootNode()->AddChild(Node);

			// Export the mesh for the world
			ExportModel(InLevel->Model, Node, "Level Mesh");
		}
	}

	TArray<AActor*> ActorToExport;
	int32 ActorCount = InLevel->Actors.Num();
	for (int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		AActor* Actor = InLevel->Actors[ActorIndex];
		if (Actor != NULL && (!bSelectedOnly || (bSelectedOnly && Actor->IsSelected())))
		{
			ActorToExport.Add(Actor);
		}
	}

	//Sort the hierarchy to make sure parent come first
	SortActorsHierarchy(ActorToExport);

	ActorCount = ActorToExport.Num();
	for (int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		AActor* Actor = ActorToExport[ActorIndex];
		//We export only valid actor
		if (Actor == nullptr)
			continue;

		bool bIsBlueprintClass = false;
		if (UClass* ActorClass = Actor->GetClass())
		{
			// Check if we export the actor as a blueprint
			bIsBlueprintClass = UBlueprint::GetBlueprintFromClass(ActorClass) != nullptr;
		}

		//Blueprint can be any type of actor so it must be done first
		if (bIsBlueprintClass)
		{
			// Export blueprint actors and all their components.
			ExportActor(Actor, true, NodeNameAdapter);
		}
		else if (Actor->IsA(ALight::StaticClass()))
		{
			ExportLight((ALight*) Actor, NodeNameAdapter );
		}
		else if (Actor->IsA(AStaticMeshActor::StaticClass()))
		{
			ExportStaticMesh( Actor, CastChecked<AStaticMeshActor>(Actor)->GetStaticMeshComponent(), NodeNameAdapter );
		}
		else if (Actor->IsA(ALandscapeProxy::StaticClass()))
		{
			ExportLandscape(CastChecked<ALandscapeProxy>(Actor), false, NodeNameAdapter);
		}
		else if (Actor->IsA(ABrush::StaticClass()))
		{
			// All brushes should be included within the world geometry exported above.
			ExportBrush((ABrush*) Actor, NULL, 0, NodeNameAdapter );
		}
		else if (Actor->IsA(AEmitter::StaticClass()))
		{
			ExportActor( Actor, false, NodeNameAdapter ); // Just export the placement of the particle emitter.
		}
		else if(Actor->IsA(ACameraActor::StaticClass()))
		{
			ExportCamera(CastChecked<ACameraActor>(Actor), false, NodeNameAdapter);
		}
		else
		{
			// Export any other type of actors and all their components.
			ExportActor(Actor, true, NodeNameAdapter);
		}
	}
}

void FFbxExporter::FillFbxLightAttribute(FbxLight* Light, FbxNode* FbxParentNode, ULightComponent* BaseLight)
{
	Light->Intensity.Set(BaseLight->Intensity);
	Light->Color.Set(Converter.ConvertToFbxColor(BaseLight->LightColor));

	// Add one user property for recording the Brightness animation
	CreateAnimatableUserProperty(FbxParentNode, BaseLight->Intensity, "UE_Intensity", "UE_Matinee_Light_Intensity");

	// Look for the higher-level light types and determine the lighting method
	if (BaseLight->IsA(UPointLightComponent::StaticClass()))
	{
		UPointLightComponent* PointLight = (UPointLightComponent*)BaseLight;
		if (BaseLight->IsA(USpotLightComponent::StaticClass()))
		{
			USpotLightComponent* SpotLight = (USpotLightComponent*)BaseLight;
			Light->LightType.Set(FbxLight::eSpot);

			// Export the spot light parameters.
			if (!FMath::IsNearlyZero(SpotLight->InnerConeAngle*2.0f))
			{
				Light->InnerAngle.Set(SpotLight->InnerConeAngle*2.0f);
			}
			else // Maya requires a non-zero inner cone angle
			{
				Light->InnerAngle.Set(0.01f);
			}
			Light->OuterAngle.Set(SpotLight->OuterConeAngle*2.0f);
		}
		else
		{
			Light->LightType.Set(FbxLight::ePoint);
		}

		// Export the point light parameters.
		Light->EnableFarAttenuation.Set(true);
		Light->FarAttenuationEnd.Set(PointLight->AttenuationRadius);
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxParentNode, PointLight->AttenuationRadius, "UE_Radius", "UE_Matinee_Light_Radius");

		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxParentNode, PointLight->LightFalloffExponent, "UE_FalloffExponent", "UE_Matinee_Light_FalloffExponent");
	}
	else if (BaseLight->IsA(UDirectionalLightComponent::StaticClass()))
	{
		// The directional light has no interesting properties.
		Light->LightType.Set(FbxLight::eDirectional);
		Light->Intensity.Set(BaseLight->Intensity*100.0f);
	}
}

/**
 * Exports the light-specific information for a light actor.
 */
void FFbxExporter::ExportLight( ALight* Actor, INodeNameAdapter& NodeNameAdapter )
{
	if (Scene == NULL || Actor == NULL || !Actor->GetLightComponent()) return;

	// Export the basic actor information.
	FbxNode* FbxActor = ExportActor( Actor, false, NodeNameAdapter ); // this is the pivot node
	// The real fbx light node
	FbxNode* FbxLightNode = FbxActor->GetParent();

	ULightComponent* BaseLight = Actor->GetLightComponent();

	FString FbxNodeName = NodeNameAdapter.GetActorNodeName(Actor);

	// Export the basic light information
	FbxLight* Light = FbxLight::Create(Scene, TCHAR_TO_UTF8(*FbxNodeName));
	FillFbxLightAttribute(Light, FbxLightNode, BaseLight);	
	FbxActor->SetNodeAttribute(Light);
}

void FFbxExporter::FillFbxCameraAttribute(FbxNode* ParentNode, FbxCamera* Camera, UCameraComponent *CameraComponent)
{
	float ApertureHeightInInches = 0.612f; // 0.612f is a magic number from Maya that represents the ApertureHeight
	float ApertureWidthInInches = CameraComponent->AspectRatio * ApertureHeightInInches;
	float FocalLength = Camera->ComputeFocalLength(CameraComponent->FieldOfView);

	if (CameraComponent->IsA(UCineCameraComponent::StaticClass()))
	{
		UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(CameraComponent);
		if (CineCameraComponent)
		{
			ApertureWidthInInches = FUnitConversion::Convert(CineCameraComponent->FilmbackSettings.SensorWidth, EUnit::Millimeters, EUnit::Inches);
			ApertureHeightInInches = FUnitConversion::Convert(CineCameraComponent->FilmbackSettings.SensorHeight, EUnit::Millimeters, EUnit::Inches);
			FocalLength = CineCameraComponent->CurrentFocalLength;
		}
	}

	// Export the view area information
	Camera->ProjectionType.Set(CameraComponent->ProjectionMode == ECameraProjectionMode::Type::Perspective ? FbxCamera::ePerspective : FbxCamera::eOrthogonal);
	Camera->SetAspect(FbxCamera::eFixedRatio, CameraComponent->AspectRatio, 1.0f);
	Camera->FilmAspectRatio.Set(CameraComponent->AspectRatio);
	Camera->SetApertureWidth(ApertureWidthInInches);
	Camera->SetApertureHeight(ApertureHeightInInches);
	Camera->SetApertureMode(FbxCamera::eFocalLength);
	Camera->FocalLength.Set(FocalLength);

	// Add one user property for recording the AspectRatio animation
	CreateAnimatableUserProperty(ParentNode, CameraComponent->AspectRatio, "UE_AspectRatio", "UE_Matinee_Camera_AspectRatio");

	// Push the near/far clip planes away, as the engine uses larger values than the default.
	Camera->SetNearPlane(10.0f);
	Camera->SetFarPlane(100000.0f);
}

void FFbxExporter::ExportCamera( ACameraActor* Actor, bool bExportComponents,INodeNameAdapter& NodeNameAdapter )
{
	if (Scene == NULL || Actor == NULL) return;

	UCameraComponent *CameraComponent = Actor->GetCameraComponent();
	// Export the basic actor information.
	FbxNode* FbxActor = ExportActor( Actor, bExportComponents, NodeNameAdapter ); // this is the pivot node
	// The real fbx camera node
	FbxNode* FbxCameraNode = FbxActor->GetParent();

	FString FbxNodeName = NodeNameAdapter.GetActorNodeName(Actor);

	// Create a properly-named FBX camera structure and instantiate it in the FBX scene graph
	FbxCamera* Camera = FbxCamera::Create(Scene, TCHAR_TO_UTF8(*FbxNodeName));
	FillFbxCameraAttribute(FbxCameraNode, Camera, CameraComponent);

	FbxActor->SetNodeAttribute(Camera);

	DefaultCamera = Camera;
}

/**
 * Exports the mesh and the actor information for a brush actor.
 */
void FFbxExporter::ExportBrush(ABrush* Actor, UModel* InModel, bool bConvertToStaticMesh, INodeNameAdapter& NodeNameAdapter )
{
	if (Scene == NULL || Actor == NULL || !Actor->GetBrushComponent()) return;

	if (!bConvertToStaticMesh)
	{
		// Retrieve the information structures, verifying the integrity of the data.
		UModel* Model = Actor->GetBrushComponent()->Brush;

		if (Model == NULL || Model->VertexBuffer.Vertices.Num() < 3 || Model->MaterialIndexBuffers.Num() == 0) return;
 
		// Create the FBX actor, the FBX geometry and instantiate it.
		FbxNode* FbxActor = ExportActor( Actor, false, NodeNameAdapter );
		Scene->GetRootNode()->AddChild(FbxActor);
 
		// Export the mesh information
		ExportModel(Model, FbxActor, TCHAR_TO_UTF8(*Actor->GetName()));
	}
	else
	{
		FRawMesh Mesh;
		TArray<FStaticMaterial>	Materials;
		GetBrushMesh(Actor,Actor->Brush,Mesh,Materials);

		if( Mesh.VertexPositions.Num() )
		{
			UStaticMesh* StaticMesh = CreateStaticMesh(Mesh,Materials,GetTransientPackage(),Actor->GetFName());
			ExportStaticMesh( StaticMesh, &Materials );
		}
	}
}

void FFbxExporter::ExportModel(UModel* Model, FbxNode* Node, const char* Name)
{
	//int32 VertexCount = Model->VertexBuffer.Vertices.Num();
	int32 MaterialCount = Model->MaterialIndexBuffers.Num();

	const float BiasedHalfWorldExtent = HALF_WORLD_MAX * 0.95f;

	// Create the mesh and three data sources for the vertex positions, normals and texture coordinates.
	FbxMesh* Mesh = FbxMesh::Create(Scene, Name);
	
	// Create control points.
	uint32 VertCount(Model->VertexBuffer.Vertices.Num());
	Mesh->InitControlPoints(VertCount);
	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	
	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}
	
	// We want to have one normal for each vertex (or control point),
	// so we set the mapping mode to eBY_CONTROL_POINT.
	FbxLayerElementNormal* LayerElementNormal= FbxLayerElementNormal::Create(Mesh, "");

	LayerElementNormal->SetMappingMode(FbxLayerElement::eByControlPoint);

	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);
	
	// Create UV for Diffuse channel.
	FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, "DiffuseUV");
	UVDiffuseLayer->SetMappingMode(FbxLayerElement::eByControlPoint);
	UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eDirect);
	Layer->SetUVs(UVDiffuseLayer, FbxLayerElement::eTextureDiffuse);
	
	for (uint32 VertexIdx = 0; VertexIdx < VertCount; ++VertexIdx)
	{
		FModelVertex& Vertex = Model->VertexBuffer.Vertices[VertexIdx];
		FVector Normal = Vertex.TangentZ;

		// If the vertex is outside of the world extent, snap it to the origin.  The faces associated with
		// these vertices will be removed before exporting.  We leave the snapped vertex in the buffer so
		// we won't have to deal with reindexing everything.
		FVector FinalVertexPos = Vertex.Position;
		if( FMath::Abs( Vertex.Position.X ) > BiasedHalfWorldExtent ||
			FMath::Abs( Vertex.Position.Y ) > BiasedHalfWorldExtent ||
			FMath::Abs( Vertex.Position.Z ) > BiasedHalfWorldExtent )
		{
			FinalVertexPos = FVector::ZeroVector;
		}

		ControlPoints[VertexIdx] = FbxVector4(FinalVertexPos.X, -FinalVertexPos.Y, FinalVertexPos.Z);
		FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxAMatrix NodeMatrix;
		FbxVector4 Trans = Node->LclTranslation.Get();
		NodeMatrix.SetT(FbxVector4(Trans[0], Trans[1], Trans[2]));
		FbxVector4 Rot = Node->LclRotation.Get();
		NodeMatrix.SetR(FbxVector4(Rot[0], Rot[1], Rot[2]));
		NodeMatrix.SetS(Node->LclScaling.Get());
		FbxNormal = NodeMatrix.MultT(FbxNormal);
		FbxNormal.Normalize();
		LayerElementNormal->GetDirectArray().Add(FbxNormal);
		
		// update the index array of the UVs that map the texture to the face
		UVDiffuseLayer->GetDirectArray().Add(FbxVector2(Vertex.TexCoord.X, -Vertex.TexCoord.Y));
	}
	
	Layer->SetNormals(LayerElementNormal);
	Layer->SetUVs(UVDiffuseLayer);
	
	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);
	
	//Make sure the Index buffer is accessible.
	
	for (auto MaterialIterator = Model->MaterialIndexBuffers.CreateIterator(); MaterialIterator; ++MaterialIterator)
	{
		BeginReleaseResource(MaterialIterator->Value.Get());
	}
	FlushRenderingCommands();

	// Create the materials and the per-material tesselation structures.
	for (auto MaterialIterator = Model->MaterialIndexBuffers.CreateIterator(); MaterialIterator; ++MaterialIterator)
	{
		UMaterialInterface* MaterialInterface = MaterialIterator.Key();
		FRawIndexBuffer16or32& IndexBuffer = *MaterialIterator.Value();
		int32 IndexCount = IndexBuffer.Indices.Num();
		if (IndexCount < 3) continue;
		
		// Are NULL materials okay?
		int32 MaterialIndex = -1;
		FbxSurfaceMaterial* FbxMaterial;
		if (MaterialInterface != NULL && MaterialInterface->GetMaterial() != NULL)
		{
			FbxMaterial = ExportMaterial(MaterialInterface);
		}
		else
		{
			// Set default material
			FbxMaterial = CreateDefaultMaterial();
		}
		MaterialIndex = Node->AddMaterial(FbxMaterial);

		// Create the Fbx polygons set.

		// Retrieve and fill in the index buffer.
		const int32 TriangleCount = IndexCount / 3;
		for( int32 TriangleIdx = 0; TriangleIdx < TriangleCount; ++TriangleIdx )
		{
			bool bSkipTriangle = false;

			for( int32 IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
			{
				// Skip triangles that belong to BSP geometry close to the world extent, since its probably
				// the automatically-added-brush for new levels.  The vertices will be left in the buffer (unreferenced)
				FVector VertexPos = Model->VertexBuffer.Vertices[ IndexBuffer.Indices[ TriangleIdx * 3 + IndexIdx ] ].Position;
				if( FMath::Abs( VertexPos.X ) > BiasedHalfWorldExtent ||
					FMath::Abs( VertexPos.Y ) > BiasedHalfWorldExtent ||
					FMath::Abs( VertexPos.Z ) > BiasedHalfWorldExtent )
				{
					bSkipTriangle = true;
					break;
				}
			}

			if( !bSkipTriangle )
			{
				// all faces of the cube have the same texture
				Mesh->BeginPolygon(MaterialIndex);
				for( int32 IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
				{
					// Control point index
					Mesh->AddPolygon(IndexBuffer.Indices[ TriangleIdx * 3 + IndexIdx ]);

				}
				Mesh->EndPolygon ();
			}
		}

		BeginInitResource(&IndexBuffer);
	}
	
	FlushRenderingCommands();

	Node->SetNodeAttribute(Mesh);
}

FString FFbxExporter::GetFbxObjectName(const FString &FbxObjectNode, INodeNameAdapter& NodeNameAdapter)
{
	FString FbxTestName = FbxObjectNode;
	int32 *NodeIndex = FbxNodeNameToIndexMap.Find(FbxTestName);
	if (NodeIndex)
	{
		FbxTestName = FString::Printf(TEXT("%s%d"), *FbxTestName, *NodeIndex);
		++(*NodeIndex);
	}
	else
	{
		FbxNodeNameToIndexMap.Add(FbxTestName, 1);
	}
	return FbxTestName;
}

void FFbxExporter::ExportStaticMesh(AActor* Actor, UStaticMeshComponent* StaticMeshComponent, INodeNameAdapter& NodeNameAdapter)
{
	if (Scene == NULL || Actor == NULL || StaticMeshComponent == NULL)
	{
		return;
	}

	// Retrieve the static mesh rendering information at the correct LOD level.
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == NULL || !StaticMesh->HasValidRenderData())
	{
		return;
	}
	int32 LODIndex = StaticMeshComponent->ForcedLodModel-1;

	FString FbxNodeName = NodeNameAdapter.GetActorNodeName(Actor);
	FString FbxMeshName = StaticMesh->GetName().Replace(TEXT("-"), TEXT("_"));
	FColorVertexBuffer* ColorBuffer = NULL;
	
	if(LODIndex == INDEX_NONE && StaticMesh->GetNumLODs() > 1)
	{
		//Create a fbx LOD Group node
		FbxNode* FbxActor = ExportActor(Actor, false, NodeNameAdapter);
		FString FbxLODGroupName = NodeNameAdapter.GetActorNodeName(Actor);
		FbxLODGroupName += TEXT("_LodGroup");
		FbxLODGroupName = GetFbxObjectName(FbxLODGroupName, NodeNameAdapter);
		FbxLODGroup *FbxLodGroupAttribute = FbxLODGroup::Create(Scene, TCHAR_TO_UTF8(*FbxLODGroupName));
		FbxActor->AddNodeAttribute(FbxLodGroupAttribute);
		FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
		//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
		for (int CurrentLodIndex = 0; CurrentLodIndex < StaticMesh->GetNumLODs(); ++CurrentLodIndex)
		{
			if (CurrentLodIndex < StaticMeshComponent->LODData.Num())
			{
				ColorBuffer = StaticMeshComponent->LODData[CurrentLodIndex].OverrideVertexColors;
			}
			else
			{
				ColorBuffer = nullptr;
			}
			FString FbxLODNodeName = NodeNameAdapter.GetActorNodeName(Actor);
			FbxLODNodeName += TEXT("_LOD") + FString::FromInt(CurrentLodIndex);
			FbxLODNodeName = GetFbxObjectName(FbxLODNodeName, NodeNameAdapter);
			FbxNode* FbxActorLOD = FbxNode::Create(Scene, TCHAR_TO_UTF8(*FbxLODNodeName));
			FbxActor->AddChild(FbxActorLOD);
			if (CurrentLodIndex + 1 < StaticMesh->GetNumLODs())
			{
				//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
				double LodScreenSize = (double)(10.0f / StaticMesh->RenderData->ScreenSize[CurrentLodIndex]);
				FbxLodGroupAttribute->AddThreshold(LodScreenSize);
			}
			ExportStaticMeshToFbx(StaticMesh, CurrentLodIndex, *FbxMeshName, FbxActorLOD, -1, ColorBuffer);
		}
	}
	else
	{
		if (LODIndex != INDEX_NONE && LODIndex < StaticMeshComponent->LODData.Num())
		{
			ColorBuffer = StaticMeshComponent->LODData[LODIndex].OverrideVertexColors;
		}
		//Export all LOD
		FbxNode* FbxActor = ExportActor(Actor, false, NodeNameAdapter);
		ExportStaticMeshToFbx(StaticMesh, LODIndex, *FbxMeshName, FbxActor, -1, ColorBuffer);
	}
}

struct FBSPExportData
{
	FRawMesh Mesh;
	TArray<FStaticMaterial> Materials;
	uint32 NumVerts;
	uint32 NumFaces;
	uint32 CurrentVertAddIndex;
	uint32 CurrentFaceAddIndex;
	bool bInitialised;

	FBSPExportData()
		:NumVerts(0)
		,NumFaces(0)
		,CurrentVertAddIndex(0)
		,CurrentFaceAddIndex(0)
		,bInitialised(false)
	{

	}
};

void FFbxExporter::ExportBSP( UModel* Model, bool bSelectedOnly )
{
	TMap< ABrush*, FBSPExportData > BrushToMeshMap;
	TArray<FStaticMaterial> AllMaterials;

	for(int32 NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Model->Nodes[NodeIndex];
		if( Node.NumVertices >= 3 )
		{
			FBspSurf& Surf = Model->Surfs[Node.iSurf];
		
			ABrush* BrushActor = Surf.Actor;

			if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly || (BrushActor && BrushActor->IsSelected() ) )
			{
				FBSPExportData& Data = BrushToMeshMap.FindOrAdd( BrushActor );

				Data.NumVerts += Node.NumVertices;
				Data.NumFaces += Node.NumVertices-2;
			}
		}
	}
	
	for(int32 NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Model->Nodes[NodeIndex];
		FBspSurf& Surf = Model->Surfs[Node.iSurf];

		ABrush* BrushActor = Surf.Actor;

		if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly || (BrushActor && BrushActor->IsSelected() && Node.NumVertices >= 3) )
		{
			FPoly Poly;
			GEditor->polyFindMaster( Model, Node.iSurf, Poly );

			FBSPExportData* ExportData = BrushToMeshMap.Find( BrushActor );
			if( NULL == ExportData )
			{
				UE_LOG(LogFbx, Fatal, TEXT("Error in FBX export of BSP."));
				return;
			}

			TArray<FStaticMaterial>& Materials = ExportData->Materials;
			FRawMesh& Mesh = ExportData->Mesh;

			//Pre-allocate space for this mesh.
			if( !ExportData->bInitialised )
			{
				ExportData->bInitialised = true;
				Mesh.VertexPositions.Empty();
				Mesh.VertexPositions.AddUninitialized(ExportData->NumVerts);

				Mesh.FaceMaterialIndices.Empty();
				Mesh.FaceMaterialIndices.AddUninitialized(ExportData->NumFaces);
				Mesh.FaceSmoothingMasks.Empty();
				Mesh.FaceSmoothingMasks.AddUninitialized(ExportData->NumFaces);
				
				uint32 NumWedges = ExportData->NumFaces*3;
				Mesh.WedgeIndices.Empty();
				Mesh.WedgeIndices.AddUninitialized(NumWedges);
				Mesh.WedgeTexCoords[0].Empty();
				Mesh.WedgeTexCoords[0].AddUninitialized(NumWedges);
				Mesh.WedgeColors.Empty();
				Mesh.WedgeColors.AddUninitialized(NumWedges);
				Mesh.WedgeTangentZ.Empty();
				Mesh.WedgeTangentZ.AddUninitialized(NumWedges);
			}
			
			UMaterialInterface*	Material = Poly.Material;

			AllMaterials.AddUnique(FStaticMaterial(Material));

			int32 MaterialIndex = ExportData->Materials.AddUnique(Material);

			const FVector& TextureBase = Model->Points[Surf.pBase];
			const FVector& TextureX = Model->Vectors[Surf.vTextureU];
			const FVector& TextureY = Model->Vectors[Surf.vTextureV];
			const FVector& Normal = Model->Vectors[Surf.vNormal];

			const int32 StartIndex = ExportData->CurrentVertAddIndex;

			for(int32 VertexIndex = 0; VertexIndex < Node.NumVertices ; VertexIndex++ )
			{
				const FVert& Vert = Model->Verts[Node.iVertPool + VertexIndex];
				const FVector& Vertex = Model->Points[Vert.pVertex];
				Mesh.VertexPositions[ExportData->CurrentVertAddIndex+VertexIndex] = Vertex;
			}
			ExportData->CurrentVertAddIndex += Node.NumVertices;

			for (int32 StartVertexIndex = 1; StartVertexIndex < Node.NumVertices - 1; ++StartVertexIndex)
			{
				// These map the node's vertices to the 3 triangle indices to triangulate the convex polygon.
				int32 TriVertIndices[3] = {
					Node.iVertPool + StartVertexIndex + 1,
					Node.iVertPool + StartVertexIndex,
					Node.iVertPool };

				int32 WedgeIndices[3] = {
					StartIndex + StartVertexIndex + 1,
					StartIndex + StartVertexIndex,
					StartIndex };

				Mesh.FaceMaterialIndices[ExportData->CurrentFaceAddIndex] = MaterialIndex;
				Mesh.FaceSmoothingMasks[ExportData->CurrentFaceAddIndex] =  ( 1 << ( Node.iSurf % 32 ) );

				for (uint32 WedgeIndex = 0; WedgeIndex < 3; ++WedgeIndex)
				{
					const FVert& Vert = Model->Verts[TriVertIndices[WedgeIndex]];
					const FVector& Vertex = Model->Points[Vert.pVertex];

					float U = ((Vertex - TextureBase) | TextureX) / UModel::GetGlobalBSPTexelScale();
					float V = ((Vertex - TextureBase) | TextureY) / UModel::GetGlobalBSPTexelScale();

					uint32 RealWedgeIndex = ( ExportData->CurrentFaceAddIndex * 3 ) + WedgeIndex;

					Mesh.WedgeIndices[RealWedgeIndex] = WedgeIndices[WedgeIndex];
					Mesh.WedgeTexCoords[0][RealWedgeIndex] = FVector2D(U,V);
					//This is not exported when exporting the whole level via ExportModel so leaving out here for now. 
					//Mesh.WedgeTexCoords[1][RealWedgeIndex] = Vert.ShadowTexCoord;
					Mesh.WedgeColors[RealWedgeIndex] = FColor(255,255,255,255);
					Mesh.WedgeTangentZ[RealWedgeIndex] = Normal;
				}

				++ExportData->CurrentFaceAddIndex;
			}
		}
	}

	for( TMap< ABrush*, FBSPExportData >::TIterator It(BrushToMeshMap); It; ++It )
	{
		if( It.Value().Mesh.VertexPositions.Num() )
		{
			UStaticMesh* NewMesh = CreateStaticMesh( It.Value().Mesh, It.Value().Materials, GetTransientPackage(), It.Key()->GetFName() );

			ExportStaticMesh( NewMesh, &AllMaterials );
		}
	}
}

void FFbxExporter::ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<FStaticMaterial>* MaterialOrder )
{
	if (Scene == NULL || StaticMesh == NULL || !StaticMesh->HasValidRenderData()) return;
	FString MeshName;
	StaticMesh->GetName(MeshName);
	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_UTF8(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);

	if (ExportOptions->LevelOfDetail && StaticMesh->GetNumLODs() > 1)
	{
		FString LodGroup_MeshName = MeshName + ("_LodGroup");
		FbxLODGroup *FbxLodGroupAttribute = FbxLODGroup::Create(Scene, TCHAR_TO_UTF8(*LodGroup_MeshName));
		MeshNode->AddNodeAttribute(FbxLodGroupAttribute);
		FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
		//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
		for (int CurrentLodIndex = 0; CurrentLodIndex < StaticMesh->GetNumLODs(); ++CurrentLodIndex)
		{
			FString FbxLODNodeName = MeshName + TEXT("_LOD") + FString::FromInt(CurrentLodIndex);
			FbxNode* FbxActorLOD = FbxNode::Create(Scene, TCHAR_TO_UTF8(*FbxLODNodeName));
			MeshNode->AddChild(FbxActorLOD);
			if (CurrentLodIndex + 1 < StaticMesh->GetNumLODs())
			{
				//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
				double LodScreenSize = (double)(10.0f / StaticMesh->RenderData->ScreenSize[CurrentLodIndex]);
				FbxLodGroupAttribute->AddThreshold(LodScreenSize);
			}
			ExportStaticMeshToFbx(StaticMesh, CurrentLodIndex, *MeshName, FbxActorLOD, -1, nullptr, MaterialOrder);
		}
	}
	else
	{
		ExportStaticMeshToFbx(StaticMesh, 0, *MeshName, MeshNode, -1, NULL, MaterialOrder);
	}
}

void FFbxExporter::ExportStaticMeshLightMap( UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannel )
{
	if (Scene == NULL || StaticMesh == NULL || !StaticMesh->HasValidRenderData()) return;

	FString MeshName;
	StaticMesh->GetName(MeshName);
	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_UTF8(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);
	ExportStaticMeshToFbx(StaticMesh, LODIndex, *MeshName, MeshNode, UVChannel);
}

void FFbxExporter::ExportSkeletalMesh( USkeletalMesh* SkeletalMesh )
{
	if (Scene == NULL || SkeletalMesh == NULL) return;

	FString MeshName;
	SkeletalMesh->GetName(MeshName);

	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_UTF8(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);

	ExportSkeletalMeshToFbx(SkeletalMesh, NULL, *MeshName, MeshNode);
}

void FFbxExporter::ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent, INodeNameAdapter& NodeNameAdapter )
{
	if (Scene == NULL || Actor == NULL || SkeletalMeshComponent == NULL) return;

	// Retrieve the skeletal mesh rendering information.
	USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;

	FString FbxNodeName = NodeNameAdapter.GetActorNodeName(Actor);

	ExportActor( Actor, true, NodeNameAdapter );
}

FbxSurfaceMaterial* FFbxExporter::CreateDefaultMaterial()
{
	// TODO(sbc): the below cast is needed to avoid clang warning.  The upstream
	// signature in FBX should really use 'const char *'.
	FbxSurfaceMaterial* FbxMaterial = Scene->GetMaterial(const_cast<char*>("Fbx Default Material"));
	
	if (!FbxMaterial)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, "Fbx Default Material");
		((FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(FbxDouble3(0.72, 0.72, 0.72));
	}
	
	return FbxMaterial;
}

void FFbxExporter::ExportLandscape(ALandscapeProxy* Actor, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter)
{
	if (Scene == NULL || Actor == NULL)
	{ 
		return;
	}

	FString FbxNodeName = NodeNameAdapter.GetActorNodeName(Actor);

	FbxNode* FbxActor = ExportActor(Actor, true, NodeNameAdapter);
	ExportLandscapeToFbx(Actor, *FbxNodeName, FbxActor, bSelectedOnly);
}

FbxDouble3 SetMaterialComponent(FColorMaterialInput& MatInput, bool ToLinear)
{
	FColor RGBColor;
	FLinearColor LinearColor;
	bool LinearSet = false;
	
	if (MatInput.Expression)
	{
		if (Cast<UMaterialExpressionConstant>(MatInput.Expression))
		{
			UMaterialExpressionConstant* Expr = Cast<UMaterialExpressionConstant>(MatInput.Expression);			
			RGBColor = FColor(Expr->R);
		}
		else if (Cast<UMaterialExpressionVectorParameter>(MatInput.Expression))
		{
			UMaterialExpressionVectorParameter* Expr = Cast<UMaterialExpressionVectorParameter>(MatInput.Expression);
			LinearColor = Expr->DefaultValue;
			LinearSet = true;
			//Linear to sRGB color space conversion
			RGBColor = Expr->DefaultValue.ToFColor(true);
		}
		else if (Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant3Vector* Expr = Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression);
			RGBColor.R = Expr->Constant.R;
			RGBColor.G = Expr->Constant.G;
			RGBColor.B = Expr->Constant.B;
		}
		else if (Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant4Vector* Expr = Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression);
			RGBColor.R = Expr->Constant.R;
			RGBColor.G = Expr->Constant.G;
			RGBColor.B = Expr->Constant.B;
			//RGBColor.A = Expr->A;
		}
		else if (Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant2Vector* Expr = Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression);
			RGBColor.R = Expr->R;
			RGBColor.G = Expr->G;
			RGBColor.B = 0;
		}
		else
		{
			RGBColor.R = MatInput.Constant.R;
			RGBColor.G = MatInput.Constant.G;
			RGBColor.B = MatInput.Constant.B;
		}
	}
	else
	{
		RGBColor.R = MatInput.Constant.R;
		RGBColor.G = MatInput.Constant.G;
		RGBColor.B = MatInput.Constant.B;
	}
	
	if (ToLinear)
	{
		if (!LinearSet)
		{
			//sRGB to linear color space conversion
			LinearColor = FLinearColor(RGBColor);
		}
		return FbxDouble3(LinearColor.R, LinearColor.G, LinearColor.B);
	}
	return FbxDouble3(RGBColor.R, RGBColor.G, RGBColor.B);
}

bool FFbxExporter::FillFbxTextureProperty(const char *PropertyName, const FExpressionInput& MaterialInput, FbxSurfaceMaterial* FbxMaterial)
{
	if (Scene == NULL)
	{
		return false;
	}

	FbxProperty FbxColorProperty = FbxMaterial->FindProperty(PropertyName);
	if (FbxColorProperty.IsValid())
	{
		if (MaterialInput.IsConnected() && MaterialInput.Expression)
		{
			if (MaterialInput.Expression->IsA(UMaterialExpressionTextureSample::StaticClass()))
			{
				UMaterialExpressionTextureSample *TextureSample = Cast<UMaterialExpressionTextureSample>(MaterialInput.Expression);
				if (TextureSample && TextureSample->Texture && TextureSample->Texture->AssetImportData)
				{
					FString TextureSourceFullPath = TextureSample->Texture->AssetImportData->GetFirstFilename();
					//Create a fbx property
					FbxFileTexture* lTexture = FbxFileTexture::Create(Scene, "EnvSamplerTex");
					lTexture->SetFileName(TCHAR_TO_UTF8(*TextureSourceFullPath));
					lTexture->SetTextureUse(FbxTexture::eStandard);
					lTexture->SetMappingType(FbxTexture::eUV);
					lTexture->ConnectDstProperty(FbxColorProperty);
					return true;
				}
			}
		}
	}
	return false;
}

/**
* Exports the profile_COMMON information for a material.
*/
FbxSurfaceMaterial* FFbxExporter::ExportMaterial(UMaterialInterface* MaterialInterface)
{
	if (Scene == nullptr || MaterialInterface == nullptr || MaterialInterface->GetMaterial() == nullptr) return nullptr;
	
	// Verify that this material has not already been exported:
	if (FbxMaterials.Find(MaterialInterface))
	{
		return *FbxMaterials.Find(MaterialInterface);
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = nullptr;

	UMaterial *Material = MaterialInterface->GetMaterial();
	
	// Set the shading model
	if (Material->GetShadingModel() == MSM_DefaultLit)
	{
		FbxMaterial = FbxSurfacePhong::Create(Scene, TCHAR_TO_UTF8(*MaterialInterface->GetName()));
		//((FbxSurfacePhong*)FbxMaterial)->Specular.Set(Material->Specular));
		//((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else // if (Material->ShadingModel == MSM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, TCHAR_TO_UTF8(*MaterialInterface->GetName()));
	}
	
	((FbxSurfaceLambert*)FbxMaterial)->TransparencyFactor.Set(Material->Opacity.Constant);

	// Fill in the profile_COMMON effect with the material information.
	//Fill the texture or constant
	if (!FillFbxTextureProperty(FbxSurfaceMaterial::sDiffuse, Material->BaseColor, FbxMaterial))
	{
		((FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(SetMaterialComponent(Material->BaseColor, true));
	}
	if (!FillFbxTextureProperty(FbxSurfaceMaterial::sEmissive, Material->EmissiveColor, FbxMaterial))
	{
		((FbxSurfaceLambert*)FbxMaterial)->Emissive.Set(SetMaterialComponent(Material->EmissiveColor, true));
	}

	//Always set the ambient to zero since we dont have ambient in unreal we want to avoid default value in DCCs
	((FbxSurfaceLambert*)FbxMaterial)->Ambient.Set(FbxDouble3(0.0, 0.0, 0.0));

	//Set the Normal map only if there is a texture sampler
	FillFbxTextureProperty(FbxSurfaceMaterial::sNormalMap, Material->Normal, FbxMaterial);

	if (Material->BlendMode == BLEND_Translucent)
	{
		if (!FillFbxTextureProperty(FbxSurfaceMaterial::sTransparentColor, Material->Opacity, FbxMaterial))
		{
			FbxDouble3 OpacityValue((FbxDouble)(Material->Opacity.Constant), (FbxDouble)(Material->Opacity.Constant), (FbxDouble)(Material->Opacity.Constant));
			((FbxSurfaceLambert*)FbxMaterial)->TransparentColor.Set(OpacityValue);
		}
		if (!FillFbxTextureProperty(FbxSurfaceMaterial::sTransparencyFactor, Material->OpacityMask, FbxMaterial))
		{
			((FbxSurfaceLambert*)FbxMaterial)->TransparencyFactor.Set(Material->OpacityMask.Constant);
		}
	}

	FbxMaterials.Add(MaterialInterface, FbxMaterial);
	
	return FbxMaterial;
}


FFbxExporter::FMatineeNodeNameAdapter::FMatineeNodeNameAdapter( AMatineeActor* InMatineeActor )
{
	MatineeActor = InMatineeActor;
}

FString FFbxExporter::FMatineeNodeNameAdapter::GetActorNodeName(const AActor* Actor)
{
	FString NodeName = Actor->GetName();
	const UInterpGroupInst* FoundGroupInst = MatineeActor->FindGroupInst( Actor );
	if( FoundGroupInst != NULL )
	{
		NodeName = FoundGroupInst->Group->GroupName.ToString();
	}

	// Maya does not support dashes.  Change all dashes to underscores
	NodeName = NodeName.Replace(TEXT("-"), TEXT("_") );

	return NodeName;
}


FFbxExporter::FMatineeAnimTrackAdapter::FMatineeAnimTrackAdapter( AMatineeActor* InMatineeActor )
{
	MatineeActor = InMatineeActor;
}

float FFbxExporter::FMatineeAnimTrackAdapter::GetAnimationStart() const
{
	return 0.f;
}


float FFbxExporter::FMatineeAnimTrackAdapter::GetAnimationLength() const
{
	return MatineeActor->MatineeData->InterpLength;
}


void FFbxExporter::FMatineeAnimTrackAdapter::UpdateAnimation( float Time )
{
	MatineeActor->UpdateInterp( Time, true );
}


/**
 * Exports the given Matinee sequence information into a FBX document.
 */
bool FFbxExporter::ExportMatinee(AMatineeActor* InMatineeActor)
{
	if (InMatineeActor == NULL || Scene == NULL) return false;

	// If the Matinee editor is not open, we need to initialize the sequence.
	//bool InitializeMatinee = InMatineeActor->MatineeData == NULL;
	//if (InitializeMatinee)
	//{
	//	InMatineeActor->InitInterp();
	//}

	// Iterate over the Matinee data groups and export the known tracks
	int32 GroupCount = InMatineeActor->GroupInst.Num();
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		UInterpGroupInst* Group = InMatineeActor->GroupInst[GroupIndex];
		AActor* Actor = Group->GetGroupActor();
		if (Group->Group == NULL || Actor == NULL) continue;

		FbxNode* FbxActor = FindActor(Actor); 
		// now it should export everybody
		check (FbxActor);

		// Look for the tracks that we currently support
		int32 TrackCount = FMath::Min(Group->TrackInst.Num(), Group->Group->InterpTracks.Num());
		for (int32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
		{
			UInterpTrackInst* TrackInst = Group->TrackInst[TrackIndex];
			UInterpTrack* Track = Group->Group->InterpTracks[TrackIndex];
			if ( Track->IsDisabled() == false )
			{
				if(TrackInst->IsA(UInterpTrackInstMove::StaticClass()) && Track->IsA(UInterpTrackMove::StaticClass()))
				{
					UInterpTrackInstMove* MoveTrackInst = (UInterpTrackInstMove*)TrackInst;
					UInterpTrackMove* MoveTrack = (UInterpTrackMove*)Track;
					ExportMatineeTrackMove(FbxActor, MoveTrackInst, MoveTrack, InMatineeActor->MatineeData->InterpLength);
				}
				else if(TrackInst->IsA(UInterpTrackInstFloatProp::StaticClass()) && Track->IsA(UInterpTrackFloatProp::StaticClass()))
				{
					UInterpTrackInstFloatProp* PropertyTrackInst = (UInterpTrackInstFloatProp*)TrackInst;
					UInterpTrackFloatProp* PropertyTrack = (UInterpTrackFloatProp*)Track;
					ExportMatineeTrackFloatProp(FbxActor, PropertyTrack);
				}
				else if(TrackInst->IsA(UInterpTrackInstAnimControl::StaticClass()) && Track->IsA(UInterpTrackAnimControl::StaticClass()))
				{
					USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(Actor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
					if(SkeletalMeshComp)
					{
						FMatineeAnimTrackAdapter AnimTrackAdapter(InMatineeActor);
						ExportAnimTrack(AnimTrackAdapter, Actor, SkeletalMeshComp);
					}
				}
			}
		}
	}

	//if (InitializeMatinee)
	//{
	//	InMatineeActor->TermInterp();
	//}

	DefaultCamera = NULL;
	return true;
}


FFbxExporter::FLevelSequenceNodeNameAdapter::FLevelSequenceNodeNameAdapter( UMovieScene* InMovieScene, IMovieScenePlayer* InMovieScenePlayer, FMovieSceneSequenceIDRef InSequenceID )
{
	MovieScene = InMovieScene;
	MovieScenePlayer = InMovieScenePlayer;
	SequenceID = InSequenceID;
}

FString FFbxExporter::FLevelSequenceNodeNameAdapter::GetActorNodeName(const AActor* Actor)
{
	FString NodeName = Actor->GetName();

	for ( const FMovieSceneBinding& MovieSceneBinding : MovieScene->GetBindings() )
	{
		for ( TWeakObjectPtr<UObject> RuntimeObject : MovieScenePlayer->FindBoundObjects(MovieSceneBinding.GetObjectGuid(), SequenceID) )
		{
			if (RuntimeObject.Get() == Actor)
			{
				NodeName = MovieSceneBinding.GetName();
			}
		}
	}

	// Maya does not support dashes.  Change all dashes to underscores		
	NodeName = NodeName.Replace(TEXT("-"), TEXT("_") );

	// Maya does not support spaces.  Change all spaces to underscores		
	NodeName = NodeName.Replace(TEXT(" "), TEXT("_") );

	return NodeName;
}

FFbxExporter::FLevelSequenceAnimTrackAdapter::FLevelSequenceAnimTrackAdapter( IMovieScenePlayer* InMovieScenePlayer, UMovieScene* InMovieScene )
{
	MovieScenePlayer = InMovieScenePlayer;
	MovieScene = InMovieScene;
}


float FFbxExporter::FLevelSequenceAnimTrackAdapter::GetAnimationStart() const
{
	return MovieScene->GetPlaybackRange().GetLowerBoundValue();
}

float FFbxExporter::FLevelSequenceAnimTrackAdapter::GetAnimationLength() const
{
	return MovieScene->GetPlaybackRange().Size<float>();
}


void FFbxExporter::FLevelSequenceAnimTrackAdapter::UpdateAnimation( float Time )
{
	FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(Time), MovieScenePlayer->GetPlaybackStatus()).SetHasJumped(true);
	return MovieScenePlayer->GetEvaluationTemplate().Evaluate( Context, *MovieScenePlayer );
}


bool FFbxExporter::ExportLevelSequence( UMovieScene* MovieScene, const TArray<FGuid>& Bindings, IMovieScenePlayer* MovieScenePlayer, FMovieSceneSequenceIDRef SequenceID )
{
	if ( MovieScene == nullptr || MovieScenePlayer == nullptr )
	{
		return false;
	}
			
	for ( const FMovieSceneBinding& MovieSceneBinding : MovieScene->GetBindings() )
	{
		// If there are specific bindings to export, export those only
		if (Bindings.Num() != 0 && !Bindings.Contains(MovieSceneBinding.GetObjectGuid()))
		{
			continue;
		}

		for ( TWeakObjectPtr<UObject> RuntimeObject : MovieScenePlayer->FindBoundObjects(MovieSceneBinding.GetObjectGuid(), SequenceID) )
		{
			if ( RuntimeObject.IsValid() )
			{
				AActor* Actor = Cast<AActor>( RuntimeObject.Get() );
				UActorComponent* Component = Cast<UActorComponent>( RuntimeObject.Get() );
				if ( Actor == nullptr && Component != nullptr )
				{
					Actor = Component->GetOwner();
				}

				if ( Actor == nullptr )
				{
					continue;
				}

				FbxNode* FbxActor = FindActor( Actor );

				// now it should export everybody
				if ( FbxActor )
				{
					USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>( Actor->GetComponentByClass( USkeletalMeshComponent::StaticClass() ) );
					
					const bool bSkip3DTransformTrack = SkeletalMeshComp && ExportOptions->MapSkeletalMotionToRoot;

					// Look for the tracks that we currently support
					for ( UMovieSceneTrack* Track : MovieSceneBinding.GetTracks() )
					{
						if ( Track->IsA( UMovieScene3DTransformTrack::StaticClass() ) && !bSkip3DTransformTrack )
						{
							UMovieScene3DTransformTrack* TransformTrack = (UMovieScene3DTransformTrack*)Track;
							ExportLevelSequence3DTransformTrack( *FbxActor, *TransformTrack, Actor, MovieScene->GetPlaybackRange() );
						}
						else if ( Track->IsA( UMovieSceneFloatTrack::StaticClass() ) )
						{
							UMovieSceneFloatTrack* FloatTrack = (UMovieSceneFloatTrack*)Track;
							ExportLevelSequenceFloatTrack( *FbxActor, *FloatTrack );
						}
						else if ( Track->IsA( UMovieSceneSkeletalAnimationTrack::StaticClass() ) )
						{
							if ( SkeletalMeshComp )
							{
								FLevelSequenceAnimTrackAdapter AnimTrackAdapter( MovieScenePlayer, MovieScene );
								ExportAnimTrack( AnimTrackAdapter, Actor, SkeletalMeshComp );
							}
						}
					}
				}
			}
		}
	}

	return true;
}


/**
 * Exports a scene node with the placement indicated by a given actor.
 * This scene node will always have two transformations: one translation vector and one Euler rotation.
 */
FbxNode* FFbxExporter::ExportActor(AActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter )
{
	// Verify that this actor isn't already exported, create a structure for it
	// and buffer it.
	FbxNode* ActorNode = FindActor(Actor);
	if (ActorNode == NULL)
	{
		FString FbxNodeName = NodeNameAdapter.GetActorNodeName(Actor);
		FbxNodeName = GetFbxObjectName(FbxNodeName, NodeNameAdapter);
		ActorNode = FbxNode::Create(Scene, TCHAR_TO_UTF8(*FbxNodeName));

		AActor* ParentActor = Actor->GetAttachParentActor();
		// this doesn't work with skeletalmeshcomponent
		FbxNode* ParentNode = FindActor(ParentActor);
		FVector ActorLocation, ActorRotation, ActorScale;

		// For cameras and lights: always add a rotation to get the correct coordinate system.
        FTransform RotationDirectionConvert = FTransform::Identity;
		if (Actor->IsA(ACameraActor::StaticClass()) || Actor->IsA(ALight::StaticClass()))
		{
			if (Actor->IsA(ACameraActor::StaticClass()))
			{
                FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
				RotationDirectionConvert = FTransform(Rotator);
			}
			else if (Actor->IsA(ALight::StaticClass()))
			{
				FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
				RotationDirectionConvert = FTransform(Rotator);
			}
		}

		//If the parent is the root or is not export use the root node as the parent
		if (bKeepHierarchy && ParentNode)
		{
			// Set the default position of the actor on the transforms
			// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
			const FTransform RelativeTransform = RotationDirectionConvert * Actor->GetTransform().GetRelativeTransform(ParentActor->GetTransform());
			ActorLocation = RelativeTransform.GetTranslation();
			ActorRotation = RelativeTransform.GetRotation().Euler();
			ActorScale = RelativeTransform.GetScale3D();
		}
		else
		{
			ParentNode = Scene->GetRootNode();
			// Set the default position of the actor on the transforms
			// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
			if (ParentActor != NULL)
			{
				//In case the parent was not export, get the absolute transform
				const FTransform AbsoluteTransform = RotationDirectionConvert * Actor->GetTransform();
				ActorLocation = AbsoluteTransform.GetTranslation();
				ActorRotation = AbsoluteTransform.GetRotation().Euler();
				ActorScale = AbsoluteTransform.GetScale3D();
			}
			else
			{
				const FTransform ConvertedTransform = RotationDirectionConvert * Actor->GetTransform();
				ActorLocation = ConvertedTransform.GetTranslation();
				ActorRotation = ConvertedTransform.GetRotation().Euler();
				ActorScale = ConvertedTransform.GetScale3D();
			}
		}

		ParentNode->AddChild(ActorNode);
		FbxActors.Add(Actor, ActorNode);

		// Set the default position of the actor on the transforms
		// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		ActorNode->LclTranslation.Set(Converter.ConvertToFbxPos(ActorLocation));
		ActorNode->LclRotation.Set(Converter.ConvertToFbxRot(ActorRotation));
		ActorNode->LclScaling.Set(Converter.ConvertToFbxScale(ActorScale));
	
		if( bExportComponents )
		{
			TInlineComponentArray<USceneComponent*> SceneComponents;
			Actor->GetComponents(SceneComponents);

			TInlineComponentArray<USceneComponent*> ComponentsToExport;
			for( int32 ComponentIndex = 0; ComponentIndex < SceneComponents.Num(); ++ComponentIndex )
			{
				USceneComponent* Component = SceneComponents[ComponentIndex];
	
				if (Component && Component->bHiddenInGame)
				{
					//Skip hidden component like camera mesh or other editor helper
					continue;
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );
				UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>( Component );

				if( StaticMeshComp && StaticMeshComp->GetStaticMesh())
				{
					ComponentsToExport.Add( StaticMeshComp );
				}
				else if( SkelMeshComp && SkelMeshComp->SkeletalMesh )
				{
					ComponentsToExport.Add( SkelMeshComp );
				}
				else if (Component->IsA(UCameraComponent::StaticClass()))
				{
					ComponentsToExport.Add(Component);
				}
				else if (Component->IsA(ULightComponent::StaticClass()))
				{
					ComponentsToExport.Add(Component);
				}
				else if (ChildActorComp && ChildActorComp->GetChildActor())
				{
					ComponentsToExport.Add(ChildActorComp);
				}
			}

			for( int32 CompIndex = 0; CompIndex < ComponentsToExport.Num(); ++CompIndex )
			{
				USceneComponent* Component = ComponentsToExport[CompIndex];

                RotationDirectionConvert = FTransform::Identity;
                // For cameras and lights: always add a rotation to get the correct coordinate system.
				if (Component->IsA(UCameraComponent::StaticClass()) || Component->IsA(ULightComponent::StaticClass()))
				{
					if (Component->IsA(UCameraComponent::StaticClass()))
					{
                    	FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
						RotationDirectionConvert = FTransform(Rotator);
					}
					else if (Component->IsA(ULightComponent::StaticClass()))
					{
						FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
						RotationDirectionConvert = FTransform(Rotator);
					}
				}

				FbxNode* ExportNode = ActorNode;
				if( ComponentsToExport.Num() > 1 )
				{
					// This actor has multiple components
					// create a child node under the actor for each component
					FbxNode* CompNode = FbxNode::Create(Scene, TCHAR_TO_UTF8(*Component->GetName()));
					
					if( Component != Actor->GetRootComponent() )
					{
						// Transform is relative to the root component
						const FTransform RelativeTransform = RotationDirectionConvert * Component->GetComponentToWorld().GetRelativeTransform(Actor->GetTransform());
						CompNode->LclTranslation.Set(Converter.ConvertToFbxPos(RelativeTransform.GetTranslation()));
						CompNode->LclRotation.Set(Converter.ConvertToFbxRot(RelativeTransform.GetRotation().Euler()));
						CompNode->LclScaling.Set(Converter.ConvertToFbxScale(RelativeTransform.GetScale3D()));
					}

					ExportNode = CompNode;
					ActorNode->AddChild(CompNode);
				}
				else if(Component != Actor->GetRootComponent())
				{
					// Merge the component relative transform in the ActorNode transform since this is the only component to export and its not the root
					const FTransform RelativeTransform = RotationDirectionConvert * Component->GetComponentToWorld().GetRelativeTransform(Actor->GetTransform());

					FTransform ActorTransform(FRotator::MakeFromEuler(ActorRotation).Quaternion(), ActorLocation, ActorScale);
					FTransform TotalTransform = RelativeTransform;
					TotalTransform.Accumulate(ActorTransform);

					ActorNode->LclTranslation.Set(Converter.ConvertToFbxPos(TotalTransform.GetLocation()));
					ActorNode->LclRotation.Set(Converter.ConvertToFbxRot(TotalTransform.GetRotation().Euler()));
					ActorNode->LclScaling.Set(Converter.ConvertToFbxScale(TotalTransform.GetScale3D()));
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );
				UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>( Component );

				if (StaticMeshComp && StaticMeshComp->GetStaticMesh())
				{
					if (USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>(StaticMeshComp))
					{
						ExportSplineMeshToFbx(SplineMeshComp, *SplineMeshComp->GetName(), ExportNode);
					}
					else if (UInstancedStaticMeshComponent* InstancedMeshComp = Cast<UInstancedStaticMeshComponent>(StaticMeshComp))
					{
						ExportInstancedMeshToFbx(InstancedMeshComp, *InstancedMeshComp->GetName(), ExportNode);
					}
					else
					{
						const int32 LODIndex = (StaticMeshComp->ForcedLodModel > 0 ? StaticMeshComp->ForcedLodModel - 1 : /* auto-select*/ 0);
						ExportStaticMeshToFbx(StaticMeshComp->GetStaticMesh(), LODIndex, *StaticMeshComp->GetName(), ExportNode);
					}
				}
				else if (SkelMeshComp && SkelMeshComp->SkeletalMesh)
				{
					ExportSkeletalMeshComponent(SkelMeshComp, *SkelMeshComp->GetName(), ExportNode);
				}
				else if (Component->IsA(UCameraComponent::StaticClass()))
				{
					FbxCamera* Camera = FbxCamera::Create(Scene, TCHAR_TO_UTF8(*Component->GetName()));
					FillFbxCameraAttribute(ActorNode, Camera, Cast<UCameraComponent>(Component));
					ExportNode->SetNodeAttribute(Camera);
				}
				else if (Component->IsA(ULightComponent::StaticClass()))
				{
					FbxLight* Light = FbxLight::Create(Scene, TCHAR_TO_UTF8(*Component->GetName()));
					FillFbxLightAttribute(Light, ActorNode, Cast<ULightComponent>(Component));
					ExportNode->SetNodeAttribute(Light);
				}
				else if (ChildActorComp && ChildActorComp->GetChildActor())
				{
					FbxNode* ChildActorNode = ExportActor(ChildActorComp->GetChildActor(), true, NodeNameAdapter);
					FbxActors.Add(ChildActorComp->GetChildActor(), ChildActorNode);
				}
			}
		}
		
	}

	return ActorNode;
}

/**
 * Exports the Matinee movement track into the FBX animation library.
 */
void FFbxExporter::ExportMatineeTrackMove(FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, float InterpLength)
{
	if (FbxActor == NULL || MoveTrack == NULL) return;
	
	// For the Y and Z angular rotations, we need to invert the relative animation frames,
	// While keeping the standard angles constant.

	if (MoveTrack != NULL)
	{
		FbxAnimLayer* BaseLayer = AnimStack->GetMember<FbxAnimLayer>(0);
		FbxAnimCurve* Curve;

		bool bPosCurve = true;
		if( MoveTrack->SubTracks.Num() == 0 )
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, false, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = false;

			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, true, InterpLength);
		}
		else
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[0]), MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[1]), MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[2]), MoveTrackInst, bPosCurve, 2, false, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = false;

			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[3]), MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[4]), MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[5]), MoveTrackInst, bPosCurve, 2, true, InterpLength);
		}
	}
}

/**
 * Exports the Matinee float property track into the FBX animation library.
 */
void FFbxExporter::ExportMatineeTrackFloatProp(FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack)
{
	if (FbxActor == NULL || PropTrack == NULL) return;
	
	FbxNodeAttribute* FbxNodeAttr = NULL;
	// camera and light is appended on the fbx pivot node
	if( FbxActor->GetChild(0) )
	{
		FbxNodeAttr = ((FbxNode*)FbxActor->GetChild(0))->GetNodeAttribute();

		if (FbxNodeAttr == NULL) return;
	}
	
	FbxProperty Property;
	FString PropertyName = PropTrack->PropertyName.ToString();
	bool IsFoV = false;
	// most properties are created as user property, only FOV of camera in FBX supports animation
	if (PropertyName == "Intensity")
	{
		Property = FbxActor->FindProperty("UE_Intensity", false);
	}
	else if (PropertyName == "FalloffExponent")
	{
		Property = FbxActor->FindProperty("UE_FalloffExponent", false);
	}
	else if (PropertyName == "AttenuationRadius")
	{
		Property = FbxActor->FindProperty("UE_Radius", false);
	}
	else if (PropertyName == "FOVAngle" && FbxNodeAttr )
	{
		Property = ((FbxCamera*)FbxNodeAttr)->FocalLength;
		IsFoV = true;
	}
	else if (PropertyName == "AspectRatio")
	{
		Property = FbxActor->FindProperty("UE_AspectRatio", false);
	}
	else if (PropertyName == "MotionBlur_Amount")
	{
		Property = FbxActor->FindProperty("UE_MotionBlur_Amount", false);
	}

	if (Property != 0)
	{
		ExportAnimatedFloat(&Property, &PropTrack->FloatTrack, IsFoV);
	}
}

void ConvertInterpToFBX(uint8 UnrealInterpMode, FbxAnimCurveDef::EInterpolationType& Interpolation, FbxAnimCurveDef::ETangentMode& Tangent)
{
	switch(UnrealInterpMode)
	{
	case CIM_Linear:
		Interpolation = FbxAnimCurveDef::eInterpolationLinear;
		Tangent = FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveAuto:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = FbxAnimCurveDef::eTangentAuto;
		break;
	case CIM_Constant:
		Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		Tangent = (FbxAnimCurveDef::ETangentMode)FbxAnimCurveDef::eConstantStandard;
		break;
	case CIM_CurveUser:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveBreak:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (FbxAnimCurveDef::ETangentMode) FbxAnimCurveDef::eTangentBreak;
		break;
	case CIM_CurveAutoClamped:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (FbxAnimCurveDef::ETangentMode) (FbxAnimCurveDef::eTangentAuto | FbxAnimCurveDef::eTangentGenericClamp);
		break;
	case CIM_Unknown:  // ???
		Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		Tangent = FbxAnimCurveDef::eTangentAuto;
		break;
	}
}


// float-float comparison that allows for a certain error in the floating point values
// due to floating-point operations never being exact.
static bool IsEquivalent(float a, float b, float Tolerance = KINDA_SMALL_NUMBER)
{
	return (a - b) > -Tolerance && (a - b) < Tolerance;
}

// Set the default FPS to 30 because the SetupMatinee MEL script sets up Maya this way.
const float FFbxExporter::BakeTransformsFPS = DEFAULT_SAMPLERATE;

/**
 * Exports a given interpolation curve into the FBX animation curve.
 */
void FFbxExporter::ExportAnimatedVector(FbxAnimCurve* FbxCurve, const char* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength)
{
	if (Scene == NULL) return;
	
	FInterpCurveVector* Curve = bPosCurve ? &MoveTrack->PosTrack : &MoveTrack->EulerTrack;

	if (Curve == NULL || CurveIndex >= 3) return;

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	int32 KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) : Curve->Points.Num();

	// Write out the key times from the curve to the FBX curve.
	TArray<float> KeyTimes;
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		float KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Curve->Points[KeyIndex].InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes[KeyIndex-1] + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes[KeyIndex-1] + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.Add(KeyTime);
	}

	// Write out the key values from the curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			// If we are baking trnasforms, ask the movement track what are transforms are at the given time.
			if( bBakeKeys )
			{
				MoveTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes[KeyIndex], KeyPosition, KeyRotation);
			}
			// Else, this information is already present in the position and rotation tracks stored on the movement track.
			else
			{
				KeyPosition = MoveTrack->PosTrack.Points[KeyIndex].OutVal;
				KeyRotation = FRotator( FQuat::MakeFromEuler(MoveTrack->EulerTrack.Points[KeyIndex].OutVal) );
			}

			if (bKeepHierarchy)
			{
				if(bPosCurve)
				{
					FinalOutVec = KeyPosition;
				}
				else
				{
					FinalOutVec = KeyRotation.Euler();
				}
			}
			else
			{
				FVector WorldSpacePos;
				FRotator WorldSpaceRotator;
				MoveTrack->ComputeWorldSpaceKeyTransform(
					MoveTrackInst,
					KeyPosition,
					KeyRotation,
					WorldSpacePos,			// Out
					WorldSpaceRotator);	// Out

				if(bPosCurve)
				{
					FinalOutVec = WorldSpacePos;
				}
				else
				{
					FinalOutVec = WorldSpaceRotator.Euler();
				}
			}
		}

		float KeyTime = KeyTimes[KeyIndex];
		float OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		float FbxKeyValue = bNegative ? -OutValue : OutValue;
		
		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);
		

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		
		if( !bBakeKeys )
		{
			ConvertInterpToFBX(Curve->Points[KeyIndex].InterpMode, Interpolation, Tangent);
		}

		if (bBakeKeys || Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			FInterpCurvePoint<FVector>& Key = Curve->Points[KeyIndex];

			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( Interpolation == FbxAnimCurveDef::eInterpolationCubic )
			{
				float OutTangentValue = (CurveIndex == 0) ? Key.LeaveTangent.X : (CurveIndex == 1) ? Key.LeaveTangent.Y : Key.LeaveTangent.Z;
				float OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes[KeyIndex + 1] - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				float OutTangentY = OutTangentValue / 3.0f;
				float RightTangent =  OutTangentY / OutTangentX ;
				
				float NextLeftTangent = 0;
				
				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<FVector>& NextKey = Curve->Points[KeyIndex + 1];
					float NextInTangentValue = (CurveIndex == 0) ? NextKey.ArriveTangent.X : (CurveIndex == 1) ? NextKey.ArriveTangent.Y : NextKey.ArriveTangent.Z;
					float NextInTangentX;
					NextInTangentX = (KeyTimes[KeyIndex + 1] - KeyTimes[KeyIndex]) / 3.0f;
					float NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void FFbxExporter::ExportMoveSubTrack(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength)
{
	if (Scene == NULL || FbxCurve == NULL) return;

	FInterpCurveFloat* Curve = &SubTrack->FloatTrack;
	UInterpTrackMove* ParentTrack = CastChecked<UInterpTrackMove>( SubTrack->GetOuter() );

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	int32 KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) : Curve->Points.Num();

	// Write out the key times from the curve to the FBX curve.
	TArray<float> KeyTimes;
	for(int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		float KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Curve->Points[KeyIndex].InVal;
		if(KeyTimes.Num() && KeyTime < KeyTimes[KeyIndex-1] + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes[KeyIndex-1] + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.Add(KeyTime);
	}
	// Write out the key values from the curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			ParentTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes[KeyIndex], KeyPosition, KeyRotation);
		
			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			ParentTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		float KeyTime = KeyTimes[KeyIndex];
		float OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		float FbxKeyValue = bNegative ? -OutValue : OutValue;

		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		
		if (bBakeKeys || Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];
			ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);

			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( Interpolation == FbxAnimCurveDef::eInterpolationCubic )
			{
				float OutTangentValue = Key.LeaveTangent;
				float OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes[KeyIndex + 1] - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				float OutTangentY = OutTangentValue / 3.0f;
				float RightTangent =  OutTangentY / OutTangentX ;

				float NextLeftTangent = 0;

				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<float>& NextKey = Curve->Points[KeyIndex + 1];
					float NextInTangentValue =  Key.LeaveTangent;
					float NextInTangentX;
					NextInTangentX = (KeyTimes[KeyIndex + 1] - KeyTimes[KeyIndex]) / 3.0f;
					float NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void FFbxExporter::ExportAnimatedFloat(FbxProperty* FbxProperty, FInterpCurveFloat* Curve, bool IsCameraFoV)
{
	if (FbxProperty == NULL || Curve == NULL) return;

	// do not export an empty anim curve
	if (Curve->Points.Num() == 0) return;

	FbxAnimCurve* AnimCurve = FbxAnimCurve::Create(Scene, "");
	FbxAnimCurveNode* CurveNode = FbxProperty->GetCurveNode(true);
	if (!CurveNode)
	{
		return;
	}
	CurveNode->SetChannelValue<double>(0U, Curve->Points[0].OutVal);
	CurveNode->ConnectToChannel(AnimCurve, 0U);

	// Write out the key times from the curve to the FBX curve.
	int32 KeyCount = Curve->Points.Num();
	TArray<float> KeyTimes;
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		float KeyTime = Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes[KeyIndex-1] + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes[KeyIndex-1] + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.Add(KeyTime);
	}

	// Write out the key values from the curve to the FBX curve.
	AnimCurve->KeyModifyBegin();
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];
		float KeyTime = KeyTimes[KeyIndex];
		
		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = AnimCurve->KeyAdd(Time);
		float OutVal = (IsCameraFoV && DefaultCamera)? DefaultCamera->ComputeFocalLength(Key.OutVal): (float)Key.OutVal;

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);
		
		if (Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			AnimCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves.
			float OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes[KeyIndex + 1] - KeyTime) / 3.0f : 0.333f;
			float OutTangentY = Key.LeaveTangent / 3.0f;
			float RightTangent =  OutTangentY / OutTangentX ;

			float NextLeftTangent = 0;

			if (KeyIndex < KeyCount - 1)
			{
				FInterpCurvePoint<float>& NextKey = Curve->Points[KeyIndex + 1];
				float NextInTangentX;
				NextInTangentX = (KeyTimes[KeyIndex + 1] - KeyTimes[KeyIndex]) / 3.0f;
				float NextInTangentY = NextKey.ArriveTangent / 3.0f;
				NextLeftTangent =  NextInTangentY / NextInTangentX ;
			}

			AnimCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent, RightTangent, NextLeftTangent );

		}
	}
	AnimCurve->KeyModifyEnd();
}

FKeyHandle FindRichCurveKey(FRichCurve& InCurve, float InKeyTime)
{
	for ( auto It = InCurve.GetKeyHandleIterator(); It; ++It )
	{
		if (IsEquivalent(InCurve.GetKeyTime(It.Key()), InKeyTime))
		{
			return It.Key();
		}
	}
	return FKeyHandle();
}


void RichCurveInterpolationToFbxInterpolation(ERichCurveInterpMode InInterpolation, ERichCurveTangentMode InTangentMode, FbxAnimCurveDef::EInterpolationType &OutInterpolation, FbxAnimCurveDef::ETangentMode &OutTangentMode)
{
	if (InInterpolation == ERichCurveInterpMode::RCIM_Cubic)
	{
		OutInterpolation = FbxAnimCurveDef::eInterpolationCubic;
		OutTangentMode = FbxAnimCurveDef::eTangentUser;

		// Always set tangent on the fbx key, so OutTangentMode should explicitly be eTangentUser unless Break.
		if (InTangentMode == RCTM_Break)
		{
			OutTangentMode = FbxAnimCurveDef::eTangentBreak;
		}
	}
	else if (InInterpolation == ERichCurveInterpMode::RCIM_Linear)
	{
		OutInterpolation = FbxAnimCurveDef::eInterpolationLinear;
		OutTangentMode = FbxAnimCurveDef::eTangentUser;
	}
	else if (InInterpolation == ERichCurveInterpMode::RCIM_Constant)
	{
		OutInterpolation = FbxAnimCurveDef::eInterpolationConstant;
		OutTangentMode = (FbxAnimCurveDef::ETangentMode)FbxAnimCurveDef::eConstantStandard;
	}
	else
	{
		OutInterpolation = FbxAnimCurveDef::eInterpolationCubic;
		OutTangentMode = FbxAnimCurveDef::eTangentUser;
	}
}

void FFbxExporter::ExportRichCurveToFbxCurve(FbxAnimCurve& InFbxCurve, FRichCurve& InRichCurve, ERichCurveValueMode ValueMode, bool bNegative)
{
	InFbxCurve.KeyModifyBegin();

	for ( auto It = InRichCurve.GetKeyHandleIterator(); It; ++It )
	{
		FKeyHandle KeyHandle = It.Key();
		float KeyTime = InRichCurve.GetKeyTime(KeyHandle);
		float Value = ValueMode == ERichCurveValueMode::Fov
			? DefaultCamera->ComputeFocalLength( InRichCurve.Eval( KeyTime ) )
			: InRichCurve.Eval( KeyTime );

		FbxTime FbxTime;
		FbxAnimCurveKey FbxKey;
		FbxTime.SetSecondDouble((float)KeyTime);

		int FbxKeyIndex = InFbxCurve.KeyAdd(FbxTime);

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;

		RichCurveInterpolationToFbxInterpolation(InRichCurve.GetKeyInterpMode(KeyHandle), InRichCurve.GetKeyTangentMode(KeyHandle), Interpolation, Tangent);

		if (bNegative)
		{
			Value = -Value;
		}

		if (Interpolation == FbxAnimCurveDef::eInterpolationCubic)
		{
			FRichCurveKey RichCurveKey = InRichCurve.GetKey( KeyHandle );

			FKeyHandle NextKeyHandle = InRichCurve.GetNextKey(KeyHandle);
			if (InRichCurve.IsKeyHandleValid(NextKeyHandle))
			{
				float LeaveTangent = RichCurveKey.LeaveTangent;
				float NextArriveTangent = InRichCurve.GetKey(NextKeyHandle).ArriveTangent;

				if (bNegative)
				{
					LeaveTangent = -LeaveTangent;
					NextArriveTangent = -NextArriveTangent;
				}

				InFbxCurve.KeySet(FbxKeyIndex, FbxTime, Value, Interpolation, Tangent, LeaveTangent, NextArriveTangent );
			}
			else
			{
				InFbxCurve.KeySet(FbxKeyIndex, FbxTime, Value, Interpolation, Tangent);
			}
		}
		else
		{
			InFbxCurve.KeySet(FbxKeyIndex, FbxTime, Value, Interpolation, Tangent);
		}
	}
	InFbxCurve.KeyModifyEnd();
}

void FFbxExporter::ExportLevelSequence3DTransformTrack( FbxNode& FbxActor, UMovieScene3DTransformTrack& TransformTrack, AActor* Actor, const TRange<float>& InPlaybackRange )
{
	FbxAnimLayer* BaseLayer = AnimStack->GetMember<FbxAnimLayer>( 0 );

	const bool bIsCameraActor = Actor->IsA(ACameraActor::StaticClass());
	const bool bIsLightActor = Actor->IsA(ALight::StaticClass());
	const bool bBakeRotations = bIsCameraActor || bIsLightActor;

	// TODO: Support more than one section?
	UMovieScene3DTransformSection* TransformSection = TransformTrack.GetAllSections().Num() > 0
		? Cast<UMovieScene3DTransformSection>(TransformTrack.GetAllSections()[0])
		: nullptr;

	if (TransformSection == nullptr)
	{
		return;
	}

	FbxAnimCurveNode* TranslationNode = FbxActor.LclTranslation.GetCurveNode( BaseLayer, true );
	FbxAnimCurveNode* RotationNode = FbxActor.LclRotation.GetCurveNode( BaseLayer, true );
	FbxAnimCurveNode* ScaleNode = FbxActor.LclScaling.GetCurveNode( BaseLayer, true );

	FbxAnimCurve* FbxCurveTransX = FbxActor.LclTranslation.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
	FbxAnimCurve* FbxCurveTransY = FbxActor.LclTranslation.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
	FbxAnimCurve* FbxCurveTransZ = FbxActor.LclTranslation.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

	FbxAnimCurve* FbxCurveRotX = FbxActor.LclRotation.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
	FbxAnimCurve* FbxCurveRotY = FbxActor.LclRotation.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
	FbxAnimCurve* FbxCurveRotZ = FbxActor.LclRotation.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

	FbxAnimCurve* FbxCurveScaleX = FbxActor.LclScaling.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
	FbxAnimCurve* FbxCurveScaleY = FbxActor.LclScaling.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
	FbxAnimCurve* FbxCurveScaleZ = FbxActor.LclScaling.GetCurve( BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );
		
	// Translation
	ExportRichCurveToFbxCurve(*FbxCurveTransX, TransformSection->GetTranslationCurve( EAxis::X ), ERichCurveValueMode::Default, false);
	ExportRichCurveToFbxCurve(*FbxCurveTransY, TransformSection->GetTranslationCurve( EAxis::Y ), ERichCurveValueMode::Default, true);
	ExportRichCurveToFbxCurve(*FbxCurveTransZ, TransformSection->GetTranslationCurve( EAxis::Z ), ERichCurveValueMode::Default, false);

	// Scale - don't generate scale keys for cameras
	if (!bIsCameraActor)
	{
		ExportRichCurveToFbxCurve(*FbxCurveScaleX, TransformSection->GetScaleCurve( EAxis::X ), ERichCurveValueMode::Default, false);
		ExportRichCurveToFbxCurve(*FbxCurveScaleY, TransformSection->GetScaleCurve( EAxis::Y ), ERichCurveValueMode::Default, false);
		ExportRichCurveToFbxCurve(*FbxCurveScaleZ, TransformSection->GetScaleCurve( EAxis::Z ), ERichCurveValueMode::Default, false);
	}

	// Rotation - bake rotation for cameras and lights
	if (!bBakeRotations)
	{
		ExportRichCurveToFbxCurve(*FbxCurveRotX, TransformSection->GetRotationCurve( EAxis::X ), ERichCurveValueMode::Default, false);
		ExportRichCurveToFbxCurve(*FbxCurveRotY, TransformSection->GetRotationCurve( EAxis::Y ), ERichCurveValueMode::Default, true);
		ExportRichCurveToFbxCurve(*FbxCurveRotZ, TransformSection->GetRotationCurve( EAxis::Z ), ERichCurveValueMode::Default, true);
	}
	else
	{
		FTransform RotationDirectionConvert;
		if (bIsCameraActor)
		{
			FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
			RotationDirectionConvert = FTransform(Rotator);
		}
		else if (bIsLightActor)
		{
			FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
			RotationDirectionConvert = FTransform(Rotator);
		}

		FbxCurveRotX->KeyModifyBegin();
		FbxCurveRotY->KeyModifyBegin();
		FbxCurveRotZ->KeyModifyBegin();

		float InterpLength = InPlaybackRange.GetUpperBoundValue() - InPlaybackRange.GetLowerBoundValue();
		int NumKeys = InterpLength * BakeTransformsFPS;
		for ( int KeyIndex = 0; KeyIndex < NumKeys; KeyIndex++ )
		{
			float KeyTime = InPlaybackRange.GetLowerBoundValue() + ( KeyIndex * InterpLength / NumKeys );

			FVector Trans;
			TransformSection->EvalTranslation(KeyTime, Trans);
			FRotator Rotator;
			TransformSection->EvalRotation(KeyTime, Rotator);
			FVector Scale;
			TransformSection->EvalScale(KeyTime, Scale);

			FTransform RelativeTransform;
			RelativeTransform.SetTranslation(Trans);
			RelativeTransform.SetRotation(Rotator.Quaternion());
			RelativeTransform.SetScale3D(Scale);

			RelativeTransform = RotationDirectionConvert * RelativeTransform;

			FbxVector4 KeyTrans = Converter.ConvertToFbxPos(RelativeTransform.GetTranslation());
			FbxVector4 KeyRot = Converter.ConvertToFbxRot(RelativeTransform.GetRotation().Euler());
			FbxVector4 KeyScale = Converter.ConvertToFbxScale(RelativeTransform.GetScale3D());

			FbxTime FbxTime;
			FbxTime.SetSecondDouble((float)KeyTime);

			FbxCurveRotX->KeySet(FbxCurveRotX->KeyAdd(FbxTime), FbxTime, KeyRot[0]);
			FbxCurveRotY->KeySet(FbxCurveRotY->KeyAdd(FbxTime), FbxTime, KeyRot[1]);
			FbxCurveRotZ->KeySet(FbxCurveRotZ->KeyAdd(FbxTime), FbxTime, KeyRot[2]);
		}

		FbxCurveRotX->KeyModifyEnd();
		FbxCurveRotY->KeyModifyEnd();
		FbxCurveRotZ->KeyModifyEnd();
	}
}

void FFbxExporter::ExportLevelSequenceFloatTrack( FbxNode& FbxActor, UMovieSceneFloatTrack& FloatTrack )
{
	// TODO: Support more than one section?
	UMovieSceneFloatSection* FloatSection = FloatTrack.GetAllSections().Num() > 0
		? Cast<UMovieSceneFloatSection>( FloatTrack.GetAllSections()[0] )
		: nullptr;

	if ( FloatSection == nullptr || FloatSection->GetFloatCurve().GetNumKeys() == 0 )
	{
		return;
	}

	FbxCamera* FbxCamera = FbxActor.GetCamera();

	FbxProperty Property;
	FString PropertyName = FloatTrack.GetTrackName().ToString();
	bool IsFoV = false;
	// most properties are created as user property, only FOV of camera in FBX supports animation
	if ( PropertyName == "Intensity" )
	{
		Property = FbxActor.FindProperty( "UE_Intensity", false );
	}
	else if ( PropertyName == "FalloffExponent" )
	{
		Property = FbxActor.FindProperty( "UE_FalloffExponent", false );
	}
	else if ( PropertyName == "AttenuationRadius" )
	{
		Property = FbxActor.FindProperty( "UE_Radius", false );
	}
	else if ( PropertyName == "FOVAngle" && FbxCamera )
	{
		Property = FbxCamera->FocalLength;
		IsFoV = true;
	}
	else if ( PropertyName == "CurrentFocalLength" && FbxCamera )
	{
		Property = FbxCamera->FocalLength;
	}
	else if ( PropertyName == "AspectRatio" )
	{
		Property = FbxActor.FindProperty( "UE_AspectRatio", false );
	}
	else if ( PropertyName == "MotionBlur_Amount" )
	{
		Property = FbxActor.FindProperty( "UE_MotionBlur_Amount", false );
	}

	if ( Property != 0 )
	{
		FRichCurve& FloatCurve = FloatSection->GetFloatCurve();

		FbxAnimCurve* AnimCurve = FbxAnimCurve::Create( Scene, "" );
		FbxAnimCurveNode* CurveNode = Property.GetCurveNode( true );
		if ( !CurveNode )
		{
			return;
		}

		CurveNode->SetChannelValue<double>( 0U, FloatCurve.GetDefaultValue() );
		CurveNode->ConnectToChannel( AnimCurve, 0U );

		ExportRichCurveToFbxCurve(*AnimCurve, FloatCurve, IsFoV ? ERichCurveValueMode::Fov : ERichCurveValueMode::Default);
	}
}
		
/**
 * Finds the given actor in the already-exported list of structures
 */
FbxNode* FFbxExporter::FindActor(AActor* Actor)
{
	if (FbxActors.Find(Actor))
	{
		return *FbxActors.Find(Actor);
	}
	else
	{
		return NULL;
	}
}

bool FFbxExporter::FindSkeleton(const USkeletalMeshComponent* SkelComp, TArray<FbxNode*>& BoneNodes)
{
	FbxNode** SkelRoot = FbxSkeletonRoots.Find(SkelComp);

	if (SkelRoot)
	{
		BoneNodes.Empty();
		GetSkeleton(*SkelRoot, BoneNodes);

		return true;
	}

	return false;
}
/**
 * Determines the UVS to weld when exporting a Static Mesh
 * 
 * @param VertRemap		Index of each UV (out)
 * @param UniqueVerts	
 */
void DetermineUVsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshVertexBuffer& VertexBuffer, int32 TexCoordSourceIndex)
{
	const int32 VertexCount = VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector2D,int32> HashedVerts;
	for(int32 Vertex=0; Vertex < VertexCount; Vertex++)
	{
		const FVector2D& PositionA = VertexBuffer.GetVertexUV(Vertex,TexCoordSourceIndex);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if ( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add(Vertex);
			VertRemap[Vertex] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[Vertex] = *FoundIndex;
		}
	}
}

void DetermineVertsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshLODResources& RenderMesh)
{
	const int32 VertexCount = RenderMesh.VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts 
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector,int32> HashedVerts;
	for(int32 a=0; a < VertexCount; a++)
	{
		const FVector& PositionA = RenderMesh.PositionVertexBuffer.VertexPosition(a);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if ( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add(a);
			VertRemap[a] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[a] = *FoundIndex;
		}
	}
}

#if WITH_PHYSX

class FCollisionFbxExporter
{
public:
	FCollisionFbxExporter(const UStaticMesh *StaticMeshToExport, FbxMesh* ExportMesh, int32 ActualMatIndexToExport)
	{
		BoxPositions[0] = FVector(-1, -1, +1);
		BoxPositions[1] = FVector(-1, +1, +1);
		BoxPositions[2] = FVector(+1, +1, +1);
		BoxPositions[3] = FVector(+1, -1, +1);

		BoxFaceRotations[0] = FRotator(0, 0, 0);
		BoxFaceRotations[1] = FRotator(90.f, 0, 0);
		BoxFaceRotations[2] = FRotator(-90.f, 0, 0);
		BoxFaceRotations[3] = FRotator(0, 0, 90.f);
		BoxFaceRotations[4] = FRotator(0, 0, -90.f);
		BoxFaceRotations[5] = FRotator(180.f, 0, 0);

		DrawCollisionSides = 16;

		SpherNumSides = DrawCollisionSides;
		SphereNumRings = DrawCollisionSides / 2;
		SphereNumVerts = (SpherNumSides + 1) * (SphereNumRings + 1);

		CapsuleNumSides = DrawCollisionSides;
		CapsuleNumRings = (DrawCollisionSides / 2) + 1;
		CapsuleNumVerts = (CapsuleNumSides + 1) * (CapsuleNumRings + 1);

		CurrentVertexOffset = 0;

		StaticMesh = StaticMeshToExport;
		Mesh = ExportMesh;
		ActualMatIndex = ActualMatIndexToExport;
	}
	
	void ExportCollisions()
	{
		const FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

		int32 VerticeNumber = 0;
		for (const FKConvexElem &ConvexElem : AggGeo.ConvexElems)
		{
			VerticeNumber += GetConvexVerticeNumber(ConvexElem);
		}
		for (const FKBoxElem &BoxElem : AggGeo.BoxElems)
		{
			VerticeNumber += GetBoxVerticeNumber();
		}
		for (const FKSphereElem &SphereElem : AggGeo.SphereElems)
		{
			VerticeNumber += GetSphereVerticeNumber();
		}
		for (const FKSphylElem &CapsuleElem : AggGeo.SphylElems)
		{
			VerticeNumber += GetCapsuleVerticeNumber();
		}

		Mesh->InitControlPoints(VerticeNumber);
		ControlPoints = Mesh->GetControlPoints();
		CurrentVertexOffset = 0;
		//////////////////////////////////////////////////////////////////////////
		// Set all vertex
		for (const FKConvexElem &ConvexElem : AggGeo.ConvexElems)
		{
			AddConvexVertex(ConvexElem);
		}

		for (const FKBoxElem &BoxElem : AggGeo.BoxElems)
		{
			AddBoxVertex(BoxElem);
		}

		for (const FKSphereElem &SphereElem : AggGeo.SphereElems)
		{
			AddSphereVertex(SphereElem);
		}

		for (const FKSphylElem &CapsuleElem : AggGeo.SphylElems)
		{
			AddCapsuleVertex(CapsuleElem);
		}

		// Set the normals on Layer 0.
		FbxLayer* Layer = Mesh->GetLayer(0);
		if (Layer == nullptr)
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer(0);
		}
		// Create and fill in the per-face-vertex normal data source.
		LayerElementNormal = FbxLayerElementNormal::Create(Mesh, "");
		// Set the normals per polygon instead of storing normals on positional control points
		LayerElementNormal->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		// Set the normal values for every polygon vertex.
		LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);

		//////////////////////////////////////////////////////////////////////////
		//Set the Normals
		for (const FKConvexElem &ConvexElem : AggGeo.ConvexElems)
		{
			AddConvexNormals(ConvexElem);
		}
		for (const FKBoxElem &BoxElem : AggGeo.BoxElems)
		{
			AddBoxNormal(BoxElem);
		}

		int32 SphereIndex = 0;
		for (const FKSphereElem &SphereElem : AggGeo.SphereElems)
		{
			AddSphereNormals(SphereElem, SphereIndex);
			SphereIndex++;
		}

		int32 CapsuleIndex = 0;
		for (const FKSphylElem &CapsuleElem : AggGeo.SphylElems)
		{
			AddCapsuleNormals(CapsuleElem, CapsuleIndex);
			CapsuleIndex++;
		}

		Layer->SetNormals(LayerElementNormal);

		//////////////////////////////////////////////////////////////////////////
		// Set polygons
		// Build list of polygon re-used multiple times to lookup Normals, UVs, other per face vertex information
		CurrentVertexOffset = 0; //Reset the current VertexCount
		for (const FKConvexElem &ConvexElem : AggGeo.ConvexElems)
		{
			AddConvexPolygon(ConvexElem);
		}

		for (const FKBoxElem &BoxElem : AggGeo.BoxElems)
		{
			AddBoxPolygons();
		}

		for (const FKSphereElem &SphereElem : AggGeo.SphereElems)
		{
			AddSpherePolygons();
		}

		for (const FKSphylElem &CapsuleElem : AggGeo.SphylElems)
		{
			AddCapsulePolygons();
		}

		//////////////////////////////////////////////////////////////////////////
		//Free the sphere resources
		for (FDynamicMeshVertex* DynamicMeshVertex : SpheresVerts)
		{
			FMemory::Free(DynamicMeshVertex);
		}
		SpheresVerts.Empty();

		//////////////////////////////////////////////////////////////////////////
		//Free the capsule resources
		for (FDynamicMeshVertex* DynamicMeshVertex : CapsuleVerts)
		{
			FMemory::Free(DynamicMeshVertex);
		}
		CapsuleVerts.Empty();
	}

private:
	uint32 GetConvexVerticeNumber(const FKConvexElem &ConvexElem)
	{
		return ConvexElem.GetConvexMesh() != nullptr ? ConvexElem.GetConvexMesh()->getNbVertices() : 0;
	}

	uint32 GetBoxVerticeNumber() { return 24; }

	uint32 GetSphereVerticeNumber() { return SphereNumVerts; }

	uint32 GetCapsuleVerticeNumber() { return CapsuleNumVerts; }

	void AddConvexVertex(const FKConvexElem &ConvexElem)
	{
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if (ConvexMesh == nullptr)
		{
			return;
		}
		const PxVec3 *VertexArray = ConvexMesh->getVertices();
		for (uint32 PosIndex = 0; PosIndex < ConvexMesh->getNbVertices(); ++PosIndex)
		{
			FVector Position = P2UVector(VertexArray[PosIndex]);
			ControlPoints[CurrentVertexOffset + PosIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
		}
		CurrentVertexOffset += ConvexMesh->getNbVertices();
	}

	void AddConvexNormals(const FKConvexElem &ConvexElem)
	{
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if (ConvexMesh == nullptr)
		{
			return;
		}
		const PxU8* PIndexBuffer = ConvexMesh->getIndexBuffer();
		int32 PolygonNumber = ConvexMesh->getNbPolygons();
		for (int32 PolyIndex = 0; PolyIndex < PolygonNumber; ++PolyIndex)
		{
			PxHullPolygon PolyData;
			if (!ConvexMesh->getPolygonData(PolyIndex, PolyData))
			{
				continue;
			}
			const PxVec3 PPlaneNormal(PolyData.mPlane[0], PolyData.mPlane[1], PolyData.mPlane[2]);
			FVector Normal = P2UVector(PPlaneNormal.getNormalized());
			FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
			// add vertices 
			for (PxU32 j = 0; j < PolyData.mNbVerts; ++j)
			{
				LayerElementNormal->GetDirectArray().Add(FbxNormal);
			}
		}
	}

	void AddConvexPolygon(const FKConvexElem &ConvexElem)
	{
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if (ConvexMesh == nullptr)
		{
			return;
		}
		const PxU8* PIndexBuffer = ConvexMesh->getIndexBuffer();
		int32 PolygonNumber = ConvexMesh->getNbPolygons();
		for (int32 PolyIndex = 0; PolyIndex < PolygonNumber; ++PolyIndex)
		{
			PxHullPolygon PolyData;
			if (!ConvexMesh->getPolygonData(PolyIndex, PolyData))
			{
				continue;
			}
			Mesh->BeginPolygon(ActualMatIndex);
			const PxU8* PolyIndices = PIndexBuffer + PolyData.mIndexBase;
			// add vertices 
			for (PxU32 j = 0; j < PolyData.mNbVerts; ++j)
			{
				const uint32 VertIndex = CurrentVertexOffset + PolyIndices[j];
				Mesh->AddPolygon(VertIndex);
			}
			Mesh->EndPolygon();
		}
		CurrentVertexOffset += ConvexMesh->getNbVertices();
	}

	void AddBoxVertex(const FKBoxElem &BoxElem)
	{
		FScaleMatrix ExtendScale(0.5f * FVector(BoxElem.X, BoxElem.Y, BoxElem.Z));
		// Calculate verts for a face pointing down Z
		FMatrix BoxTransform = BoxElem.GetTransform().ToMatrixWithScale();
		for (int32 f = 0; f < 6; f++)
		{
			FMatrix FaceTransform = FRotationMatrix(BoxFaceRotations[f])*ExtendScale*BoxTransform;

			for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
			{
				FVector4 VertexPosition = FaceTransform.TransformPosition(BoxPositions[VertexIndex]);
				ControlPoints[CurrentVertexOffset + VertexIndex] = FbxVector4(VertexPosition.X, -VertexPosition.Y, VertexPosition.Z);
			}
			CurrentVertexOffset += 4;
		}
	}

	void AddBoxNormal(const FKBoxElem &BoxElem)
	{
		FScaleMatrix ExtendScale(0.5f * FVector(BoxElem.X, BoxElem.Y, BoxElem.Z));
		FMatrix BoxTransform = BoxElem.GetTransform().ToMatrixWithScale();
		for (int32 f = 0; f < 6; f++)
		{
			FMatrix FaceTransform = FRotationMatrix(BoxFaceRotations[f])*ExtendScale*BoxTransform;
			FVector4 TangentZ = FaceTransform.TransformVector(FVector(0, 0, 1));
			FbxVector4 FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
			FbxNormal.Normalize();
			for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
			{
				LayerElementNormal->GetDirectArray().Add(FbxNormal);
			}
		}
	}

	void AddBoxPolygons()
	{
		for (int32 f = 0; f < 6; f++)
		{
			Mesh->BeginPolygon(ActualMatIndex);
			for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
			{
				const uint32 VertIndex = CurrentVertexOffset + VertexIndex;
				Mesh->AddPolygon(VertIndex);
			}
			Mesh->EndPolygon();
			CurrentVertexOffset += 4;
		}
	}

	void AddSphereVertex(const FKSphereElem &SphereElem)
	{
		FMatrix SphereTransform = FScaleMatrix(SphereElem.Radius * FVector(1.0f)) * SphereElem.GetTransform().ToMatrixWithScale();
		FDynamicMeshVertex* Verts = (FDynamicMeshVertex*)FMemory::Malloc(SphereNumVerts * sizeof(FDynamicMeshVertex));
		// Calculate verts for one arc
		FDynamicMeshVertex* ArcVerts = (FDynamicMeshVertex*)FMemory::Malloc((SphereNumRings + 1) * sizeof(FDynamicMeshVertex));

		for (int32 i = 0; i < SphereNumRings + 1; i++)
		{
			FDynamicMeshVertex* ArcVert = &ArcVerts[i];

			float angle = ((float)i / SphereNumRings) * PI;

			// Note- unit sphere, so position always has mag of one. We can just use it for normal!			
			ArcVert->Position.X = 0.0f;
			ArcVert->Position.Y = FMath::Sin(angle);
			ArcVert->Position.Z = FMath::Cos(angle);

			ArcVert->SetTangents(
				FVector(1, 0, 0),
				FVector(0.0f, -ArcVert->Position.Z, ArcVert->Position.Y),
				ArcVert->Position
				);
		}

		// Then rotate this arc SpherNumSides+1 times.
		for (int32 s = 0; s < SpherNumSides + 1; s++)
		{
			FRotator ArcRotator(0, 360.f * (float)s / SpherNumSides, 0);
			FRotationMatrix ArcRot(ArcRotator);

			for (int32 v = 0; v < SphereNumRings + 1; v++)
			{
				int32 VIx = (SphereNumRings + 1)*s + v;

				Verts[VIx].Position = ArcRot.TransformPosition(ArcVerts[v].Position);

				Verts[VIx].SetTangents(
					ArcRot.TransformVector(ArcVerts[v].TangentX),
					ArcRot.TransformVector(ArcVerts[v].GetTangentY()),
					ArcRot.TransformVector(ArcVerts[v].TangentZ)
					);
			}
		}

		// Add all of the vertices we generated to the mesh builder.
		for (int32 VertexIndex = 0; VertexIndex < SphereNumVerts; VertexIndex++)
		{
			FVector Position = SphereTransform.TransformPosition(Verts[VertexIndex].Position);
			ControlPoints[CurrentVertexOffset + VertexIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
		}
		CurrentVertexOffset += SphereNumVerts;
		// Free our local copy of arc verts
		FMemory::Free(ArcVerts);
		SpheresVerts.Add(Verts);
	}

	void AddSphereNormals(const FKSphereElem &SphereElem, int32 SphereIndex)
	{
		FMatrix SphereTransform = FScaleMatrix(SphereElem.Radius * FVector(1.0f)) * SphereElem.GetTransform().ToMatrixWithScale();
		for (int32 s = 0; s < SpherNumSides; s++)
		{
			int32 a0start = (s + 0) * (SphereNumRings + 1);
			int32 a1start = (s + 1) * (SphereNumRings + 1);

			for (int32 r = 0; r < SphereNumRings; r++)
			{
				if (r != 0)
				{
					int32 indexV = a0start + r + 0;
					FVector TangentZ = SphereTransform.TransformVector(SpheresVerts[SphereIndex][indexV].TangentZ);
					FbxVector4 FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a1start + r + 0;
					TangentZ = SphereTransform.TransformVector(SpheresVerts[SphereIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a0start + r + 1;
					TangentZ = SphereTransform.TransformVector(SpheresVerts[SphereIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);
				}
				if (r != SphereNumRings - 1)
				{
					int32 indexV = a1start + r + 0;
					FVector TangentZ = SphereTransform.TransformVector(SpheresVerts[SphereIndex][indexV].TangentZ);
					FbxVector4 FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a1start + r + 1;
					TangentZ = SphereTransform.TransformVector(SpheresVerts[SphereIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a0start + r + 1;
					TangentZ = SphereTransform.TransformVector(SpheresVerts[SphereIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);
				}
			}
		}
	}

	void AddSpherePolygons()
	{
		for (int32 s = 0; s < SpherNumSides; s++)
		{
			int32 a0start = (s + 0) * (SphereNumRings + 1);
			int32 a1start = (s + 1) * (SphereNumRings + 1);

			for (int32 r = 0; r < SphereNumRings; r++)
			{
				if (r != 0)
				{
					Mesh->BeginPolygon(ActualMatIndex);
					Mesh->AddPolygon(CurrentVertexOffset + a0start + r + 0);
					Mesh->AddPolygon(CurrentVertexOffset + a1start + r + 0);
					Mesh->AddPolygon(CurrentVertexOffset + a0start + r + 1);
					Mesh->EndPolygon();
				}
				if (r != SphereNumRings - 1)
				{
					Mesh->BeginPolygon(ActualMatIndex);
					Mesh->AddPolygon(CurrentVertexOffset + a1start + r + 0);
					Mesh->AddPolygon(CurrentVertexOffset + a1start + r + 1);
					Mesh->AddPolygon(CurrentVertexOffset + a0start + r + 1);
					Mesh->EndPolygon();
				}
			}
		}
		CurrentVertexOffset += SphereNumVerts;
	}

	void AddCapsuleVertex(const FKSphylElem &CapsuleElem)
	{
		FMatrix CapsuleTransform = CapsuleElem.GetTransform().ToMatrixWithScale();
		float Length = CapsuleElem.Length;
		float Radius = CapsuleElem.Radius;
		FDynamicMeshVertex* Verts = (FDynamicMeshVertex*)FMemory::Malloc(CapsuleNumVerts * sizeof(FDynamicMeshVertex));

		// Calculate verts for one arc
		FDynamicMeshVertex* ArcVerts = (FDynamicMeshVertex*)FMemory::Malloc((CapsuleNumRings + 1) * sizeof(FDynamicMeshVertex));

		for (int32 RingIdx = 0; RingIdx < CapsuleNumRings + 1; RingIdx++)
		{
			FDynamicMeshVertex* ArcVert = &ArcVerts[RingIdx];

			float Angle;
			float ZOffset;
			if (RingIdx <= DrawCollisionSides / 4)
			{
				Angle = ((float)RingIdx / (CapsuleNumRings - 1)) * PI;
				ZOffset = 0.5 * Length;
			}
			else
			{
				Angle = ((float)(RingIdx - 1) / (CapsuleNumRings - 1)) * PI;
				ZOffset = -0.5 * Length;
			}

			// Note- unit sphere, so position always has mag of one. We can just use it for normal!		
			FVector SpherePos;
			SpherePos.X = 0.0f;
			SpherePos.Y = Radius * FMath::Sin(Angle);
			SpherePos.Z = Radius * FMath::Cos(Angle);

			ArcVert->Position = SpherePos + FVector(0, 0, ZOffset);

			ArcVert->SetTangents(
				FVector(1, 0, 0),
				FVector(0.0f, -SpherePos.Z, SpherePos.Y),
				SpherePos
				);
		}

		// Then rotate this arc NumSides+1 times.
		for (int32 SideIdx = 0; SideIdx < CapsuleNumSides + 1; SideIdx++)
		{
			const FRotator ArcRotator(0, 360.f * ((float)SideIdx / CapsuleNumSides), 0);
			const FRotationMatrix ArcRot(ArcRotator);

			for (int32 VertIdx = 0; VertIdx < CapsuleNumRings + 1; VertIdx++)
			{
				int32 VIx = (CapsuleNumRings + 1)*SideIdx + VertIdx;

				Verts[VIx].Position = ArcRot.TransformPosition(ArcVerts[VertIdx].Position);

				Verts[VIx].SetTangents(
					ArcRot.TransformVector(ArcVerts[VertIdx].TangentX),
					ArcRot.TransformVector(ArcVerts[VertIdx].GetTangentY()),
					ArcRot.TransformVector(ArcVerts[VertIdx].TangentZ)
					);
			}
		}

		// Add all of the vertices we generated to the mesh builder.
		for (int32 VertexIndex = 0; VertexIndex < CapsuleNumVerts; VertexIndex++)
		{
			FVector Position = CapsuleTransform.TransformPosition(Verts[VertexIndex].Position);
			ControlPoints[CurrentVertexOffset + VertexIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
		}
		CurrentVertexOffset += CapsuleNumVerts;
		// Free our local copy of arc verts
		FMemory::Free(ArcVerts);
		CapsuleVerts.Add(Verts);
	}
	
	void AddCapsuleNormals(const FKSphylElem &CapsuleElem, int32 CapsuleIndex)
	{
		FMatrix CapsuleTransform = CapsuleElem.GetTransform().ToMatrixWithScale();
		// Add all of the triangles to the mesh.
		for (int32 SideIdx = 0; SideIdx < CapsuleNumSides; SideIdx++)
		{
			const int32 a0start = (SideIdx + 0) * (CapsuleNumRings + 1);
			const int32 a1start = (SideIdx + 1) * (CapsuleNumRings + 1);

			for (int32 RingIdx = 0; RingIdx < CapsuleNumRings; RingIdx++)
			{
				if (RingIdx != 0)
				{
					int32 indexV = a0start + RingIdx + 0;
					FVector TangentZ = CapsuleTransform.TransformVector(CapsuleVerts[CapsuleIndex][indexV].TangentZ);
					FbxVector4 FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a1start + RingIdx + 0;
					TangentZ = CapsuleTransform.TransformVector(CapsuleVerts[CapsuleIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a0start + RingIdx + 1;
					TangentZ = CapsuleTransform.TransformVector(CapsuleVerts[CapsuleIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);
				}
				if (RingIdx != CapsuleNumRings - 1)
				{
					int32 indexV = a1start + RingIdx + 0;
					FVector TangentZ = CapsuleTransform.TransformVector(CapsuleVerts[CapsuleIndex][indexV].TangentZ);
					FbxVector4 FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a1start + RingIdx + 1;
					TangentZ = CapsuleTransform.TransformVector(CapsuleVerts[CapsuleIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);

					indexV = a0start + RingIdx + 1;
					TangentZ = CapsuleTransform.TransformVector(CapsuleVerts[CapsuleIndex][indexV].TangentZ);
					FbxNormal = FbxVector4(TangentZ.X, -TangentZ.Y, TangentZ.Z);
					FbxNormal.Normalize();
					LayerElementNormal->GetDirectArray().Add(FbxNormal);
				}
			}
		}
	}

	void AddCapsulePolygons()
	{
		// Add all of the triangles to the mesh.
		for (int32 SideIdx = 0; SideIdx < CapsuleNumSides; SideIdx++)
		{
			const int32 a0start = (SideIdx + 0) * (CapsuleNumRings + 1);
			const int32 a1start = (SideIdx + 1) * (CapsuleNumRings + 1);

			for (int32 RingIdx = 0; RingIdx < CapsuleNumRings; RingIdx++)
			{
				if (RingIdx != 0)
				{
					Mesh->BeginPolygon(ActualMatIndex);
					Mesh->AddPolygon(CurrentVertexOffset + a0start + RingIdx + 0);
					Mesh->AddPolygon(CurrentVertexOffset + a1start + RingIdx + 0);
					Mesh->AddPolygon(CurrentVertexOffset + a0start + RingIdx + 1);
					Mesh->EndPolygon();
				}
				if (RingIdx != CapsuleNumRings - 1)
				{
					Mesh->BeginPolygon(ActualMatIndex);
					Mesh->AddPolygon(CurrentVertexOffset + a1start + RingIdx + 0);
					Mesh->AddPolygon(CurrentVertexOffset + a1start + RingIdx + 1);
					Mesh->AddPolygon(CurrentVertexOffset + a0start + RingIdx + 1);
					Mesh->EndPolygon();
				}
			}
		}
		CurrentVertexOffset += CapsuleNumVerts;
	}

	//////////////////////////////////////////////////////////////////////////
	//Box data
	FVector BoxPositions[4];
	FRotator BoxFaceRotations[6];
	

	int32 DrawCollisionSides;
	//////////////////////////////////////////////////////////////////////////
	//Sphere data
	int32 SpherNumSides;
	int32 SphereNumRings;
	int32 SphereNumVerts;
	TArray<FDynamicMeshVertex*> SpheresVerts;

	//////////////////////////////////////////////////////////////////////////
	//Capsule data
	int32 CapsuleNumSides;
	int32 CapsuleNumRings;
	int32 CapsuleNumVerts;
	TArray<FDynamicMeshVertex*> CapsuleVerts;

	//////////////////////////////////////////////////////////////////////////
	//Mesh Data
	uint32 CurrentVertexOffset;

	const UStaticMesh *StaticMesh;
	FbxMesh* Mesh;
	int32 ActualMatIndex;
	FbxVector4* ControlPoints;
	FbxLayerElementNormal* LayerElementNormal;
};

FbxNode* FFbxExporter::ExportCollisionMesh(const UStaticMesh* StaticMesh, const TCHAR* MeshName, FbxNode* ParentActor)
{
	const FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;
	if (AggGeo.GetElementCount() <= 0)
	{
		return nullptr;
	}
	FbxMesh* Mesh = FbxMeshes.FindRef(StaticMesh);
	if (!Mesh)
	{
		//We export collision only if the mesh is already exported
		return nullptr;
	}
	//Name the mesh attribute with the mesh name
	FString MeshCollisionName = TEXT("UCX_");
	MeshCollisionName += MeshName;
	Mesh = FbxMesh::Create(Scene, TCHAR_TO_UTF8(*MeshCollisionName));
	//Name the node with the actor name
	MeshCollisionName = TEXT("UCX_");
	MeshCollisionName += UTF8_TO_TCHAR(ParentActor->GetName()); //-V595
	FbxNode* FbxActor = FbxNode::Create(Scene, TCHAR_TO_UTF8(*MeshCollisionName));

	FbxNode *ParentOfParentMesh = nullptr;
	if (ParentActor != nullptr)
	{
		FbxActor->LclTranslation.Set(ParentActor->LclTranslation.Get());
		FbxActor->LclRotation.Set(ParentActor->LclRotation.Get());
		FbxActor->LclScaling.Set(ParentActor->LclScaling.Get());
		ParentOfParentMesh = ParentActor->GetParent();
	}

	if (ParentOfParentMesh == nullptr)
	{
		ParentOfParentMesh = Scene->GetRootNode();
	}
	
	Scene->GetRootNode()->AddChild(FbxActor);

	//Export all collision elements in one mesh
	FbxSurfaceMaterial* FbxMaterial = nullptr;
	int32 ActualMatIndex = FbxActor->AddMaterial(FbxMaterial);
	FCollisionFbxExporter CollisionFbxExporter(StaticMesh, Mesh, ActualMatIndex);
	CollisionFbxExporter.ExportCollisions();

	//Set the original meshes in case it was already existing
	FbxActor->SetNodeAttribute(Mesh);
	return FbxActor;
}
#endif

/**
 * Exports a static mesh
 * @param StaticMesh	The static mesh to export
 * @param MeshName		The name of the mesh for the FBX file
 * @param FbxActor		The fbx node representing the mesh
 * @param ExportLOD		The LOD of the mesh to export
 * @param LightmapUVChannel If set, performs a "lightmap export" and exports only the single given UV channel
 * @param ColorBuffer	Vertex color overrides to export
 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
 */
FbxNode* FFbxExporter::ExportStaticMeshToFbx(const UStaticMesh* StaticMesh, int32 ExportLOD, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel /*= -1*/, const FColorVertexBuffer* ColorBuffer /*= NULL*/, const TArray<FStaticMaterial>* MaterialOrderOverride /*= NULL*/)
{
	FbxMesh* Mesh = nullptr;
	if ((ExportLOD == 0 || ExportLOD == -1) && LightmapUVChannel == -1 && ColorBuffer == nullptr && MaterialOrderOverride == nullptr)
	{
		Mesh = FbxMeshes.FindRef(StaticMesh);
	}

	if (!Mesh)
	{
		Mesh = FbxMesh::Create(Scene, TCHAR_TO_UTF8(MeshName));

		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport(ExportLOD);

		// Verify the integrity of the static mesh.
		if (RenderMesh.VertexBuffer.GetNumVertices() == 0)
		{
			return nullptr;
		}

		if (RenderMesh.Sections.Num() == 0)
		{
			return nullptr;
		}

		// Remaps an Unreal vert to final reduced vertex list
		TArray<int32> VertRemap;
		TArray<int32> UniqueVerts;

		if (ExportOptions->WeldedVertices)
		{
			// Weld verts
			DetermineVertsToWeld(VertRemap, UniqueVerts, RenderMesh);
		}
		else
		{
			// Do not weld verts
			VertRemap.AddUninitialized(RenderMesh.VertexBuffer.GetNumVertices());
			for (int32 i = 0; i < VertRemap.Num(); i++)
			{
				VertRemap[i] = i;
			}
			UniqueVerts = VertRemap;
		}

		// Create and fill in the vertex position data source.
		// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
		const int32 VertexCount = VertRemap.Num();
		const int32 PolygonsCount = RenderMesh.Sections.Num();

		Mesh->InitControlPoints(UniqueVerts.Num());

		FbxVector4* ControlPoints = Mesh->GetControlPoints();
		for (int32 PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex)
		{
			int32 UnrealPosIndex = UniqueVerts[PosIndex];
			FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(UnrealPosIndex);
			ControlPoints[PosIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
		}

		// Set the normals on Layer 0.
		FbxLayer* Layer = Mesh->GetLayer(0);
		if (Layer == nullptr)
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer(0);
		}

		// Build list of Indices re-used multiple times to lookup Normals, UVs, other per face vertex information
		TArray<uint32> Indices;
		for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
		{
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			const FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
			const uint32 TriangleCount = Polygons.NumTriangles;
			for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			{
				for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
				{
					uint32 UnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
					Indices.Add(UnrealVertIndex);
				}
			}
		}

		// Create and fill in the per-face-vertex normal data source.
		// We extract the Z-tangent and the X/Y-tangents which are also stored in the render mesh.
		FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create(Mesh, "");
		FbxLayerElementTangent* LayerElementTangent = FbxLayerElementTangent::Create(Mesh, "");
		FbxLayerElementBinormal* LayerElementBinormal = FbxLayerElementBinormal::Create(Mesh, "");

		// Set 3 NTBs per triangle instead of storing on positional control points
		LayerElementNormal->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		LayerElementTangent->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		LayerElementBinormal->SetMappingMode(FbxLayerElement::eByPolygonVertex);

		// Set the NTBs values for every polygon vertex.
		LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);
		LayerElementTangent->SetReferenceMode(FbxLayerElement::eDirect);
		LayerElementBinormal->SetReferenceMode(FbxLayerElement::eDirect);

		TArray<FbxVector4> FbxNormals;
		TArray<FbxVector4> FbxTangents;
		TArray<FbxVector4> FbxBinormals;
		
		FbxNormals.AddUninitialized(VertexCount);
		FbxTangents.AddUninitialized(VertexCount);
		FbxBinormals.AddUninitialized(VertexCount);
		
		for (int32 NTBIndex = 0; NTBIndex < VertexCount; ++NTBIndex)
		{
			FVector Normal = (FVector)(RenderMesh.VertexBuffer.VertexTangentZ(NTBIndex));
			FbxVector4& FbxNormal = FbxNormals[NTBIndex];
			FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
			FbxNormal.Normalize();

			FVector Tangent = (FVector)(RenderMesh.VertexBuffer.VertexTangentX(NTBIndex));
			FbxVector4& FbxTangent = FbxTangents[NTBIndex];
			FbxTangent = FbxVector4(Tangent.X, -Tangent.Y, Tangent.Z);
			FbxTangent.Normalize();

			FVector Binormal = -(FVector)(RenderMesh.VertexBuffer.VertexTangentY(NTBIndex));
			FbxVector4& FbxBinormal = FbxBinormals[NTBIndex];
			FbxBinormal = FbxVector4(Binormal.X, -Binormal.Y, Binormal.Z);
			FbxBinormal.Normalize();
		}

		// Add one normal per each face index (3 per triangle)
		for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
		{
			uint32 UnrealVertIndex = Indices[FbxVertIndex];
			LayerElementNormal->GetDirectArray().Add(FbxNormals[UnrealVertIndex]);
			LayerElementTangent->GetDirectArray().Add(FbxTangents[UnrealVertIndex]);
			LayerElementBinormal->GetDirectArray().Add(FbxBinormals[UnrealVertIndex]);
		}
		
		Layer->SetNormals(LayerElementNormal);
		Layer->SetTangents(LayerElementTangent);
		Layer->SetBinormals(LayerElementBinormal);
		
		FbxNormals.Empty();
		FbxTangents.Empty();
		FbxBinormals.Empty();

		// Create and fill in the per-face-vertex texture coordinate data source(s).
		// Create UV for Diffuse channel.
		int32 TexCoordSourceCount = (LightmapUVChannel == -1) ? RenderMesh.VertexBuffer.GetNumTexCoords() : LightmapUVChannel + 1;
		int32 TexCoordSourceIndex = (LightmapUVChannel == -1) ? 0 : LightmapUVChannel;
		for (; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex)
		{
			FbxLayer* UVsLayer = (LightmapUVChannel == -1) ? Mesh->GetLayer(TexCoordSourceIndex) : Mesh->GetLayer(0);
			if (UVsLayer == NULL)
			{
				Mesh->CreateLayer();
				UVsLayer = (LightmapUVChannel == -1) ? Mesh->GetLayer(TexCoordSourceIndex) : Mesh->GetLayer(0);
			}
			check(UVsLayer);

			FString UVChannelNameBuilder = TEXT("UVmap_") + FString::FromInt(TexCoordSourceIndex);
			const char* UVChannelName = TCHAR_TO_UTF8(*UVChannelNameBuilder); // actually UTF8 as required by Fbx, but can't use UE4's UTF8CHAR type because that's a uint8 aka *unsigned* char
			if ((LightmapUVChannel >= 0) || ((LightmapUVChannel == -1) && (TexCoordSourceIndex == StaticMesh->LightMapCoordinateIndex)))
			{
				UVChannelName = "LightMapUV";
			}

			FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, UVChannelName);

			// Note: when eINDEX_TO_DIRECT is used, IndexArray must be 3xTriangle count, DirectArray can be smaller
			UVDiffuseLayer->SetMappingMode(FbxLayerElement::eByPolygonVertex);
			UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);

			TArray<int32> UvsRemap;
			TArray<int32> UniqueUVs;
			if (ExportOptions->WeldedVertices)
			{
				// Weld UVs
				DetermineUVsToWeld(UvsRemap, UniqueUVs, RenderMesh.VertexBuffer, TexCoordSourceIndex);
			}
			else
			{
				// Do not weld UVs
				UvsRemap = VertRemap;
				UniqueUVs = UvsRemap;
			}

			// Create the texture coordinate data source.
			for (int32 FbxVertIndex = 0; FbxVertIndex < UniqueUVs.Num(); FbxVertIndex++)
			{
				int32 UnrealVertIndex = UniqueUVs[FbxVertIndex];
				const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(UnrealVertIndex, TexCoordSourceIndex);
				UVDiffuseLayer->GetDirectArray().Add(FbxVector2(TexCoord.X, -TexCoord.Y + 1.0));
			}

			// For each face index, point to a texture uv
			UVDiffuseLayer->GetIndexArray().SetCount(Indices.Num());
			for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
			{
				uint32 UnrealVertIndex = Indices[FbxVertIndex];
				int32 NewVertIndex = UvsRemap[UnrealVertIndex];
				UVDiffuseLayer->GetIndexArray().SetAt(FbxVertIndex, NewVertIndex);
			}

			UVsLayer->SetUVs(UVDiffuseLayer, FbxLayerElement::eTextureDiffuse);
		}

		FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
		MatLayer->SetMappingMode(FbxLayerElement::eByPolygon);
		MatLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
		Layer->SetMaterials(MatLayer);

		// Keep track of the number of tri's we export
		uint32 AccountedTriangles = 0;
		for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
		{
			const FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			UMaterialInterface* Material = StaticMesh->GetMaterial(Polygons.MaterialIndex);

			FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material) : NULL;
			if (!FbxMaterial)
			{
				FbxMaterial = CreateDefaultMaterial();
			}
			int32 MatIndex = FbxActor->AddMaterial(FbxMaterial);

			// Determine the actual material index
			int32 ActualMatIndex = MatIndex;

			if (MaterialOrderOverride)
			{
				ActualMatIndex = MaterialOrderOverride->Find(Material);
			}
			// Static meshes contain one triangle list per element.
			// [GLAFORTE] Could it occasionally contain triangle strips? How do I know?
			uint32 TriangleCount = Polygons.NumTriangles;

			// Copy over the index buffer into the FBX polygons set.
			for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			{
				Mesh->BeginPolygon(ActualMatIndex);
				for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
				{
					uint32 OriginalUnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
					int32 RemappedVertIndex = VertRemap[OriginalUnrealVertIndex];
					Mesh->AddPolygon(RemappedVertIndex);
				}
				Mesh->EndPolygon();
			}

			AccountedTriangles += TriangleCount;
		}

#ifdef TODO_FBX
		// Throw a warning if this is a lightmap export and the exported poly count does not match the raw triangle data count
		if (LightmapUVChannel != -1 && AccountedTriangles != RenderMesh.RawTriangles.GetElementCount())
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "StaticMeshEditor_LightmapExportFewerTriangles", "Fewer polygons have been exported than the raw triangle count.  This Lightmapped UV mesh may contain fewer triangles than the destination mesh on import.") );
		}

		// Create and fill in the smoothing data source.
		FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create(Mesh, "");
		SmoothingInfo->SetMappingMode(FbxLayerElement::eByPolygon);
		SmoothingInfo->SetReferenceMode(FbxLayerElement::eDirect);
		FbxLayerElementArrayTemplate<int>& SmoothingArray = SmoothingInfo->GetDirectArray();
		Layer->SetSmoothing(SmoothingInfo);

		// This is broken. We are exporting the render mesh but providing smoothing
		// information from the source mesh. The render triangles are not in the
		// same order. Therefore we should export the raw mesh or not export
		// smoothing group information!
		int32 TriangleCount = RenderMesh.RawTriangles.GetElementCount();
		FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock(LOCK_READ_ONLY);
		for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
		{
			FStaticMeshTriangle* Triangle = (RawTriangleData++);

			SmoothingArray.Add(Triangle->SmoothingMask);
		}
		RenderMesh.RawTriangles.Unlock();
#endif // #if TODO_FBX

		// Create and fill in the vertex color data source.
		const FColorVertexBuffer* ColorBufferToUse = ColorBuffer ? ColorBuffer : &RenderMesh.ColorVertexBuffer;
		uint32 ColorVertexCount = ColorBufferToUse->GetNumVertices();

		// Only export vertex colors if they exist
		if (ExportOptions->VertexColor && ColorVertexCount > 0)
		{
			FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create(Mesh, "");
			VertexColor->SetMappingMode(FbxLayerElement::eByPolygonVertex);
			VertexColor->SetReferenceMode(FbxLayerElement::eIndexToDirect);
			FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
			Layer->SetVertexColors(VertexColor);

			for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
			{
				FLinearColor VertColor(1.0f, 1.0f, 1.0f);
				uint32 UnrealVertIndex = Indices[FbxVertIndex];
				if (UnrealVertIndex < ColorVertexCount)
				{
					VertColor = ColorBufferToUse->VertexColor(UnrealVertIndex).ReinterpretAsLinear();
				}

				VertexColorArray.Add(FbxColor(VertColor.R, VertColor.G, VertColor.B, VertColor.A));
			}

			VertexColor->GetIndexArray().SetCount(Indices.Num());
			for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
			{
				VertexColor->GetIndexArray().SetAt(FbxVertIndex, FbxVertIndex);
			}
		}

		if ((ExportLOD == 0 || ExportLOD == -1) && LightmapUVChannel == -1 && ColorBuffer == nullptr && MaterialOrderOverride == nullptr)
		{
			FbxMeshes.Add(StaticMesh, Mesh);
		}
#if WITH_PHYSX
		if ((ExportLOD == 0 || ExportLOD == -1) && ExportOptions->Collision)
		{
			ExportCollisionMesh(StaticMesh, MeshName, FbxActor);
		}
#endif
	}
	else
	{
		//Materials in fbx are store in the node and not in the mesh, so even if the mesh was already export
		//we have to find and assign the mesh material.
		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport(ExportLOD);
		const int32 PolygonsCount = RenderMesh.Sections.Num();
		uint32 AccountedTriangles = 0;
		for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
		{
			const FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			UMaterialInterface* Material = StaticMesh->GetMaterial(Polygons.MaterialIndex);

			FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material) : NULL;
			if (!FbxMaterial)
			{
				FbxMaterial = CreateDefaultMaterial();
			}
			FbxActor->AddMaterial(FbxMaterial);
		}
	}

	//Set the original meshes in case it was already existing
	FbxActor->SetNodeAttribute(Mesh);

	return FbxActor;
}

void FFbxExporter::ExportSplineMeshToFbx(const USplineMeshComponent* SplineMeshComp, const TCHAR* MeshName, FbxNode* FbxActor)
{
	const UStaticMesh* StaticMesh = SplineMeshComp->GetStaticMesh();
	check(StaticMesh);

	const int32 LODIndex = (SplineMeshComp->ForcedLodModel > 0 ? SplineMeshComp->ForcedLodModel - 1 : /* auto-select*/ 0);
	const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport(LODIndex);

	// Verify the integrity of the static mesh.
	if (RenderMesh.VertexBuffer.GetNumVertices() == 0)
	{
		return;
	}

	if (RenderMesh.Sections.Num() == 0)
	{
		return;
	}

	// Remaps an Unreal vert to final reduced vertex list
	TArray<int32> VertRemap;
	TArray<int32> UniqueVerts;

	if (ExportOptions->WeldedVertices)
	{
		// Weld verts
		DetermineVertsToWeld(VertRemap, UniqueVerts, RenderMesh);
	}
	else
	{
		// Do not weld verts
		VertRemap.AddUninitialized(RenderMesh.VertexBuffer.GetNumVertices());
		for (int32 i = 0; i < VertRemap.Num(); i++)
		{
			VertRemap[i] = i;
		}
		UniqueVerts = VertRemap;
	}

	FbxMesh* Mesh = FbxMesh::Create(Scene, TCHAR_TO_UTF8(MeshName));

	// Create and fill in the vertex position data source.
	// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
	const int32 VertexCount = VertRemap.Num();
	const int32 PolygonsCount = RenderMesh.Sections.Num();

	Mesh->InitControlPoints(UniqueVerts.Num());

	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for (int32 PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex)
	{
		int32 UnrealPosIndex = UniqueVerts[PosIndex];
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(UnrealPosIndex);

		const FTransform SliceTransform = SplineMeshComp->CalcSliceTransform(USplineMeshComponent::GetAxisValue(Position, SplineMeshComp->ForwardAxis));
		USplineMeshComponent::GetAxisValue(Position, SplineMeshComp->ForwardAxis) = 0;
		Position = SliceTransform.TransformPosition(Position);

		ControlPoints[PosIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
	}

	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}

	// Build list of Indices re-used multiple times to lookup Normals, UVs, other per face vertex information
	TArray<uint32> Indices;
	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		const FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
		const uint32 TriangleCount = Polygons.NumTriangles;
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				uint32 UnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
				Indices.Add(UnrealVertIndex);
			}
		}
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create(Mesh, "");

	// Set 3 normals per triangle instead of storing normals on positional control points
	LayerElementNormal->SetMappingMode(FbxLayerElement::eByPolygonVertex);

	// Set the normal values for every polygon vertex.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);

	TArray<FbxVector4> FbxNormals;
	FbxNormals.AddUninitialized(VertexCount);
	for (int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex)
	{
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(VertIndex);
		const FTransform SliceTransform = SplineMeshComp->CalcSliceTransform(USplineMeshComponent::GetAxisValue(Position, SplineMeshComp->ForwardAxis));
		FVector Normal = FVector(RenderMesh.VertexBuffer.VertexTangentZ(VertIndex));
		Normal = SliceTransform.TransformVector(Normal);
		FbxVector4& FbxNormal = FbxNormals[VertIndex];
		FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxNormal.Normalize();
	}

	// Add one normal per each face index (3 per triangle)
	for (uint32 UnrealVertIndex : Indices)
	{
		LayerElementNormal->GetDirectArray().Add(FbxNormals[UnrealVertIndex]);
	}
	Layer->SetNormals(LayerElementNormal);
	FbxNormals.Empty();

	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	int32 TexCoordSourceCount = RenderMesh.VertexBuffer.GetNumTexCoords();
	for (int32 TexCoordSourceIndex = 0; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex)
	{
		FbxLayer* UVsLayer = Mesh->GetLayer(TexCoordSourceIndex);
		if (UVsLayer == NULL)
		{
			Mesh->CreateLayer();
			UVsLayer = Mesh->GetLayer(TexCoordSourceIndex);
		}
		FString UVChannelNameBuilder = TEXT("UVmap_") + FString::FromInt(TexCoordSourceIndex);
		const char* UVChannelName = TCHAR_TO_UTF8(*UVChannelNameBuilder); // actually UTF8 as required by Fbx, but can't use UE4's UTF8CHAR type because that's a uint8 aka *unsigned* char
		if (TexCoordSourceIndex == StaticMesh->LightMapCoordinateIndex)
		{
			UVChannelName = "LightMapUV";
		}

		FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, UVChannelName);

		// Note: when eINDEX_TO_DIRECT is used, IndexArray must be 3xTriangle count, DirectArray can be smaller
		UVDiffuseLayer->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);

		TArray<int32> UvsRemap;
		TArray<int32> UniqueUVs;
		if (ExportOptions->WeldedVertices)
		{
			// Weld UVs
			DetermineUVsToWeld(UvsRemap, UniqueUVs, RenderMesh.VertexBuffer, TexCoordSourceIndex);
		}
		else
		{
			// Do not weld UVs
			UvsRemap = VertRemap;
			UniqueUVs = UvsRemap;
		}

		// Create the texture coordinate data source.
		for (int32 UnrealVertIndex : UniqueUVs)
		{
			const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(UnrealVertIndex, TexCoordSourceIndex);
			UVDiffuseLayer->GetDirectArray().Add(FbxVector2(TexCoord.X, -TexCoord.Y + 1.0));
		}

		// For each face index, point to a texture uv
		UVDiffuseLayer->GetIndexArray().SetCount(Indices.Num());
		for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
		{
			uint32 UnrealVertIndex = Indices[FbxVertIndex];
			int32 NewVertIndex = UvsRemap[UnrealVertIndex];
			UVDiffuseLayer->GetIndexArray().SetAt(FbxVertIndex, NewVertIndex);
		}

		UVsLayer->SetUVs(UVDiffuseLayer, FbxLayerElement::eTextureDiffuse);
	}

	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);

	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		const FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		UMaterialInterface* Material = StaticMesh->GetMaterial(Polygons.MaterialIndex);

		FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material) : NULL;
		if (!FbxMaterial)
		{
			FbxMaterial = CreateDefaultMaterial();
		}
		int32 MatIndex = FbxActor->AddMaterial(FbxMaterial);

		// Static meshes contain one triangle list per element.
		uint32 TriangleCount = Polygons.NumTriangles;

		// Copy over the index buffer into the FBX polygons set.
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			Mesh->BeginPolygon(MatIndex);
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				uint32 OriginalUnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
				int32 RemappedVertIndex = VertRemap[OriginalUnrealVertIndex];
				Mesh->AddPolygon(RemappedVertIndex);
			}
			Mesh->EndPolygon();
		}
	}

#ifdef TODO_FBX
	// This is broken. We are exporting the render mesh but providing smoothing
	// information from the source mesh. The render triangles are not in the
	// same order. Therefore we should export the raw mesh or not export
	// smoothing group information!
	int32 TriangleCount = RenderMesh.RawTriangles.GetElementCount();
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock(LOCK_READ_ONLY);
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
	{
		FStaticMeshTriangle* Triangle = (RawTriangleData++);

		SmoothingArray.Add(Triangle->SmoothingMask);
	}
	RenderMesh.RawTriangles.Unlock();
#endif // #if TODO_FBX

	// Create and fill in the vertex color data source.
	const FColorVertexBuffer* ColorBufferToUse = &RenderMesh.ColorVertexBuffer;
	uint32 ColorVertexCount = ColorBufferToUse->GetNumVertices();

	// Only export vertex colors if they exist
	if (ExportOptions->VertexColor && ColorVertexCount > 0)
	{
		FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create(Mesh, "");
		VertexColor->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		VertexColor->SetReferenceMode(FbxLayerElement::eIndexToDirect);
		FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		Layer->SetVertexColors(VertexColor);

		for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
		{
			FLinearColor VertColor(1.0f, 1.0f, 1.0f);
			uint32 UnrealVertIndex = Indices[FbxVertIndex];
			if (UnrealVertIndex < ColorVertexCount)
			{
				VertColor = ColorBufferToUse->VertexColor(UnrealVertIndex).ReinterpretAsLinear();
			}

			VertexColorArray.Add(FbxColor(VertColor.R, VertColor.G, VertColor.B, VertColor.A));
		}

		VertexColor->GetIndexArray().SetCount(Indices.Num());
		for (int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++)
		{
			VertexColor->GetIndexArray().SetAt(FbxVertIndex, FbxVertIndex);
		}
	}

	FbxActor->SetNodeAttribute(Mesh);
}

void FFbxExporter::ExportInstancedMeshToFbx(const UInstancedStaticMeshComponent* InstancedMeshComp, const TCHAR* MeshName, FbxNode* FbxActor)
{
	const UStaticMesh* StaticMesh = InstancedMeshComp->GetStaticMesh();
	check(StaticMesh);

	const int32 LODIndex = (InstancedMeshComp->ForcedLodModel > 0 ? InstancedMeshComp->ForcedLodModel - 1 : /* auto-select*/ 0);
	const int32 NumInstances = InstancedMeshComp->GetInstanceCount();
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		FTransform RelativeTransform;
		if (ensure(InstancedMeshComp->GetInstanceTransform(InstanceIndex, RelativeTransform, /*bWorldSpace=*/false)))
		{
			FbxNode* InstNode = FbxNode::Create(Scene, TCHAR_TO_UTF8(*FString::Printf(TEXT("%d"), InstanceIndex)));

			InstNode->LclTranslation.Set(Converter.ConvertToFbxPos(RelativeTransform.GetTranslation()));
			InstNode->LclRotation.Set(Converter.ConvertToFbxRot(RelativeTransform.GetRotation().Euler()));
			InstNode->LclScaling.Set(Converter.ConvertToFbxScale(RelativeTransform.GetScale3D()));

			// Todo - export once and then clone the node
			ExportStaticMeshToFbx(StaticMesh, LODIndex, *FString::Printf(TEXT("%d"), InstanceIndex), InstNode);
			FbxActor->AddChild(InstNode);
		}
	}
}

/**
 * Exports a Landscape
 */
void FFbxExporter::ExportLandscapeToFbx(ALandscapeProxy* Landscape, const TCHAR* MeshName, FbxNode* FbxActor, bool bSelectedOnly)
{
	const ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	
	TSet<ULandscapeComponent*> SelectedComponents;
	if (bSelectedOnly && LandscapeInfo)
	{
		SelectedComponents = LandscapeInfo->GetSelectedComponents();
	}

	bSelectedOnly = bSelectedOnly && SelectedComponents.Num() > 0;

	int32 MinX = MAX_int32, MinY = MAX_int32;
	int32 MaxX = MIN_int32, MaxY = MIN_int32;

	// Find range of entire landscape
	for (int32 ComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++)
	{
		ULandscapeComponent* Component = Landscape->LandscapeComponents[ComponentIndex];

		if (bSelectedOnly && !SelectedComponents.Contains(Component))
		{
			continue;
		}

		Component->GetComponentExtent(MinX, MinY, MaxX, MaxY);
	}

	FbxMesh* Mesh = FbxMesh::Create(Scene, TCHAR_TO_UTF8(MeshName));

	// Create and fill in the vertex position data source.
	const int32 ComponentSizeQuads = ((Landscape->ComponentSizeQuads + 1) >> Landscape->ExportLOD) - 1;
	const float ScaleFactor = (float)Landscape->ComponentSizeQuads / (float)ComponentSizeQuads;
	const int32 NumComponents = bSelectedOnly ? SelectedComponents.Num() : Landscape->LandscapeComponents.Num();
	const int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	const int32 VertexCount = NumComponents * VertexCountPerComponent;
	const int32 TriangleCount = NumComponents * FMath::Square(ComponentSizeQuads) * 2;
	
	Mesh->InitControlPoints(VertexCount);

	// Normals and Tangents
	FbxLayerElementNormal* LayerElementNormals= FbxLayerElementNormal::Create(Mesh, "");
	LayerElementNormals->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementNormals->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementTangent* LayerElementTangents= FbxLayerElementTangent::Create(Mesh, "");
	LayerElementTangents->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementTangents->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementBinormal* LayerElementBinormals= FbxLayerElementBinormal::Create(Mesh, "");
	LayerElementBinormals->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementBinormals->SetReferenceMode(FbxLayerElement::eDirect);

	// Add Texture UVs (which are simply incremented 1.0 per vertex)
	FbxLayerElementUV* LayerElementTextureUVs = FbxLayerElementUV::Create(Mesh, "TextureUVs");
	LayerElementTextureUVs->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementTextureUVs->SetReferenceMode(FbxLayerElement::eDirect);

	// Add Weightmap UVs (to match up with an exported weightmap, not the original weightmap UVs, which are per-component)
	const FVector2D UVScale = FVector2D(1.0f, 1.0f) / FVector2D((MaxX - MinX) + 1, (MaxY - MinY) + 1);
	FbxLayerElementUV* LayerElementWeightmapUVs = FbxLayerElementUV::Create(Mesh, "WeightmapUVs");
	LayerElementWeightmapUVs->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementWeightmapUVs->SetReferenceMode(FbxLayerElement::eDirect);

	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	FbxLayerElementArrayTemplate<FbxVector4>& Normals = LayerElementNormals->GetDirectArray();
	Normals.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector4>& Tangents = LayerElementTangents->GetDirectArray();
	Tangents.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector4>& Binormals = LayerElementBinormals->GetDirectArray();
	Binormals.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector2>& TextureUVs = LayerElementTextureUVs->GetDirectArray();
	TextureUVs.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector2>& WeightmapUVs = LayerElementWeightmapUVs->GetDirectArray();
	WeightmapUVs.Resize(VertexCount);

	TArray<uint8> VisibilityData;
	VisibilityData.Empty(VertexCount);
	VisibilityData.AddZeroed(VertexCount);

	for (int32 ComponentIndex = 0, SelectedComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++)
	{
		ULandscapeComponent* Component = Landscape->LandscapeComponents[ComponentIndex];

		if (bSelectedOnly && !SelectedComponents.Contains(Component))
		{
			continue;
		}

		FLandscapeComponentDataInterface CDI(Component, Landscape->ExportLOD);
		const int32 BaseVertIndex = SelectedComponentIndex++ * VertexCountPerComponent;

		TArray<uint8> CompVisData;
		for (int32 AllocIdx = 0; AllocIdx < Component->WeightmapLayerAllocations.Num(); AllocIdx++)
		{
			FWeightmapLayerAllocationInfo& AllocInfo = Component->WeightmapLayerAllocations[AllocIdx];
			if (AllocInfo.LayerInfo == ALandscapeProxy::VisibilityLayer)
			{
				CDI.GetWeightmapTextureData(AllocInfo.LayerInfo, CompVisData);
			}
		}

		if (CompVisData.Num() > 0)
		{
			for (int32 i = 0; i < VertexCountPerComponent; ++i)
			{
				VisibilityData[BaseVertIndex + i] = CompVisData[CDI.VertexIndexToTexel(i)];
			}
		}
		
		for (int32 VertIndex = 0; VertIndex < VertexCountPerComponent; VertIndex++)
		{
			int32 VertX, VertY;
			CDI.VertexIndexToXY(VertIndex, VertX, VertY);

			FVector Position = CDI.GetLocalVertex(VertX, VertY) + Component->RelativeLocation;
			FbxVector4 FbxPosition = FbxVector4(Position.X, -Position.Y, Position.Z);
			ControlPoints[BaseVertIndex + VertIndex] = FbxPosition;

			FVector Normal, TangentX, TangentY;
			CDI.GetLocalTangentVectors(VertX, VertY, TangentX, TangentY, Normal);
			Normal /= Component->GetComponentTransform().GetScale3D(); Normal.Normalize();
			TangentX /= Component->GetComponentTransform().GetScale3D(); TangentX.Normalize();
			TangentY /= Component->GetComponentTransform().GetScale3D(); TangentY.Normalize();
			FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z); FbxNormal.Normalize();
			Normals.SetAt(BaseVertIndex + VertIndex, FbxNormal);
			FbxVector4 FbxTangent = FbxVector4(TangentX.X, -TangentX.Y, TangentX.Z); FbxTangent.Normalize();
			Tangents.SetAt(BaseVertIndex + VertIndex, FbxTangent);
			FbxVector4 FbxBinormal = FbxVector4(TangentY.X, -TangentY.Y, TangentY.Z); FbxBinormal.Normalize();
			Binormals.SetAt(BaseVertIndex + VertIndex, FbxBinormal);

			FVector2D TextureUV = FVector2D(VertX * ScaleFactor + Component->GetSectionBase().X, VertY * ScaleFactor + Component->GetSectionBase().Y);
			FbxVector2 FbxTextureUV = FbxVector2(TextureUV.X, TextureUV.Y);
			TextureUVs.SetAt(BaseVertIndex + VertIndex, FbxTextureUV);

			FVector2D WeightmapUV = (TextureUV - FVector2D(MinX, MinY)) * UVScale;
			FbxVector2 FbxWeightmapUV = FbxVector2(WeightmapUV.X, WeightmapUV.Y);
			WeightmapUVs.SetAt(BaseVertIndex + VertIndex, FbxWeightmapUV);
		}
	}

	FbxLayer* Layer0 = Mesh->GetLayer(0);
	if (Layer0 == NULL)
	{
		Mesh->CreateLayer();
		Layer0 = Mesh->GetLayer(0);
	}

	Layer0->SetNormals(LayerElementNormals);
	Layer0->SetTangents(LayerElementTangents);
	Layer0->SetBinormals(LayerElementBinormals);
	Layer0->SetUVs(LayerElementTextureUVs);
	Layer0->SetUVs(LayerElementWeightmapUVs, FbxLayerElement::eTextureBump);

	// this doesn't seem to work, on import the mesh has no smoothing layer at all
	//FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create(Mesh, "");
	//SmoothingInfo->SetMappingMode(FbxLayerElement::eAllSame);
	//SmoothingInfo->SetReferenceMode(FbxLayerElement::eDirect);
	//FbxLayerElementArrayTemplate<int>& Smoothing = SmoothingInfo->GetDirectArray();
	//Smoothing.Add(0);
	//Layer0->SetSmoothing(SmoothingInfo);

	FbxLayerElementMaterial* LayerElementMaterials = FbxLayerElementMaterial::Create(Mesh, "");
	LayerElementMaterials->SetMappingMode(FbxLayerElement::eAllSame);
	LayerElementMaterials->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer0->SetMaterials(LayerElementMaterials);

	UMaterialInterface* Material = Landscape->GetLandscapeMaterial();
	FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material) : NULL;
	if (!FbxMaterial)
	{
		FbxMaterial = CreateDefaultMaterial();
	}
	const int32 MaterialIndex = FbxActor->AddMaterial(FbxMaterial);
	LayerElementMaterials->GetIndexArray().Add(MaterialIndex);

	const int32 VisThreshold = 170;
	// Copy over the index buffer into the FBX polygons set.
	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ComponentIndex++)
	{
		int32 BaseVertIndex = ComponentIndex * VertexCountPerComponent;

		for (int32 Y = 0; Y < ComponentSizeQuads; Y++)
		{
			for (int32 X = 0; X < ComponentSizeQuads; X++)
			{
				if (VisibilityData[BaseVertIndex + Y * (ComponentSizeQuads + 1) + X] < VisThreshold)
				{
					Mesh->BeginPolygon();
					Mesh->AddPolygon(BaseVertIndex + (X + 0) + (Y + 0)*(ComponentSizeQuads + 1));
					Mesh->AddPolygon(BaseVertIndex + (X + 1) + (Y + 1)*(ComponentSizeQuads + 1));
					Mesh->AddPolygon(BaseVertIndex + (X + 1) + (Y + 0)*(ComponentSizeQuads + 1));
					Mesh->EndPolygon();

					Mesh->BeginPolygon();
					Mesh->AddPolygon(BaseVertIndex + (X + 0) + (Y + 0)*(ComponentSizeQuads + 1));
					Mesh->AddPolygon(BaseVertIndex + (X + 0) + (Y + 1)*(ComponentSizeQuads + 1));
					Mesh->AddPolygon(BaseVertIndex + (X + 1) + (Y + 1)*(ComponentSizeQuads + 1));
					Mesh->EndPolygon();
				}
			}
		}
	}

	FbxActor->SetNodeAttribute(Mesh);
}


} // namespace UnFbx
