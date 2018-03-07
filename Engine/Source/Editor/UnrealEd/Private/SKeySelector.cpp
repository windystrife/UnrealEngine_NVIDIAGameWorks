// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SKeySelector.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SComboBox.h"
#include "ScopedTransaction.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "Widgets/Input/SSearchBox.h"
#include "SListViewSelectorDropdownMenu.h"

#define LOCTEXT_NAMESPACE "KeySelector"

static const FString BigTooltipDocLink = TEXT("Shared/Editor/ProjectSettings");

class FKeyTreeInfo
{
public:
	/** This data item's children */
	TArray< TSharedPtr<FKeyTreeInfo> > Children;

private:
	/** This data item's name */
	FText Name;

	/** The actual key associated with this item */
	TSharedPtr<FKey> Key;

public:
	FKeyTreeInfo(FText InName, TSharedPtr<FKey> InKey)
	: Name(InName)
	, Key(InKey)
	{
	}

	FKeyTreeInfo(TSharedPtr<FKeyTreeInfo> InInfo)
	: Name(InInfo->Name)
	, Key(InInfo->Key)
	{
	}

	FText GetDescription() const
	{
		if (Key.IsValid())
		{
			return Key->GetDisplayName();
		}
		else
		{
			return Name;
		}
	}

	TSharedPtr<FKey> GetKey() const
	{
		return Key;
	}

	bool MatchesSearchTokens(const TArray<FString>& SearchTokens)
	{
		FString Description = GetDescription().ToString();

		for (auto Token : SearchTokens)
		{
			if (!Description.Contains(Token))
			{
				return false;
			}
		}

		return true;
	}
};

void SKeySelector::Construct(const FArguments& InArgs)
{
	SearchText = FText::GetEmpty();

	OnKeyChanged = InArgs._OnKeyChanged;
	CurrentKey = InArgs._CurrentKey;

	TMap<FName, FKeyTreeItem> TreeRootsForCatgories;

	// Ensure that Gamepad, Keyboard, and Mouse will appear at the top of the list, other categories will dynamically get added as the keys are encountered
	TreeRootsForCatgories.Add(EKeys::NAME_GamepadCategory, *new (KeyTreeRoot) FKeyTreeItem(MakeShareable(new FKeyTreeInfo(EKeys::GetMenuCategoryDisplayName(EKeys::NAME_GamepadCategory), nullptr))));
	TreeRootsForCatgories.Add(EKeys::NAME_KeyboardCategory, *new (KeyTreeRoot) FKeyTreeItem(MakeShareable(new FKeyTreeInfo(EKeys::GetMenuCategoryDisplayName(EKeys::NAME_KeyboardCategory), nullptr))));
	TreeRootsForCatgories.Add(EKeys::NAME_MouseCategory, *new (KeyTreeRoot) FKeyTreeItem(MakeShareable(new FKeyTreeInfo(EKeys::GetMenuCategoryDisplayName(EKeys::NAME_MouseCategory), nullptr))));

	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);

	for (FKey Key : AllKeys)
	{
		if (!InArgs._FilterBlueprintBindable || Key.IsBindableInBlueprints())
		{
			const FName KeyMenuCategory = Key.GetMenuCategory();
			FKeyTreeItem* KeyCategory = TreeRootsForCatgories.Find(KeyMenuCategory);
			if (KeyCategory == nullptr)
			{
				KeyCategory = new (KeyTreeRoot) FKeyTreeItem(MakeShareable(new FKeyTreeInfo(EKeys::GetMenuCategoryDisplayName(KeyMenuCategory), nullptr)));
				TreeRootsForCatgories.Add(KeyMenuCategory, *KeyCategory);
			}
			(*KeyCategory)->Children.Add(MakeShareable(new FKeyTreeInfo(FText(), MakeShareable(new FKey(Key)))));
		}
	}

	// if we allow NoClear, add a "None" option to be able to clear out a binding
	if (InArgs._AllowClear)
	{
		new (KeyTreeRoot) FKeyTreeItem(MakeShareable(new FKeyTreeInfo(FText(), MakeShareable(new FKey(NAME_None)))));
	}

	TreeViewWidth = InArgs._TreeViewWidth;
	TreeViewHeight = InArgs._TreeViewHeight;
	CategoryFont = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 );
	KeyFont = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10);

	FilteredKeyTreeRoot = KeyTreeRoot;

	this->ChildSlot
	[
		SAssignNew(KeyComboButton, SComboButton)
		.OnGetMenuContent(this, &SKeySelector::GetMenuContent)
		.ContentPadding(0)
		.ToolTipText(LOCTEXT("KeySelector", "Select the key value."))
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SKeySelector::GetKeyIconImage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(this, &SKeySelector::GetKeyDescription)
				.Font(InArgs._Font)
			]
		]
	];
}

//=======================================================================
// Attribute Helpers

FText SKeySelector::GetKeyDescription() const
{
	TOptional<FKey> CurrentKeyValue = CurrentKey.Get();
	if (CurrentKeyValue.IsSet())
	{
		return CurrentKeyValue.GetValue().GetDisplayName();
	}
	return LOCTEXT("MultipleValues", "Multiple Values");
}

const FSlateBrush* SKeySelector::GetKeyIconImage() const
{
	TOptional<FKey> CurrentKeyValue = CurrentKey.Get();
	if (CurrentKeyValue.IsSet())
	{
		return GetIconFromKey(CurrentKeyValue.GetValue());
	}
	return nullptr;
}

//=======================================================================
// Key TreeView Support
TSharedRef<ITableRow> SKeySelector::GenerateKeyTreeRow(FKeyTreeItem InItem, const TSharedRef<STableViewBase>& OwnerTree)
{
	const bool bIsCategory = !InItem->GetKey().IsValid();
	const FText Description = InItem->GetDescription();

	// Determine the best icon the to represents this item
	const FSlateBrush* IconBrush = nullptr;
	if (InItem->GetKey().IsValid())
	{
		IconBrush = GetIconFromKey(*InItem->GetKey().Get());
	}

	return SNew(SComboRow<FKeyTreeItem>, OwnerTree)
		.ToolTip(IDocumentation::Get()->CreateToolTip(Description, nullptr, BigTooltipDocLink, Description.ToString()))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.f)
			[
				SNew(SImage)
				.Image(IconBrush)
				.Visibility(bIsCategory ? EVisibility::Collapsed : EVisibility::Visible)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.f)
			[
				SNew(STextBlock)
				.Text(Description)
				.HighlightText(SearchText)
				.Font(bIsCategory ? CategoryFont : KeyFont)
			]
		];
}

void SKeySelector::OnKeySelectionChanged(FKeyTreeItem Selection, ESelectInfo::Type SelectInfo)
{
	// When the user is navigating, do not act upon the selection change
	if (SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}

	// Only handle selection for non-read only items, since STreeViewItem doesn't actually support read-only
	if (Selection.IsValid())
	{
		if (Selection->GetKey().IsValid())
		{
			const FScopedTransaction Transaction(LOCTEXT("ChangeKey", "Change Key Value"));

			KeyComboButton->SetIsOpen(false);

			OnKeyChanged.ExecuteIfBound(Selection->GetKey());
		}
		else
		{
			// Expand / contract the category, if applicable
			if (Selection->Children.Num() > 0)
			{
				const bool bIsExpanded = KeyTreeView->IsItemExpanded(Selection);
				KeyTreeView->SetItemExpansion(Selection, !bIsExpanded);

				if (SelectInfo == ESelectInfo::OnMouseClick)
				{
					KeyTreeView->ClearSelection();
				}
			}
		}
	}
}

void SKeySelector::GetKeyChildren(FKeyTreeItem InItem, TArray<FKeyTreeItem>& OutChildren)
{
	OutChildren = InItem->Children;
}

TSharedRef<SWidget>	SKeySelector::GetMenuContent()
{
	if (!MenuContent.IsValid())
	{
		// Pre-build the tree view and search box as it is needed as a parameter for the context menu's container.
		SAssignNew(KeyTreeView, SKeyTreeView)
			.TreeItemsSource(&FilteredKeyTreeRoot)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SKeySelector::GenerateKeyTreeRow)
			.OnSelectionChanged(this, &SKeySelector::OnKeySelectionChanged)
			.OnGetChildren(this, &SKeySelector::GetKeyChildren);

		SAssignNew(FilterTextBox, SSearchBox)
			.OnTextChanged(this, &SKeySelector::OnFilterTextChanged)
			.OnTextCommitted(this, &SKeySelector::OnFilterTextCommitted);

		MenuContent = SNew(SListViewSelectorDropdownMenu<FKeyTreeItem>, FilterTextBox, KeyTreeView)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.f, 4.f, 4.f, 4.f)
				[
					FilterTextBox.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.f, 4.f, 4.f, 4.f)
				[
					SNew(SBox)
					.HeightOverride(TreeViewHeight)
					.WidthOverride(TreeViewWidth)
					[
						KeyTreeView.ToSharedRef()
					]
				]
			];


		KeyComboButton->SetMenuContentWidgetToFocus(FilterTextBox);
	}
	else
	{
		// Clear the selection in such a way as to also clear the keyboard selector
		KeyTreeView->SetSelection(NULL, ESelectInfo::OnNavigation);
		KeyTreeView->ClearExpandedItems();
	}

	// Clear the filter text box with each opening
	if (FilterTextBox.IsValid())
	{
		FilterTextBox->SetText(FText::GetEmpty());
	}

	return MenuContent.ToSharedRef();
}

//=======================================================================
// Search Support
void SKeySelector::OnFilterTextChanged(const FText& NewText)
{
	SearchText = NewText;
	FilteredKeyTreeRoot.Empty();

	TArray<FString> Tokens;
	GetSearchTokens(SearchText.ToString(), Tokens);

	GetChildrenMatchingSearch(Tokens, KeyTreeRoot, FilteredKeyTreeRoot);
	KeyTreeView->RequestTreeRefresh();

	// Select the first non-category item
	auto SelectedItems = KeyTreeView->GetSelectedItems();
	if (FilteredKeyTreeRoot.Num() > 0)
	{
		// Categories have children, we don't want to select categories
		if (FilteredKeyTreeRoot[0]->Children.Num() > 0)
		{
			KeyTreeView->SetSelection(FilteredKeyTreeRoot[0]->Children[0], ESelectInfo::OnNavigation);
		}
		else
		{
			KeyTreeView->SetSelection(FilteredKeyTreeRoot[0], ESelectInfo::OnNavigation);
		}
	}
}

void SKeySelector::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		auto SelectedItems = KeyTreeView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			KeyTreeView->SetSelection(SelectedItems[0]);
		}
	}
}

void SKeySelector::GetSearchTokens(const FString& SearchString, TArray<FString>& OutTokens) const
{
	if (SearchString.Contains("\"") && SearchString.ParseIntoArray(OutTokens, TEXT("\""), true) > 0)
	{
		for (auto &TokenIt : OutTokens)
		{
			// we have the token, we don't need the quotes anymore, they'll just confused the comparison later on
			TokenIt = TokenIt.TrimQuotes();
			// We remove the spaces as all later comparison strings will also be de-spaced
			TokenIt = TokenIt.Replace(TEXT(" "), TEXT(""));
		}

		// due to being able to handle multiple quoted blocks like ("Make Epic" "Game Now") we can end up with
		// and empty string between (" ") blocks so this simply removes them
		struct FRemoveMatchingStrings
		{
			bool operator()(const FString& RemovalCandidate) const
			{
				return RemovalCandidate.IsEmpty();
			}
		};
		OutTokens.RemoveAll(FRemoveMatchingStrings());
	}
	else
	{
		// unquoted search equivalent to a match-any-of search
		SearchString.ParseIntoArray(OutTokens, TEXT(" "), true);
	}
}

bool SKeySelector::GetChildrenMatchingSearch(const TArray<FString>& InSearchTokens, const TArray<FKeyTreeItem>& UnfilteredList, TArray<FKeyTreeItem>& OutFilteredList)
{
	bool bReturnVal = false;

	for (auto it = UnfilteredList.CreateConstIterator(); it; ++it)
	{
		FKeyTreeItem Item = *it;
		FKeyTreeItem NewInfo = MakeShareable(new FKeyTreeInfo(Item));
		TArray<FKeyTreeItem> ValidChildren;

		// Have to run GetChildrenMatchingSearch first, so that we can make sure we get valid children for the list!
		if (GetChildrenMatchingSearch(InSearchTokens, Item->Children, ValidChildren)
			|| InSearchTokens.Num() == 0
			|| Item->MatchesSearchTokens(InSearchTokens))
		{
			NewInfo->Children = ValidChildren;
			OutFilteredList.Add(NewInfo);

			KeyTreeView->SetItemExpansion(NewInfo, InSearchTokens.Num() > 0);

			bReturnVal = true;
		}
	}

	return bReturnVal;
}

const FSlateBrush* SKeySelector::GetIconFromKey(FKey Key) const
{
	return FEditorStyle::GetBrush(EKeys::GetMenuCategoryPaletteIcon(Key.GetMenuCategory()));
}

#undef LOCTEXT_NAMESPACE
