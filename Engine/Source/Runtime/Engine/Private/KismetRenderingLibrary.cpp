// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetRenderingLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Misc/App.h"
#include "TextureResource.h"
#include "SceneUtils.h"
#include "Logging/MessageLog.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "PackageTools.h"
#endif

//////////////////////////////////////////////////////////////////////////
// UKismetRenderingLibrary

#define LOCTEXT_NAMESPACE "KismetRenderingLibrary"

UKismetRenderingLibrary::UKismetRenderingLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKismetRenderingLibrary::ClearRenderTarget2D(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, FLinearColor ClearColor)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (TextureRenderTarget
		&& TextureRenderTarget->Resource
		&& World)
	{
		FTextureRenderTargetResource* RenderTargetResource = TextureRenderTarget->GameThread_GetRenderTargetResource();
		ENQUEUE_RENDER_COMMAND(ClearRTCommand)(
			[RenderTargetResource, ClearColor](FRHICommandList& RHICmdList)
			{
				SetRenderTarget(RHICmdList, RenderTargetResource->GetRenderTargetTexture(), FTextureRHIRef(), true);
				DrawClearQuad(RHICmdList, ClearColor);
			});
	}
}

UTextureRenderTarget2D* UKismetRenderingLibrary::CreateRenderTarget2D(UObject* WorldContextObject, int32 Width, int32 Height, ETextureRenderTargetFormat Format)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (Width > 0 && Height > 0 && World && FApp::CanEverRender())
	{
		UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(WorldContextObject);
		check(NewRenderTarget2D);
		NewRenderTarget2D->RenderTargetFormat = Format;
		NewRenderTarget2D->InitAutoFormat(Width, Height); 
		NewRenderTarget2D->UpdateResourceImmediate(true);

		return NewRenderTarget2D; 
	}

	return nullptr;
}

// static 
void UKismetRenderingLibrary::ReleaseRenderTarget2D(UTextureRenderTarget2D* TextureRenderTarget)
{
	if (!TextureRenderTarget)
	{
		return;
	}

	TextureRenderTarget->ReleaseResource();
}

void UKismetRenderingLibrary::DrawMaterialToRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, UMaterialInterface* Material)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (!World)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_InvalidWorldContextObject", "DrawMaterialToRenderTarget: WorldContextObject is not valid."));
	}
	else if (!Material)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_InvalidMaterial", "DrawMaterialToRenderTarget: Material must be non-null."));
	}
	else if (!TextureRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_InvalidTextureRenderTarget", "DrawMaterialToRenderTarget: TextureRenderTarget must be non-null."));
	}
	else if (!TextureRenderTarget->Resource)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("DrawMaterialToRenderTarget_ReleasedTextureRenderTarget", "DrawMaterialToRenderTarget: render target has been released."));
	}
	else
	{
		UCanvas* Canvas = World->GetCanvasForDrawMaterialToRenderTarget();

		FCanvas RenderCanvas(
			TextureRenderTarget->GameThread_GetRenderTargetResource(),
			nullptr,
			World,
			World->FeatureLevel);

		Canvas->Init(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY, nullptr, &RenderCanvas);
		Canvas->Update();



		TDrawEvent<FRHICommandList>* DrawMaterialToTargetEvent = new TDrawEvent<FRHICommandList>();

		FName RTName = TextureRenderTarget->GetFName();
		ENQUEUE_RENDER_COMMAND(BeginDrawEventCommand)(
			[RTName, DrawMaterialToTargetEvent](FRHICommandList& RHICmdList)
			{
				BEGIN_DRAW_EVENTF(
					RHICmdList, 
					DrawCanvasToTarget, 
					(*DrawMaterialToTargetEvent), 
					*RTName.ToString());
			});

		Canvas->K2_DrawMaterial(Material, FVector2D(0, 0), FVector2D(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY), FVector2D(0, 0));

		RenderCanvas.Flush_GameThread();
		Canvas->Canvas = NULL;

		FTextureRenderTargetResource* RenderTargetResource = TextureRenderTarget->GameThread_GetRenderTargetResource();
		ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
			[RenderTargetResource, DrawMaterialToTargetEvent](FRHICommandList& RHICmdList)
			{
				RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, true, FResolveParams());
				STOP_DRAW_EVENT((*DrawMaterialToTargetEvent));
				delete DrawMaterialToTargetEvent;
			}
		);
	}
}

void UKismetRenderingLibrary::ExportRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, const FString& FilePath, const FString& FileName)
{
	FString TotalFileName = FPaths::Combine(*FilePath, *FileName);
	FText PathError;
	FPaths::ValidatePath(TotalFileName, &PathError);


	if (!TextureRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_InvalidTextureRenderTarget", "ExportRenderTarget: TextureRenderTarget must be non-null."));
	}
	else if (!TextureRenderTarget->Resource)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_ReleasedTextureRenderTarget", "ExportRenderTarget: render target has been released."));
	}
	else if (!PathError.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(FText::Format(LOCTEXT("ExportRenderTarget_InvalidFilePath", "ExportRenderTarget: Invalid file path provided: '{0}'"), PathError));
	}
	else if (FileName.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_InvalidFileName", "ExportRenderTarget: FileName must be non-empty."));
	}
	else
	{
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*TotalFileName);

		if (Ar)
		{
			FBufferArchive Buffer;

			bool bSuccess = false;
			if (TextureRenderTarget->RenderTargetFormat == RTF_RGBA16f)
			{
				bSuccess = FImageUtils::ExportRenderTarget2DAsHDR(TextureRenderTarget, Buffer);
			}
			else
			{
				bSuccess = FImageUtils::ExportRenderTarget2DAsPNG(TextureRenderTarget, Buffer);
			}

			if (bSuccess)
			{
				Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
			}

			delete Ar;
		}
		else
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("ExportRenderTarget_FileWriterFailedToCreate", "ExportRenderTarget: FileWrite failed to create."));
		}
	}
}
/*

void UKismetRenderingLibrary::CreateTexture2DFromRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* RenderTarget, const FString &TextureAssetName)
{
	if (RenderTarget && Texture)
	{

		//FImageUtils::CreateTexture2D

		UTexture2D* NewTexture = RenderTarget->ConstructTexture2D(Texture->GetOuter(), Texture->GetName(), RenderTarget->GetMaskedFlags(), CTF_Default, NULL);

		check(NewTexture == Texture);
		NewTexture->UpdateResource();
	}
	else if (!RenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ConvertRenderTargetToTexture2D_InvalidRenderTarget", "ExportRenderTarget: RenderTarget must be non-null."));
	}
	else if (!Texture)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ConvertRenderTargetToTexture2D_InvalidTexture", "ExportRenderTarget: Texture must be non-null."));
	}

}*/

UTexture2D* UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly(UTextureRenderTarget2D* RenderTarget, FString InName, enum TextureCompressionSettings CompressionSettings, enum TextureMipGenSettings MipSettings)
{
#if WITH_EDITOR
	if (!RenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("RenderTargetCreateStaticTexture2D_InvalidRenderTarget", "RenderTargetCreateStaticTexture2DEditorOnly: RenderTarget must be non-null."));
		return nullptr;
	}
	else if (!RenderTarget->Resource)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("RenderTargetCreateStaticTexture2D_ReleasedRenderTarget", "RenderTargetCreateStaticTexture2DEditorOnly: RenderTarget has been released."));
		return nullptr;
	}
	else
	{
		FString Name;
		FString PackageName;
		IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		//Use asset name only if directories are specified, otherwise full path
		if (!InName.Contains(TEXT("/")))
		{
			FString AssetName = RenderTarget->GetOutermost()->GetName();
			const FString SanitizedBasePackageName = PackageTools::SanitizePackageName(AssetName);
			const FString PackagePath = FPackageName::GetLongPackagePath(SanitizedBasePackageName) + TEXT("/");
			AssetTools.CreateUniqueAssetName(PackagePath, InName, PackageName, Name);
		}
		else
		{
			InName.RemoveFromStart(TEXT("/"));
			InName.RemoveFromStart(TEXT("Content/"));
			InName.StartsWith(TEXT("Game/")) == true ? InName.InsertAt(0, TEXT("/")) : InName.InsertAt(0, TEXT("/Game/"));
			AssetTools.CreateUniqueAssetName(InName, TEXT(""), PackageName, Name);
		}

		UObject* NewObj = nullptr;

		// create a static 2d texture
		NewObj = RenderTarget->ConstructTexture2D(CreatePackage(NULL, *PackageName), Name, RenderTarget->GetMaskedFlags(), CTF_Default | CTF_AllowMips, NULL);
		UTexture2D* NewTex = Cast<UTexture2D>(NewObj);

		if (NewTex != nullptr)
		{
			// package needs saving
			NewObj->MarkPackageDirty();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewObj);

			// Update Compression and Mip settings
			NewTex->CompressionSettings = CompressionSettings;
			NewTex->MipGenSettings = MipSettings;
			NewTex->PostEditChange();

			return NewTex;
		}
		FMessageLog("Blueprint").Warning(LOCTEXT("RenderTargetCreateStaticTexture2D_FailedToCreateTexture", "RenderTargetCreateStaticTexture2DEditorOnly: Failed to create a new texture."));
	}
#else
	FMessageLog("Blueprint").Error(LOCTEXT("Texture2D's cannot be created at runtime.", "RenderTargetCreateStaticTexture2DEditorOnly: Can't create Texture2D at run time. "));
#endif
	return nullptr;
}


void UKismetRenderingLibrary::ConvertRenderTargetToTexture2DEditorOnly( UObject* WorldContextObject, UTextureRenderTarget2D* RenderTarget, UTexture2D* Texture )
{
#if WITH_EDITOR
	if (!RenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ConvertRenderTargetToTexture2D_InvalidRenderTarget", "ConvertRenderTargetToTexture2DEditorOnly: RenderTarget must be non-null."));
	}
	else if (!RenderTarget->Resource)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ConvertRenderTargetToTexture2D_ReleasedTextureRenderTarget", "ConvertRenderTargetToTexture2DEditorOnly: render target has been released."));
	}
	else if (!Texture)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ConvertRenderTargetToTexture2D_InvalidTexture", "ConvertRenderTargetToTexture2DEditorOnly: Texture must be non-null."));
	}
	else
	{
		UTexture2D* NewTexture = RenderTarget->ConstructTexture2D(Texture->GetOuter(), Texture->GetName(), RenderTarget->GetMaskedFlags(), CTF_Default, NULL);

		check(NewTexture == Texture);

		NewTexture->Modify();
		NewTexture->MarkPackageDirty();
		NewTexture->PostEditChange();
		NewTexture->UpdateResource();
	}
#else
	FMessageLog("Blueprint").Error(LOCTEXT("Convert to render target can't be used at run time.", "ConvertRenderTarget: Can't convert render target to texture2d at run time. "));
#endif

}

void UKismetRenderingLibrary::ExportTexture2D(UObject* WorldContextObject, UTexture2D* Texture, const FString& FilePath, const FString& FileName)
{
	FString TotalFileName = FPaths::Combine(*FilePath, *FileName);
	FText PathError;
	FPaths::ValidatePath(TotalFileName, &PathError);

	if (Texture && !FileName.IsEmpty() && PathError.IsEmpty())
	{
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*TotalFileName);

		if (Ar)
		{
			FBufferArchive Buffer;
			bool bSuccess = FImageUtils::ExportTexture2DAsHDR(Texture, Buffer);

			if (bSuccess)
			{
				Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
			}

			delete Ar;
		}
		else
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("ExportTexture2D_FileWriterFailedToCreate", "ExportTexture2D: FileWrite failed to create."));
		}
	}

	else if (!Texture)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportTexture2D_InvalidTextureRenderTarget", "ExportTexture2D: TextureRenderTarget must be non-null."));
	}
	if (!PathError.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(FText::Format(LOCTEXT("ExportTexture2D_InvalidFilePath", "ExportTexture2D: Invalid file path provided: '{0}'"), PathError));
	}
	if (FileName.IsEmpty())
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("ExportTexture2D_InvalidFileName", "ExportTexture2D: FileName must be non-empty."));
	}
}

void UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, UCanvas*& Canvas, FVector2D& Size, FDrawToRenderTargetContext& Context)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	Canvas = NULL;
	Size = FVector2D(0, 0);
	Context = FDrawToRenderTargetContext();

	if (!World)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("BeginDrawCanvasToRenderTarget_InvalidWorldContextObject", "BeginDrawCanvasToRenderTarget: WorldContextObject is not valid."));
	}
	else if (!TextureRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("BeginDrawCanvasToRenderTarget_InvalidTextureRenderTarget", "BeginDrawCanvasToRenderTarget: TextureRenderTarget must be non-null."));
	}
	else if (!TextureRenderTarget->Resource)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("BeginDrawCanvasToRenderTarget_ReleasedTextureRenderTarget", "BeginDrawCanvasToRenderTarget: render target has been released."));
	}
	else
	{
		Context.RenderTarget = TextureRenderTarget;

		Canvas = World->GetCanvasForRenderingToTarget();

		Size = FVector2D(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY);

		FCanvas* NewCanvas = new FCanvas(
			TextureRenderTarget->GameThread_GetRenderTargetResource(),
			nullptr,
			World,
			World->FeatureLevel,
			// Draw immediately so that interleaved SetVectorParameter (etc) function calls work as expected
			FCanvas::CDM_ImmediateDrawing);
		Canvas->Init(TextureRenderTarget->SizeX, TextureRenderTarget->SizeY, nullptr, NewCanvas);
		Canvas->Update();

		Context.DrawEvent = new TDrawEvent<FRHICommandList>();

		FName RTName = TextureRenderTarget->GetFName();
		TDrawEvent<FRHICommandList>* DrawEvent = Context.DrawEvent;
		ENQUEUE_RENDER_COMMAND(BeginDrawEventCommand)(
			[RTName, DrawEvent](FRHICommandList& RHICmdList)
			{
				BEGIN_DRAW_EVENTF(
					RHICmdList, 
					DrawCanvasToTarget, 
					(*DrawEvent), 
					*RTName.ToString());
			});
	}
}

void UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(UObject* WorldContextObject, const FDrawToRenderTargetContext& Context)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (World)
	{
		UCanvas* WorldCanvas = World->GetCanvasForRenderingToTarget();

		if (WorldCanvas->Canvas)
		{
			WorldCanvas->Canvas->Flush_GameThread();
			delete WorldCanvas->Canvas;
			WorldCanvas->Canvas = nullptr;
		}
		
		if (Context.RenderTarget)
		{
			FTextureRenderTargetResource* RenderTargetResource = Context.RenderTarget->GameThread_GetRenderTargetResource();
			TDrawEvent<FRHICommandList>* DrawEvent = Context.DrawEvent;
			ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
				[RenderTargetResource, DrawEvent](FRHICommandList& RHICmdList)
				{
					RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, true, FResolveParams());
					STOP_DRAW_EVENT((*DrawEvent));
					delete DrawEvent;
				}
			);

			// Remove references to the context now that we've resolved it, to avoid a crash when EndDrawCanvasToRenderTarget is called multiple times with the same context
			// const cast required, as BP will treat Context as an output without the const
			const_cast<FDrawToRenderTargetContext&>(Context) = FDrawToRenderTargetContext();
		}
		else
		{
			FMessageLog("Blueprint").Warning(LOCTEXT("EndDrawCanvasToRenderTarget_InvalidContext", "EndDrawCanvasToRenderTarget: Context must be valid."));
		}
	}
	else
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("EndDrawCanvasToRenderTarget_InvalidWorldContextObject", "EndDrawCanvasToRenderTarget: WorldContextObject is not valid."));
	}
}

FSkelMeshSkinWeightInfo UKismetRenderingLibrary::MakeSkinWeightInfo(int32 Bone0, uint8 Weight0, int32 Bone1, uint8 Weight1, int32 Bone2, uint8 Weight2, int32 Bone3, uint8 Weight3)
{
	FSkelMeshSkinWeightInfo Info;
	FMemory::Memzero(&Info, sizeof(FSkelMeshSkinWeightInfo));
	Info.Bones[0] = Bone0;
	Info.Weights[0] = Weight0;
	Info.Bones[1] = Bone1;
	Info.Weights[1] = Weight1;
	Info.Bones[2] = Bone2;
	Info.Weights[2] = Weight2;
	Info.Bones[3] = Bone3;
	Info.Weights[3] = Weight3;
	return Info;
}


void UKismetRenderingLibrary::BreakSkinWeightInfo(FSkelMeshSkinWeightInfo InWeight, int32& Bone0, uint8& Weight0, int32& Bone1, uint8& Weight1, int32& Bone2, uint8& Weight2, int32& Bone3, uint8& Weight3)
{
	FMemory::Memzero(&InWeight, sizeof(FSkelMeshSkinWeightInfo));
	Bone0 = InWeight.Bones[0];
	Weight0 = InWeight.Weights[0];
	Bone1 = InWeight.Bones[1];
	Weight1 = InWeight.Weights[1];
	Bone2 = InWeight.Bones[2];
	Weight2 = InWeight.Weights[2];
	Bone3 = InWeight.Bones[3];
	Weight3 = InWeight.Weights[3];
}

#undef LOCTEXT_NAMESPACE
