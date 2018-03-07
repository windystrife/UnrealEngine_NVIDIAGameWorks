// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialBakingModule.h"
#include "MaterialRenderItem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ExportMaterialProxy.h"
#include "Interfaces/IMainFrameModule.h"
#include "MaterialOptionsWindow.h"
#include "MaterialOptions.h"
#include "PropertyEditorModule.h"
#include "MaterialOptionsCustomization.h"
#include "UObject/UObjectGlobals.h"
#include "MaterialBakingStructures.h"
#include "Framework/Application/SlateApplication.h"
#include "MaterialBakingHelpers.h"
#include "Async/ParallelFor.h"

#if WITH_EDITOR
#include "FileHelper.h"
#endif

IMPLEMENT_MODULE(FMaterialBakingModule, MaterialBaking);

#define LOCTEXT_NAMESPACE "MaterialBakingModule"

/** Cvars for advanced features */
static TAutoConsoleVariable<int32> CVarUseMaterialProxyCaching(
	TEXT("MaterialBaking.UseMaterialProxyCaching"),
	1,
	TEXT("Determines whether or not Material Proxies should be cached to speed up material baking.\n")
	TEXT("0: Turned Off\n")
	TEXT("1: Turned On"),	
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSaveIntermediateTextures(
	TEXT("MaterialBaking.SaveIntermediateTextures"),
	0,
	TEXT("Determines whether or not to save out intermediate BMP images for each flattened material property.\n")
	TEXT("0: Turned Off\n")
	TEXT("1: Turned On"),
	ECVF_Default);

void FMaterialBakingModule::StartupModule()
{
	// Set which properties should enforce gamme correction
	FMemory::Memset(PerPropertyGamma, (uint8)false);
	PerPropertyGamma[MP_Normal] = true;
	PerPropertyGamma[MP_Opacity] = true;
	PerPropertyGamma[MP_OpacityMask] = true;

	// Set which pixel format should be used for the possible baked out material properties
	FMemory::Memset(PerPropertyFormat, (uint8)PF_Unknown);
	PerPropertyFormat[MP_EmissiveColor] = PF_FloatRGBA;
	PerPropertyFormat[MP_Opacity] = PF_B8G8R8A8;
	PerPropertyFormat[MP_OpacityMask] = PF_B8G8R8A8;
	PerPropertyFormat[MP_BaseColor] = PF_B8G8R8A8;
	PerPropertyFormat[MP_Metallic] = PF_B8G8R8A8;
	PerPropertyFormat[MP_Specular] = PF_B8G8R8A8;
	PerPropertyFormat[MP_Roughness] = PF_B8G8R8A8;
	PerPropertyFormat[MP_Normal] = PF_B8G8R8A8;
	PerPropertyFormat[MP_AmbientOcclusion] = PF_B8G8R8A8;
	PerPropertyFormat[MP_SubsurfaceColor] = PF_B8G8R8A8;

	// Register property customization
	FPropertyEditorModule& Module = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Module.RegisterCustomPropertyTypeLayout(TEXT("PropertyEntry"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertyEntryCustomization::MakeInstance));
	
	// Register callback for modified objects
	FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FMaterialBakingModule::OnObjectModified);
}

void FMaterialBakingModule::ShutdownModule()
{
	// Urnegister customization and callback
	FPropertyEditorModule& Module = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Module.UnregisterCustomPropertyTypeLayout(TEXT("PropertyEntry"));
	FCoreUObjectDelegates::OnObjectModified.RemoveAll(this);
}

void FMaterialBakingModule::BakeMaterials(const TArray<FMaterialData*>& MaterialSettings, const TArray<FMeshData*>& MeshSettings, TArray<FBakeOutput>& Output)
{
	checkf(MaterialSettings.Num() == MeshSettings.Num(), TEXT("Number of material settings does not match that of MeshSettings"));
	const int32 NumMaterials = MaterialSettings.Num();
	const bool bSaveIntermediateTextures = CVarSaveIntermediateTextures.GetValueOnAnyThread() == 1;

	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		const FMaterialData* CurrentMaterialSettings = MaterialSettings[MaterialIndex];
		const FMeshData* CurrentMeshSettings = MeshSettings[MaterialIndex];
		FBakeOutput& CurrentOutput = [&Output]() -> FBakeOutput&
		{
			Output.AddDefaulted(1);
			return Output.Last();
		}();

		// Canvas per size / special property?
		TArray<TPair<UTextureRenderTarget2D*, FSceneViewFamily>> TargetsViewFamilyPairs;
		TArray<FExportMaterialProxy*> MaterialRenderProxies;
		TArray<EMaterialProperty> MaterialPropertiesToBakeOut;

		TArray<UTexture*> MaterialTextures;
		CurrentMaterialSettings->Material->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);

		// Force load materials used by the current material
		for (UTexture* Texture : MaterialTextures)
		{
			if (Texture != NULL)
			{
				UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
				if (Texture2D)
				{
					Texture2D->SetForceMipLevelsToBeResident(30.0f);
					Texture2D->WaitForStreaming();
				}
			}
		}
		
		// Generate view family, material proxy and render targets for each material property being baked out
		for (TMap<EMaterialProperty, FIntPoint>::TConstIterator PropertySizeIterator = CurrentMaterialSettings->PropertySizes.CreateConstIterator(); PropertySizeIterator; ++PropertySizeIterator)
		{
			const EMaterialProperty& Property = PropertySizeIterator.Key();
			UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(PerPropertyGamma[Property], PerPropertyFormat[Property], PropertySizeIterator.Value());
			FExportMaterialProxy* Proxy = CreateMaterialProxy(CurrentMaterialSettings->Material, Property);

			FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget->GameThread_GetRenderTargetResource(), nullptr,
				FEngineShowFlags(ESFIM_Game))
				.SetWorldTimes(0.0f, 0.0f, 0.0f)
				.SetGammaCorrection(RenderTarget->GameThread_GetRenderTargetResource()->GetDisplayGamma()));

			TargetsViewFamilyPairs.Add(TPair<UTextureRenderTarget2D*, FSceneViewFamily>(RenderTarget, ViewFamily));
			MaterialRenderProxies.Add(Proxy);
			MaterialPropertiesToBakeOut.Add(Property);
		}

		const int32 NumPropertiesToRender = CurrentMaterialSettings->PropertySizes.Num();
		if (NumPropertiesToRender > 0)
		{
			FCanvas Canvas(TargetsViewFamilyPairs[0].Key->GameThread_GetRenderTargetResource(), nullptr, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, GMaxRHIFeatureLevel);
			Canvas.SetAllowedModes(FCanvas::Allow_Flush);

			UTextureRenderTarget2D* PreviousRenderTarget = nullptr;
			FTextureRenderTargetResource* RenderTargetResource = nullptr;

			FMeshMaterialRenderItem RenderItem(CurrentMaterialSettings, CurrentMeshSettings, MaterialPropertiesToBakeOut[0]);
			FCanvas::FCanvasSortElement& SortElement = Canvas.GetSortElement(Canvas.TopDepthSortKey());

			for (int32 PropertyIndex = 0; PropertyIndex < NumPropertiesToRender; ++PropertyIndex)
			{
				const EMaterialProperty Property = MaterialPropertiesToBakeOut[PropertyIndex];
				// Update render item
				RenderItem.MaterialProperty = Property;
				RenderItem.MaterialRenderProxy = MaterialRenderProxies[PropertyIndex];

				UTextureRenderTarget2D* RenderTarget = TargetsViewFamilyPairs[PropertyIndex].Key;
				if (RenderTarget != nullptr)
				{
					// Check if the render target changed, in which case we will need to reinit the resource, view family and possibly the mesh data
					if (RenderTarget != PreviousRenderTarget)
					{
						// Setup render target and view family
						RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
						Canvas.SetRenderTarget_GameThread(RenderTargetResource);
						const FRenderTarget* CanvasRenderTarget = Canvas.GetRenderTarget();

						RenderItem.ViewFamily = &TargetsViewFamilyPairs[PropertyIndex].Value;

						if (PreviousRenderTarget != nullptr && (PreviousRenderTarget->GetSurfaceHeight() != RenderTarget->GetSurfaceHeight() || PreviousRenderTarget->GetSurfaceWidth() != RenderTarget->GetSurfaceWidth()))
						{
							RenderItem.GenerateRenderData();
						}

						Canvas.SetRenderTargetRect(FIntRect(0, 0, RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight()));
						Canvas.SetBaseTransform(Canvas.CalcBaseTransform2D(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight()));
						PreviousRenderTarget = RenderTarget;
					}

					// Clear canvas before rendering
					Canvas.Clear(RenderTarget->ClearColor);

					SortElement.RenderBatchArray.Add(&RenderItem);

					// Do rendering
					Canvas.Flush_GameThread();
					FlushRenderingCommands();

					SortElement.RenderBatchArray.Empty();
					ReadTextureOutput(RenderTargetResource, Property, CurrentOutput);
					FMaterialBakingHelpers::PerformUVBorderSmear(CurrentOutput.PropertyData[Property], RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight(), Property == MP_Normal);
#if WITH_EDITOR
					// If saving intermediates is turned on
					if (bSaveIntermediateTextures)
					{
						const UEnum* PropertyEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMaterialProperty"));
						FName PropertyName = PropertyEnum->GetNameByValue(Property);
						FString TrimmedPropertyName = PropertyName.ToString();
						TrimmedPropertyName.RemoveFromStart(TEXT("MP_"));

						const FString DirectoryPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("MaterialBaking/"));
						FString FilenameString = FString::Printf(*(DirectoryPath + TEXT("%s-%d-%s.bmp")),
							*CurrentMaterialSettings->Material->GetName(), MaterialIndex, *TrimmedPropertyName);
						FFileHelper::CreateBitmap(*FilenameString, CurrentOutput.PropertySizes[Property].X, CurrentOutput.PropertySizes[Property].Y, CurrentOutput.PropertyData[Property].GetData());
					}
				}
#endif // WITH_EDITOR
			}
		}
	}

	if (!CVarUseMaterialProxyCaching.GetValueOnAnyThread())
	{
		CleanupMaterialProxies();
	}
}

bool FMaterialBakingModule::SetupMaterialBakeSettings(TArray<TWeakObjectPtr<UObject>>& OptionObjects, int32 NumLODs)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Material Baking Options"))
		.SizingRule(ESizingRule::Autosized);

	TSharedPtr<SMaterialOptions> Options;

	Window->SetContent
	(
		SAssignNew(Options, SMaterialOptions)
		.WidgetWindow(Window)
		.NumLODs(NumLODs)
		.SettingsObjects(OptionObjects)
	);

	TSharedPtr<SWindow> ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
		return !Options->WasUserCancelled();
	}

	return false;
}

void FMaterialBakingModule::CleanupMaterialProxies()
{
	for (auto Iterator : MaterialProxyPool)
	{
		delete Iterator.Value;
	}
	MaterialProxyPool.Reset();
}

UTextureRenderTarget2D* FMaterialBakingModule::CreateRenderTarget(bool bInForceLinearGamma, EPixelFormat InPixelFormat, const FIntPoint& InTargetSize)
{
	UTextureRenderTarget2D* RenderTarget = nullptr;
	const FIntPoint ClampedTargetSize(FMath::Clamp(InTargetSize.X, 1, (int32)GetMax2DTextureDimension()), FMath::Clamp(InTargetSize.Y, 1, (int32)GetMax2DTextureDimension()));
	auto RenderTargetComparison = [bInForceLinearGamma, InPixelFormat, ClampedTargetSize](const UTextureRenderTarget2D* CompareRenderTarget) -> bool
	{
		return (CompareRenderTarget->SizeX == ClampedTargetSize.X && CompareRenderTarget->SizeY == ClampedTargetSize.Y && CompareRenderTarget->OverrideFormat == InPixelFormat && CompareRenderTarget->bForceLinearGamma == bInForceLinearGamma);
	};

	// Find any pooled render target with suitable properties.
	UTextureRenderTarget2D** FindResult = RenderTargetPool.FindByPredicate(RenderTargetComparison);
	
	if (FindResult)
	{
		RenderTarget = *FindResult;
	}
	else
	{
		// Not found - create a new one.
		RenderTarget = NewObject<UTextureRenderTarget2D>();
		check(RenderTarget);
		RenderTarget->AddToRoot();
		RenderTarget->ClearColor = FLinearColor(1.0f, 0.0f, 1.0f);
		RenderTarget->ClearColor.A = 1.0f;
		RenderTarget->TargetGamma = 0.0f;
		RenderTarget->InitCustomFormat(ClampedTargetSize.X, ClampedTargetSize.Y, InPixelFormat, bInForceLinearGamma);

		RenderTargetPool.Add(RenderTarget);
	}

	checkf(RenderTarget != nullptr, TEXT("Unable to create or find valid render target"));
	return RenderTarget;
}

FExportMaterialProxy* FMaterialBakingModule::CreateMaterialProxy(UMaterialInterface* Material, const EMaterialProperty Property)
{
	FExportMaterialProxy* Proxy = nullptr;

	// Find any pooled material proxy matching this material and property
	FExportMaterialProxy** FindResult = MaterialProxyPool.Find(TPair<UMaterialInterface*, EMaterialProperty>(Material, Property));
	if (FindResult)
	{
		Proxy = *FindResult;
	}
	else
	{
		Proxy = new FExportMaterialProxy(Material, Property);		
		MaterialProxyPool.Add(TPair<UMaterialInterface*, EMaterialProperty>(Material, Property), Proxy);
	}

	return Proxy;
}

void FMaterialBakingModule::ReadTextureOutput(FTextureRenderTargetResource* RenderTargetResource, EMaterialProperty Property, FBakeOutput& Output)
{	
	checkf(!Output.PropertyData.Contains(Property) && !Output.PropertySizes.Contains(Property), TEXT("Should be reading same property data twice"));

	TArray<FColor>& OutputColor = Output.PropertyData.Add(Property);
	FIntPoint& OutputSize = Output.PropertySizes.Add(Property);	
	
	// Retrieve rendered size
	OutputSize = RenderTargetResource->GetSizeXY();

	const bool bNormalmap = (Property== MP_Normal);
	FReadSurfaceDataFlags ReadPixelFlags(bNormalmap ? RCM_SNorm : RCM_UNorm);
	ReadPixelFlags.SetLinearToGamma(false);
		
	if (Property != MP_EmissiveColor)
	{
		// Read out pixel data
		RenderTargetResource->ReadPixels(OutputColor, ReadPixelFlags);
	}
	else
	{
		// Emissive is a special case	
		TArray<FFloat16Color> Color16;
		RenderTargetResource->ReadFloat16Pixels(Color16);		
		
		const int32 NumThreads = [&]()
		{
			return FPlatformProcess::SupportsMultithreading() ? FPlatformMisc::NumberOfCores() : 1;
		}();

		float* MaxValue = new float[NumThreads];
		const int32 LinesPerThread = FMath::CeilToInt((float)OutputSize.Y / (float)NumThreads);

		// Find maximum float value across texture
		ParallelFor(NumThreads, [&Color16, LinesPerThread, MaxValue, OutputSize](int32 Index)
		{
			const int32 EndY = FMath::Min((Index + 1) * LinesPerThread, OutputSize.Y);			
			float& CurrentMaxValue = MaxValue[Index];
			const FFloat16Color MagentaFloat16 = FFloat16Color(FLinearColor(1.0f, 0.0f, 1.0f));
			for (int32 PixelY = Index * LinesPerThread; PixelY < EndY; ++PixelY)
			{
				const int32 YOffset = PixelY * OutputSize.X;
				for (int32 PixelX = 0; PixelX < OutputSize.X; PixelX++)
				{
					const FFloat16Color& Pixel16 = Color16[PixelX + YOffset];
					// Find maximum channel value across texture
					if (!(Pixel16 == MagentaFloat16))
					{
						CurrentMaxValue = FMath::Max(CurrentMaxValue, FMath::Max3(Pixel16.R.GetFloat(), Pixel16.G.GetFloat(), Pixel16.B.GetFloat()));
					}
				}
			}
		});

		const float GlobalMaxValue = [&MaxValue, NumThreads]
		{
			float TempValue = 0.0f;
			for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ++ThreadIndex)
			{
				TempValue = FMath::Max(TempValue, MaxValue[ThreadIndex]);
			}

			return TempValue;
		}();
		
		if (GlobalMaxValue <= 0.01f)
		{
			// Black emissive, drop it			
		}

		// Now convert Float16 to Color using the scale
		OutputColor.SetNumUninitialized(Color16.Num());
		const float Scale = 255.0f / GlobalMaxValue;
		ParallelFor(NumThreads, [&Color16, LinesPerThread, &OutputColor, OutputSize, Scale](int32 Index)
		{
			const int32 EndY = FMath::Min((Index + 1) * LinesPerThread, OutputSize.Y);
			for (int32 PixelY = Index * LinesPerThread; PixelY < EndY; ++PixelY)
			{
				const int32 YOffset = PixelY * OutputSize.X;
				for (int32 PixelX = 0; PixelX < OutputSize.X; PixelX++)
				{
					const FFloat16Color& Pixel16 = Color16[PixelX + YOffset];
					FColor& Pixel8 = OutputColor[PixelX + YOffset];
					Pixel8.R = (uint8)FMath::RoundToInt(Pixel16.R.GetFloat() * Scale);
					Pixel8.G = (uint8)FMath::RoundToInt(Pixel16.G.GetFloat() * Scale);
					Pixel8.B = (uint8)FMath::RoundToInt(Pixel16.B.GetFloat() * Scale);
					Pixel8.A = 255;
				}
			}
		});

		// This scale will be used in the proxy material to get the original range of emissive values outside of 0-1
		Output.EmissiveScale = GlobalMaxValue;
	}	
}

void FMaterialBakingModule::OnObjectModified(UObject* Object)
{
	if (CVarUseMaterialProxyCaching.GetValueOnAnyThread())
	{
		if (Object && Object->IsA<UMaterialInterface>())
		{
			UMaterialInterface* Material = Cast<UMaterialInterface>(Object);
			if (MaterialProxyPool.Contains(TPair<UMaterialInterface*, EMaterialProperty>(Material, MP_BaseColor)))
			{
				for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
				{
					MaterialProxyPool.Remove(TPair<UMaterialInterface*, EMaterialProperty>(Material, (EMaterialProperty)PropertyIndex));
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE //"MaterialBakingModule"
