// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusSceneCaptureCubemap.h"
#include "OculusHMDPrivate.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

//-------------------------------------------------------------------------------------------------
// UOculusSceneCaptureCubemap
//-------------------------------------------------------------------------------------------------

UOculusSceneCaptureCubemap::UOculusSceneCaptureCubemap()
	: Stage(None)
	, CaptureBoxSideRes(2048)
	, CaptureFormat(EPixelFormat::PF_A16B16G16R16)
	, OverriddenLocation(FVector::ZeroVector)
	, OverriddenOrientation(FQuat::Identity)
	, CaptureOffset(FVector::ZeroVector)
{
}

void UOculusSceneCaptureCubemap::StartCapture(UWorld* World, uint32 InCaptureBoxSideRes, EPixelFormat InFormat)
{
	CaptureBoxSideRes = InCaptureBoxSideRes;
	CaptureFormat = InFormat;

	FVector Location = OverriddenLocation;
	FQuat Orientation= OverriddenOrientation;

	APlayerController* CapturePlayerController = UGameplayStatics::GetPlayerController(GWorld, 0);
	if (CapturePlayerController)
	{
		FRotator Rotation;
		CapturePlayerController->GetPlayerViewPoint(Location, Rotation);
		Rotation.Pitch = Rotation.Roll = 0;
		Orientation = FQuat(Rotation);

		Location += CaptureOffset;
	}

	if (!OverriddenOrientation.IsIdentity())
	{
		Orientation = OverriddenOrientation;
	}
	if (!OverriddenLocation.IsZero())
	{
		Location = OverriddenLocation;
	}

	const FVector ZAxis(0, 0, 1);
	const FVector YAxis(0, 1, 0);
	const FQuat FaceOrientations[]= {	{ZAxis, PI/2}, { ZAxis, -PI/2},	// right, left
										{YAxis, -PI/2}, { YAxis, PI/2},	// top, bottom
										{ZAxis, 0},    { ZAxis, -PI} }; // front, back

	for (int i = 0; i < 6; ++i)
	{
		USceneCaptureComponent2D* CaptureComponent = NewObject<USceneCaptureComponent2D>();
		CaptureComponent->SetVisibility(true);
		CaptureComponent->SetHiddenInGame(false);

		CaptureComponent->CaptureStereoPass = EStereoscopicPass::eSSP_FULL;//LEFT_EYE; //??
		CaptureComponent->FOVAngle = 90.f;
		CaptureComponent->bCaptureEveryFrame = true;
		CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

		const FName TargetName = MakeUniqueObjectName(this, UTextureRenderTarget2D::StaticClass(), TEXT("SceneCaptureTextureTarget"));
		CaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>(this, TargetName);
		CaptureComponent->TextureTarget->InitCustomFormat(CaptureBoxSideRes, CaptureBoxSideRes, CaptureFormat, false);

		CaptureComponents.Add(CaptureComponent);

		CaptureComponent->RegisterComponentWithWorld(GWorld);

		CaptureComponent->SetWorldLocationAndRotation(Location, Orientation * FaceOrientations[i]);
		CaptureComponent->UpdateContent();
	}
	Stage = SettingPos;

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;

	AStaticMeshActor* InGameActor;
	InGameActor = World->SpawnActor<AStaticMeshActor>(SpawnInfo);

	OutputDir = FPaths::ProjectSavedDir() + TEXT("/Cubemaps");
	IFileManager::Get().MakeDirectory(*OutputDir);
}

void UOculusSceneCaptureCubemap::Tick(float DeltaTime)
{
	OculusHMD::ExecuteOnRenderThread([]()
	{
		TickRenderingTickables();
	});

	if (Stage == SettingPos)
	{
		Stage = Capturing;
		return;
	}

	//Read Whole Capture Buffer
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	TArray<FColor> OneFaceSurface, WholeCubemapData;
	OneFaceSurface.AddUninitialized(CaptureBoxSideRes * CaptureBoxSideRes);
	WholeCubemapData.AddUninitialized(CaptureBoxSideRes * 6 * CaptureBoxSideRes);
	// Read pixels
	for (int cubeFaceIdx = 0; cubeFaceIdx < 6; ++cubeFaceIdx)
	{
		auto RenderTarget = CaptureComponents[cubeFaceIdx]->TextureTarget->GameThread_GetRenderTargetResource();
		RenderTarget->ReadPixelsPtr(OneFaceSurface.GetData(), FReadSurfaceDataFlags());

		// enforce alpha to be 1
		for (FColor& Color : OneFaceSurface)
		{
			Color.A = 255;
		}

		// copy subimage into whole cubemap array
		const uint32 Stride = CaptureBoxSideRes * 6;
		const uint32 XOff = cubeFaceIdx*CaptureBoxSideRes;
		const uint32 StripSizeInBytes = CaptureBoxSideRes * sizeof(FColor);
		for (uint32 y = 0; y < CaptureBoxSideRes; ++y)
		{
			FMemory::Memcpy(WholeCubemapData.GetData() + XOff + y * Stride, OneFaceSurface.GetData() + y * CaptureBoxSideRes, StripSizeInBytes);
		}
	}

	ImageWrapper->SetRaw(WholeCubemapData.GetData(), WholeCubemapData.GetAllocatedSize(), CaptureBoxSideRes * 6, CaptureBoxSideRes, ERGBFormat::BGRA, 8);
	const TArray<uint8>& PNGData = ImageWrapper->GetCompressed(100);

	const FString Filename = OutputDir + FString::Printf(TEXT("/Cubemap-%d-%s.png"), CaptureBoxSideRes, *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M.%S")));

	FFileHelper::SaveArrayToFile(PNGData, *Filename);

	check (Stage == Capturing);
	Stage = Finished;
	for (int i = 0; i < CaptureComponents.Num(); ++i)
	{
		CaptureComponents[i]->UnregisterComponent();
	}
	CaptureComponents.SetNum(0);
	RemoveFromRoot(); // We're done here, so remove ourselves from the root set. @TODO: Fix this later
}


#if !UE_BUILD_SHIPPING
void UOculusSceneCaptureCubemap::CaptureCubemapCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	bool bCreateGearVRCubemap = false;
	FVector CaptureOffset(FVector::ZeroVector);
	float Yaw = 0.f;
	for (const FString& Arg : Args)
	{
		FParse::Value(*Arg, TEXT("XOFF="), CaptureOffset.X);
		FParse::Value(*Arg, TEXT("YOFF="), CaptureOffset.Y);
		FParse::Value(*Arg, TEXT("ZOFF="), CaptureOffset.Z);
		FParse::Value(*Arg, TEXT("YAW="), Yaw);
		if (Arg.Equals(TEXT("GEARVR"), ESearchCase::IgnoreCase))
		{
			bCreateGearVRCubemap = true;
		}
	}

	UOculusSceneCaptureCubemap*	CubemapCapturer = NewObject<UOculusSceneCaptureCubemap>();
	CubemapCapturer->AddToRoot(); // TODO: Don't add the object to the GC root
	CubemapCapturer->SetOffset(CaptureOffset);
	if (Yaw != 0.f)
	{
		FRotator Rotation(FRotator::ZeroRotator);
		Rotation.Yaw = Yaw;
		const FQuat Orient(Rotation);
		CubemapCapturer->SetInitialOrientation(Orient);
	}
	const uint32 CaptureHeight = 2048;
	CubemapCapturer->StartCapture(World, bCreateGearVRCubemap ? CaptureHeight / 2 : CaptureHeight);
}
#endif
