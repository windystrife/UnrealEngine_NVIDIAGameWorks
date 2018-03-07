// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "STextPropertyEditableTextBox.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "PackageName.h"
#include "AssetRegistryModule.h"
#include "StringTable.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Serialization/TextReferenceCollector.h"

#define LOCTEXT_NAMESPACE "STextPropertyEditableTextBox"

FText STextPropertyEditableTextBox::MultipleValuesText(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"));

#if USE_STABLE_LOCALIZATION_KEYS

void IEditableTextProperty::StaticStableTextId(UObject* InObject, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey)
{
	UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	StaticStableTextId(Package, InEditAction, InTextSource, InProposedNamespace, InProposedKey, OutStableNamespace, OutStableKey);
}

void IEditableTextProperty::StaticStableTextId(UPackage* InPackage, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey)
{
	bool bPersistKey = false;

	const FString PackageNamespace = TextNamespaceUtil::EnsurePackageNamespace(InPackage);
	if (!PackageNamespace.IsEmpty())
	{
		// Make sure the proposed namespace is using the correct namespace for this package
		OutStableNamespace = TextNamespaceUtil::BuildFullNamespace(InProposedNamespace, PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);

		if (InProposedNamespace.Equals(OutStableNamespace, ESearchCase::CaseSensitive) || InEditAction == ETextPropertyEditAction::EditedNamespace)
		{
			// If the proposal was already using the correct namespace (or we just set the namespace), attempt to persist the proposed key too
			if (!InProposedKey.IsEmpty())
			{
				// If we changed the source text, then we can persist the key if this text is the *only* reference using that ID
				// If we changed the identifier, then we can persist the key only if doing so won't cause an identify conflict
				const FTextReferenceCollector::EComparisonMode ReferenceComparisonMode = InEditAction == ETextPropertyEditAction::EditedSource ? FTextReferenceCollector::EComparisonMode::MatchId : FTextReferenceCollector::EComparisonMode::MismatchSource;
				const int32 RequiredReferenceCount = InEditAction == ETextPropertyEditAction::EditedSource ? 1 : 0;

				int32 ReferenceCount = 0;
				FTextReferenceCollector(InPackage, ReferenceComparisonMode, OutStableNamespace, InProposedKey, InTextSource, ReferenceCount);

				if (ReferenceCount == RequiredReferenceCount)
				{
					bPersistKey = true;
					OutStableKey = InProposedKey;
				}
			}
		}
		else if (InEditAction != ETextPropertyEditAction::EditedNamespace)
		{
			// If our proposed namespace wasn't correct for our package, and we didn't just set it (which doesn't include the package namespace)
			// then we should clear out any user specified part of it
			OutStableNamespace = TextNamespaceUtil::BuildFullNamespace(FString(), PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);
		}
	}

	if (!bPersistKey)
	{
		OutStableKey = FGuid::NewGuid().ToString();
	}
}

#endif // USE_STABLE_LOCALIZATION_KEYS

void STextPropertyEditableStringTableReference::Construct(const FArguments& InArgs, const TSharedRef<IEditableTextProperty>& InEditableTextProperty)
{
	EditableTextProperty = InEditableTextProperty;

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	HorizontalBox->AddSlot()
		[
			SAssignNew(StringTableCombo, SComboBox<TSharedPtr<FAvailableStringTable>>)
			.ComboBoxStyle(InArgs._ComboStyle)
			.OptionsSource(&StringTableComboOptions)
			.OnGenerateWidget(this, &STextPropertyEditableStringTableReference::MakeStringTableComboWidget)
			.OnSelectionChanged(this, &STextPropertyEditableStringTableReference::OnStringTableComboChanged)
			.OnComboBoxOpening(this, &STextPropertyEditableStringTableReference::OnStringTableComboOpening)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &STextPropertyEditableStringTableReference::GetStringTableComboContent)
				.ToolTipText(this, &STextPropertyEditableStringTableReference::GetStringTableComboToolTip)
			]
		];

	HorizontalBox->AddSlot()
		[
			SAssignNew(KeyCombo, SComboBox<TSharedPtr<FString>>)
			.ComboBoxStyle(InArgs._ComboStyle)
			.OptionsSource(&KeyComboOptions)
			.OnGenerateWidget(this, &STextPropertyEditableStringTableReference::MakeKeyComboWidget)
			.OnSelectionChanged(this, &STextPropertyEditableStringTableReference::OnKeyComboChanged)
			.OnComboBoxOpening(this, &STextPropertyEditableStringTableReference::OnKeyComboOpening)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &STextPropertyEditableStringTableReference::GetKeyComboContent)
				.ToolTipText(this, &STextPropertyEditableStringTableReference::GetKeyComboToolTip)
			]
		];

	if (InArgs._AllowUnlink)
	{
		HorizontalBox->AddSlot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(InArgs._ButtonStyle)
				.Text(LOCTEXT("UnlinkStringTable", "Unlink"))
				.IsEnabled(this, &STextPropertyEditableStringTableReference::IsUnlinkEnabled)
				.OnClicked(this, &STextPropertyEditableStringTableReference::OnUnlinkClicked)
			];
	}

	ChildSlot
	[
		HorizontalBox
	];
}

void STextPropertyEditableStringTableReference::GetTableIdAndKey(FName& OutTableId, FString& OutKey) const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts > 0)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		FStringTableRegistry::Get().FindTableIdAndKey(PropertyValue, OutTableId, OutKey);

		// Verify that all texts are using the same string table and key
		for (int32 TextIndex = 1; TextIndex < NumTexts; ++TextIndex)
		{
			FName TmpTableId;
			FString TmpKey;
			if (FStringTableRegistry::Get().FindTableIdAndKey(PropertyValue, TmpTableId, TmpKey) && OutTableId == TmpTableId)
			{
				if (!OutKey.Equals(TmpKey, ESearchCase::CaseSensitive))
				{
					// Not using the same key - clear the key but keep the table and keep enumerating to verify the table on the remaining texts
					OutKey.Reset();
				}
			}
			else
			{
				// Not using a string table, or using a different string table - clear both table ID and key
				OutTableId = FName();
				OutKey.Reset();
				break;
			}
		}
	}
}

void STextPropertyEditableStringTableReference::SetTableIdAndKey(const FName InTableId, const FString& InKey)
{
	const FText TextToSet = FText::FromStringTable(InTableId, InKey);
	if (TextToSet.IsFromStringTable())
	{
		const int32 NumTexts = EditableTextProperty->GetNumTexts();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			EditableTextProperty->SetText(TextIndex, TextToSet);
		}
	}
}

TSharedRef<SWidget> STextPropertyEditableStringTableReference::MakeStringTableComboWidget(TSharedPtr<FAvailableStringTable> InItem)
{
	return SNew(STextBlock)
		.Text(InItem->DisplayName)
		.ToolTipText(FText::FromName(InItem->TableId));
}

void STextPropertyEditableStringTableReference::OnStringTableComboChanged(TSharedPtr<FAvailableStringTable> NewSelection, ESelectInfo::Type SelectInfo)
{
	// If it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct && NewSelection.IsValid())
	{
		// Make sure any selected string table asset is loaded
		FName TableId = NewSelection->TableId;
		IStringTableEngineBridge::RedirectAndLoadStringTableAsset(TableId, EStringTableLoadingPolicy::FindOrFullyLoad);

		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
		if (StringTable.IsValid())
		{
			// Just use the first key when changing the string table
			StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
			{
				SetTableIdAndKey(TableId, InKey);
				return false; // stop enumeration
			});
		}
	}
}

void STextPropertyEditableStringTableReference::OnStringTableComboOpening()
{
	FName CurrentTableId;
	{
		FString TmpKey;
		GetTableIdAndKey(CurrentTableId, TmpKey);
	}

	TSharedPtr<FAvailableStringTable> SelectedStringTableComboEntry;
	StringTableComboOptions.Reset();

	// Process assets first (as they may currently be unloaded)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		
		TArray<FAssetData> StringTableAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UStringTable::StaticClass()->GetFName(), StringTableAssets);
		
		for (const FAssetData& StringTableAsset : StringTableAssets)
		{
			TSharedRef<FAvailableStringTable> AvailableStringTableEntry = MakeShared<FAvailableStringTable>();
			AvailableStringTableEntry->TableId = StringTableAsset.ObjectPath;
			AvailableStringTableEntry->DisplayName = FText::FromName(StringTableAsset.AssetName);
			if (StringTableAsset.ObjectPath == CurrentTableId)
			{
				SelectedStringTableComboEntry = AvailableStringTableEntry;
			}
			StringTableComboOptions.Add(AvailableStringTableEntry);
		}
	}

	// Process the remaining non-asset string tables now
	FStringTableRegistry::Get().EnumerateStringTables([&](const FName& InTableId, const FStringTableConstRef& InStringTable) -> bool
	{
		const bool bAlreadyAdded = StringTableComboOptions.ContainsByPredicate([InTableId](const TSharedPtr<FAvailableStringTable>& InAvailableStringTable)
		{
			return InAvailableStringTable->TableId == InTableId;
		});

		if (!bAlreadyAdded)
		{
			TSharedRef<FAvailableStringTable> AvailableStringTableEntry = MakeShared<FAvailableStringTable>();
			AvailableStringTableEntry->TableId = InTableId;
			AvailableStringTableEntry->DisplayName = FText::FromName(InTableId);
			if (InTableId == CurrentTableId)
			{
				SelectedStringTableComboEntry = AvailableStringTableEntry;
			}
			StringTableComboOptions.Add(AvailableStringTableEntry);
		}

		return true; // continue enumeration
	});

	StringTableComboOptions.Sort([](const TSharedPtr<FAvailableStringTable>& InOne, const TSharedPtr<FAvailableStringTable>& InTwo)
	{
		return InOne->DisplayName.ToString() < InTwo->DisplayName.ToString();
	});

	if (SelectedStringTableComboEntry.IsValid())
	{
		StringTableCombo->SetSelectedItem(SelectedStringTableComboEntry);
	}
	else
	{
		StringTableCombo->ClearSelection();
	}
}

FText STextPropertyEditableStringTableReference::GetStringTableComboContent() const
{
	FName CurrentTableId;
	{
		FString TmpKey;
		GetTableIdAndKey(CurrentTableId, TmpKey);
	}

	return FText::FromString(FPackageName::GetLongPackageAssetName(CurrentTableId.ToString()));
}

FText STextPropertyEditableStringTableReference::GetStringTableComboToolTip() const
{
	FName CurrentTableId;
	{
		FString TmpKey;
		GetTableIdAndKey(CurrentTableId, TmpKey);
	}

	return FText::FromName(CurrentTableId);
}

TSharedRef<SWidget> STextPropertyEditableStringTableReference::MakeKeyComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.ToolTipText(FText::FromString(*InItem));
}

void STextPropertyEditableStringTableReference::OnKeyComboChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	// If it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct && NewSelection.IsValid())
	{
		FName CurrentTableId;
		{
			FString TmpKey;
			GetTableIdAndKey(CurrentTableId, TmpKey);
		}

		SetTableIdAndKey(CurrentTableId, *NewSelection);
	}
}

void STextPropertyEditableStringTableReference::OnKeyComboOpening()
{
	FName CurrentTableId;
	FString CurrentKey;
	GetTableIdAndKey(CurrentTableId, CurrentKey);

	TSharedPtr<FString> SelectedKeyComboEntry;
	KeyComboOptions.Reset();

	if (!CurrentTableId.IsNone())
	{
		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(CurrentTableId);
		if (StringTable.IsValid())
		{
			StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
			{
				TSharedRef<FString> KeyComboEntry = MakeShared<FString>(InKey);
				if (InKey.Equals(CurrentKey, ESearchCase::CaseSensitive))
				{
					SelectedKeyComboEntry = KeyComboEntry;
				}
				KeyComboOptions.Add(KeyComboEntry);
				return true; // continue enumeration
			});
		}
	}

	KeyComboOptions.Sort([](const TSharedPtr<FString>& InOne, const TSharedPtr<FString>& InTwo)
	{
		return *InOne < *InTwo;
	});

	if (SelectedKeyComboEntry.IsValid())
	{
		KeyCombo->SetSelectedItem(SelectedKeyComboEntry);
	}
	else
	{
		KeyCombo->ClearSelection();
	}
}

FText STextPropertyEditableStringTableReference::GetKeyComboContent() const
{
	FString CurrentKey;
	{
		FName TmpTableId;
		GetTableIdAndKey(TmpTableId, CurrentKey);
	}

	return FText::FromString(MoveTemp(CurrentKey));
}

FText STextPropertyEditableStringTableReference::GetKeyComboToolTip() const
{
	return GetKeyComboContent();
}

bool STextPropertyEditableStringTableReference::IsUnlinkEnabled() const
{
	bool bEnabled = false;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
	{
		const FText CurrentText = EditableTextProperty->GetText(TextIndex);
		if (CurrentText.IsFromStringTable())
		{
			bEnabled = true;
			break;
		}
	}

	return bEnabled;
}

FReply STextPropertyEditableStringTableReference::OnUnlinkClicked()
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
	{
		const FText CurrentText = EditableTextProperty->GetText(TextIndex);
		if (CurrentText.IsFromStringTable())
		{
			EditableTextProperty->SetText(TextIndex, FText::GetEmpty());
		}
	}

	return FReply::Handled();
}

void STextPropertyEditableTextBox::Construct(const FArguments& InArgs, const TSharedRef<IEditableTextProperty>& InEditableTextProperty)
{
	EditableTextProperty = InEditableTextProperty;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	const bool bIsPassword = EditableTextProperty->IsPassword();
	bIsMultiLine = EditableTextProperty->IsMultiLineText();
	if (bIsMultiLine)
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredWidth)
				.MaxDesiredHeight(InArgs._MaxDesiredHeight)
				[
					SAssignNew(MultiLineWidget, SMultiLineEditableTextBox)
					.Text(this, &STextPropertyEditableTextBox::GetTextValue)
					.ToolTipText(this, &STextPropertyEditableTextBox::GetToolTipText)
					.Style(InArgs._Style)
					.Font(InArgs._Font)
					.ForegroundColor(InArgs._ForegroundColor)
					.SelectAllTextWhenFocused(false)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextChanged(this, &STextPropertyEditableTextBox::OnTextChanged)
					.OnTextCommitted(this, &STextPropertyEditableTextBox::OnTextCommitted)
					.SelectAllTextOnCommit(false)
					.IsReadOnly(this, &STextPropertyEditableTextBox::IsSourceTextReadOnly)
					.AutoWrapText(InArgs._AutoWrapText)
					.WrapTextAt(InArgs._WrapTextAt)
					.ModiferKeyForNewLine(EModifierKey::Shift)
					.IsPassword(bIsPassword)
				]
			]
		];

		PrimaryWidget = MultiLineWidget;
	}
	else
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredWidth)
				[
					SAssignNew(SingleLineWidget, SEditableTextBox)
					.Text(this, &STextPropertyEditableTextBox::GetTextValue)
					.ToolTipText(this, &STextPropertyEditableTextBox::GetToolTipText)
					.Style(InArgs._Style)
					.Font(InArgs._Font)
					.ForegroundColor(InArgs._ForegroundColor)
					.SelectAllTextWhenFocused(true)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextChanged(this, &STextPropertyEditableTextBox::OnTextChanged)
					.OnTextCommitted(this, &STextPropertyEditableTextBox::OnTextCommitted)
					.SelectAllTextOnCommit(true)
					.IsReadOnly(this, &STextPropertyEditableTextBox::IsSourceTextReadOnly)
					.IsPassword(bIsPassword)
				]
			]
		];

		PrimaryWidget = SingleLineWidget;
	}

	HorizontalBox->AddSlot()
		.AutoWidth()
		[
			SNew(SComboButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(4, 0))
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(LOCTEXT("AdvancedTextSettingsComboToolTip", "Edit advanced text settings."))
			.MenuContent()
			[
				SNew(SBox)
				.WidthOverride(340)
				.Padding(4)
				[
					SNew(SGridPanel)
					.FillColumn(1, 1.0f)

					// Inline Text
					+SGridPanel::Slot(0, 0)
					.ColumnSpan(2)
					.Padding(2)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "LargeText")
						.Text(LOCTEXT("TextInlineTextLabel", "Inline Text"))
					]

					// Localizable?
					+SGridPanel::Slot(0, 1)
					.Padding(2)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextLocalizableLabel", "Localizable:"))
					]
					+SGridPanel::Slot(1, 1)
					.Padding(2)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FMargin(0, 0, 4, 0))

							+SUniformGridPanel::Slot(0, 0)
							[
								SNew(SCheckBox)
								.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
								.ToolTipText(LOCTEXT("TextLocalizableToggleYesToolTip", "Assign this text a key and allow it to be gathered for localization."))
								.Padding(FMargin(4, 2))
								.HAlign(HAlign_Center)
								.IsEnabled(this, &STextPropertyEditableTextBox::IsCultureInvariantFlagEnabled)
								.IsChecked(this, &STextPropertyEditableTextBox::GetLocalizableCheckState, true/*bActiveState*/)
								.OnCheckStateChanged(this, &STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged, true/*bActiveState*/)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("TextLocalizableToggleYes", "Yes"))
								]
							]

							+SUniformGridPanel::Slot(1, 0)
							[
								SNew(SCheckBox)
								.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
								.ToolTipText(LOCTEXT("TextLocalizableToggleNoToolTip", "Mark this text as 'culture invariant' to prevent it being gathered for localization."))
								.Padding(FMargin(4, 2))
								.HAlign(HAlign_Center)
								.IsEnabled(this, &STextPropertyEditableTextBox::IsCultureInvariantFlagEnabled)
								.IsChecked(this, &STextPropertyEditableTextBox::GetLocalizableCheckState, false/*bActiveState*/)
								.OnCheckStateChanged(this, &STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged, false/*bActiveState*/)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("TextLocalizableToggleNo", "No"))
								]
							]
						]
					]

#if USE_STABLE_LOCALIZATION_KEYS
					// Package
					+SGridPanel::Slot(0, 2)
					.Padding(2)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextPackageLabel", "Package:"))
					]
					+SGridPanel::Slot(1, 2)
					.Padding(2)
					[
						SNew(SEditableTextBox)
						.Text(this, &STextPropertyEditableTextBox::GetPackageValue)
						.IsReadOnly(true)
					]
#endif // USE_STABLE_LOCALIZATION_KEYS

					// Namespace
					+SGridPanel::Slot(0, 3)
					.Padding(2)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextNamespaceLabel", "Namespace:"))
					]
					+SGridPanel::Slot(1, 3)
					.Padding(2)
					[
						SAssignNew(NamespaceEditableTextBox, SEditableTextBox)
						.Text(this, &STextPropertyEditableTextBox::GetNamespaceValue)
						.SelectAllTextWhenFocused(true)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextChanged(this, &STextPropertyEditableTextBox::OnNamespaceChanged)
						.OnTextCommitted(this, &STextPropertyEditableTextBox::OnNamespaceCommitted)
						.SelectAllTextOnCommit(true)
						.IsReadOnly(this, &STextPropertyEditableTextBox::IsIdentityReadOnly)
					]

					// Key
					+SGridPanel::Slot(0, 4)
					.Padding(2)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextKeyLabel", "Key:"))
					]
					+SGridPanel::Slot(1, 4)
					.Padding(2)
					[
						SAssignNew(KeyEditableTextBox, SEditableTextBox)
						.Text(this, &STextPropertyEditableTextBox::GetKeyValue)
#if USE_STABLE_LOCALIZATION_KEYS
						.SelectAllTextWhenFocused(true)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextChanged(this, &STextPropertyEditableTextBox::OnKeyChanged)
						.OnTextCommitted(this, &STextPropertyEditableTextBox::OnKeyCommitted)
						.SelectAllTextOnCommit(true)
						.IsReadOnly(this, &STextPropertyEditableTextBox::IsIdentityReadOnly)
#else	// USE_STABLE_LOCALIZATION_KEYS
						.IsReadOnly(true)
#endif	// USE_STABLE_LOCALIZATION_KEYS
					]

					// Referenced Text
					+SGridPanel::Slot(0, 5)
					.ColumnSpan(2)
					.Padding(2)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "LargeText")
						.Text(LOCTEXT("TextReferencedTextLabel", "Referenced Text"))
					]

					// String Table
					+SGridPanel::Slot(0, 6)
					.Padding(2)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextStringTableLabel", "String Table:"))
					]
					+SGridPanel::Slot(1, 6)
					.Padding(2)
					[
						SNew(STextPropertyEditableStringTableReference, InEditableTextProperty)
						.AllowUnlink(true)
						.IsEnabled(this, &STextPropertyEditableTextBox::CanEdit)
					]
				]
			]
		];

	HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("Icons.Warning"))
			.Visibility(this, &STextPropertyEditableTextBox::GetTextWarningImageVisibility)
			.ToolTipText(LOCTEXT("TextNotLocalizedWarningToolTip", "This text is marked as 'culture invariant' and won't be gathered for localization.\nYou can change this by editing the advanced text settings."))
		];

	SetEnabled(TAttribute<bool>(this, &STextPropertyEditableTextBox::CanEdit));
}

void STextPropertyEditableTextBox::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	if (bIsMultiLine)
	{
		OutMinDesiredWidth = 250.0f;
	}
	else
	{
		OutMinDesiredWidth = 125.0f;
	}

	OutMaxDesiredWidth = 600.0f;
}

bool STextPropertyEditableTextBox::SupportsKeyboardFocus() const
{
	return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
}

FReply STextPropertyEditableTextBox::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	// Forward keyboard focus to our editable text widget
	return FReply::Handled().SetUserFocus(PrimaryWidget.ToSharedRef(), InFocusEvent.GetCause());
}

void STextPropertyEditableTextBox::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const float CurrentHeight = AllottedGeometry.GetLocalSize().Y;
	if (bIsMultiLine && PreviousHeight.IsSet() && PreviousHeight.GetValue() != CurrentHeight)
	{
		EditableTextProperty->RequestRefresh();
	}
	PreviousHeight = CurrentHeight;
}

bool STextPropertyEditableTextBox::CanEdit() const
{
	const bool bIsReadOnly = FTextLocalizationManager::Get().IsLocalizationLocked() || EditableTextProperty->IsReadOnly();
	return !bIsReadOnly;
}

bool STextPropertyEditableTextBox::IsCultureInvariantFlagEnabled() const
{
	return !IsSourceTextReadOnly();
}

bool STextPropertyEditableTextBox::IsSourceTextReadOnly() const
{
	if (!CanEdit())
	{
		return true;
	}

	// We can't edit the source string of string table references
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);
		if (TextValue.IsFromStringTable())
		{
			return true;
		}
	}

	return false;
}

bool STextPropertyEditableTextBox::IsIdentityReadOnly() const
{
	if (!CanEdit())
	{
		return true;
	}

	// We can't edit the identity of texts that don't gather for localization
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);
		if (!TextValue.ShouldGatherForLocalization())
		{
			return true;
		}
	}

	return false;
}

FText STextPropertyEditableTextBox::GetToolTipText() const
{
	FText LocalizedTextToolTip;
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);

		if (TextValue.IsFromStringTable())
		{
			FName TableId;
			FString Key;
			FStringTableRegistry::Get().FindTableIdAndKey(TextValue, TableId, Key);

			LocalizedTextToolTip = FText::Format(
				LOCTEXT("StringTableTextToolTipFmt", "--- String Table Reference ---\nTable ID: {0}\nKey: {1}"), 
				FText::FromName(TableId), FText::FromString(Key)
				);
		}
		else
		{
			bool bIsLocalized = false;
			FString Namespace;
			FString Key;
			const FString* SourceString = FTextInspector::GetSourceString(TextValue);

			if (SourceString && TextValue.ShouldGatherForLocalization())
			{
				bIsLocalized = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(FTextInspector::GetSharedDisplayString(TextValue), Namespace, Key);
			}

			if (bIsLocalized)
			{
				const FString PackageNamespace = TextNamespaceUtil::ExtractPackageNamespace(Namespace);
				const FString TextNamespace = TextNamespaceUtil::StripPackageNamespace(Namespace);

				LocalizedTextToolTip = FText::Format(
					LOCTEXT("LocalizedTextToolTipFmt", "--- Localized Text ---\nPackage: {0}\nNamespace: {1}\nKey: {2}\nSource: {3}"), 
					FText::FromString(PackageNamespace), FText::FromString(TextNamespace), FText::FromString(Key), FText::FromString(*SourceString)
					);
			}
		}
	}
	
	FText BaseToolTipText = EditableTextProperty->GetToolTipText();
	if (FTextLocalizationManager::Get().IsLocalizationLocked())
	{
		const FText LockdownToolTip = FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled() 
			? LOCTEXT("LockdownToolTip_Preview", "Localization is locked down due to the active game localization preview")
			: LOCTEXT("LockdownToolTip_Other", "Localization is locked down");
		BaseToolTipText = BaseToolTipText.IsEmptyOrWhitespace() ? LockdownToolTip : FText::Format(LOCTEXT("ToolTipLockdownFmt", "!!! {0} !!!\n\n{1}"), LockdownToolTip, BaseToolTipText);
	}

	if (LocalizedTextToolTip.IsEmptyOrWhitespace())
	{
		return BaseToolTipText;
	}
	if (BaseToolTipText.IsEmptyOrWhitespace())
	{
		return LocalizedTextToolTip;
	}

	return FText::Format(LOCTEXT("ToolTipCompleteFmt", "{0}\n\n{1}"), BaseToolTipText, LocalizedTextToolTip);
}

FText STextPropertyEditableTextBox::GetTextValue() const
{
	FText TextValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		TextValue = EditableTextProperty->GetText(0);
	}
	else if (NumTexts > 1)
	{
		TextValue = MultipleValuesText;
	}

	return TextValue;
}

void STextPropertyEditableTextBox::OnTextChanged(const FText& NewText)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	FText TextErrorMsg;

	// Don't validate the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString().Equals(MultipleValuesText.ToString(), ESearchCase::CaseSensitive)))
	{
		EditableTextProperty->IsValidText(NewText, TextErrorMsg);
	}

	// Update or clear the error message
	SetTextError(TextErrorMsg);
}

void STextPropertyEditableTextBox::OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || !NewText.ToString().Equals(MultipleValuesText.ToString(), ESearchCase::CaseSensitive)))
	{
		FText TextErrorMsg;
		if (EditableTextProperty->IsValidText(NewText, TextErrorMsg))
		{
			// Valid text; clear any error
			SetTextError(FText::GetEmpty());
		}
		else
		{
			// Invalid text; set the error and prevent the new text from being set
			SetTextError(TextErrorMsg);
			return;
		}

		const FString& SourceString = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new text is different
			if (PropertyValue.ToString().Equals(NewText.ToString(), ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Is the new text is empty, just use the empty instance
			if (NewText.IsEmpty())
			{
				EditableTextProperty->SetText(TextIndex, FText::GetEmpty());
				continue;
			}

			// Maintain culture invariance when editing the text
			if (PropertyValue.IsCultureInvariant())
			{
				EditableTextProperty->SetText(TextIndex, FText::AsCultureInvariant(NewText.ToString()));
				continue;
			}

			FString NewNamespace;
			FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
			{
				// Get the stable namespace and key that we should use for this property
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->GetStableTextId(
					TextIndex, 
					IEditableTextProperty::ETextPropertyEditAction::EditedSource, 
					TextSource ? *TextSource : FString(), 
					FTextInspector::GetNamespace(PropertyValue).Get(FString()), 
					FTextInspector::GetKey(PropertyValue).Get(FString()), 
					NewNamespace, 
					NewKey
					);
			}
#else	// USE_STABLE_LOCALIZATION_KEYS
			{
				// We want to preserve the namespace set on this property if it's *not* the default value
				if (!EditableTextProperty->IsDefaultValue())
				{
					// Some properties report that they're not the default, but still haven't been set from a property, so we also check the property key to see if it's a valid GUID before allowing the namespace to persist
					FGuid TmpGuid;
					if (FGuid::Parse(FTextInspector::GetKey(PropertyValue).Get(FString()), TmpGuid))
					{
						NewNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
					}
				}

				NewKey = FGuid::NewGuid().ToString();
			}
#endif	// USE_STABLE_LOCALIZATION_KEYS

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, NewText));
		}
	}
}

void STextPropertyEditableTextBox::SetTextError(const FText& InErrorMsg)
{
	if (MultiLineWidget.IsValid())
	{
		MultiLineWidget->SetError(InErrorMsg);
	}

	if (SingleLineWidget.IsValid())
	{
		SingleLineWidget->SetError(InErrorMsg);
	}
}

FText STextPropertyEditableTextBox::GetNamespaceValue() const
{
	FText NamespaceValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
		if (FoundNamespace.IsSet())
		{
			NamespaceValue = FText::FromString(TextNamespaceUtil::StripPackageNamespace(FoundNamespace.GetValue()));
		}
	}
	else if (NumTexts > 1)
	{
		NamespaceValue = MultipleValuesText;
	}

	return NamespaceValue;
}

void STextPropertyEditableTextBox::OnNamespaceChanged(const FText& NewText)
{
	FText ErrorMessage;
	const FText ErrorCtx = LOCTEXT("TextNamespaceErrorCtx", "Namespace");
	IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

	NamespaceEditableTextBox->SetError(ErrorMessage);
}

void STextPropertyEditableTextBox::OnNamespaceCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (!IsValidIdentity(NewText))
	{
		return;
	}

	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& TextNamespace = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new namespace is different - we want to keep the keys stable where possible
			const FString CurrentTextNamespace = TextNamespaceUtil::StripPackageNamespace(FTextInspector::GetNamespace(PropertyValue).Get(FString()));
			if (CurrentTextNamespace.Equals(TextNamespace, ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Get the stable namespace and key that we should use for this property
			FString NewNamespace;
			FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
			{
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->GetStableTextId(
					TextIndex, 
					IEditableTextProperty::ETextPropertyEditAction::EditedNamespace, 
					TextSource ? *TextSource : FString(), 
					TextNamespace, 
					FTextInspector::GetKey(PropertyValue).Get(FString()), 
					NewNamespace, 
					NewKey
					);
			}
#else	// USE_STABLE_LOCALIZATION_KEYS
			{
				NewNamespace = TextNamespace;

				// If the current key is a GUID, then we can preserve that when setting the new namespace
				NewKey = FTextInspector::GetKey(PropertyValue).Get(FString());
				{
					FGuid TmpGuid;
					if (!FGuid::Parse(NewKey, TmpGuid))
					{
						NewKey = FGuid::NewGuid().ToString();
					}
				}
			}
#endif	// USE_STABLE_LOCALIZATION_KEYS

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
		}
	}
}

FText STextPropertyEditableTextBox::GetKeyValue() const
{
	FText KeyValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundKey = FTextInspector::GetKey(PropertyValue);
		if (FoundKey.IsSet())
		{
			KeyValue = FText::FromString(FoundKey.GetValue());
		}
	}
	else if (NumTexts > 1)
	{
		KeyValue = MultipleValuesText;
	}

	return KeyValue;
}

#if USE_STABLE_LOCALIZATION_KEYS

void STextPropertyEditableTextBox::OnKeyChanged(const FText& NewText)
{
	FText ErrorMessage;
	const FText ErrorCtx = LOCTEXT("TextKeyErrorCtx", "Key");
	const bool bIsValidName = IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

	if (NewText.IsEmptyOrWhitespace())
	{
		ErrorMessage = LOCTEXT("TextKeyEmptyErrorMsg", "Key cannot be empty so a new key will be assigned");
	}
	else if (bIsValidName)
	{
		// Valid name, so check it won't cause an identity conflict (only test if we have a single text selected to avoid confusion)
		const int32 NumTexts = EditableTextProperty->GetNumTexts();
		if (NumTexts == 1)
		{
			const FText PropertyValue = EditableTextProperty->GetText(0);

			const FString TextNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
			const FString TextKey = NewText.ToString();

			// Get the stable namespace and key that we should use for this property
			// If it comes back with the same namespace but a different key then it means there was an identity conflict
			FString NewNamespace;
			FString NewKey;
			const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
			EditableTextProperty->GetStableTextId(
				0,
				IEditableTextProperty::ETextPropertyEditAction::EditedKey,
				TextSource ? *TextSource : FString(),
				TextNamespace,
				TextKey,
				NewNamespace,
				NewKey
				);

			if (TextNamespace.Equals(NewNamespace, ESearchCase::CaseSensitive) && !TextKey.Equals(NewKey, ESearchCase::CaseSensitive))
			{
				ErrorMessage = LOCTEXT("TextKeyConflictErrorMsg", "Identity (namespace & key) is being used by a different text within this package so a new key will be assigned");
			}
		}
	}

	KeyEditableTextBox->SetError(ErrorMessage);
}

void STextPropertyEditableTextBox::OnKeyCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (!IsValidIdentity(NewText))
	{
		return;
	}

	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& TextKey = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new key is different - we want to keep the keys stable where possible
			const FString CurrentTextKey = FTextInspector::GetKey(PropertyValue).Get(FString());
			if (CurrentTextKey.Equals(TextKey, ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Get the stable namespace and key that we should use for this property
			FString NewNamespace;
			FString NewKey;
			const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
			EditableTextProperty->GetStableTextId(
				TextIndex, 
				IEditableTextProperty::ETextPropertyEditAction::EditedKey, 
				TextSource ? *TextSource : FString(), 
				FTextInspector::GetNamespace(PropertyValue).Get(FString()), 
				TextKey, 
				NewNamespace, 
				NewKey
				);

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
		}
	}
}

FText STextPropertyEditableTextBox::GetPackageValue() const
{
	FText PackageValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
		if (FoundNamespace.IsSet())
		{
			PackageValue = FText::FromString(TextNamespaceUtil::ExtractPackageNamespace(FoundNamespace.GetValue()));
		}
	}
	else if (NumTexts > 1)
	{
		PackageValue = MultipleValuesText;
	}

	return PackageValue;
}

#endif // USE_STABLE_LOCALIZATION_KEYS

ECheckBoxState STextPropertyEditableTextBox::GetLocalizableCheckState(bool bActiveState) const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);

		const bool bIsLocalized = !PropertyValue.IsCultureInvariant();
		return bIsLocalized == bActiveState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged(ECheckBoxState InCheckboxState, bool bActiveState)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	if (bActiveState)
	{
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Assign a key to any currently culture invariant texts
			if (PropertyValue.IsCultureInvariant())
			{
				// Get the stable namespace and key that we should use for this property
				FString NewNamespace;
				FString NewKey;
				EditableTextProperty->GetStableTextId(
					TextIndex,
					IEditableTextProperty::ETextPropertyEditAction::EditedKey,
					PropertyValue.ToString(),
					FString(),
					FString(),
					NewNamespace,
					NewKey
					);

				EditableTextProperty->SetText(TextIndex, FInternationalization::Get().ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*PropertyValue.ToString(), *NewNamespace, *NewKey));
			}
		}
	}
	else
	{
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Clear the identity from any non-culture invariant texts
			if (!PropertyValue.IsCultureInvariant())
			{
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->SetText(TextIndex, FText::AsCultureInvariant(PropertyValue.ToString()));
			}
		}
	}
}

EVisibility STextPropertyEditableTextBox::GetTextWarningImageVisibility() const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		return PropertyValue.IsCultureInvariant() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

bool STextPropertyEditableTextBox::IsValidIdentity(const FText& InIdentity, FText* OutReason, const FText* InErrorCtx) const
{
	const FString InvalidIdentityChars = FString::Printf(TEXT("%s%c%c"), INVALID_NAME_CHARACTERS, TextNamespaceUtil::PackageNamespaceStartMarker, TextNamespaceUtil::PackageNamespaceEndMarker);
	return FName::IsValidXName(InIdentity.ToString(), InvalidIdentityChars, OutReason, InErrorCtx);
}

#undef LOCTEXT_NAMESPACE
