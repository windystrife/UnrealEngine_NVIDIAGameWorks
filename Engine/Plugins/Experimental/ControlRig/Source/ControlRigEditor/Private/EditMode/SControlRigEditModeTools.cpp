// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SControlRigEditModeTools.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "ISequencer.h"
#include "PropertyHandle.h"
#include "ControlRig.h"
#include "ControlRigEditModeSettings.h"
#include "IDetailRootObjectCustomization.h"
#include "ModuleManager.h"
#include "SControlManipulatorPicker.h"
#include "Rigs/HierarchicalRig.h"
#include "Rigs/HumanRig.h"
#include "ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "SExpandableArea.h"
#include "SScrollBox.h"

#define LOCTEXT_NAMESPACE "ControlRigRootCustomization"


class FControlRigRootCustomization : public IDetailRootObjectCustomization
{
public:
	// IDetailRootObjectCustomization interface
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) override
	{
		return SNullWidget::NullWidget;
	}

	virtual bool IsObjectVisible(const UObject* InRootObject) const override
	{
		return true;
	}

	virtual bool ShouldDisplayHeader(const UObject* InRootObject) const override
	{
		return false;
	}
};

void SControlRigEditModeTools::Construct(const FArguments& InArgs)
{
	// initialize settings view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = true;
		DetailsViewArgs.bShowActorLabel = false;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
		DetailsViewArgs.bShowScrollBar = false; // Don't need to show this, as we are putting it in a scroll box
	}

	DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	DetailsView->SetKeyframeHandler(SharedThis(this));
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	DetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	DetailsView->SetRootObjectCustomizationInstance(MakeShareable(new FControlRigRootCustomization));

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(PickerExpander, SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(LOCTEXT("Picker_Header", "Controls"))
				.AreaTitleFont(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f))
				.BodyContent()
				[
					SAssignNew(ControlPicker, SControlManipulatorPicker)
					.OnManipulatorsPicked(this, &SControlRigEditModeTools::OnManipulatorsPicked)
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				DetailsView.ToSharedRef()
			]
		]
	];

	// Bind notification when edit mode selection changes, so we can update picker
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (ControlRigEditMode)
	{
		ControlRigEditMode->OnNodesSelected().AddSP(this, &SControlRigEditModeTools::OnSelectionSetChanged);
	}	
}

void SControlRigEditModeTools::SetDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	DetailsView->SetObjects(InObjects);

	// Look for the first UHierarchicalRig
	UHierarchicalRig* Rig = nullptr;
	for (TWeakObjectPtr<UObject> ObjPtr : InObjects)
	{
		Rig = Cast<UHierarchicalRig>(ObjPtr.Get());
		if (Rig)
		{
			break;
		}
	}

	ControlPicker->SetHierarchicalRig(Rig);

	// Expand when you have a rig, collapse when set to null
	PickerExpander->SetExpanded(Rig != nullptr);
}

void SControlRigEditModeTools::SetSequencer(TSharedPtr<ISequencer> InSequencer)
{
	WeakSequencer = InSequencer;
}

bool SControlRigEditModeTools::IsPropertyKeyable(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	FCanKeyPropertyParams CanKeyPropertyParams(InObjectClass, InPropertyHandle);

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid() && Sequencer->CanKeyProperty(CanKeyPropertyParams))
	{
		return true;
	}

	return false;
}

bool SControlRigEditModeTools::IsPropertyKeyingEnabled() const
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid() && Sequencer->GetFocusedMovieSceneSequence())
	{
		return true;
	}

	return false;
}

void SControlRigEditModeTools::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects(Objects);
	FKeyPropertyParams KeyPropertyParams(Objects, KeyedPropertyHandle, ESequencerKeyMode::ManualKeyForced);

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->KeyProperty(KeyPropertyParams);
	}
}

bool SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization(const FPropertyAndParent& InPropertyAndParent) const
{
	auto ShouldPropertyBeVisible = [](const UProperty& InProperty)
	{
		bool bShow = InProperty.HasAnyPropertyFlags(CPF_Interp) || InProperty.HasMetaData(UControlRig::AnimationInputMetaName) || InProperty.HasMetaData(UControlRig::AnimationOutputMetaName);

		// Show 'PickerIKTogglePos' properties
		bShow |= (InProperty.GetFName() == GET_MEMBER_NAME_CHECKED(FLimbControl, PickerIKTogglePos));
		bShow |= (InProperty.GetFName() == GET_MEMBER_NAME_CHECKED(FSpineControl, PickerIKTogglePos));

		// Always show settings properties
		bShow |= Cast<UClass>(InProperty.GetOuter()) == UControlRigEditModeSettings::StaticClass();

		return bShow;
	};

	bool bContainsVisibleProperty = false;
	if (InPropertyAndParent.Property.IsA<UStructProperty>())
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(&InPropertyAndParent.Property);
		for (TFieldIterator<UProperty> PropertyIt(StructProperty->Struct); PropertyIt; ++PropertyIt)
		{
			if (ShouldPropertyBeVisible(**PropertyIt))
			{
				return true;
			}
		}
	}

	return ShouldPropertyBeVisible(InPropertyAndParent.Property) || (InPropertyAndParent.ParentProperty && ShouldPropertyBeVisible(*InPropertyAndParent.ParentProperty));
}

bool SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization(const FPropertyAndParent& InPropertyAndParent) const
{
	auto ShouldPropertyBeEnabled = [](const UProperty& InProperty)
	{
		bool bShow = InProperty.HasAnyPropertyFlags(CPF_Interp) || InProperty.HasMetaData(UControlRig::AnimationInputMetaName);

		// Always show settings properties
		bShow |= Cast<UClass>(InProperty.GetOuter()) == UControlRigEditModeSettings::StaticClass();

		return bShow;
	};

	bool bContainsVisibleProperty = false;
	if (InPropertyAndParent.Property.IsA<UStructProperty>())
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(&InPropertyAndParent.Property);
		for (TFieldIterator<UProperty> PropertyIt(StructProperty->Struct); PropertyIt; ++PropertyIt)
		{
			if (ShouldPropertyBeEnabled(**PropertyIt))
			{
				return false;
			}
		}
	}

	return !(ShouldPropertyBeEnabled(InPropertyAndParent.Property) || (InPropertyAndParent.ParentProperty && ShouldPropertyBeEnabled(*InPropertyAndParent.ParentProperty)));
}

static bool bPickerChangingSelection = false;

void SControlRigEditModeTools::OnManipulatorsPicked(const TArray<FName>& Manipulators)
{
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (ControlRigEditMode)
	{
		bPickerChangingSelection = true;
		ControlRigEditMode->ClearNodeSelection();
		ControlRigEditMode->SetNodeSelection(Manipulators, true);
		bPickerChangingSelection = false;
	}
}

void SControlRigEditModeTools::OnSelectionSetChanged(const TArray<FName>& SelectedManipulators)
{
	// Don't want to udpate picker selection set if its the picker causing the change
	if (!bPickerChangingSelection)
	{
		ControlPicker->SetSelectedManipulators(SelectedManipulators);
	}
}

#undef LOCTEXT_NAMESPACE
