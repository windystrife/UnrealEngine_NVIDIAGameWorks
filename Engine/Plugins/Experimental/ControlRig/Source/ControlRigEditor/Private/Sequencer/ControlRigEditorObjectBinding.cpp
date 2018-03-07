// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorObjectBinding.h"
#include "ISequencer.h"
#include "ControlRigSequence.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "MultiBoxBuilder.h"
#include "ModuleManager.h"
#include "SlateApplication.h"
#include "ControlRigEditMode.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "ControlRigEditorObjectBinding"

FControlRigEditorObjectBinding::FControlRigEditorObjectBinding(TSharedRef<ISequencer> InSequencer)
	: Sequencer(InSequencer)
{
}

TSharedRef<ISequencerEditorObjectBinding> FControlRigEditorObjectBinding::CreateEditorObjectBinding(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FControlRigEditorObjectBinding(InSequencer));
}

void FControlRigEditorObjectBinding::BuildSequencerAddMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddSubMenu(
		LOCTEXT("AddControlRig_Label", "ControlRig"),
		LOCTEXT("AddControlRig_ToolTip", "Add a binding to an animation ControlRig and allow it to be animated by Sequencer"),
		FNewMenuDelegate::CreateRaw(this, &FControlRigEditorObjectBinding::AddSpawnControlRigMenuExtensions),
		false /*bInOpenSubMenuOnClick*/,
		FSlateIcon()
		);
}

bool FControlRigEditorObjectBinding::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return InSequence->IsA<UControlRigSequence>();
}

void FControlRigEditorObjectBinding::AddSpawnControlRigMenuExtensions(FMenuBuilder& MenuBuilder)
{
	class FControlRigClassFilter : public IClassViewerFilter
	{
	public:
		bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			const bool bChildOfObjectClass = InClass->IsChildOf(UControlRig::StaticClass());
			const bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			return bChildOfObjectClass && bMatchesFlags;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(UControlRig::StaticClass());
			const bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			return bChildOfObjectClass && bMatchesFlags;
		}
	};

	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowDisplayNames = true;

	TSharedPtr<FControlRigClassFilter> ClassFilter = MakeShareable(new FControlRigClassFilter);
	Options.ClassFilter = ClassFilter;
	Options.bShowNoneOption = false;

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(this, &FControlRigEditorObjectBinding::HandleControlRigClassPicked));
	MenuBuilder.AddWidget(ClassViewer, FText::GetEmpty(), true);
}

void FControlRigEditorObjectBinding::HandleControlRigClassPicked(UClass* InClass)
{
	FSlateApplication::Get().DismissAllMenus();

	if (InClass && InClass->IsChildOf(UControlRig::StaticClass()) && Sequencer.IsValid())
	{
		FGuid NewGuid = Sequencer.Pin()->MakeNewSpawnable(*InClass);
		Sequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		Sequencer.Pin()->SelectObject(NewGuid);

		if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
		{
			ControlRigEditMode->ReBindToActor();
		}
	}
}

#undef LOCTEXT_NAMESPACE