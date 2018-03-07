// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Redirector.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetTools.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Redirector::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Redirectors = GetTypedWeakObjectPtrs<UObjectRedirector>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Redirector_FindTarget","Find Target"),
		LOCTEXT("Redirector_FindTargetTooltip", "Finds the asset that this redirector targets in the asset tree."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_Redirector::ExecuteFindTarget, Redirectors ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Redirector_FixUp","Fix Up"),
		LOCTEXT("Redirector_FixUpTooltip", "Finds referencers to selected redirectors and resaves them if possible, then deletes any redirectors that had all their referencers fixed."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_Redirector::ExecuteFixUp, Redirectors ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_Redirector::AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType )
{
	if ( ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened )
	{
		// Sync to the target instead of opening an editor when double clicked
		TArray<UObjectRedirector*> Redirectors;
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			UObjectRedirector* Redirector = Cast<UObjectRedirector>(*ObjIt);
			if ( Redirector )
			{
				Redirectors.Add(Redirector);
			}
		}

		if ( Redirectors.Num() > 0 )
		{
			FindTargets(Redirectors);
		}
	}
	else
	{
		FAssetTypeActions_Base::AssetsActivated(InObjects, ActivationType);
	}
}

void FAssetTypeActions_Redirector::ExecuteFindTarget(TArray<TWeakObjectPtr<UObjectRedirector>> Objects)
{
	TArray<UObjectRedirector*> Redirectors;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			Redirectors.Add(Object);
		}
	}

	if ( Redirectors.Num() > 0 )
	{
		FindTargets(Redirectors);
	}
}

void FAssetTypeActions_Redirector::ExecuteFixUp(TArray<TWeakObjectPtr<UObjectRedirector>> Objects)
{
	// This will fix references to selected redirectors, except in the following cases:
	// Redirectors referenced by unloaded maps will not be fixed up, but any references to it that can be fixed up will
	// Redirectors referenced by code will not be completely fixed up
	// Redirectors that are not at head revision or checked out by another user will not be completely fixed up
	// Redirectors whose referencers are not at head revision, are checked out by another user, or are refused to be checked out will not be completely fixed up.

	if ( Objects.Num() > 0 )
	{
		TArray<UObjectRedirector*> ObjectRedirectors;
		for (auto Object : Objects)
		{
			ObjectRedirectors.Add(Object.Get());
		}

		FAssetTools::Get().FixupReferencers(ObjectRedirectors);
	}
}

void FAssetTypeActions_Redirector::FindTargets(const TArray<UObjectRedirector*>& Redirectors) const
{
	TArray<UObject*> ObjectsToSync;
	for (auto RedirectorIt = Redirectors.CreateConstIterator(); RedirectorIt; ++RedirectorIt)
	{
		const UObjectRedirector* Redirector = *RedirectorIt;
		if ( Redirector && Redirector->DestinationObject )
		{
			ObjectsToSync.Add(Redirector->DestinationObject);
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

#undef LOCTEXT_NAMESPACE
