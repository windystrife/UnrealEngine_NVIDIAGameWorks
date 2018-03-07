// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AdvancedPreviewScene.h"
#include "UnrealClient.h"

#include "Components/PostProcessComponent.h"
#include "Engine/Texture.h"
#include "Engine/TextureCube.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/Package.h"

#include "Editor/EditorPerProjectUserSettings.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"

#include "AssetViewerSettings.h"

#include "Engine/StaticMesh.h"
#include "AdvancedPreviewSceneCommands.h"
#include "UICommandList.h"
#include "Framework/Application/SlateApplication.h"

FAdvancedPreviewScene::FAdvancedPreviewScene(ConstructionValues CVS, float InFloorOffset)
	: FPreviewScene(CVS)
{
	DefaultSettings = UAssetViewerSettings::Get();
	check(DefaultSettings);

	RefreshDelegate = DefaultSettings->OnAssetViewerSettingsChanged().AddRaw(this, &FAdvancedPreviewScene::OnAssetViewerSettingsRefresh);

	CurrentProfileIndex = DefaultSettings->Profiles.IsValidIndex(CurrentProfileIndex) ? GetDefault<UEditorPerProjectUserSettings>()->AssetViewerProfileIndex : 0;
	ensureMsgf(DefaultSettings->Profiles.IsValidIndex(CurrentProfileIndex), TEXT("Invalid default settings pointer or current profile index"));
	FPreviewSceneProfile& Profile = DefaultSettings->Profiles[CurrentProfileIndex];
	Profile.LoadEnvironmentMap();

	SphereReflectionComponent = nullptr;

	const FTransform Transform(FRotator(0, 0, 0), FVector(0, 0, 0), FVector(1));

	// Set up sky light using the set cube map texture, reusing the sky light from PreviewScene class
	SetSkyCubemap(Profile.EnvironmentCubeMap.Get());
	SetSkyBrightness(Profile.SkyLightIntensity);
	
	// Large scale to prevent sphere from clipping
	const FTransform SphereTransform(FRotator(0, 0, 0), FVector(0, 0, 0), FVector(2000));
	SkyComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());

	// Set up sky sphere showing hte same cube map as used by the sky light
	UStaticMesh* SkySphere = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/AssetViewer/Sphere_inversenormals.Sphere_inversenormals"), NULL, LOAD_None, NULL);
	check(SkySphere);
	SkyComponent->SetStaticMesh(SkySphere);
	SkyComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterial* SkyMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMaterials/AssetViewer/M_SkyBox.M_SkyBox"), NULL, LOAD_None, NULL);
	check(SkyMaterial);

	InstancedSkyMaterial = NewObject<UMaterialInstanceConstant>(GetTransientPackage());
	InstancedSkyMaterial->Parent = SkyMaterial;		

	UTextureCube* DefaultTexture = LoadObject<UTextureCube>(NULL, TEXT("/Engine/MapTemplates/Sky/SunsetAmbientCubemap.SunsetAmbientCubemap"));
    
	InstancedSkyMaterial->SetTextureParameterValueEditorOnly(FName("SkyBox"), ( Profile.EnvironmentCubeMap.Get() != nullptr ) ? Profile.EnvironmentCubeMap.Get() : DefaultTexture );
	InstancedSkyMaterial->SetScalarParameterValueEditorOnly(FName("CubemapRotation"), Profile.LightingRigRotation / 360.0f);
	InstancedSkyMaterial->SetScalarParameterValueEditorOnly(FName("Intensity"), Profile.SkyLightIntensity);
	InstancedSkyMaterial->PostLoad();
	SkyComponent->SetMaterial(0, InstancedSkyMaterial);
	AddComponent(SkyComponent, SphereTransform);
	
	PostProcessComponent = NewObject<UPostProcessComponent>();
	PostProcessComponent->Settings = Profile.PostProcessingSettings;
	PostProcessComponent->bUnbound = true;
	AddComponent(PostProcessComponent, Transform);

	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/AssetViewer/Floor_Mesh.Floor_Mesh"), NULL, LOAD_None, NULL);
	check(FloorMesh);
	FloorMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());
	FloorMeshComponent->SetStaticMesh(FloorMesh);

	FTransform FloorTransform(FRotator(0, 0, 0), FVector(0, 0, -(InFloorOffset)), FVector(4.0f, 4.0f, 1.0f ));
	AddComponent(FloorMeshComponent, FloorTransform);	

	SetLightDirection(Profile.DirectionalLightRotation);

	bRotateLighting = Profile.bRotateLightingRig;
	CurrentRotationSpeed = Profile.RotationSpeed;
	bSkyChanged = false;
	bUseSkylight = true;

	BindCommands();
}

FAdvancedPreviewScene::~FAdvancedPreviewScene()
{
	if (UObjectInitialized())
	{
		DefaultSettings = UAssetViewerSettings::Get();
		if (DefaultSettings)
		{
			DefaultSettings->OnAssetViewerSettingsChanged().Remove(RefreshDelegate);
		}
	}
}

void FAdvancedPreviewScene::UpdateScene(FPreviewSceneProfile& Profile, bool bUpdateSkyLight /*= true*/, bool bUpdateEnvironment  /*= true*/, bool bUpdatePostProcessing /*= true*/, bool bUpdateDirectionalLight /*= true*/)
{
	if (bUpdateSkyLight)
	{
		// Threshold to ensure we only update the intensity if it is going to make a difference
		if (!FMath::IsNearlyEqual(bUseSkylight ? SkyLight->Intensity : SphereReflectionComponent->Brightness, Profile.SkyLightIntensity, 0.05f))
		{
			static const FName IntensityName("Intensity");
			if (bUseSkylight)
			{
				SetSkyBrightness(Profile.SkyLightIntensity);
			}
			else
			{
				SphereReflectionComponent->Brightness = Profile.SkyLightIntensity;
			}
			
			InstancedSkyMaterial->SetScalarParameterValueEditorOnly(IntensityName, Profile.SkyLightIntensity);
			bSkyChanged = true;
		}
	}

	if (bUpdateEnvironment)
	{
		static const FName SkyBoxName("SkyBox");
		static const FName CubeMapRotationName("CubemapRotation");

		UTextureCube* EnvironmentTexture = Profile.EnvironmentCubeMap.LoadSynchronous();
		UTexture* Texture = EnvironmentTexture;
		InstancedSkyMaterial->GetTextureParameterValue(SkyBoxName, Texture);

		if (Texture != EnvironmentTexture)
		{
			InstancedSkyMaterial->SetTextureParameterValueEditorOnly(SkyBoxName, EnvironmentTexture);
			if (bUseSkylight)
			{
				SetSkyCubemap(EnvironmentTexture);
			}
			else
			{
				SphereReflectionComponent->Cubemap = EnvironmentTexture;
			}
			
			bSkyChanged = true;
		}
		
		static const float OneOver360 = 1.0f / 360.0f;
		float Rotation = Profile.LightingRigRotation;
		InstancedSkyMaterial->GetScalarParameterValue(CubeMapRotationName, Rotation);
		if (!FMath::IsNearlyEqual(Rotation, Profile.LightingRigRotation, 0.05f))
		{			
			InstancedSkyMaterial->SetScalarParameterValueEditorOnly(CubeMapRotationName, Profile.LightingRigRotation * OneOver360);

			// Update light direction as well
			FRotator LightDir = GetLightDirection();
			LightDir.Yaw = Profile.LightingRigRotation;
			SetLightDirection(LightDir);
			DefaultSettings->Profiles[CurrentProfileIndex].DirectionalLightRotation = LightDir;
			if (bUseSkylight)
			{
				SkyLight->SourceCubemapAngle = Profile.LightingRigRotation;
			}
			else
			{
				SphereReflectionComponent->SourceCubemapAngle = Profile.LightingRigRotation;
			}
			bSkyChanged = true;
		}
	}		

	if (bUpdatePostProcessing)
	{
		PostProcessComponent->Settings = Profile.PostProcessingSettings;
		PostProcessComponent->bEnabled = Profile.bPostProcessingEnabled;
		bPostProcessing = Profile.bPostProcessingEnabled;
	}

	if (bUpdateDirectionalLight)
	{
		if (!FMath::IsNearlyEqual(DirectionalLight->Intensity, Profile.DirectionalLightIntensity, 0.05f))
		{
			DirectionalLight->SetIntensity(Profile.DirectionalLightIntensity);
		}
		DirectionalLight->SetLightColor(Profile.DirectionalLightColor);
	}

	SkyComponent->SetVisibility(Profile.bShowEnvironment, true);
	if (bUseSkylight)
	{
		SkyLight->SetVisibility(Profile.bShowEnvironment, true);
	}
	else
	{
		SphereReflectionComponent->SetVisibility(Profile.bShowEnvironment, true);
	}
	FloorMeshComponent->SetVisibility(Profile.bShowFloor, true);

	bRotateLighting = Profile.bRotateLightingRig;
	CurrentRotationSpeed = Profile.RotationSpeed;
}

FLinearColor FAdvancedPreviewScene::GetBackgroundColor() const
{
	FLinearColor Color = DefaultSettings->Profiles[CurrentProfileIndex].EnvironmentColor;
	return Color * DefaultSettings->Profiles[CurrentProfileIndex].EnvironmentIntensity;
}

void FAdvancedPreviewScene::SetFloorOffset(const float InFloorOffset)
{
	FTransform FloorTransform(FRotator(0, 0, 0), FVector(0, 0, -(InFloorOffset)), FVector(4.0f, 4.0f, 1.0f));

	FloorMeshComponent->SetRelativeTransform(FloorTransform);
}

void FAdvancedPreviewScene::SetProfileIndex(const int32 InProfileIndex)
{
	CurrentProfileIndex = InProfileIndex;
	DefaultSettings->Profiles[CurrentProfileIndex].LoadEnvironmentMap();
	SetLightDirection(DefaultSettings->Profiles[CurrentProfileIndex].DirectionalLightRotation);
	UpdateScene(DefaultSettings->Profiles[CurrentProfileIndex]);
	DefaultSettings->OnAssetViewerSettingsChanged().Broadcast(NAME_None);
}

void FAdvancedPreviewScene::Tick(float DeltaTime)
{
	checkf(DefaultSettings && DefaultSettings->Profiles.IsValidIndex(CurrentProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	FPreviewSceneProfile& Profile = DefaultSettings->Profiles[CurrentProfileIndex];
	if (Profile.bRotateLightingRig)
	{
		CurrentRotationSpeed = Profile.RotationSpeed;
		Profile.LightingRigRotation = FMath::Fmod(FMath::Max(FMath::Min(Profile.LightingRigRotation + (CurrentRotationSpeed * DeltaTime), 360.0f), 0.0f), 360.0f);

		FRotator LightDir = GetLightDirection();
		LightDir.Yaw += DeltaTime * -CurrentRotationSpeed;
		SetLightDirection(LightDir);		
		DefaultSettings->Profiles[CurrentProfileIndex].DirectionalLightRotation = LightDir;
	}

	if (!FMath::IsNearlyEqual(PreviousRotation, Profile.LightingRigRotation, 0.05f))
	{		
		if (bUseSkylight)
		{
			SkyLight->SourceCubemapAngle = Profile.LightingRigRotation;
			SkyLight->SetCaptureIsDirty();
			SkyLight->MarkRenderStateDirty();
			SkyLight->UpdateSkyCaptureContents(PreviewWorld);
		}
		else
		{
			SphereReflectionComponent->SourceCubemapAngle = Profile.LightingRigRotation;
			SphereReflectionComponent->SetCaptureIsDirty();
			SphereReflectionComponent->MarkRenderStateDirty();
			SphereReflectionComponent->UpdateReflectionCaptureContents(PreviewWorld);
		}

		InstancedSkyMaterial->SetScalarParameterValueEditorOnly(FName("CubemapRotation"), Profile.LightingRigRotation / 360.0f);
		InstancedSkyMaterial->PostEditChange();

		PreviewWorld->UpdateAllReflectionCaptures();
		PreviewWorld->UpdateAllSkyCaptures();

		PreviousRotation = Profile.LightingRigRotation;
	}

	// Update the sky every tick rather than every mouse move (UpdateScene call)
	if (bSkyChanged)
	{
		if (bUseSkylight)
		{
			SkyLight->SetCaptureIsDirty();
			SkyLight->MarkRenderStateDirty();
			SkyLight->UpdateSkyCaptureContents(PreviewWorld);
		}
		else
		{
			SphereReflectionComponent->SetCaptureIsDirty();
			SphereReflectionComponent->MarkRenderStateDirty();
			SphereReflectionComponent->UpdateReflectionCaptureContents(PreviewWorld);
		}


		InstancedSkyMaterial->PostEditChange();
		bSkyChanged = false;
	}
}

bool FAdvancedPreviewScene::IsTickable() const
{
	return true;
}

TStatId FAdvancedPreviewScene::GetStatId() const
{
	return TStatId();
}

const bool FAdvancedPreviewScene::HandleViewportInput(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	bool bResult = false;
	const bool bMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton) || InViewport->KeyState(EKeys::MiddleMouseButton) || InViewport->KeyState(EKeys::RightMouseButton);
	
	
	const bool bSkyMove = InViewport->KeyState(EKeys::K);
	const bool bLightMoveDown = InViewport->KeyState(EKeys::L);
	

	// Look at which axis is being dragged and by how much
	const float DragX = (Key == EKeys::MouseX) ? Delta : 0.f;	
	const float DragY = (Key == EKeys::MouseY) ? Delta : 0.f;

	// Move the sky around if K is down and the mouse has moved on the X-axis
	if (bSkyMove && bMouseButtonDown)
	{
		static const float SkyRotationSpeed = 0.22f;
		float SkyRotation = GetSkyRotation();
		SkyRotation += -DragX * SkyRotationSpeed;
		SetSkyRotation(SkyRotation);
		bResult = true;
	}

	if (bLightMoveDown && (!FMath::IsNearlyZero(DragX) || !FMath::IsNearlyZero(DragY)))
	{
		// Save light rotation
		DefaultSettings->Profiles[CurrentProfileIndex].DirectionalLightRotation = GetLightDirection();
	}

	return bResult;
}

const bool FAdvancedPreviewScene::HandleInputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool Gamepad)
{
	if (Event == IE_Pressed)
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		if (UICommandList->ProcessCommandBindings(Key, KeyState, (Event == IE_Repeat)))
		{
			return true;
		}
	}

	return false;
}

void FAdvancedPreviewScene::SetFloorVisibility(const bool bVisible, const bool bDirect)
{
	// If not direct set visibility in profile and refresh the scene
	if (!bDirect)
	{
		FName PropertyName("bShowFloor");

		UProperty* FloorProperty = FindField<UProperty>(FPreviewSceneProfile::StaticStruct(), PropertyName);
		DefaultSettings->Profiles[CurrentProfileIndex].bShowFloor = bVisible;

		FPropertyChangedEvent PropertyEvent(FloorProperty);
		DefaultSettings->PostEditChangeProperty(PropertyEvent);
	}
	else
	{
		// Otherwise set visiblity directly on the component
		FloorMeshComponent->SetVisibility(bVisible ? DefaultSettings->Profiles[CurrentProfileIndex].bShowFloor : bVisible);
	}
}

void FAdvancedPreviewScene::SetEnvironmentVisibility(const bool bVisible, const bool bDirect)
{
	// If not direct set visibility in profile and refresh the scene
	if (!bDirect)
	{
		UProperty* EnvironmentProperty = FindField<UProperty>(FPreviewSceneProfile::StaticStruct(), GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bShowEnvironment));
		DefaultSettings->Profiles[CurrentProfileIndex].bShowEnvironment = bVisible;

		FPropertyChangedEvent PropertyEvent(EnvironmentProperty);
		DefaultSettings->PostEditChangeProperty(PropertyEvent);
	}
	else
	{
		// Otherwise set visibility directly on the component
		SkyComponent->SetVisibility(bVisible ? DefaultSettings->Profiles[CurrentProfileIndex].bShowEnvironment : bVisible);
		if (bUseSkylight)
		{
			SkyLight->SetVisibility(bVisible ? DefaultSettings->Profiles[CurrentProfileIndex].bShowEnvironment : bVisible);
		}
		else
		{
			SphereReflectionComponent->SetVisibility(bVisible ? DefaultSettings->Profiles[CurrentProfileIndex].bShowEnvironment : bVisible);
		}
	}
}

const float FAdvancedPreviewScene::GetSkyRotation() const
{
	checkf(DefaultSettings && DefaultSettings->Profiles.IsValidIndex(CurrentProfileIndex), TEXT("Invalid default settings pointer or current profile index"));
	return DefaultSettings->Profiles[CurrentProfileIndex].LightingRigRotation;
}

void FAdvancedPreviewScene::SetSkyRotation(const float SkyRotation)
{
	checkf(DefaultSettings && DefaultSettings->Profiles.IsValidIndex(CurrentProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	float ClampedSkyRotation = FMath::Fmod(SkyRotation, 360.0f);
	// Clamp and wrap around sky rotation
	if (ClampedSkyRotation < 0.0f)
	{
		ClampedSkyRotation += 360.0f;
	}
	DefaultSettings->Profiles[CurrentProfileIndex].LightingRigRotation = ClampedSkyRotation;
}

const bool FAdvancedPreviewScene::IsUsingPostProcessing() const
{
	return bPostProcessing;
}

const int32 FAdvancedPreviewScene::GetCurrentProfileIndex() const
{
	return CurrentProfileIndex;
}

const UStaticMeshComponent* FAdvancedPreviewScene::GetFloorMeshComponent() const
{
	checkf(FloorMeshComponent != nullptr, TEXT("Invalid floor mesh component pointer"));
	return FloorMeshComponent;
}

void FAdvancedPreviewScene::BindCommands()
{
	UICommandList = MakeShared<FUICommandList>();

	const FAdvancedPreviewSceneCommands& Commands = FAdvancedPreviewSceneCommands::Get();

	UICommandList->MapAction(Commands.ToggleFloor,
		FExecuteAction::CreateRaw(this, &FAdvancedPreviewScene::HandleToggleFloor));

	UICommandList->MapAction(Commands.ToggleSky,
		FExecuteAction::CreateRaw(this, &FAdvancedPreviewScene::HandleToggleSky));
}

void FAdvancedPreviewScene::HandleToggleSky()
{
	SetEnvironmentVisibility(!DefaultSettings->Profiles[CurrentProfileIndex].bShowEnvironment);
}

void FAdvancedPreviewScene::HandleToggleFloor()
{
	SetFloorVisibility(!DefaultSettings->Profiles[CurrentProfileIndex].bShowFloor);
}

void FAdvancedPreviewScene::OnAssetViewerSettingsRefresh(const FName& InPropertyName)
{
	if (DefaultSettings->Profiles.IsValidIndex(CurrentProfileIndex))
	{
		const bool bNameNone = InPropertyName == NAME_None;

		const bool bUpdateEnvironment = (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, EnvironmentCubeMap)) || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, LightingRigRotation) || (InPropertyName == GET_MEMBER_NAME_CHECKED(UAssetViewerSettings, Profiles)));
		const bool bUpdateSkyLight = bUpdateEnvironment || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, SkyLightIntensity) || (InPropertyName == GET_MEMBER_NAME_CHECKED(UAssetViewerSettings, Profiles)));
		const bool bUpdateDirectionalLight = (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, DirectionalLightIntensity)) || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, DirectionalLightColor));
		const bool bUpdatePostProcessing = (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, PostProcessingSettings)) || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bPostProcessingEnabled));

		UpdateScene(DefaultSettings->Profiles[CurrentProfileIndex], bUpdateSkyLight || bNameNone, bUpdateEnvironment || bNameNone, bUpdatePostProcessing || bNameNone, bUpdateDirectionalLight || bNameNone);
	}
}