// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerContextMenus.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "SectionLayout.h"
#include "SSequencerSection.h"
#include "SequencerSettings.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"
#include "ScopedTransaction.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneKeyStruct.h"
#include "Framework/Commands/GenericCommands.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "IntegralKeyDetailsCustomization.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Curves/IntegralCurve.h"
#include "Editor.h"
#include "SequencerUtilities.h"
#include "NotifyHook.h"
#include "EditorUndoClient.h"
#include "ClassViewerModule.h"
#include "MovieSceneEasingFunction.h"
#include "ClassViewerFilter.h"
#include "SNumericEntryBox.h"
#include "SCheckBox.h"
#include "SBoxPanel.h"
#include "SBox.h"

#define LOCTEXT_NAMESPACE "SequencerContextMenus"


void FKeyContextMenu::BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& InSequencer)
{
	TSharedRef<FKeyContextMenu> Menu = MakeShareable(new FKeyContextMenu(InSequencer));
	Menu->PopulateMenu(MenuBuilder);
}


void FKeyContextMenu::PopulateMenu(FMenuBuilder& MenuBuilder)
{
	FSequencer* SequencerPtr = &Sequencer.Get();
	TSharedRef<FKeyContextMenu> Shared = AsShared();

	if(CanAddPropertiesMenu())
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("KeyProperties", "Properties"),
			LOCTEXT("KeyPropertiesTooltip", "Modify the key properties"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddPropertiesMenu(SubMenuBuilder); }),
			FUIAction (
				FExecuteAction(),
				// @todo sequencer: only one struct per structure view supported right now :/
				FCanExecuteAction::CreateLambda([=]{ return (Shared->Sequencer->GetSelection().GetSelectedKeys().Num() == 1); })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	MenuBuilder.BeginSection("SequencerKeyEdit", LOCTEXT("EditMenu", "Edit"));
	{
		TSharedPtr<ISequencerHotspot> Hotspot = SequencerPtr->GetHotspot();

		if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
		{
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		}
	}
	MenuBuilder.EndSection(); // SequencerKeyEdit

	MenuBuilder.BeginSection("SequencerInterpolation", LOCTEXT("KeyInterpolationMenu", "Key Interpolation"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationAuto", "Cubic (Auto)"),
			LOCTEXT("SetKeyInterpolationAutoTooltip", "Set key interpolation to auto"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyAuto"),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SetInterpTangentMode, RCIM_Cubic, RCTM_Auto),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(SequencerPtr, &FSequencer::IsInterpTangentModeSelected, RCIM_Cubic, RCTM_Auto) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationUser", "Cubic (User)"),
			LOCTEXT("SetKeyInterpolationUserTooltip", "Set key interpolation to user"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyUser"),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SetInterpTangentMode, RCIM_Cubic, RCTM_User),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(SequencerPtr, &FSequencer::IsInterpTangentModeSelected, RCIM_Cubic, RCTM_User) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationBreak", "Cubic (Break)"),
			LOCTEXT("SetKeyInterpolationBreakTooltip", "Set key interpolation to break"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyBreak"),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SetInterpTangentMode, RCIM_Cubic, RCTM_Break),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(SequencerPtr, &FSequencer::IsInterpTangentModeSelected, RCIM_Cubic, RCTM_Break) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationLinear", "Linear"),
			LOCTEXT("SetKeyInterpolationLinearTooltip", "Set key interpolation to linear"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyLinear"),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SetInterpTangentMode, RCIM_Linear, RCTM_Auto),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(SequencerPtr, &FSequencer::IsInterpTangentModeSelected, RCIM_Linear, RCTM_Auto) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetKeyInterpolationConstant", "Constant"),
			LOCTEXT("SetKeyInterpolationConstantTooltip", "Set key interpolation to constant"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyConstant"),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SetInterpTangentMode, RCIM_Constant, RCTM_Auto),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(SequencerPtr, &FSequencer::IsInterpTangentModeSelected, RCIM_Constant, RCTM_Auto) ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection(); // SequencerInterpolation

	MenuBuilder.BeginSection("SequencerKeys", LOCTEXT("KeysMenu", "Keys"));
	{
		const bool bUseFrames = SequencerSnapValues::IsTimeSnapIntervalFrameRate(Sequencer->GetFixedFrameInterval());

		MenuBuilder.AddMenuEntry(
			bUseFrames ? LOCTEXT("SetKeyFrame", "Set Key Frame") : LOCTEXT("SetKeyTime", "Set Key Time"),
			bUseFrames ? LOCTEXT("SetKeyFrameTooltip", "Set key frame") : LOCTEXT("SetKeyTimeTooltip", "Set key time"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SetKeyTime, bUseFrames),
				FCanExecuteAction::CreateSP(SequencerPtr, &FSequencer::CanSetKeyTime))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SnapToFrame", "Snap to Frame"),
			LOCTEXT("SnapToFrameToolTip", "Snap selected keys to frame"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(SequencerPtr, &FSequencer::SnapToFrame),
				FCanExecuteAction::CreateSP(SequencerPtr, &FSequencer::CanSnapToFrame))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteKey", "Delete"),
			LOCTEXT("DeleteKeyToolTip", "Deletes the selected keys"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(SequencerPtr, &FSequencer::DeleteSelectedKeys))
		);
	}
	MenuBuilder.EndSection(); // SequencerKeys
}

bool FKeyContextMenu::CanAddPropertiesMenu() const
{
	for (const FSequencerSelectedKey& Key : Sequencer->GetSelection().GetSelectedKeys())
	{
		if (Key.KeyArea.IsValid() && Key.KeyHandle.IsSet())
		{
			TSharedPtr<FStructOnScope> KeyStruct = Key.KeyArea->GetKeyStruct(Key.KeyHandle.GetValue());

			if (KeyStruct.IsValid())
			{
				return true;
			}
		}
	}

	return false;
}

/** Widget that represents a details panel that refreshes on undo, and handles modification of the section on edit */
class SInlineDetailsView : public SCompoundWidget, public FEditorUndoClient, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SInlineDetailsView){}
	SLATE_END_ARGS()

	~SInlineDetailsView()
	{
		GEditor->UnregisterForUndo(this);
	}

	void Construct(const FArguments&, TSharedRef<FSequencer> InSequencer)
	{
		GEditor->RegisterForUndo(this);
		
		WeakSequencer = InSequencer;
		Initialize();
	}
	
	/** (Re)Initialize this widget's details panel */
	void Initialize()
	{
		// Reset the section and widget content
		WeakSection = nullptr;
		ChildSlot
		[
			SNullWidget::NullWidget
		];

		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (!Sequencer.IsValid())
		{
			return;
		}

		// Set up the details panel only if a single selected key with a valid key struct exists
		TSharedPtr<FStructOnScope> SelectedKeyStruct;
		FSequencerSelectedKey SelectedKey;

		for (const FSequencerSelectedKey& Key : Sequencer->GetSelection().GetSelectedKeys())
		{
			if (Key.KeyArea.IsValid() && Key.KeyHandle.IsSet())
			{
				TSharedPtr<FStructOnScope> KeyStruct = Key.KeyArea->GetKeyStruct(Key.KeyHandle.GetValue());
				if (KeyStruct.IsValid())
				{
					if (SelectedKey.IsValid())
					{
						// @todo sequencer: only one struct per structure view supported right now :/
						return;
					}

					SelectedKey = Key;
					SelectedKeyStruct = KeyStruct;
				}
			}
		}

		// If there're no selected keys, or too many, bail
		if (!SelectedKey.IsValid())
		{
			return;
		}

		// Set up the details panel
		WeakSection = SelectedKey.Section;

		FDetailsViewArgs DetailsViewArgs;
		{
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bCustomFilterAreaLocation = true;
			DetailsViewArgs.bCustomNameAreaLocation = true;
			DetailsViewArgs.bHideSelectionTip = true;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bSearchInitialKeyFocus = true;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bShowOptions = false;
			DetailsViewArgs.bShowModifiedPropertiesOption = false;
			DetailsViewArgs.bShowScrollBar = false;
			DetailsViewArgs.NotifyHook = this;
		}

		FStructureDetailsViewArgs StructureViewArgs;
		{
			StructureViewArgs.bShowObjects = false;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = false;
		}

		TSharedRef<IStructureDetailsView> StructureDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
			.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr, LOCTEXT("MessageData", "Message Data"));

		// register details customizations for this instance
		StructureDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyLayout(FIntegralKey::StaticStruct(), FOnGetDetailCustomizationInstance::CreateStatic(&FIntegralKeyDetailsCustomization::MakeInstance, TWeakObjectPtr<const UMovieSceneSection>(WeakSection)));

		StructureDetailsView->SetStructureData(SelectedKeyStruct);
		StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SInlineDetailsView::OnFinishedChangingProperties, SelectedKeyStruct);

		ChildSlot
		[
			StructureDetailsView->GetWidget().ToSharedRef()
		];
	}

	void OnFinishedChangingProperties(const FPropertyChangedEvent& ChangeEvent, TSharedPtr<FStructOnScope> KeyStruct)
	{
		if (KeyStruct->GetStruct()->IsChildOf(FMovieSceneKeyStruct::StaticStruct()))
		{
			((FMovieSceneKeyStruct*)KeyStruct->GetStructMemory())->PropagateChanges(ChangeEvent);
		}

		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
		}
	}

	virtual void NotifyPreChange( UProperty* PropertyAboutToChange )
	{
		if (UMovieSceneSection* Section = WeakSection.Get())
		{
			Section->Modify();
		}
	}

	virtual void PostUndo(bool bSuccess) override
	{
		Initialize();
	}

	virtual void PostRedo(bool bSuccess) override
	{
		Initialize();
	}

private:
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	TWeakPtr<FSequencer> WeakSequencer;
};

void FKeyContextMenu::AddPropertiesMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddWidget(SNew(SInlineDetailsView, Sequencer), FText::GetEmpty(), true);
}


void FSectionContextMenu::BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& InSequencer, float InMouseDownTime)
{
	TSharedRef<FSectionContextMenu> Menu = MakeShareable(new FSectionContextMenu(InSequencer, InMouseDownTime));
	Menu->PopulateMenu(MenuBuilder);
}


void FSectionContextMenu::PopulateMenu(FMenuBuilder& MenuBuilder)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FSectionContextMenu> Shared = AsShared();

	MenuBuilder.AddSubMenu(
		LOCTEXT("SectionProperties", "Properties"),
		LOCTEXT("SectionPropertiesTooltip", "Modify the section properties"),
		FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddPropertiesMenu(SubMenuBuilder); })
	);

	MenuBuilder.BeginSection("SequencerKeyEdit", LOCTEXT("EditMenu", "Edit"));
	{
		TSharedPtr<FPasteFromHistoryContextMenu> PasteFromHistoryMenu;
		TSharedPtr<FPasteContextMenu> PasteMenu;

		if (Sequencer->GetClipboardStack().Num() != 0)
		{
			FPasteContextMenuArgs PasteArgs = FPasteContextMenuArgs::PasteAt(MouseDownTime);
			PasteMenu = FPasteContextMenu::CreateMenu(*Sequencer, PasteArgs);
			PasteFromHistoryMenu = FPasteFromHistoryContextMenu::CreateMenu(*Sequencer, PasteArgs);
		}

		MenuBuilder.AddSubMenu(
			LOCTEXT("Paste", "Paste"),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ if (PasteMenu.IsValid()) { PasteMenu->PopulateMenu(SubMenuBuilder); } }),
			FUIAction (
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([=]{ return PasteMenu.IsValid() && PasteMenu->IsValidPaste(); })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddSubMenu(
			LOCTEXT("PasteFromHistory", "Paste From History"),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ if (PasteFromHistoryMenu.IsValid()) { PasteFromHistoryMenu->PopulateMenu(SubMenuBuilder); } }),
			FUIAction (
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([=]{ return PasteFromHistoryMenu.IsValid(); })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection(); // SequencerKeyEdit

	MenuBuilder.BeginSection("SequencerSections", LOCTEXT("SectionsMenu", "Sections"));
	{
		if (CanPrimeForRecording())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("PrimeForRecording", "Primed For Recording"),
				LOCTEXT("PrimeForRecordingTooltip", "Prime this track for recording a new sequence."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=]{ return Shared->TogglePrimeForRecording(); }),
					FCanExecuteAction(),
					FGetActionCheckState::CreateLambda([=]{ return Shared->IsPrimedForRecording() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}

		if (CanSelectAllKeys())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SelectAllKeys", "Select All Keys"),
				LOCTEXT("SelectAllKeysTooltip", "Select all keys in section"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([=]{ return Shared->SelectAllKeys(); }))
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CopyAllKeys", "Copy All Keys"),
				LOCTEXT("CopyAllKeysTooltip", "Copy all keys in section"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([=] { return Shared->CopyAllKeys(); }))
			);
		}

		MenuBuilder.AddSubMenu(
			LOCTEXT("EditSection", "Edit"),
			LOCTEXT("EditSectionTooltip", "Edit section"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& InMenuBuilder){ Shared->AddEditMenu(InMenuBuilder); }));

		if (CanSetExtrapolationMode())
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("SetPreInfinityExtrap", "Pre-Infinity"),
				LOCTEXT("SetPreInfinityExtrapTooltip", "Set pre-infinity extrapolation"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddExtrapolationMenu(SubMenuBuilder, true); }));

			MenuBuilder.AddSubMenu(
				LOCTEXT("SetPostInfinityExtrap", "Post-Infinity"),
				LOCTEXT("SetPostInfinityExtrapTooltip", "Set post-infinity extrapolation"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddExtrapolationMenu(SubMenuBuilder, false); }));
		}

		MenuBuilder.AddSubMenu(
			LOCTEXT("OrderSection", "Order"),
			LOCTEXT("OrderSectionTooltip", "Order section"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddOrderMenu(SubMenuBuilder); }));			

		if (GetSupportedBlendTypes().Num() > 1)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("BlendTypeSection", "Blend Type"),
				LOCTEXT("BlendTypeSectionTooltip", "Change the way in which this section blends with other sections of the same type"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddBlendTypeMenu(SubMenuBuilder); }));
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleSectionActive", "Active"),
			LOCTEXT("ToggleSectionActiveTooltip", "Toggle section active/inactive"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ Shared->ToggleSectionActive(); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([=]{ return Shared->IsSectionActive(); })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "ToggleSectionLocked", "Locked"),
			NSLOCTEXT("Sequencer", "ToggleSectionLockedTooltip", "Toggle section locked/unlocked"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ Shared->ToggleSectionLocked(); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([=]{ return Shared->IsSectionLocked(); })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		// @todo Sequencer this should delete all selected sections
		// delete/selection needs to be rethought in general
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteSection", "Delete"),
			LOCTEXT("DeleteSectionToolTip", "Deletes this section"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([=]{ return Shared->DeleteSection(); }))
		);
	}
	MenuBuilder.EndSection(); // SequencerSections
}


void FSectionContextMenu::AddEditMenu(FMenuBuilder& MenuBuilder)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FSectionContextMenu> Shared = AsShared();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("TrimSectionLeft", "Trim Left"),
		LOCTEXT("TrimSectionLeftTooltip", "Trim section at current MouseDownTime to the left"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->TrimSection(true); }),
			FCanExecuteAction::CreateLambda([=]{ return Shared->IsTrimmable(); }))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("TrimSectionRight", "Trim Right"),
		LOCTEXT("TrimSectionRightTooltip", "Trim section at current MouseDownTime to the right"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->TrimSection(false); }),
			FCanExecuteAction::CreateLambda([=]{ return Shared->IsTrimmable(); }))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SplitSection", "Split"),
		LOCTEXT("SplitSectionTooltip", "Split section at current MouseDownTime"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->SplitSection(); }),
			FCanExecuteAction::CreateLambda([=]{ return Shared->IsTrimmable(); }))
	);
		
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReduceKeysSection", "Reduce Keys"),
		LOCTEXT("ReduceKeysTooltip", "Reduce keys in this section"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->ReduceKeys(); }),
			FCanExecuteAction::CreateLambda([=]{ return Shared->CanReduceKeys(); }))
	);
}


void FSectionContextMenu::AddExtrapolationMenu(FMenuBuilder& MenuBuilder, bool bPreInfinity)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FSectionContextMenu> Shared = AsShared();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SetExtrapCycle", "Cycle"),
		LOCTEXT("SetExtrapCycleTooltip", "Set extrapolation cycle"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->SetExtrapolationMode(RCCE_Cycle, bPreInfinity); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Shared->IsExtrapolationModeSelected(RCCE_Cycle, bPreInfinity); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SetExtrapCycleWithOffset", "Cycle with Offset"),
		LOCTEXT("SetExtrapCycleWithOffsetTooltip", "Set extrapolation cycle with offset"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->SetExtrapolationMode(RCCE_CycleWithOffset, bPreInfinity); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Shared->IsExtrapolationModeSelected(RCCE_CycleWithOffset, bPreInfinity); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SetExtrapOscillate", "Oscillate"),
		LOCTEXT("SetExtrapOscillateTooltip", "Set extrapolation oscillate"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->SetExtrapolationMode(RCCE_Oscillate, bPreInfinity); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Shared->IsExtrapolationModeSelected(RCCE_Oscillate, bPreInfinity); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SetExtrapLinear", "Linear"),
		LOCTEXT("SetExtrapLinearTooltip", "Set extrapolation linear"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->SetExtrapolationMode(RCCE_Linear, bPreInfinity); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Shared->IsExtrapolationModeSelected(RCCE_Linear, bPreInfinity); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SetExtrapConstant", "Constant"),
		LOCTEXT("SetExtrapConstantTooltip", "Set extrapolation constant"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Shared->SetExtrapolationMode(RCCE_Constant, bPreInfinity); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Shared->IsExtrapolationModeSelected(RCCE_Constant, bPreInfinity); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}

FMovieSceneBlendTypeField FSectionContextMenu::GetSupportedBlendTypes() const
{
	FMovieSceneBlendTypeField BlendTypes = FMovieSceneBlendTypeField::All();

	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (Section)
		{
			// Remove unsupported blend types
			BlendTypes.Remove(Section->GetSupportedBlendTypes().Invert());
		}
	}

	return BlendTypes;
}


/** A widget which wraps the section details view which is an FNotifyHook which is used to forward
	changes to the section to sequencer. */
class SSectionDetailsNotifyHookWrapper : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SSectionDetailsNotifyHookWrapper) {}
	SLATE_END_ARGS();

	void Construct(FArguments InArgs) { }

	void SetDetailsAndSequencer(TSharedRef<SWidget> InDetailsPanel, TSharedRef<ISequencer> InSequencer)
	{
		ChildSlot
		[
			InDetailsPanel
		];
		Sequencer = InSequencer;
	}

	//~ FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}

private:
	TSharedPtr<ISequencer> Sequencer;
};


void FSectionContextMenu::AddPropertiesMenu(FMenuBuilder& MenuBuilder)
{
	TSharedRef<SSectionDetailsNotifyHookWrapper> DetailsNotifyWrapper = SNew(SSectionDetailsNotifyHookWrapper);
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.NotifyHook = &DetailsNotifyWrapper.Get();
	}

	TArray<TWeakObjectPtr<UObject>> Sections;
	{
		for (auto Section : Sequencer->GetSelection().GetSelectedSections())
		{
			if (Section.IsValid())
			{
				Sections.Add(Section);
			}
		}
	}

	TSharedRef<IDetailsView> DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	{
		DetailsView->SetObjects(Sections);
	}

	DetailsNotifyWrapper->SetDetailsAndSequencer(DetailsView, Sequencer);
	MenuBuilder.AddWidget(DetailsNotifyWrapper, FText::GetEmpty(), true);
}


void FSectionContextMenu::AddOrderMenu(FMenuBuilder& MenuBuilder)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FSectionContextMenu> Shared = AsShared();

	MenuBuilder.AddMenuEntry(LOCTEXT("BringToFront", "Bring To Front"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([=]{ return Shared->BringToFront(); })));

	MenuBuilder.AddMenuEntry(LOCTEXT("SendToBack", "Send To Back"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([=]{ return Shared->SendToBack(); })));

	MenuBuilder.AddMenuEntry(LOCTEXT("BringForward", "Bring Forward"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([=]{ return Shared->BringForward(); })));

	MenuBuilder.AddMenuEntry(LOCTEXT("SendBackward", "Send Backward"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([=]{ return Shared->SendBackward(); })));
}

void FSectionContextMenu::AddBlendTypeMenu(FMenuBuilder& MenuBuilder)
{
	TArray<TWeakObjectPtr<UMovieSceneSection>> Sections;

	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (Section)
		{
			Sections.Add(Section);
		}
	}

	FSequencerUtilities::PopulateMenu_SetBlendType(MenuBuilder, Sections);
}

void FSectionContextMenu::SelectAllKeys()
{
	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (!Section)
		{
			continue;
		}

		FSectionLayout Layout(*Handle.TrackNode, Handle.SectionIndex);
		for (const FSectionLayoutElement& Element : Layout.GetElements())
		{
			TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
			if (KeyArea.IsValid())
			{
				for (const FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles())
				{
					FSequencerSelectedKey SelectKey(*Section, KeyArea, KeyHandle);
					Sequencer->GetSelection().AddToSelection(SelectKey);
				}
			}
		}
	}
}

void FSectionContextMenu::CopyAllKeys()
{
	SelectAllKeys();
	Sequencer->CopySelectedKeys();
}

void FSectionContextMenu::TogglePrimeForRecording() const
{
	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	if(SelectedSections.Num() > 0)
	{
		const FSectionHandle& Handle = SelectedSections[0];
		UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Handle.GetSectionObject());
		if (SubSection)
		{
			SubSection->SetAsRecording(SubSection != UMovieSceneSubSection::GetRecordingSection());
		}	
	}
}


bool FSectionContextMenu::IsPrimedForRecording() const
{
	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	if(SelectedSections.Num() > 0)
	{
		const FSectionHandle& Handle = SelectedSections[0];
		UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Handle.GetSectionObject());
		if (SubSection)
		{
			return SubSection == UMovieSceneSubSection::GetRecordingSection();
		}	
	}

	return false;
}

bool FSectionContextMenu::CanPrimeForRecording() const
{
	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	if(SelectedSections.Num() > 0)
	{
		const FSectionHandle& Handle = SelectedSections[0];
		UMovieSceneSubSection* SubSection = ExactCast<UMovieSceneSubSection>(Handle.GetSectionObject());
		if (SubSection)
		{
			return true;
		}	
	}

	return false;
}


bool FSectionContextMenu::CanSelectAllKeys() const
{
	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (!Section)
		{
			continue;
		}

		FSectionLayout Layout(*Handle.TrackNode, Handle.SectionIndex);
		for (const FSectionLayoutElement& Element : Layout.GetElements())
		{
			TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
			if (KeyArea.IsValid() && Element.GetKeyArea()->GetUnsortedKeyHandles().Num() > 0)
			{
				return true;
			}
		}
	}

	return false;
}

bool FSectionContextMenu::CanSetExtrapolationMode() const
{
	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		FSectionLayout Layout(*Handle.TrackNode, Handle.SectionIndex);
		for (const FSectionLayoutElement& Element : Layout.GetElements())
		{
			TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
			if (KeyArea.IsValid() && KeyArea->CanSetExtrapolationMode())
			{
				return true;
			}
		}
	}
	return false;
}

void FSectionContextMenu::TrimSection(bool bTrimLeft)
{
	FScopedTransaction TrimSectionTransaction(LOCTEXT("TrimSection_Transaction", "Trim Section"));

	MovieSceneToolHelpers::TrimSection(Sequencer->GetSelection().GetSelectedSections(), Sequencer->GetLocalTime(), bTrimLeft);
	Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}


void FSectionContextMenu::SplitSection()
{
	FScopedTransaction SplitSectionTransaction(LOCTEXT("SplitSection_Transaction", "Split Section"));

	MovieSceneToolHelpers::SplitSection(Sequencer->GetSelection().GetSelectedSections(), Sequencer->GetLocalTime());
	Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


void FSectionContextMenu::ReduceKeys()
{
	FScopedTransaction ReduceKeysTransaction(LOCTEXT("ReduceKeys_Transaction", "Reduce Keys"));

	TSet<TSharedPtr<IKeyArea> > KeyAreas;
	for (auto DisplayNode : Sequencer->GetSelection().GetSelectedOutlinerNodes())
	{
		SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
	}

	if (KeyAreas.Num() == 0)
	{
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Sequencer->GetSelection().GetNodesWithSelectedKeysOrSections();
		for (auto DisplayNode : SelectedNodes)
		{
			SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
		}
	}

	for (auto KeyArea : KeyAreas)
	{
		if (KeyArea.IsValid())
		{
			if (KeyArea->GetRichCurve() && KeyArea->GetOwningSection())
			{
				KeyArea->GetOwningSection()->Modify();
				KeyArea->GetRichCurve()->RemoveRedundantKeys(KINDA_SMALL_NUMBER);
			}
		}
	}

	Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

bool FSectionContextMenu::IsTrimmable() const
{
	for (auto Section : Sequencer->GetSelection().GetSelectedSections())
	{
		if (Section.IsValid() && Section->IsTimeWithinSection(Sequencer->GetLocalTime()))
		{
			return true;
		}
	}
	return false;
}

bool FSectionContextMenu::CanReduceKeys() const
{
	TSet<TSharedPtr<IKeyArea> > KeyAreas;
	for (auto DisplayNode : Sequencer->GetSelection().GetSelectedOutlinerNodes())
	{
		SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
	}

	if (KeyAreas.Num() == 0)
	{
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Sequencer->GetSelection().GetNodesWithSelectedKeysOrSections();
		for (auto DisplayNode : SelectedNodes)
		{
			SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
		}
	}

	return KeyAreas.Num() != 0;
}


void FSectionContextMenu::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	FScopedTransaction SetExtrapolationModeTransaction(LOCTEXT("SetExtrapolationMode_Transaction", "Set Extrapolation Mode"));

	bool bAnythingChanged = false;

	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (!Section)
		{
			continue;
		}

		if (Section->TryModify())
		{
			FSectionLayout Layout(*Handle.TrackNode, Handle.SectionIndex);
			for (const FSectionLayoutElement& Element : Layout.GetElements())
			{
				TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
				if (KeyArea.IsValid())
				{
					bAnythingChanged = true;
					KeyArea->SetExtrapolationMode(ExtrapMode, bPreInfinity);
				}
			}
		}
	}

	if (bAnythingChanged)
	{
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
	else
	{
		SetExtrapolationModeTransaction.Cancel();
	}
}


bool FSectionContextMenu::IsExtrapolationModeSelected(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) const
{
	// @todo Sequencer should operate on selected sections
	bool bAllSelected = false;

	TArray<FSectionHandle> SelectedSections = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget())->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	for (const FSectionHandle& Handle : SelectedSections)
	{
		FSectionLayout Layout(*Handle.TrackNode, Handle.SectionIndex);
		for (const FSectionLayoutElement& Element : Layout.GetElements())
		{
			TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
			if (KeyArea.IsValid())
			{
				bAllSelected = true;
				if (KeyArea->GetExtrapolationMode(bPreInfinity) != ExtrapMode)
				{
					bAllSelected = false;
					break;
				}
			}
		}
	}

	return bAllSelected;
}


void FSectionContextMenu::ToggleSectionActive()
{
	FScopedTransaction ToggleSectionActiveTransaction( LOCTEXT("ToggleSectionActive_Transaction", "Toggle Section Active") );
	bool bIsActive = !IsSectionActive();
	bool bAnythingChanged = false;

	for (auto Section : Sequencer->GetSelection().GetSelectedSections())
	{
		if (Section.IsValid())
		{
			bAnythingChanged = true;
			Section->Modify();
			Section->SetIsActive(bIsActive);
		}
	}

	if (bAnythingChanged)
	{
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
	else
	{
		ToggleSectionActiveTransaction.Cancel();
	}
}


bool FSectionContextMenu::IsSectionActive() const
{
	// Active only if all are active
	for (auto Section : Sequencer->GetSelection().GetSelectedSections())
	{
		if (Section.IsValid() && !Section->IsActive())
		{
			return false;
		}
	}

	return true;
}


void FSectionContextMenu::ToggleSectionLocked()
{
	FScopedTransaction ToggleSectionLockedTransaction( NSLOCTEXT("Sequencer", "ToggleSectionLocked_Transaction", "Toggle Section Locked") );
	bool bIsLocked = !IsSectionLocked();
	bool bAnythingChanged = false;

	for (auto Section : Sequencer->GetSelection().GetSelectedSections())
	{
		if (Section.IsValid())
		{
			bAnythingChanged = true;
			Section->Modify();
			Section->SetIsLocked(bIsLocked);
		}
	}

	if (bAnythingChanged)
	{
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
	else
	{
		ToggleSectionLockedTransaction.Cancel();
	}
}


bool FSectionContextMenu::IsSectionLocked() const
{
	// Locked only if all are locked
	for (auto Section : Sequencer->GetSelection().GetSelectedSections())
	{
		if (Section.IsValid() && !Section->IsLocked())
		{
			return false;
		}
	}

	return true;
}


void FSectionContextMenu::DeleteSection()
{
	Sequencer->DeleteSections(Sequencer->GetSelection().GetSelectedSections());
}


/** Information pertaining to a specific row in a track, required for z-ordering operations */
struct FTrackSectionRow
{
	/** The minimum z-order value for all the sections in this row */
	int32 MinOrderValue;

	/** The maximum z-order value for all the sections in this row */
	int32 MaxOrderValue;

	/** All the sections contained in this row */
	TArray<UMovieSceneSection*> Sections;

	/** A set of sections that are to be operated on */
	TSet<UMovieSceneSection*> SectionToReOrder;

	void AddSection(UMovieSceneSection* InSection)
	{
		Sections.Add(InSection);
		MinOrderValue = FMath::Min(MinOrderValue, InSection->GetOverlapPriority());
		MaxOrderValue = FMath::Max(MaxOrderValue, InSection->GetOverlapPriority());
	}
};


/** Generate the data required for re-ordering rows based on the current sequencer selection */
/** @note: Produces a map of track -> rows, keyed on row index. Only returns rows that contain selected sections */
TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>> GenerateTrackRowsFromSelection(FSequencer& Sequencer)
{
	TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>> TrackRows;

	for (const TWeakObjectPtr<UMovieSceneSection>& SectionPtr : Sequencer.GetSelection().GetSelectedSections())
	{
		UMovieSceneSection* Section = SectionPtr.Get();
		if (!Section)
		{
			continue;
		}

		UMovieSceneTrack* Track = Section->GetTypedOuter<UMovieSceneTrack>();
		if (!Track)
		{
			continue;
		}

		FTrackSectionRow& Row = TrackRows.FindOrAdd(Track).FindOrAdd(Section->GetRowIndex());
		Row.SectionToReOrder.Add(Section);
	}

	// Now ensure all rows that we're operating on are fully populated
	for (auto& Pair : TrackRows)
	{
		UMovieSceneTrack* Track = Pair.Key;
		for (auto& RowPair : Pair.Value)
		{
			const int32 RowIndex = RowPair.Key;
			for (UMovieSceneSection* Section : Track->GetAllSections())
			{
				if (Section->GetRowIndex() == RowIndex)
				{
					RowPair.Value.AddSection(Section);
				}
			}
		}
	}

	return TrackRows;
}


/** Modify all the sections contained within the specified data structure */
void ModifySections(TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>>& TrackRows)
{
	for (auto& Pair : TrackRows)
	{
		UMovieSceneTrack* Track = Pair.Key;
		for (auto& RowPair : Pair.Value)
		{
			for (UMovieSceneSection* Section : RowPair.Value.Sections)
			{
				Section->Modify();
			}
		}
	}
}


void FSectionContextMenu::BringToFront()
{
	TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>> TrackRows = GenerateTrackRowsFromSelection(*Sequencer);
	if (TrackRows.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("BringToFrontTransaction", "Bring to Front"));
	ModifySections(TrackRows);

	for (auto& Pair : TrackRows)
	{
		UMovieSceneTrack* Track = Pair.Key;
		TMap<int32, FTrackSectionRow>& Rows = Pair.Value;

		for (auto& RowPair : Rows)
		{
			FTrackSectionRow& Row = RowPair.Value;

			Row.Sections.StableSort([&](UMovieSceneSection& A, UMovieSceneSection& B){
				bool bIsActiveA = Row.SectionToReOrder.Contains(&A);
				bool bIsActiveB = Row.SectionToReOrder.Contains(&B);

				// Sort secondarily on overlap priority
				if (bIsActiveA == bIsActiveB)
				{
					return A.GetOverlapPriority() < B.GetOverlapPriority();
				}
				// Sort and primarily on whether we're sending to the back or not (bIsActive)
				else
				{
					return !bIsActiveA;
				}
			});

			int32 CurrentPriority = Row.MinOrderValue;
			for (UMovieSceneSection* Section : Row.Sections)
			{
				Section->SetOverlapPriority(CurrentPriority++);
			}
		}
	}

	Sequencer->SetLocalTimeDirectly(Sequencer->GetLocalTime());
}


void FSectionContextMenu::SendToBack()
{
	TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>> TrackRows = GenerateTrackRowsFromSelection(*Sequencer);
	if (TrackRows.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SendToBackTransaction", "Send to Back"));
	ModifySections(TrackRows);

	for (auto& Pair : TrackRows)
	{
		UMovieSceneTrack* Track = Pair.Key;
		TMap<int32, FTrackSectionRow>& Rows = Pair.Value;

		for (auto& RowPair : Rows)
		{
			FTrackSectionRow& Row = RowPair.Value;

			Row.Sections.StableSort([&](UMovieSceneSection& A, UMovieSceneSection& B){
				bool bIsActiveA = Row.SectionToReOrder.Contains(&A);
				bool bIsActiveB = Row.SectionToReOrder.Contains(&B);

				// Sort secondarily on overlap priority
				if (bIsActiveA == bIsActiveB)
				{
					return A.GetOverlapPriority() < B.GetOverlapPriority();
				}
				// Sort and primarily on whether we're bringing to the front or not (bIsActive)
				else
				{
					return bIsActiveA;
				}
			});

			int32 CurrentPriority = Row.MinOrderValue;
			for (UMovieSceneSection* Section : Row.Sections)
			{
				Section->SetOverlapPriority(CurrentPriority++);
			}
		}
	}

	Sequencer->SetLocalTimeDirectly(Sequencer->GetLocalTime());
}


void FSectionContextMenu::BringForward()
{
	TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>> TrackRows = GenerateTrackRowsFromSelection(*Sequencer);
	if (TrackRows.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("BringForwardTransaction", "Bring Forward"));
	ModifySections(TrackRows);

	for (auto& Pair : TrackRows)
	{
		UMovieSceneTrack* Track = Pair.Key;
		TMap<int32, FTrackSectionRow>& Rows = Pair.Value;

		for (auto& RowPair : Rows)
		{
			FTrackSectionRow& Row = RowPair.Value;

			Row.Sections.Sort([&](UMovieSceneSection& A, UMovieSceneSection& B){
				return A.GetOverlapPriority() < B.GetOverlapPriority();
			});

			for (int32 SectionIndex = Row.Sections.Num() - 1; SectionIndex > 0; --SectionIndex)
			{
				UMovieSceneSection* ThisSection = Row.Sections[SectionIndex];
				if (Row.SectionToReOrder.Contains(ThisSection))
				{
					UMovieSceneSection* OtherSection = Row.Sections[SectionIndex + 1];

					Row.Sections.Swap(SectionIndex, SectionIndex+1);

					const int32 SwappedPriority = OtherSection->GetOverlapPriority();
					OtherSection->SetOverlapPriority(ThisSection->GetOverlapPriority());
					ThisSection->SetOverlapPriority(SwappedPriority);
				}
			}
		}
	}

	Sequencer->SetLocalTimeDirectly(Sequencer->GetLocalTime());
}


void FSectionContextMenu::SendBackward()
{
	TMap<UMovieSceneTrack*, TMap<int32, FTrackSectionRow>> TrackRows = GenerateTrackRowsFromSelection(*Sequencer);
	if (TrackRows.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SendBackwardTransaction", "Send Backward"));
	ModifySections(TrackRows);

	for (auto& Pair : TrackRows)
	{
		UMovieSceneTrack* Track = Pair.Key;
		TMap<int32, FTrackSectionRow>& Rows = Pair.Value;

		for (auto& RowPair : Rows)
		{
			FTrackSectionRow& Row = RowPair.Value;

			Row.Sections.Sort([&](UMovieSceneSection& A, UMovieSceneSection& B){
				return A.GetOverlapPriority() < B.GetOverlapPriority();
			});

			for (int32 SectionIndex = 1; SectionIndex < Row.Sections.Num(); ++SectionIndex)
			{
				UMovieSceneSection* ThisSection = Row.Sections[SectionIndex];
				if (Row.SectionToReOrder.Contains(ThisSection))
				{
					UMovieSceneSection* OtherSection = Row.Sections[SectionIndex - 1];

					Row.Sections.Swap(SectionIndex, SectionIndex - 1);

					const int32 SwappedPriority = OtherSection->GetOverlapPriority();
					OtherSection->SetOverlapPriority(ThisSection->GetOverlapPriority());
					ThisSection->SetOverlapPriority(SwappedPriority);
				}
			}
		}
	}

	Sequencer->SetLocalTimeDirectly(Sequencer->GetLocalTime());
}


bool FPasteContextMenu::BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& InSequencer, const FPasteContextMenuArgs& Args)
{
	TSharedRef<FPasteContextMenu> Menu = MakeShareable(new FPasteContextMenu(InSequencer, Args));
	Menu->Setup();
	if (!Menu->IsValidPaste())
	{
		return false;
	}

	Menu->PopulateMenu(MenuBuilder);
	return true;
}


TSharedRef<FPasteContextMenu> FPasteContextMenu::CreateMenu(FSequencer& InSequencer, const FPasteContextMenuArgs& Args)
{
	TSharedRef<FPasteContextMenu> Menu = MakeShareable(new FPasteContextMenu(InSequencer, Args));
	Menu->Setup();
	return Menu;
}


TArray<TSharedRef<FSequencerSectionKeyAreaNode>> KeyAreaNodesBuffer;

void FPasteContextMenu::GatherPasteDestinationsForNode(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, const FName& CurrentScope, TMap<FName, FSequencerClipboardReconciler>& Map)
{
	KeyAreaNodesBuffer.Reset();
	if (InNode.GetType() == ESequencerNode::KeyArea)
	{
		KeyAreaNodesBuffer.Add(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(InNode.AsShared()));
	}
	else
	{
		InNode.GetChildKeyAreaNodesRecursively(KeyAreaNodesBuffer);
	}

	if (!KeyAreaNodesBuffer.Num())
	{
		return;
	}

	FName ThisScope;
	{
		FString ThisScopeString;
		if (!CurrentScope.IsNone())
		{
			ThisScopeString.Append(CurrentScope.ToString());
			ThisScopeString.AppendChar('.');
		}
		ThisScopeString.Append(InNode.GetDisplayName().ToString());
		ThisScope = *ThisScopeString;
	}

	FSequencerClipboardReconciler* Reconciler = Map.Find(ThisScope);
	if (!Reconciler)
	{
		Reconciler = &Map.Add(ThisScope, FSequencerClipboardReconciler(Args.Clipboard.ToSharedRef()));
	}

	FSequencerClipboardPasteGroup Group = Reconciler->AddDestinationGroup();
	for (const TSharedRef<FSequencerSectionKeyAreaNode>& KeyAreaNode : KeyAreaNodesBuffer)
	{
		TSharedPtr<IKeyArea> KeyArea = KeyAreaNode->GetKeyArea(InSection);
		if (KeyArea.IsValid())
		{
			Group.Add(*KeyArea.Get());
		}
	}

	// Add children
	for (const TSharedPtr<FSequencerDisplayNode>& Child : InNode.GetChildNodes())
	{
		GatherPasteDestinationsForNode(*Child, InSection, ThisScope, Map);
	}
}


void GetFullNodePath(FSequencerDisplayNode& InNode, FString& Path)
{
	TSharedPtr<FSequencerDisplayNode> Parent = InNode.GetParent();
	if (Parent.IsValid())
	{
		GetFullNodePath(*Parent, Path);
	}

	if (!Path.IsEmpty())
	{
		Path.AppendChar('.');
	}

	Path.Append(InNode.GetDisplayName().ToString());
}


TSharedPtr<FSequencerTrackNode> GetTrackFromNode(FSequencerDisplayNode& InNode, FString& Scope)
{
	if (InNode.GetType() == ESequencerNode::Track)
	{
		return StaticCastSharedRef<FSequencerTrackNode>(InNode.AsShared());
	}
	else if (InNode.GetType() == ESequencerNode::Object)
	{
		return nullptr;
	}

	TSharedPtr<FSequencerDisplayNode> Parent = InNode.GetParent();
	if (Parent.IsValid())
	{
		TSharedPtr<FSequencerTrackNode> Track = GetTrackFromNode(*Parent, Scope);
		if (Track.IsValid())
		{
			FString ThisScope = InNode.GetDisplayName().ToString();
			if (!Scope.IsEmpty())
			{
				ThisScope.AppendChar('.');
				ThisScope.Append(Scope);
				Scope = MoveTemp(ThisScope);
			}
			return Track;
		}
	}

	return nullptr;
}


void FPasteContextMenu::Setup()
{
	if (!Args.Clipboard.IsValid())
	{
		if (Sequencer->GetClipboardStack().Num() != 0)
		{
			Args.Clipboard = Sequencer->GetClipboardStack().Last();
		}
		else
		{
			return;
		}
	}

	// Gather a list of sections we want to paste into
	TArray<FSectionHandle> SectionHandles;

	if (Args.DestinationNodes.Num())
	{
		// Paste into only these nodes
		for (const TSharedRef<FSequencerDisplayNode>& Node : Args.DestinationNodes)
		{
			FString Scope;
			TSharedPtr<FSequencerTrackNode> TrackNode = GetTrackFromNode(*Node, Scope);
			if (!TrackNode.IsValid())
			{
				continue;
			}

			const TArray<UMovieSceneSection*>& Sections = TrackNode->GetTrack()->GetAllSections();
			UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime(Sections, Args.PasteAtTime);
			int32 SectionIndex = INDEX_NONE;
			if (Section)
			{
				SectionIndex = Sections.IndexOfByKey(Section);
			}

			if (SectionIndex != INDEX_NONE)
			{
				SectionHandles.Add(FSectionHandle(TrackNode, SectionIndex));
			}
		}
	}
	else
	{
		// Use the selected sections
		TSharedRef<SSequencer> SequencerWidget = StaticCastSharedRef<SSequencer>(Sequencer->GetSequencerWidget());
		SectionHandles = SequencerWidget->GetSectionHandles(Sequencer->GetSelection().GetSelectedSections());
	}

	TMap<FName, TArray<FSectionHandle>> SectionsByType;
	for (const FSectionHandle& Section : SectionHandles)
	{
		UMovieSceneTrack* Track = Section.TrackNode->GetTrack();
		if (Track)
		{
			SectionsByType.FindOrAdd(Track->GetClass()->GetFName()).Add(Section);
		}
	}

	for (auto& Pair : SectionsByType)
	{
		FPasteDestination& Destination = PasteDestinations[PasteDestinations.AddDefaulted()];
		if (Pair.Value.Num() == 1)
		{
			FString Path;
			GetFullNodePath(*Pair.Value[0].TrackNode, Path);
			Destination.Name = FText::FromString(Path);
		}
		else
		{
			Destination.Name = FText::Format(LOCTEXT("PasteMenuHeaderFormat", "{0} ({1} tracks)"), FText::FromName(Pair.Key), FText::AsNumber(Pair.Value.Num()));
		}

		for (const FSectionHandle& Section : Pair.Value)
		{
			GatherPasteDestinationsForNode(*Section.TrackNode, Section.GetSectionObject(), NAME_None, Destination.Reconcilers);
		}

		// Reconcile and remove invalid pastes
		for (auto It = Destination.Reconcilers.CreateIterator(); It; ++It)
		{
			if (!It.Value().Reconcile())
			{
				It.RemoveCurrent();
			}
		}
		if (!Destination.Reconcilers.Num())
		{
			PasteDestinations.RemoveAt(PasteDestinations.Num() - 1, 1, false);
		}
	}
}


bool FPasteContextMenu::IsValidPaste() const
{
	return Args.Clipboard.IsValid() && PasteDestinations.Num() != 0;
}


void FPasteContextMenu::PopulateMenu(FMenuBuilder& MenuBuilder)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FPasteContextMenu> Shared = AsShared();

	bool bElevateMenu = PasteDestinations.Num() == 1;
	for (int32 Index = 0; Index < PasteDestinations.Num(); ++Index)
	{
		if (bElevateMenu)
		{
			MenuBuilder.BeginSection("PasteInto", FText::Format(LOCTEXT("PasteIntoTitle", "Paste Into {0}"), PasteDestinations[Index].Name));
			AddPasteMenuForTrackType(MenuBuilder, Index);
			MenuBuilder.EndSection();
			break;
		}

		MenuBuilder.AddSubMenu(
			PasteDestinations[Index].Name,
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->AddPasteMenuForTrackType(SubMenuBuilder, Index); })
		);
	}
}


void FPasteContextMenu::AddPasteMenuForTrackType(FMenuBuilder& MenuBuilder, int32 DestinationIndex)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FPasteContextMenu> Shared = AsShared();

	for (auto& Pair : PasteDestinations[DestinationIndex].Reconcilers)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromName(Pair.Key),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=](){ Shared->PasteInto(DestinationIndex, Pair.Key); })
			)
		);
	}
}


bool FPasteContextMenu::AutoPaste()
{
	if (PasteDestinations.Num() == 1)
	{
		for (auto& Pair : PasteDestinations[0].Reconcilers)
		{
			if (Pair.Value.CanAutoPaste())
			{
				PasteInto(0, Pair.Key);
				return true;
			}
		}
	}

	return false;
}


void FPasteContextMenu::PasteInto(int32 DestinationIndex, FName KeyAreaName)
{
	FSequencerClipboardReconciler& Reconciler = PasteDestinations[DestinationIndex].Reconcilers[KeyAreaName];

	TSet<FSequencerSelectedKey> NewSelection;

	FSequencerPasteEnvironment PasteEnvironment;
	PasteEnvironment.CardinalTime = Args.PasteAtTime;
	PasteEnvironment.OnKeyPasted = [&](FKeyHandle Handle, IKeyArea& KeyArea){
		NewSelection.Add(FSequencerSelectedKey(*KeyArea.GetOwningSection(), KeyArea.AsShared(), Handle));
	};

	FScopedTransaction Transaction(LOCTEXT("PasteKeysTransaction", "Paste Keys"));
	if (!Reconciler.Paste(PasteEnvironment))
	{
		Transaction.Cancel();
	}
	else
	{
		SSequencerSection::ThrobSelection();

		// @todo sequencer: selection in transactions
		FSequencerSelection& Selection = Sequencer->GetSelection();
		Selection.SuspendBroadcast();
		Selection.EmptySelectedKeys();

		for (const FSequencerSelectedKey& Key : NewSelection)
		{
			Selection.AddToSelection(Key);
		}
		Selection.ResumeBroadcast();
		Selection.GetOnKeySelectionChanged().Broadcast();

		Sequencer->OnClipboardUsed(Args.Clipboard);
	}
}


bool FPasteFromHistoryContextMenu::BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& InSequencer, const FPasteContextMenuArgs& Args)
{
	if (InSequencer.GetClipboardStack().Num() == 0)
	{
		return false;
	}

	TSharedRef<FPasteFromHistoryContextMenu> Menu = MakeShareable(new FPasteFromHistoryContextMenu(InSequencer, Args));
	Menu->PopulateMenu(MenuBuilder);
	return true;
}


TSharedPtr<FPasteFromHistoryContextMenu> FPasteFromHistoryContextMenu::CreateMenu(FSequencer& InSequencer, const FPasteContextMenuArgs& Args)
{
	if (InSequencer.GetClipboardStack().Num() == 0)
	{
		return nullptr;
	}

	return MakeShareable(new FPasteFromHistoryContextMenu(InSequencer, Args));
}


void FPasteFromHistoryContextMenu::PopulateMenu(FMenuBuilder& MenuBuilder)
{
	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FPasteFromHistoryContextMenu> Shared = AsShared();

	MenuBuilder.BeginSection("SequencerPasteHistory", LOCTEXT("PasteFromHistory", "Paste From History"));

	for (int32 Index = Sequencer->GetClipboardStack().Num() - 1; Index >= 0; --Index)
	{
		FPasteContextMenuArgs ThisPasteArgs = Args;
		ThisPasteArgs.Clipboard = Sequencer->GetClipboardStack()[Index];

		TSharedRef<FPasteContextMenu> PasteMenu = FPasteContextMenu::CreateMenu(*Sequencer, ThisPasteArgs);

		MenuBuilder.AddSubMenu(
			ThisPasteArgs.Clipboard->GetDisplayText(),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ PasteMenu->PopulateMenu(SubMenuBuilder); }),
			FUIAction (
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([=]{ return PasteMenu->IsValidPaste(); })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	MenuBuilder.EndSection();
}

void FEasingContextMenu::BuildMenu(FMenuBuilder& MenuBuilder, const TArray<FEasingAreaHandle>& InEasings, FSequencer& Sequencer, float InMouseDownTime)
{
	TSharedRef<FEasingContextMenu> EasingMenu = MakeShareable(new FEasingContextMenu(InEasings));
	EasingMenu->PopulateMenu(MenuBuilder);

	MenuBuilder.AddMenuSeparator();

	FSectionContextMenu::BuildMenu(MenuBuilder, Sequencer, InMouseDownTime);
}

void FEasingContextMenu::PopulateMenu(FMenuBuilder& MenuBuilder)
{
	FText SectionText = Easings.Num() == 1 ? LOCTEXT("EasingCurve", "Easing Curve") : FText::Format(LOCTEXT("EasingCurvesFormat", "Easing Curves ({0} curves)"), FText::AsNumber(Easings.Num()));
	MenuBuilder.BeginSection("SequencerEasingEdit", SectionText);
	{
		// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
		TSharedRef<FEasingContextMenu> Shared = AsShared();

		auto OnBeginSliderMovement = [=]
		{
			if (ensure(!Shared->ScopedTransaction.IsValid()))
			{
				Shared->ScopedTransaction.Reset(new FScopedTransaction(LOCTEXT("SetEasingTimeText", "Set Easing Length")));
			}
		};
		auto OnEndSliderMovement = [=](float NewLength)
		{
			Shared->OnUpdateLength(NewLength);
			Shared->ScopedTransaction.Reset();
		};

		TSharedRef<SWidget> SpinBox = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(FMargin(5.f,0.f))
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SNumericEntryBox<float>)
					.SpinBoxStyle(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
					.EditableTextBoxStyle(&FEditorStyle::GetWidgetStyle<FEditableTextBoxStyle>("Sequencer.HyperlinkTextBox"))
					// Don't update the value when undetermined text changes
					.OnUndeterminedValueChanged_Lambda([](FText){})
					.AllowSpin(true)
					.MinValue(0.f)
					.MaxValue(TOptional<float>())
					.MaxSliderValue(TOptional<float>())
					.MinSliderValue(0.f)
					.Delta(0.001f)
					.Value_Lambda([=]{ return Shared->GetCurrentLength(); })
					.OnValueChanged_Lambda([=](float NewLength){ Shared->OnUpdateLength(NewLength); })
					.OnValueCommitted_Lambda([=](float NewLength, ETextCommit::Type){ Shared->OnUpdateLength(NewLength); })
					.OnBeginSliderMovement_Lambda(OnBeginSliderMovement)
					.OnEndSliderMovement_Lambda(OnEndSliderMovement)
					.BorderForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
				]
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([=]{ return Shared->GetAutoEasingCheckState(); })
				.OnCheckStateChanged_Lambda([=](ECheckBoxState CheckState){ return Shared->SetAutoEasing(CheckState == ECheckBoxState::Checked); })
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AutomaticEasingText", "Auto?"))
				]
			];
		MenuBuilder.AddWidget(SpinBox, LOCTEXT("EasingAmountLabel", "Easing Length"));

		MenuBuilder.AddSubMenu(
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([=]{ return Shared->GetEasingTypeText(); })),
			LOCTEXT("EasingTypeToolTip", "Change the type of curve used for the easing"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->EasingTypeMenu(SubMenuBuilder); })
		);

		MenuBuilder.AddSubMenu(
			LOCTEXT("EasingOptions", "Options"),
			LOCTEXT("EasingOptionsToolTip", "Edit easing settings for this curve"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){ Shared->EasingOptionsMenu(SubMenuBuilder); })
		);
	}
	MenuBuilder.EndSection();
}

TOptional<float> FEasingContextMenu::GetCurrentLength() const
{
	TOptional<float> Value;

	for (const FEasingAreaHandle& Handle : Easings)
	{
		UMovieSceneSection* Section = Handle.Section.GetSectionObject();
		if (Section)
		{
			if (Handle.EasingType == ESequencerEasingType::In && Section->Easing.GetEaseInTime() == Value.Get(Section->Easing.GetEaseInTime()))
			{
				Value = Section->Easing.GetEaseInTime();
			}
			else if (Handle.EasingType == ESequencerEasingType::Out && Section->Easing.GetEaseOutTime() == Value.Get(Section->Easing.GetEaseOutTime()))
			{
				Value = Section->Easing.GetEaseOutTime();
			}
			else
			{
				return TOptional<float>();
			}
		}
	}

	return Value;
}

void FEasingContextMenu::OnUpdateLength(float NewLength)
{
	for (const FEasingAreaHandle& Handle : Easings)
	{
		if (UMovieSceneSection* Section = Handle.Section.GetSectionObject())
		{
			Section->Modify();
			if (Handle.EasingType == ESequencerEasingType::In)
			{
				Section->Easing.bManualEaseIn = true;
				Section->Easing.ManualEaseInTime = FMath::Min(Section->GetRange().Size<float>(), NewLength);
			}
			else
			{
				Section->Easing.bManualEaseOut = true;
				Section->Easing.ManualEaseOutTime = FMath::Min(Section->GetRange().Size<float>(), NewLength);
			}
		}
	}
}

ECheckBoxState FEasingContextMenu::GetAutoEasingCheckState() const
{
	TOptional<bool> IsChecked;
	for (const FEasingAreaHandle& Handle : Easings)
	{
		if (UMovieSceneSection* Section = Handle.Section.GetSectionObject())
		{
			if (Handle.EasingType == ESequencerEasingType::In)
			{
				if (IsChecked.IsSet() && IsChecked.GetValue() != !Section->Easing.bManualEaseIn)
				{
					return ECheckBoxState::Undetermined;
				}
				IsChecked = !Section->Easing.bManualEaseIn;
			}
			else
			{
				if (IsChecked.IsSet() && IsChecked.GetValue() != !Section->Easing.bManualEaseOut)
				{
					return ECheckBoxState::Undetermined;
				}
				IsChecked = !Section->Easing.bManualEaseOut;
			}
		}
	}
	return IsChecked.IsSet() ? IsChecked.GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked : ECheckBoxState::Undetermined;
}

void FEasingContextMenu::SetAutoEasing(bool bAutoEasing)
{
	FScopedTransaction Transaction(LOCTEXT("SetAutoEasingText", "Set Automatic Easing"));

	TArray<UMovieSceneTrack*> AllTracks;

	for (const FEasingAreaHandle& Handle : Easings)
	{
		if (UMovieSceneSection* Section = Handle.Section.GetSectionObject())
		{
			AllTracks.AddUnique(Section->GetTypedOuter<UMovieSceneTrack>());

			Section->Modify();
			if (Handle.EasingType == ESequencerEasingType::In)
			{
				Section->Easing.bManualEaseIn = !bAutoEasing;
			}
			else
			{
				Section->Easing.bManualEaseOut = !bAutoEasing;
			}
		}
	}

	for (UMovieSceneTrack* Track : AllTracks)
	{
		Track->UpdateEasing();
	}
}

FText FEasingContextMenu::GetEasingTypeText() const
{
	FText CurrentText;
	UClass* ClassType = nullptr;
	for (const FEasingAreaHandle& Handle : Easings)
	{
		if (UMovieSceneSection* Section = Handle.Section.GetSectionObject())
		{
			UObject* Object = Handle.EasingType == ESequencerEasingType::In ? Section->Easing.EaseIn.GetObject() : Section->Easing.EaseOut.GetObject();
			if (Object)
			{
				if (!ClassType)
				{
					ClassType = Object->GetClass();
				}
				else if (Object->GetClass() != ClassType)
				{
					CurrentText = LOCTEXT("MultipleEasingTypesText", "<Multiple>");
					break;
				}
			}
		}
	}
	if (CurrentText.IsEmpty())
	{
		CurrentText = ClassType ? ClassType->GetDisplayNameText() : LOCTEXT("NoneEasingText", "None");
	}

	return FText::Format(LOCTEXT("EasingTypeTextFormat", "Method ({0})"), CurrentText);
}

void FEasingContextMenu::EasingTypeMenu(FMenuBuilder& MenuBuilder)
{
	struct FFilter : IClassViewerFilter
	{
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			bool bIsCorrectInterface = InClass->ImplementsInterface(UMovieSceneEasingFunction::StaticClass());
			bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			return bIsCorrectInterface && bMatchesFlags;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			bool bIsCorrectInterface = InUnloadedClassData->ImplementsInterface(UMovieSceneEasingFunction::StaticClass());
			bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			return bIsCorrectInterface && bMatchesFlags;
		}
	};

	FClassViewerModule& ClassViewer = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions InitOptions;
	InitOptions.bShowDisplayNames = true;
	InitOptions.ClassFilter = MakeShared<FFilter>();

	// Copy a reference to the context menu by value into each lambda handler to ensure the type stays alive until the menu is closed
	TSharedRef<FEasingContextMenu> Shared = AsShared();

	TSharedRef<SWidget> ClassViewerWidget = ClassViewer.CreateClassViewer(InitOptions, FOnClassPicked::CreateLambda([=](UClass* NewClass) { Shared->OnEasingTypeChanged(NewClass); }));

	MenuBuilder.AddWidget(ClassViewerWidget, FText(), true, false);
}

void FEasingContextMenu::OnEasingTypeChanged(UClass* NewClass)
{
	FScopedTransaction Transaction(LOCTEXT("SetEasingType", "Set Easing Method"));

	for (const FEasingAreaHandle& Handle : Easings)
	{
		UMovieSceneSection* Section = Handle.Section.GetSectionObject();
		if (!Section)
		{
			continue;
		}

		Section->Modify();

		TScriptInterface<IMovieSceneEasingFunction>& EaseObject = Handle.EasingType == ESequencerEasingType::In ? Section->Easing.EaseIn : Section->Easing.EaseOut;
		if (!EaseObject.GetObject() || EaseObject.GetObject()->GetClass() != NewClass)
		{
			UObject* NewEasingFunction = NewObject<UObject>(Section, NewClass);

			EaseObject.SetObject(NewEasingFunction);
			EaseObject.SetInterface(Cast<IMovieSceneEasingFunction>(NewEasingFunction));
		}
	}
}

void FEasingContextMenu::EasingOptionsMenu(FMenuBuilder& MenuBuilder)
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ nullptr,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowScrollBar = false;

	TSharedRef<IDetailsView> DetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	
	TArray<UObject*> Objects;
	for (const FEasingAreaHandle& Handle : Easings)
	{
		if (UMovieSceneSection* Section = Handle.Section.GetSectionObject())
		{
			if (Handle.EasingType == ESequencerEasingType::In)
			{
				Objects.AddUnique(Section->Easing.EaseIn.GetObject());
			}
			else
			{
				Objects.AddUnique(Section->Easing.EaseOut.GetObject());
			}
		}
	}

	DetailsView->SetObjects(Objects, true);

	MenuBuilder.AddWidget(DetailsView, FText(), true, false);
}


#undef LOCTEXT_NAMESPACE
