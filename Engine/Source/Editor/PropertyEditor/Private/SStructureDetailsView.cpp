// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SStructureDetailsView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "AssetSelection.h"
#include "DetailCategoryBuilderImpl.h"
#include "UserInterface/PropertyDetails/PropertyDetailsUtilities.h"
#include "StructurePropertyNode.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SSearchBox.h"


#define LOCTEXT_NAMESPACE "SStructureDetailsView"


SStructureDetailsView::~SStructureDetailsView()
{
	auto RootNodeLocal = GetRootNode();

	if (RootNodeLocal.IsValid())
	{
		SaveExpandedItems(RootNodeLocal.ToSharedRef());
	}
}

UStruct* SStructureDetailsView::GetBaseScriptStruct() const
{
	const UStruct* Struct = StructData.IsValid() ? StructData->GetStruct() : NULL;
	return const_cast<UStruct*>(Struct);
}

void SStructureDetailsView::Construct(const FArguments& InArgs)
{
	DetailsViewArgs = InArgs._DetailsViewArgs;
	
	CustomName = InArgs._CustomName;

	// Create the root property now
	// Only one root node in a structure details view
	RootNodes.Empty(1);
	RootNodes.Add(MakeShareable(new FStructurePropertyNode));
		
	PropertyUtilities = MakeShareable( new FPropertyDetailsUtilities( *this ) );
	
	ColumnSizeData.LeftColumnWidth = TAttribute<float>(this, &SStructureDetailsView::OnGetLeftColumnWidth);
	ColumnSizeData.RightColumnWidth = TAttribute<float>(this, &SStructureDetailsView::OnGetRightColumnWidth);
	ColumnSizeData.OnWidthChanged = SSplitter::FOnSlotResized::CreateSP(this, &SStructureDetailsView::OnSetColumnWidth);

	TSharedRef<SScrollBar> ExternalScrollbar = 
		SNew(SScrollBar)
		.AlwaysShowScrollbar( DetailsViewArgs.bShowScrollBar )
		.Visibility(DetailsViewArgs.bShowScrollBar ? EVisibility::Visible : EVisibility::Collapsed);

		FMenuBuilder DetailViewOptions( true, NULL );

		FUIAction ShowOnlyModifiedAction( 
			FExecuteAction::CreateSP(this, &SStructureDetailsView::OnShowOnlyModifiedClicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SStructureDetailsView::IsShowOnlyModifiedChecked)
		);

		if (DetailsViewArgs.bShowModifiedPropertiesOption)
		{
			DetailViewOptions.AddMenuEntry( 
				LOCTEXT("ShowOnlyModified", "Show Only Modified Properties"),
				LOCTEXT("ShowOnlyModified_ToolTip", "Displays only properties which have been changed from their default"),
				FSlateIcon(),
				ShowOnlyModifiedAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton 
			);
		}

		FUIAction ShowAllAdvancedAction( 
			FExecuteAction::CreateSP(this, &SStructureDetailsView::OnShowAllAdvancedClicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SStructureDetailsView::IsShowAllAdvancedChecked)
		);

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ShowAllAdvanced", "Show All Advanced Details"),
			LOCTEXT("ShowAllAdvanced_ToolTip", "Shows all advanced detail sections in each category"),
			FSlateIcon(),
			ShowAllAdvancedAction,
			NAME_None,
			EUserInterfaceActionType::ToggleButton 
			);

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("CollapseAll", "Collapse All Categories"),
			LOCTEXT("CollapseAll_ToolTip", "Collapses all root level categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SStructureDetailsView::SetRootExpansionStates, /*bExpanded=*/false, /*bRecurse=*/false)));
		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ExpandAll", "Expand All Categories"),
			LOCTEXT("ExpandAll_ToolTip", "Expands all root level categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SStructureDetailsView::SetRootExpansionStates, /*bExpanded=*/true, /*bRecurse=*/false)));

	TSharedRef<SHorizontalBox> FilterBoxRow = SNew( SHorizontalBox )
		.Visibility(this, &SStructureDetailsView::GetFilterBoxVisibility)
		+SHorizontalBox::Slot()
		.FillWidth( 1 )
		.VAlign( VAlign_Center )
		[
			// Create the search box
			SAssignNew( SearchBox, SSearchBox )
			.OnTextChanged(this, &SStructureDetailsView::OnFilterTextChanged)
		];

	if (DetailsViewArgs.bShowOptions)
	{
		FilterBoxRow->AddSlot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew( SComboButton )
				.ContentPadding(0)
				.ForegroundColor( FSlateColor::UseForeground() )
				.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
				.MenuContent()
				[
					DetailViewOptions.MakeWidget()
				]
				.ButtonContent()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("GenericViewButton") )
				]
			];
	}

	SAssignNew(DetailTree, SDetailTree)
		.Visibility(this, &SStructureDetailsView::GetTreeVisibility)
		.TreeItemsSource(&RootTreeNodes)
		.OnGetChildren(this, &SStructureDetailsView::OnGetChildrenForDetailTree)
		.OnSetExpansionRecursive(this, &SStructureDetailsView::SetNodeExpansionStateRecursive)
		.OnGenerateRow(this, &SStructureDetailsView::OnGenerateRowForDetailTree)
		.OnExpansionChanged(this, &SStructureDetailsView::OnItemExpansionChanged)
		.SelectionMode(ESelectionMode::None)
		.ExternalScrollbar(ExternalScrollbar);

	ChildSlot
	[
		SNew( SBox )
		.Visibility(this, &SStructureDetailsView::GetPropertyEditingVisibility)
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 0.0f, 0.0f, 2.0f )
			[
				FilterBoxRow
			]
			+ SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(0)
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				[
					DetailTree.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SBox )
					.WidthOverride( 16.0f )
					[
						ExternalScrollbar
					]
				]
			]
		]
	];
}

void SStructureDetailsView::SetStructureData(TSharedPtr<FStructOnScope> InStructData)
{
	TSharedPtr<FComplexPropertyNode> RootNode = GetRootNode();
	//PRE SET
	SaveExpandedItems(RootNode.ToSharedRef() );
	RootNode->AsStructureNode()->SetStructure(nullptr);
	RootNodesPendingKill.Add(RootNode);

	RootNodes.Empty(1);

	RootNode = MakeShareable(new FStructurePropertyNode);
	RootNodes.Add(RootNode);

	//SET
	StructData = InStructData;
	RootNode->AsStructureNode()->SetStructure(StructData);
	if (!StructData.IsValid())
	{
		bIsLocked = false;
	}
	
	//POST SET
	DestroyColorPicker();
	ColorPropertyNode = NULL;

	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = NULL;
	InitParams.Property = NULL;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	InitParams.bAllowChildren = true;
	InitParams.bForceHiddenPropertyVisibility = FPropertySettings::Get().ShowHiddenProperties();
	InitParams.bCreateCategoryNodes = false;

	RootNode->InitNode(InitParams);
	RootNode->SetDisplayNameOverride(CustomName);

	RestoreExpandedItems(RootNode.ToSharedRef());

	UpdatePropertyMaps();

	UpdateFilteredDetails();
}

void SStructureDetailsView::ForceRefresh()
{
	SetStructureData(StructData);
}

void SStructureDetailsView::ClearSearch()
{
	CurrentFilter.FilterStrings.Empty();
	SearchBox->SetText(FText::GetEmpty());
	RerunCurrentFilter();
}


const TArray< TWeakObjectPtr<UObject> >& SStructureDetailsView::GetSelectedObjects() const
{
	static const TArray< TWeakObjectPtr<UObject> > DummyRef;
	return DummyRef;
}

const TArray< TWeakObjectPtr<AActor> >& SStructureDetailsView::GetSelectedActors() const
{
	static const TArray< TWeakObjectPtr<AActor> > DummyRef;
	return DummyRef;
}

const FSelectedActorInfo& SStructureDetailsView::GetSelectedActorInfo() const
{
	static const FSelectedActorInfo DummyRef;
	return DummyRef;
}

bool SStructureDetailsView::IsConnected() const
{
	const FStructurePropertyNode* RootNode = GetRootNode().IsValid() ? GetRootNode()->AsStructureNode() : nullptr;
	return StructData.IsValid() && StructData->IsValid() && RootNode && RootNode->HasValidStructData();
}

FRootPropertyNodeList& SStructureDetailsView::GetRootNodes()
{
	return RootNodes;
}

TSharedPtr<class FComplexPropertyNode> SStructureDetailsView::GetRootNode()
{
	return RootNodes[0];
}

const TSharedPtr<class FComplexPropertyNode> SStructureDetailsView::GetRootNode() const
{
	return RootNodes[0];
}

void SStructureDetailsView::CustomUpdatePropertyMap(TSharedPtr<FDetailLayoutBuilderImpl>& InDetailLayout)
{
	InDetailLayout->DefaultCategory(NAME_None).SetDisplayName(NAME_None, CustomName);
}

EVisibility SStructureDetailsView::GetPropertyEditingVisibility() const
{
	const FStructurePropertyNode* RootNode = GetRootNode().IsValid() ? GetRootNode()->AsStructureNode() : nullptr;
	return StructData.IsValid() && StructData->IsValid() && RootNode && RootNode->HasValidStructData() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
