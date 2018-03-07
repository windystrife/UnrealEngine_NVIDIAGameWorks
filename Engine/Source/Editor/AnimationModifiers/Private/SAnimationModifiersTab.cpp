// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimationModifiersTab.h"

#include "AnimationModifierDetailCustomization.h"
#include "AnimationModifiersAssetUserData.h"
#include "SModifierListview.h"
#include "SButton.h"
#include "SComboButton.h"
#include "Animation/AnimSequence.h"

#include "AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Dialogs.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Toolkits/AssetEditorManager.h"
#include "Editor.h"
#include "SComboButton.h"
#include "SButton.h"
#include "ScopedTransaction.h"
#include "SMenuAnchor.h"
#include "Engine/BlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "SAnimationModifiersTab"

/** ClassViewerFilter for Animation Modifier classes */
class FModifierClassFilter : public IClassViewerFilter
{
public:
	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InClass->IsChildOf(UAnimationModifier::StaticClass());
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InClass->IsChildOf(UAnimationModifier::StaticClass());
	}
};

SAnimationModifiersTab::SAnimationModifiersTab()
	: Skeleton(nullptr), AnimationSequence(nullptr), AssetUserData(nullptr), bDirty(false)
{	
}

SAnimationModifiersTab::~SAnimationModifiersTab()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	FAssetEditorManager::Get().OnAssetOpenedInEditor().RemoveAll(this);	
}

UAnimationModifier* SAnimationModifiersTab::CreateModifierInstance(UObject* Outer, UClass* InClass)
{
	checkf(Outer, TEXT("Invalid outer value for modifier instantiation"));
	UAnimationModifier* ProcessorInstance = NewObject<UAnimationModifier>(Outer, InClass);
	checkf(ProcessorInstance, TEXT("Unable to instantiate modifier class"));
	ProcessorInstance->SetFlags(RF_Transactional);
	return ProcessorInstance;
}


void SAnimationModifiersTab::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bDirty)
	{
		RetrieveModifierData();
		ModifierListView->Refresh();
		bDirty = false;
	}
}

void SAnimationModifiersTab::PostUndo(bool bSuccess)
{
	Refresh();
}

void SAnimationModifiersTab::PostRedo(bool bSuccess)
{
	Refresh();
}

void SAnimationModifiersTab::Construct(const FArguments& InArgs)
{
	HostingApp = InArgs._InHostingApp;
	
	// Retrieve asset and modifier data
	RetrieveAnimationAsset();
	RetrieveModifierData();
		
	CreateInstanceDetailsView();
	
	this->ChildSlot
	[
		SNew(SOverlay)		
		+SOverlay::Slot()
		[				
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(3.0f, 3.0f)
					.AutoWidth()
					[
						SAssignNew(AddModifierCombobox, SComboButton)
						.OnGetMenuContent(this, &SAnimationModifiersTab::GetModifierPicker)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AddModifier", "Add Modifier"))
						]
					]

					+ SHorizontalBox::Slot()
					.Padding(3.0f, 3.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SAnimationModifiersTab::OnApplyAllModifiersClicked)
						.ContentPadding(FMargin(5))
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ApplyAllModifiers", "Apply All Modifiers"))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[				
					SNew(SSplitter)
					.Orientation(EOrientation::Orient_Vertical)

					+ SSplitter::Slot()
					.Value(.5f)
					[
						SNew(SBox)
						.Padding(2.0f)
						[
							SAssignNew(ModifierListView, SModifierListView)
							.Items(&ModifierItems)
							.InstanceDetailsView(ModifierInstanceDetailsView)
							.OnApplyModifier(FOnModifierArray::CreateSP(this, &SAnimationModifiersTab::OnApplyModifier))
							.OnRevertModifier(FOnModifierArray::CreateSP(this, &SAnimationModifiersTab::OnRevertModifier))
							.OnRemoveModifier(FOnModifierArray::CreateSP(this, &SAnimationModifiersTab::OnRemoveModifier))
							.OnOpenModifier(FOnSingleModifier::CreateSP(this, &SAnimationModifiersTab::OnOpenModifier))
							.OnMoveUpModifier(FOnSingleModifier::CreateSP(this, &SAnimationModifiersTab::OnMoveModifierUp))
							.OnMoveDownModifier(FOnSingleModifier::CreateSP(this, &SAnimationModifiersTab::OnMoveModifierDown))
						]
					]

					+ SSplitter::Slot()
					.Value(.5f)
					[	
						SNew(SBox)
						.Padding(2.0f)
						[
							ModifierInstanceDetailsView->AsShared()
						]
					]
				]
			]	
		]
	];

	// Ensure that this tab is only enabled if we have a valid AssetUserData instance
	this->ChildSlot.GetWidget()->SetEnabled(TAttribute<bool>::Create([this]() { return AssetUserData != nullptr; }));

	// Register delegates
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
	FAssetEditorManager::Get().OnAssetOpenedInEditor().AddSP(this, &SAnimationModifiersTab::OnAssetOpened);
}


TSharedRef<SWidget> SAnimationModifiersTab::GetModifierPicker()
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowNoneOption = false;	
	TSharedPtr<FModifierClassFilter> ClassFilter = MakeShareable(new FModifierClassFilter);
	Options.ClassFilter = ClassFilter;

	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SAnimationModifiersTab::OnModifierPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked)
			]
		];
}

void SAnimationModifiersTab::OnModifierPicked(UClass* PickedClass)
{
	FScopedTransaction Transaction(LOCTEXT("AddModifierTransaction", "Adding Animation Modifier"));
	
	UObject* Outer = AssetUserData;
	UAnimationModifier* Processor = CreateModifierInstance(Outer, PickedClass);	
	AssetUserData->Modify();
	AssetUserData->AddAnimationModifier(Processor);
	
	// Close the combo box
	AddModifierCombobox->SetIsOpen(false);

	// Refresh the UI
	Refresh();
}

void SAnimationModifiersTab::RetrieveAnimationAsset()
{
	TSharedPtr<FAssetEditorToolkit> AssetEditor = HostingApp.Pin();
	const TArray<UObject*>* EditedObjects = AssetEditor->GetObjectsCurrentlyBeingEdited();
	AssetUserData = nullptr;

	if (EditedObjects)
	{
		// Try and find an AnimSequence or Skeleton asset in the currently being edited objects, and retrieve or add ModfiersAssetUserDat
		for (UObject* Object : (*EditedObjects))
		{
			if (Object->IsA<UAnimSequence>())
			{
				AnimationSequence = Cast<UAnimSequence>(Object);

				AssetUserData = AnimationSequence->GetAssetUserData<UAnimationModifiersAssetUserData>();
				if (!AssetUserData)
				{
					AssetUserData = NewObject<UAnimationModifiersAssetUserData>(AnimationSequence, UAnimationModifiersAssetUserData::StaticClass());
					checkf(AssetUserData, TEXT("Unable to instantiate AssetUserData class"));
					AssetUserData->SetFlags(RF_Transactional);
					AnimationSequence->AddAssetUserData(AssetUserData);
				}
				
				break;
			}
			else if (Object->IsA<USkeleton>())
			{
				Skeleton = Cast<USkeleton>(Object);

				AssetUserData = Skeleton->GetAssetUserData<UAnimationModifiersAssetUserData>();
				if (!AssetUserData)
				{
					AssetUserData = NewObject<UAnimationModifiersAssetUserData>(Skeleton, UAnimationModifiersAssetUserData::StaticClass());
					checkf(AssetUserData, TEXT("Unable to instantiate AssetUserData class"));
					AssetUserData->SetFlags(RF_Transactional);
					Skeleton->AddAssetUserData(AssetUserData);
				}
				
				break;
			}
		}
	}
}

void SAnimationModifiersTab::CreateInstanceDetailsView()
{
	// Create a property view
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
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;	

	ModifierInstanceDetailsView = EditModule.CreateDetailView(DetailsViewArgs);	
	ModifierInstanceDetailsView->SetDisableCustomDetailLayouts(true);
}

FReply SAnimationModifiersTab::OnApplyAllModifiersClicked()
{
	FScopedTransaction Transaction(LOCTEXT("ApplyAllModifiersTransaction", "Applying All Animation Modifier(s)"));
	const TArray<UAnimationModifier*>& ModifierInstances = AssetUserData->GetAnimationModifierInstances();	
	ApplyModifiers(ModifierInstances);
	return FReply::Handled();
}

void SAnimationModifiersTab::OnApplyModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances)
{
	TArray<UAnimationModifier*> ModifierInstances;
	for (TWeakObjectPtr<UAnimationModifier> InstancePtr : Instances)
	{
		checkf(InstancePtr.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));
		UAnimationModifier* Instance = InstancePtr.Get();
		ModifierInstances.Add(Instance);	
	}

	FScopedTransaction Transaction(LOCTEXT("ApplyModifiersTransaction", "Applying Animation Modifier(s)"));
	ApplyModifiers(ModifierInstances);
}

void SAnimationModifiersTab::FindAnimSequencesForSkeleton(TArray<UAnimSequence *>& ReferencedAnimSequences)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Search for referencing packages to the currently open skeleton
	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(Skeleton->GetOuter()->GetFName(), Referencers);
	for (const FAssetIdentifier& Identifier : Referencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(Identifier.PackageName, Assets);

		for (const FAssetData& Asset : Assets)
		{
			// Only add assets whos class is of UAnimSequence
			if (Asset.GetClass()->IsChildOf(UAnimSequence::StaticClass()))
			{
				ReferencedAnimSequences.Add(CastChecked<UAnimSequence>(Asset.GetAsset()));
			}
		}
	}
}

void SAnimationModifiersTab::OnRevertModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances)
{
	TArray<UAnimationModifier*> ModifierInstances;
	for (TWeakObjectPtr<UAnimationModifier> InstancePtr : Instances)
	{
		checkf(InstancePtr.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));
		UAnimationModifier* Instance = InstancePtr.Get();
		ModifierInstances.Add(Instance);
	}
	
	FScopedTransaction Transaction(LOCTEXT("RevertModifiersTransaction", "Reverting Animation Modifier(s)"));
	RevertModifiers(ModifierInstances);
}

void SAnimationModifiersTab::OnRemoveModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances)
{
	const bool bShouldRevert = OpenMsgDlgInt(EAppMsgType::YesNo, LOCTEXT("RemoveAndRevertPopupText", "Should the Modifiers be reverted before removing them?"), FText::FromString("Revert before Removing")) == EAppReturnType::Yes;

	FScopedTransaction Transaction(LOCTEXT("RemoveModifiersTransaction", "Removing Animation Modifier(s)"));	
	AssetUserData->Modify();

	if (bShouldRevert)
	{
		TArray<UAnimationModifier*> ModifierInstances;
		for (TWeakObjectPtr<UAnimationModifier> InstancePtr : Instances)
		{
			checkf(InstancePtr.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));
			UAnimationModifier* Instance = InstancePtr.Get();
			ModifierInstances.Add(Instance);
		}

		RevertModifiers(ModifierInstances);
	}

	for (TWeakObjectPtr<UAnimationModifier> InstancePtr : Instances)
	{
		checkf(InstancePtr.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));

		UAnimationModifier* Instance = InstancePtr.Get();
		AssetUserData->Modify();
		AssetUserData->RemoveAnimationModifierInstance(Instance);
	}

	Refresh();
}

void SAnimationModifiersTab::OnOpenModifier(const TWeakObjectPtr<UAnimationModifier>& Instance)
{
	checkf(Instance.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));
	const UAnimationModifier* ModifierInstance = Instance.Get();
	const UBlueprintGeneratedClass* BPGeneratedClass = Cast<UBlueprintGeneratedClass>(ModifierInstance->GetClass());

	if (BPGeneratedClass && BPGeneratedClass->ClassGeneratedBy)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(BPGeneratedClass->ClassGeneratedBy);
		if (Blueprint)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
		}		
	}
}

void SAnimationModifiersTab::OnMoveModifierUp(const TWeakObjectPtr<UAnimationModifier>& Instance)
{
	FScopedTransaction Transaction(LOCTEXT("MoveModifierUpTransaction", "Moving Animation Modifier Up"));
	checkf(Instance.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));
	AssetUserData->Modify();

	AssetUserData->ChangeAnimationModifierIndex(Instance.Get(), -1);
	Refresh();
}

void SAnimationModifiersTab::OnMoveModifierDown(const TWeakObjectPtr<UAnimationModifier>& Instance)
{
	FScopedTransaction Transaction(LOCTEXT("MoveModifierDownTransaction", "Moving Animation Modifier Down"));
	checkf(Instance.IsValid(), TEXT("Invalid weak object ptr to modifier instance"));
	AssetUserData->Modify();

	AssetUserData->ChangeAnimationModifierIndex(Instance.Get(), 1);
	Refresh();
}

void SAnimationModifiersTab::Refresh()
{
	bDirty = true;
}

void SAnimationModifiersTab::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	if (Blueprint)
	{
		Refresh();
	}
}

void SAnimationModifiersTab::OnAssetOpened(UObject* Object, IAssetEditorInstance* Instance)
{	
	RetrieveAnimationAsset();
	RetrieveModifierData();
	ModifierListView->Refresh();
}

void SAnimationModifiersTab::ApplyModifiers(const TArray<UAnimationModifier*>& Modifiers)
{
	bool bApply = true;

	TArray<UAnimSequence*> AnimSequences;
	if (AnimationSequence != nullptr)
	{
		AnimSequences.Add(AnimationSequence);
	}
	else if (Skeleton != nullptr)
	{
		// Double check with the user for applying all modifiers to referenced animation sequences for the skeleton
		bApply = OpenMsgDlgInt(EAppMsgType::YesNo, LOCTEXT("ApplyingSkeletonModifierPopupText", "Are you sure you want to apply the modifiers to all animation sequences referenced by the current skeleton?"), FText::FromString("Are you sure?")) == EAppReturnType::Yes;
		
		if (bApply)
		{
			FindAnimSequencesForSkeleton(AnimSequences);
			Skeleton->Modify();
		}
	}

	if (bApply)
	{
		for (UAnimSequence* AnimSequence : AnimSequences)
		{
			AnimSequence->Modify();
		}

		for (UAnimationModifier* Instance : Modifiers)
		{
			checkf(Instance, TEXT("Invalid modifier instance"));
			Instance->Modify();
			for (UAnimSequence* AnimSequence : AnimSequences)
			{
				ensure(!(Skeleton != nullptr) || AnimSequence->GetSkeleton() == Skeleton);
				Instance->ApplyToAnimationSequence(AnimSequence);
			}
		}
	}
}

void SAnimationModifiersTab::RevertModifiers(const TArray<UAnimationModifier*>& Modifiers)
{
	bool bRevert = true;
	TArray<UAnimSequence*> AnimSequences;
	if (AnimationSequence != nullptr)
	{
		AnimSequences.Add(AnimationSequence);
	}
	else if (Skeleton != nullptr)
	{
		// Double check with the user for reverting all modifiers from referenced animation sequences for the skeleton
		bRevert = OpenMsgDlgInt(EAppMsgType::YesNo, LOCTEXT("RevertingSkeletonModifierPopupText", "Are you sure you want to revert the modifiers from all animation sequences referenced by the current skeleton?"), FText::FromString("Are you sure?")) == EAppReturnType::Yes;

		if ( bRevert)
		{
			FindAnimSequencesForSkeleton(AnimSequences);
			Skeleton->Modify();
		}
	}

	if (bRevert)
	{
		for (UAnimSequence* AnimSequence : AnimSequences)
		{
			AnimSequence->Modify();
		}

		for (UAnimationModifier* Instance : Modifiers)
		{
			checkf(Instance, TEXT("Invalid modifier instance"));
			Instance->Modify();
			for (UAnimSequence* AnimSequence : AnimSequences)
			{
				ensure(!(Skeleton != nullptr) || AnimSequence->GetSkeleton() == Skeleton);
				Instance->RevertFromAnimationSequence(AnimSequence);
			}
		}
	}	
}

void SAnimationModifiersTab::RetrieveModifierData()
{
	ResetModifierData();

	if (AssetUserData)
	{
		const TArray<UAnimationModifier*>& ModifierInstances = AssetUserData->GetAnimationModifierInstances();
		for (int32 ModifierIndex = 0; ModifierIndex < ModifierInstances.Num(); ++ModifierIndex)
		{
			UAnimationModifier* Modifier = ModifierInstances[ModifierIndex];
			checkf(Modifier != nullptr, TEXT("Invalid modifier ptr entry"));
			FModifierListviewItem* Item = new FModifierListviewItem();
			Item->Instance = Modifier;
			Item->Class = Modifier->GetClass();
			Item->Index = ModifierIndex;
			Item->OuterClass = AssetUserData->GetOuter()->GetClass();
			ModifierItems.Add(ModifierListviewItem(Item));

			// Register a delegate for when a BP is compiled, this so we can refresh the UI and prevent issues with invalid instance data
			if (Item->Class->ClassGeneratedBy)
			{
				checkf(Item->Class->ClassGeneratedBy != nullptr, TEXT("Invalid ClassGeneratedBy value"));
				UBlueprint* Blueprint = CastChecked<UBlueprint>(Item->Class->ClassGeneratedBy);
				Blueprint->OnCompiled().AddSP(this, &SAnimationModifiersTab::OnBlueprintCompiled);
				DelegateRegisteredBlueprints.Add(Blueprint);
			}
		}
	}
}

void SAnimationModifiersTab::ResetModifierData()
{
	const int32 NumProcessors = (AssetUserData != nullptr) ? AssetUserData->GetAnimationModifierInstances().Num() : 0;
	DelegateRegisteredBlueprints.Empty(NumProcessors);
	ModifierItems.Empty(NumProcessors);

	for (UBlueprint* Blueprint : DelegateRegisteredBlueprints)
	{
		Blueprint->OnCompiled().RemoveAll(this);
	}
}

#undef LOCTEXT_NAMESPACE //"SAnimationModifiersTab"