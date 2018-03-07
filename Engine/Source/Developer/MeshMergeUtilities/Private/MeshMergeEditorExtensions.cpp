// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeEditorExtensions.h"
#include "StaticMeshEditorModule.h"
#include "ISkeletalMeshEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Modules/ModuleManager.h"
#include "IPersonaToolkit.h"
#include "Internationalization/Internationalization.h"
#include "Templates/SharedPointer.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "MeshMergeUtilities.h"
#include "Modules/ModuleManager.h"
#include "MeshMergeModule.h"

#define LOCTEXT_NAMESPACE "MeshMergeEditorExtensions"

FDelegateHandle FMeshMergeEditorExtensions::SkeletalMeshEditorExtenderHandle;
FDelegateHandle FMeshMergeEditorExtensions::StaticMeshEditorExtenderHandle;

void FMeshMergeEditorExtensions::OnModulesChanged(FName InModuleName, EModuleChangeReason InChangeReason)
{
	// If one of the modules we are interested in is loaded apply editor extensions
	if (InChangeReason == EModuleChangeReason::ModuleLoaded)
	{		
		if (InModuleName == "SkeletalMeshEditor")
		{
			AddSkeletalMeshEditorToolbarExtender();
		}
		else if (InModuleName == "StaticMeshEditor")
		{
			AddStaticMeshEditorToolbarExtender();
		}
	}
}

void FMeshMergeEditorExtensions::RemoveExtenders()
{
	RemoveSkeletalMeshEditorToolbarExtender();
	RemoveStaticMeshEditorToolbarExtender();
}

TSharedRef<FExtender> FMeshMergeEditorExtensions::GetStaticMeshEditorToolbarExtender(const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> Objects)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	checkf(Objects.Num() && Objects[0]->IsA<UStaticMesh>(), TEXT("Invalid object for static mesh editor toolbar extender"));

	// Add button on static mesh editor toolbar
	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		CommandList,
		FToolBarExtensionDelegate::CreateStatic(&FMeshMergeEditorExtensions::HandleAddStaticMeshActionExtenderToToolbar, Cast<UStaticMesh>(Objects[0]))
	);

	return Extender;
}

void FMeshMergeEditorExtensions::AddStaticMeshEditorToolbarExtender()
{
	IStaticMeshEditorModule& StaticMeshEditorModule = FModuleManager::Get().LoadModuleChecked<IStaticMeshEditorModule>("StaticMeshEditor");

	auto& ToolbarExtenders = StaticMeshEditorModule.GetToolBarExtensibilityManager()->GetExtenderDelegates();
	ToolbarExtenders.Add(FAssetEditorExtender::CreateStatic(&FMeshMergeEditorExtensions::GetStaticMeshEditorToolbarExtender));
	StaticMeshEditorExtenderHandle = ToolbarExtenders.Last().GetHandle();
}

void FMeshMergeEditorExtensions::RemoveStaticMeshEditorToolbarExtender()
{
	IStaticMeshEditorModule* StaticMeshEditorModule = FModuleManager::Get().GetModulePtr<IStaticMeshEditorModule>("StaticMeshEditor");

	if (StaticMeshEditorModule)
	{
		StaticMeshEditorModule->GetToolBarExtensibilityManager()->GetExtenderDelegates().RemoveAll([=](const auto& In) { return In.GetHandle() == SkeletalMeshEditorExtenderHandle; });
	}
}

void FMeshMergeEditorExtensions::HandleAddStaticMeshActionExtenderToToolbar(FToolBarBuilder& ParentToolbarBuilder, UStaticMesh* StaticMesh)
{
	ParentToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([StaticMesh]()
		{
			const IMeshMergeModule& Module = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities");
			Module.GetUtilities().BakeMaterialsForMesh(StaticMesh);
		})),
		NAME_None,
		LOCTEXT("BakeMaterials", "Bake out Materials"),
		LOCTEXT("BakeMaterialsTooltip", "Bake out Materials for given LOD(s)."),
		FSlateIcon("EditorStyle", "Persona.BakeMaterials")
	);
}

void FMeshMergeEditorExtensions::AddSkeletalMeshEditorToolbarExtender()
{
	ISkeletalMeshEditorModule& SkeletalMeshEditorModule = FModuleManager::Get().LoadModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	auto& ToolbarExtenders = SkeletalMeshEditorModule.GetAllSkeletalMeshEditorToolbarExtenders();

	ToolbarExtenders.Add(ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender::CreateStatic(&FMeshMergeEditorExtensions::GetSkeletalMeshEditorToolbarExtender));
	SkeletalMeshEditorExtenderHandle = ToolbarExtenders.Last().GetHandle();
}

void FMeshMergeEditorExtensions::RemoveSkeletalMeshEditorToolbarExtender()
{
	ISkeletalMeshEditorModule* SkeletalMeshEditorModule = FModuleManager::Get().GetModulePtr<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	if (SkeletalMeshEditorModule)
	{
		typedef ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender DelegateType;
		SkeletalMeshEditorModule->GetAllSkeletalMeshEditorToolbarExtenders().RemoveAll([=](const DelegateType& In) { return In.GetHandle() == SkeletalMeshEditorExtenderHandle; });
	}
}

TSharedRef<FExtender> FMeshMergeEditorExtensions::GetSkeletalMeshEditorToolbarExtender(const TSharedRef<FUICommandList> CommandList, TSharedRef<ISkeletalMeshEditor> InSkeletalMeshEditor)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);
	UMeshComponent* MeshComponent = InSkeletalMeshEditor->GetPersonaToolkit()->GetPreviewMeshComponent();

	// Add button on skeletal mesh editor toolbar
	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		CommandList,
		FToolBarExtensionDelegate::CreateStatic(&FMeshMergeEditorExtensions::HandleAddSkeletalMeshActionExtenderToToolbar, MeshComponent)
	);

	return Extender;
}

void FMeshMergeEditorExtensions::HandleAddSkeletalMeshActionExtenderToToolbar(FToolBarBuilder& ParentToolbarBuilder, UMeshComponent* InMeshComponent)
{
	ParentToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([InMeshComponent]()
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent))
			{
				IMeshMergeModule& Module = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities");				
				Module.GetUtilities().BakeMaterialsForComponent(SkeletalMeshComponent);
			}
		})),
		NAME_None,
		LOCTEXT("BakeMaterials", "Bake out Materials"),
		LOCTEXT("BakeMaterialsTooltip", "Bake out Materials for given LOD(s)."),
		FSlateIcon("EditorStyle", "Persona.BakeMaterials")
	);
}

#undef LOCTEXT_NAMESPACE // "MeshMergeEditorExtensions"