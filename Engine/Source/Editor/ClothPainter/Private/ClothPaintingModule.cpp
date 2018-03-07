// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClothPaintingModule.h"

#include "SClothPaintTab.h"

#include "ModuleManager.h"
#include "PropertyEditorModule.h" 
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

#include "EditorModeRegistry.h"
#include "ClothingPaintEditMode.h"

#include "PropertyEditorModule.h"
#include "ClothPaintSettingsCustomization.h"
#include "Settings/EditorExperimentalSettings.h"
#include "ClothPaintToolCommands.h"
#include "ISkeletalMeshEditorModule.h"
#include "MultiBoxBuilder.h"
#include "ClothPainterCommands.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "ClothPaintingModule"

IMPLEMENT_MODULE(FClothPaintingModule, ClothPainter);

DECLARE_DELEGATE_OneParam(FOnToggleClothPaintMode, bool);

struct FClothPaintTabSummoner : public FWorkflowTabFactory
{
public:
	/** Tab ID name */
	static const FName TabName;

	FClothPaintTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
		: FWorkflowTabFactory(TabName, InHostingApp)
	{
		TabLabel = LOCTEXT("ClothPaintTabLabel", "Clothing");
		TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SkeletalMesh");
	}

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		return SNew(SClothPaintTab).InHostingApp(HostingApp);
	}

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return LOCTEXT("ClothPaintTabToolTip", "Tab for Painting Cloth properties");
	}

	static TSharedPtr<FWorkflowTabFactory> CreateFactory(TSharedPtr<FAssetEditorToolkit> InAssetEditor)
	{
		return MakeShareable(new FClothPaintTabSummoner(InAssetEditor));
	}

protected:

};
const FName FClothPaintTabSummoner::TabName = TEXT("ClothPainting");

void FClothPaintingModule::StartupModule()
{	
	SetupMode();

	// Register any commands for the cloth painter
	ClothPaintToolCommands::RegisterClothPaintToolCommands();
	FClothPainterCommands::Register();

	if(!SkelMeshEditorExtenderHandle.IsValid())
	{
		ISkeletalMeshEditorModule& SkelMeshEditorModule = FModuleManager::Get().LoadModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
		TArray<ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender>& Extenders = SkelMeshEditorModule.GetAllSkeletalMeshEditorToolbarExtenders();
		
		Extenders.Add(ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender::CreateRaw(this, &FClothPaintingModule::ExtendSkelMeshEditorToolbar));
		SkelMeshEditorExtenderHandle = Extenders.Last().GetHandle();
	}
}

void FClothPaintingModule::SetupMode()
{
	// Add application mode extender
	Extender = FWorkflowApplicationModeExtender::CreateRaw(this, &FClothPaintingModule::ExtendApplicationMode);
	FWorkflowCentricApplication::GetModeExtenderList().Add(Extender);

	FEditorModeRegistry::Get().RegisterMode<FClothingPaintEditMode>(PaintModeID, LOCTEXT("ClothPaintEditMode", "Cloth Painting"), FSlateIcon(), false);
}

TSharedRef<FApplicationMode> FClothPaintingModule::ExtendApplicationMode(const FName ModeName, TSharedRef<FApplicationMode> InMode)
{
	// For skeleton and animation editor modes add our custom tab factory to it
	if (ModeName == TEXT("SkeletalMeshEditorMode"))
	{
		InMode->AddTabFactory(FCreateWorkflowTabFactory::CreateStatic(&FClothPaintTabSummoner::CreateFactory));
		RegisteredApplicationModes.Add(InMode);
	}
	
	return InMode;
}

TSharedRef<FExtender> FClothPaintingModule::ExtendSkelMeshEditorToolbar(const TSharedRef<FUICommandList> InCommandList, TSharedRef<ISkeletalMeshEditor> InSkeletalMeshEditor)
{
	// Add toolbar extender
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	TWeakPtr<ISkeletalMeshEditor> Ptr(InSkeletalMeshEditor);

	InCommandList->MapAction(FClothPainterCommands::Get().TogglePaintMode,
		FExecuteAction::CreateRaw(this, &FClothPaintingModule::OnToggleMode, Ptr),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &FClothPaintingModule::GetIsPaintToolsButtonChecked, Ptr)
	);

	ToolbarExtender->AddToolBarExtension("Asset", EExtensionHook::After, InCommandList, FToolBarExtensionDelegate::CreateLambda(
		[this, Ptr](FToolBarBuilder& Builder)
	{
		Builder.AddToolBarButton(
			FClothPainterCommands::Get().TogglePaintMode, 
			NAME_None, 
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FClothPaintingModule::GetPaintToolsButtonText, Ptr)), 
			TAttribute<FText>(), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), 
			"LevelEditor.MeshPaintMode.TexturePaint"));
	}));

	return ToolbarExtender.ToSharedRef();
}

FText FClothPaintingModule::GetPaintToolsButtonText(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor) const
{
	TSharedPtr<SClothPaintTab> ClothTab = GetActiveClothTab(InSkeletalMeshEditor, false);

	if(ClothTab.IsValid())
	{
		if(ClothTab->IsPaintModeActive())
		{
			return LOCTEXT("ToggleButton_Deactivate", "Deactivate Cloth Paint");
		}
	}

	return LOCTEXT("ToggleButton_Activate", "Activate Cloth Paint");
}

bool FClothPaintingModule::GetIsPaintToolsButtonChecked(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor) const
{
	TSharedPtr<SClothPaintTab> ClothTab = GetActiveClothTab(InSkeletalMeshEditor, false);

	if(ClothTab.IsValid())
	{
		return ClothTab->IsPaintModeActive();
	}

	return false;
}

void FClothPaintingModule::OnToggleMode(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor)
{
	TSharedPtr<SClothPaintTab> ClothTab = GetActiveClothTab(InSkeletalMeshEditor);

	if(ClothTab.IsValid())
	{
		ClothTab->TogglePaintMode();
	}
}

TSharedPtr<SClothPaintTab> FClothPaintingModule::GetActiveClothTab(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor, bool bInvoke /*= true*/) const
{
	TSharedPtr<FTabManager> TabManager = InSkeletalMeshEditor.Pin()->GetTabManager();

	if(bInvoke)
	{
		TabManager->InvokeTab(FTabId(FClothPaintTabSummoner::TabName));
	}

	TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(FTabId(FClothPaintTabSummoner::TabName));

	if(Tab.IsValid())
	{
		TSharedPtr<SWidget> TabContent = Tab->GetContent();
		TSharedPtr<SClothPaintTab> ClothingTab = StaticCastSharedPtr<SClothPaintTab>(TabContent);
		return ClothingTab;
	}

	return nullptr;
}

void FClothPaintingModule::ShutdownModule()
{
	ShutdownMode();

	// Remove skel mesh editor extenders
	ISkeletalMeshEditorModule& SkelMeshEditorModule = FModuleManager::GetModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	TArray<ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender>& Extenders = SkelMeshEditorModule.GetAllSkeletalMeshEditorToolbarExtenders();
	
	Extenders.RemoveAll([=](const ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender& InDelegate) {return InDelegate.GetHandle() == SkelMeshEditorExtenderHandle; });
}

void FClothPaintingModule::ShutdownMode()
{
	// Remove extender delegate
	FWorkflowCentricApplication::GetModeExtenderList().RemoveAll([this](FWorkflowApplicationModeExtender& StoredExtender)
	{
		return StoredExtender.GetHandle() == Extender.GetHandle();
	});

	// During shutdown clean up all factories from any modes which are still active/alive
	for(TWeakPtr<FApplicationMode> WeakMode : RegisteredApplicationModes)
	{
		if(WeakMode.IsValid())
		{
			TSharedPtr<FApplicationMode> Mode = WeakMode.Pin();
			Mode->RemoveTabFactory(FClothPaintTabSummoner::TabName);
		}
	}

	RegisteredApplicationModes.Empty();

	FEditorModeRegistry::Get().UnregisterMode(PaintModeID);
}

#undef LOCTEXT_NAMESPACE // "AnimationModifiersModule"