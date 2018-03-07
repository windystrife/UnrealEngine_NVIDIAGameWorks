// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "BufferVisualizationData.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Misc/ConfigCacheIni.h"

static FBufferVisualizationData GBufferVisualizationData;

static TAutoConsoleVariable<int32> BufferVisualizationDumpFramesAsHDR(
	TEXT("r.BufferVisualizationDumpFramesAsHDR"),
	0,
	TEXT("When saving out buffer visualization materials in a HDR capable format\n")
	TEXT("0: Do not override default save format.\n")
	TEXT("1: Force HDR format for buffer visualization materials."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarBufferVisualizationDumpFrames(
	TEXT("r.BufferVisualizationDumpFrames"),
	0,
	TEXT("When screenshots or movies dumps are requested, also save out dumps of the current buffer visualization materials\n")
	TEXT("0:off (default)\n")
	TEXT("1:on"),
	ECVF_RenderThreadSafe);

void FBufferVisualizationData::Initialize()
{
	if (!bIsInitialized)
	{
		if (AllowDebugViewmodes())
		{
			check(MaterialMap.Num() == 0);

			FConfigSection* MaterialSection = GConfig->GetSectionPrivate( TEXT("Engine.BufferVisualizationMaterials"), false, true, GEngineIni );

			if (MaterialSection != NULL)
			{
				for (FConfigSection::TIterator It(*MaterialSection); It; ++It)
				{
					FString MaterialName;
					if( FParse::Value( *It.Value().GetValue(), TEXT("Material="), MaterialName, true ) )
					{
						UMaterial* Material = LoadObject<UMaterial>(NULL, *MaterialName);
			
						if (Material)
						{
							Material->AddToRoot();
							Record& Rec = MaterialMap.Add(It.Key(), Record());
							Rec.Name = It.Key().GetPlainNameString();
							Rec.Material = Material;
							FText DisplayName;
							FParse::Value( *It.Value().GetValue(), TEXT("Name="), DisplayName, TEXT("Engine.BufferVisualizationMaterials") );
							Rec.DisplayName = DisplayName;
						}
					}
				}
			}

			ConfigureConsoleCommand();
		}

		bIsInitialized = true;
	}
}

void FBufferVisualizationData::ConfigureConsoleCommand()
{
	struct Local
	{
		FString& Message;

		Local(FString& OutString)
			: Message(OutString)
		{
		}

		void ProcessValue(const FString& InMaterialName, const UMaterial* InMaterial, const FText& InDisplayName)
		{
			Message += FString(TEXT("\n  "));
			Message += InMaterialName;
		}
	};

	FString AvailableVisualizationMaterials;
	Local It(AvailableVisualizationMaterials);
	IterateOverAvailableMaterials(It);

	ConsoleDocumentationVisualizationMode = TEXT("When the viewport view-mode is set to 'Buffer Visualization', this command specifies which of the various channels to display. Values entered other than the allowed values shown below will be ignored.");
	ConsoleDocumentationVisualizationMode += AvailableVisualizationMaterials;

	IConsoleManager::Get().RegisterConsoleVariable(
		GetVisualizationTargetConsoleCommandName(),
		TEXT(""),
		*ConsoleDocumentationVisualizationMode,
		ECVF_Cheat);

	ConsoleDocumentationOverviewTargets = TEXT("Specify the list of post process materials that can be used in the buffer visualization overview. Put nothing between the commas to leave a gap.\n\n\tChoose from:\n");
	ConsoleDocumentationOverviewTargets += AvailableVisualizationMaterials;

	IConsoleManager::Get().RegisterConsoleVariable(TEXT("r.BufferVisualizationOverviewTargets"),
		TEXT("BaseColor,Specular,SubsurfaceColor,WorldNormal,SeparateTranslucencyRGB,,,Opacity,SeparateTranslucencyA,,,,SceneDepth,Roughness,Metallic,ShadingModel,,SceneDepthWorldUnits,SceneColor,PreTonemapHDRColor,PostTonemapHDRColor"), 
		*ConsoleDocumentationOverviewTargets,
		ECVF_Default);
}

UMaterial* FBufferVisualizationData::GetMaterial(FName InMaterialName)
{
	Record* Result = MaterialMap.Find(InMaterialName);
	if (Result)
	{
		return Result->Material;
	}
	else
	{
		return NULL;
	}
}

void FBufferVisualizationData::SetCurrentOverviewMaterialNames(const FString& InNameList)
{
	CurrentOverviewMaterialNames = InNameList;
}

bool FBufferVisualizationData::IsDifferentToCurrentOverviewMaterialNames(const FString& InNameList)
{
	return InNameList != CurrentOverviewMaterialNames;
}

TArray<UMaterial*>& FBufferVisualizationData::GetOverviewMaterials()
{
	return OverviewMaterials;
}

FBufferVisualizationData& GetBufferVisualizationData()
{
	if (!GBufferVisualizationData.IsInitialized())
	{
		GBufferVisualizationData.Initialize();
	}

	return GBufferVisualizationData;
}
