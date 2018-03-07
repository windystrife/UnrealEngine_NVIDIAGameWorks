// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergingTool/SMeshMergingDialog.h"
#include "MeshMergingTool/MeshMergingTool.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailChildrenBuilder.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "Components/ChildActorComponent.h"
#include "Components/ShapeComponent.h"

#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SMeshMergingDialog"

//////////////////////////////////////////////////////////////////////////
// SMeshMergingDialog

SMeshMergingDialog::SMeshMergingDialog()
{
	bRefreshListView = false;
	NumSelectedMeshComponents = 0;
}

SMeshMergingDialog::~SMeshMergingDialog()
{
	// Remove all delegates
	USelection::SelectionChangedEvent.RemoveAll(this);
	USelection::SelectObjectEvent.RemoveAll(this);	
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshMergingDialog::Construct(const FArguments& InArgs, FMeshMergingTool* InTool)
{
	checkf(InTool != nullptr, TEXT("Invalid owner tool supplied"));
	Tool = InTool;

	UpdateSelectedStaticMeshComponents();
	CreateSettingsView();
	
	// Create widget layout
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				// Static mesh component selection
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeStaticMeshComponentsLabel", "Mesh Components to be incorporated in the merge:"))						
					]
				]
				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					[
						SAssignNew(ComponentsListView, SListView<TSharedPtr<FMergeComponentData>>)
						.ListItemsSource(&SelectedComponents)
						.OnGenerateRow(this, &SMeshMergingDialog::MakeComponentListItemWidget)
						.ToolTipText(LOCTEXT("SelectedComponentsListBoxToolTip", "The selected mesh components will be incorporated into the merged mesh"))
					]
			]
		]

		+ SVerticalBox::Slot()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				// Static mesh component selection
				+ SVerticalBox::Slot()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SettingsView->AsShared()
					]
				]
			]
		]

		// Replace source actors
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
		[
			SNew(SCheckBox)
			.Type(ESlateCheckBoxType::CheckBox)
			.IsChecked(this, &SMeshMergingDialog::GetReplaceSourceActors)
			.OnCheckStateChanged(this, &SMeshMergingDialog::SetReplaceSourceActors)
			.ToolTipText(LOCTEXT("ReplaceSourceActorsToolTip", "When enabled the Source Actors will be replaced with the newly generated merged mesh"))
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ReplaceSourceActorsLabel", "Replace Source Actors"))
				.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Yellow)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Visibility_Lambda([this]()->EVisibility { return this->GetContentEnabledState() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DeleteUndo", "Insufficient mesh components found for merging."))
			]
		]
	];	


	// Selection change
	USelection::SelectionChangedEvent.AddRaw(this, &SMeshMergingDialog::OnLevelSelectionChanged);
	USelection::SelectObjectEvent.AddRaw(this, &SMeshMergingDialog::OnLevelSelectionChanged);
	FEditorDelegates::MapChange.AddSP(this, &SMeshMergingDialog::OnMapChange);
	FEditorDelegates::NewCurrentLevel.AddSP(this, &SMeshMergingDialog::OnNewCurrentLevel);

	MergeSettings = UMeshMergingSettingsObject::Get();
	SettingsView->SetObject(MergeSettings);
}

void SMeshMergingDialog::OnMapChange(uint32 MapFlags)
{
	Reset();
}

void SMeshMergingDialog::OnNewCurrentLevel()
{
	Reset();
}

void SMeshMergingDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Check if we need to update selected components and the listbox
	if (bRefreshListView == true)
	{
		StoreCheckBoxState();
		UpdateSelectedStaticMeshComponents();
		ComponentsListView->ClearSelection();
		ComponentsListView->RequestListRefresh();
		bRefreshListView = false;		
	}
}

void SMeshMergingDialog::Reset()
{	
	bRefreshListView = true;
}

ECheckBoxState SMeshMergingDialog::GetReplaceSourceActors() const
{
	return (Tool->bReplaceSourceActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetReplaceSourceActors(ECheckBoxState NewValue)
{
	Tool->bReplaceSourceActors = (ECheckBoxState::Checked == NewValue);
}

bool SMeshMergingDialog::GetContentEnabledState() const
{
	return (GetNumSelectedMeshComponents() >= 1); // Only enabled if a mesh is selected
}

void SMeshMergingDialog::UpdateSelectedStaticMeshComponents()
{	
	NumSelectedMeshComponents = 0;

	// Retrieve selected actors
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}

	// Retrieve static mesh components from selected actors
	SelectedComponents.Empty();
	for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
	{
		AActor* Actor = Actors[ActorIndex];
		check(Actor != nullptr);

		TArray<UChildActorComponent*> ChildActorComponents;
		Actor->GetComponents<UChildActorComponent>(ChildActorComponents);
		for (UChildActorComponent* ChildComponent : ChildActorComponents)
		{
			// Push actor at the back of array so we will process it
			AActor* ChildActor = ChildComponent->GetChildActor();
			if (ChildActor)
			{
				Actors.Add(ChildActor);
			}
		}
		
		TArray<UPrimitiveComponent*> PrimComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimComponents);
		for (UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			bool bInclude = false; // Should put into UI list
			bool bShouldIncorporate = false; // Should default to part of merged mesh
			bool bIsMesh = false;
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimComponent))
			{
				bShouldIncorporate = (StaticMeshComponent->GetStaticMesh() != nullptr);
				bInclude = true;
				bIsMesh = true;
			}
			else if (UShapeComponent* ShapeComponent = Cast<UShapeComponent>(PrimComponent))
			{
				bShouldIncorporate = true;
				bInclude = true;
			}

			if (bInclude)
			{
				SelectedComponents.Add(TSharedPtr<FMergeComponentData>(new FMergeComponentData(PrimComponent)));
				TSharedPtr<FMergeComponentData>& ComponentData = SelectedComponents.Last();

				ComponentData->bShouldIncorporate = bShouldIncorporate;

				// See if we stored a checkbox state for this mesh component, and set accordingly
				ECheckBoxState* StoredState = StoredCheckBoxStates.Find(PrimComponent);
				if (StoredState != nullptr)
				{
					ComponentData->bShouldIncorporate = (*StoredState == ECheckBoxState::Checked);
				}

				// Keep count of selected meshes
				if (ComponentData->bShouldIncorporate && bIsMesh)
				{
					NumSelectedMeshComponents++;
				}
			}

		}		
	}
}

TSharedRef<ITableRow> SMeshMergingDialog::MakeComponentListItemWidget(TSharedPtr<FMergeComponentData> ComponentData, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(ComponentData->PrimComponent != nullptr);

	// Retrieve information about the mesh component
	const FString OwningActorName = ComponentData->PrimComponent->GetOwner()->GetName();

	// If box should be enabled
	bool bEnabled = true;
	bool bIsMesh = false;

	FString ComponentInfo;
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(ComponentData->PrimComponent.Get()))
	{
		ComponentInfo = (StaticMeshComponent->GetStaticMesh() != nullptr) ? StaticMeshComponent->GetStaticMesh()->GetName() : TEXT("No Static Mesh Available");
		bEnabled = (StaticMeshComponent->GetStaticMesh() != nullptr);
		bIsMesh = true;
	}
	else if (UShapeComponent* ShapeComponent = Cast<UShapeComponent>(ComponentData->PrimComponent.Get()))
	{
		ComponentInfo = ShapeComponent->GetClass()->GetName();
	}

	const FString ComponentName = ComponentData->PrimComponent->GetName();

	// See if we stored a checkbox state for this mesh component, and set accordingly
	ECheckBoxState State = bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	ECheckBoxState* StoredState = StoredCheckBoxStates.Find(ComponentData->PrimComponent.Get());
	if (StoredState)
	{
		State = *StoredState;
	}
	

	return SNew(STableRow<TSharedPtr<FMergeComponentData>>, OwnerTable)
		[
			SNew(SBox)
			[
				// Disable UI element if this static mesh component has invalid static mesh data
				SNew(SHorizontalBox)				
				.IsEnabled(bEnabled)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(State)
					.ToolTipText(LOCTEXT("IncorporateCheckBoxToolTip", "When ticked the Component will be incorporated into the merge"))
					
					.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
					{
						ComponentData->bShouldIncorporate = (NewState == ECheckBoxState::Checked);

						if (bIsMesh)
						{
							this->NumSelectedMeshComponents += (NewState == ECheckBoxState::Checked) ? 1 : -1;
						}
					})
				]

				+ SHorizontalBox::Slot()
				.Padding(5.0, 0, 0, 0)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(OwningActorName + " - " + ComponentInfo + " - " + ComponentName))
				]
			]
		];

}

void SMeshMergingDialog::CreateSettingsView()
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = true;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.bCustomNameAreaLocation = false;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	
		
	// Tiny hack to hide this setting, since we have no way / value to go off to 
	struct Local
	{
		/** Delegate to show all properties */
		static bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent, bool bInShouldShowNonEditable)
		{
			return (PropertyAndParent.Property.GetFName() != GET_MEMBER_NAME_CHECKED(FMaterialProxySettings, GutterSpace));
		}
	};

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);
	SettingsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&Local::IsPropertyVisible, true));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMeshMergingDialog::OnLevelSelectionChanged(UObject* Obj)
{
	Reset();
}

void SMeshMergingDialog::StoreCheckBoxState()
{
	StoredCheckBoxStates.Empty();

	// Loop over selected mesh component and store its checkbox state
	for (TSharedPtr<FMergeComponentData> SelectedComponent : SelectedComponents )
	{
		UPrimitiveComponent* PrimComponent = SelectedComponent->PrimComponent.Get();
		const ECheckBoxState State = SelectedComponent->bShouldIncorporate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		StoredCheckBoxStates.Add(PrimComponent, State);
	}
}

#undef LOCTEXT_NAMESPACE
