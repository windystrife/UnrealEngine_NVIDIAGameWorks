// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ParticleSystem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/FeedbackContext.h"
#include "Editor/Cascade/Public/CascadeModule.h"
#include "Particles/ParticleEmitter.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_ParticleSystem::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto ParticleSystems = GetTypedWeakObjectPtrs<UParticleSystem>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ParticleSystem_CopyParameters", "Copy Parameters"),
		LOCTEXT("ParticleSystem_CopyParametersTooltip", "Copies particle system parameters to the clipboard."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_ParticleSystem::ExecuteCopyParameters, ParticleSystems ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ParticleSystem_ConvertToSeeded", "Convert To Seeded"),
		LOCTEXT("ParticleSystem_ConvertToSeededTooltip", "Convert all modules in this particle system to random seeded modules"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_ParticleSystem::ConvertToSeeded, ParticleSystems ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_ParticleSystem::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto ParticleSystem = Cast<UParticleSystem>(*ObjIt);
		if (ParticleSystem != NULL)
		{
			ICascadeModule* CascadeModule = &FModuleManager::LoadModuleChecked<ICascadeModule>( "Cascade" );
			CascadeModule->CreateCascade(Mode, EditWithinLevelEditor, ParticleSystem);
		}
	}
}

void FAssetTypeActions_ParticleSystem::ExecuteCopyParameters(TArray<TWeakObjectPtr<UParticleSystem>> Objects)
{
	FString ClipboardString = TEXT("");
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			TArray<TArray<FString>> ParticleSysParamList;
			TArray<TArray<FString>> ParticleParameterList;
			Object->GetParametersUtilized(ParticleSysParamList, ParticleParameterList);

			ClipboardString += FString::Printf(TEXT("ParticleSystem parameters for %s\n"), *(Object->GetPathName()));
			for (int32 EmitterIndex = 0; EmitterIndex < Object->Emitters.Num(); EmitterIndex++)
			{
				ClipboardString += FString::Printf(TEXT("\tEmitter %2d - "), EmitterIndex);
				UParticleEmitter* Emitter = Object->Emitters[EmitterIndex];
				if (Emitter)
				{
					ClipboardString += FString::Printf(TEXT("%s\n"), *(Emitter->GetEmitterName().ToString()));
				}
				else
				{
					ClipboardString += FString(TEXT("* EMPTY *\n"));
				}

				TArray<FString>& ParticleSysParams = ParticleSysParamList[EmitterIndex];
				for (int32 PSPIndex = 0; PSPIndex < ParticleSysParams.Num(); PSPIndex++)
				{
					if (PSPIndex == 0)
					{
						ClipboardString += FString(TEXT("\t\tParticleSysParam List\n"));
					}
					ClipboardString += FString::Printf(TEXT("\t\t\t%s"), *(ParticleSysParams[PSPIndex]));
				}

				TArray<FString>& ParticleParameters = ParticleParameterList[EmitterIndex];
				for (int32 PPIndex = 0; PPIndex < ParticleParameters.Num(); PPIndex++)
				{
					if (PPIndex == 0)
					{
						ClipboardString += FString(TEXT("\t\tParticleParameter List\n"));
					}
					ClipboardString += FString::Printf(TEXT("\t\t\t%s"), *(ParticleParameters[PPIndex]));
				}
			}
		}
	}

	FPlatformApplicationMisc::ClipboardCopy(*ClipboardString);
}

void FAssetTypeActions_ParticleSystem::ConvertToSeeded(TArray<TWeakObjectPtr<UParticleSystem>> Objects)
{
	ICascadeModule* CascadeModule = &FModuleManager::LoadModuleChecked<ICascadeModule>( "Cascade" );

	if( Objects.Num() > 0 )
	{
		GWarn->BeginSlowTask( LOCTEXT("ParticleSystem_ConvertToSeeded_SlowTask", "Converting Particle Systems to Seeded"), true );
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				GWarn->StatusUpdate( 
					ObjIt.GetIndex(), 
					Objects.Num(),
					FText::Format( LOCTEXT("ParticleSystem_ConvertToSeeded_StatusUpdate", "Converting {0} to Seeded"), FText::FromString( Object->GetName() ) ) );

				CascadeModule->ConvertModulesToSeeded(Object);
				CascadeModule->RefreshCascade(Object);
			}
		}
		GWarn->EndSlowTask();
	}

}

#undef LOCTEXT_NAMESPACE
