// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorCombo.h"
#include "IDocumentation.h"

#include "PropertyEditorHelpers.h"
#include "UserInterface/PropertyEditor/SPropertyComboBox.h"


#define LOCTEXT_NAMESPACE "PropertyEditor"

static int32 FindEnumValueIndex(UEnum* Enum, FString const& ValueString)
{
	int32 Index = INDEX_NONE;
	for(int32 ValIndex = 0; ValIndex < Enum->NumEnums(); ++ValIndex)
	{
		FString const EnumName    = Enum->GetNameStringByIndex(ValIndex);
		FString const DisplayName = Enum->GetDisplayNameTextByIndex(ValIndex).ToString();

		if (DisplayName.Len() > 0)
		{
			if (DisplayName == ValueString)
			{
				Index = ValIndex;
				break;
			}
		}
		
		if (EnumName == ValueString)
		{
			Index = ValIndex;
			break;
		}
	}
	return Index;
}

void SPropertyEditorCombo::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 400.0f;
}

bool SPropertyEditorCombo::Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();
	int32 ArrayIndex = PropertyNode->GetArrayIndex();

	if(	((Property->IsA(UByteProperty::StaticClass()) && Cast<const UByteProperty>(Property)->Enum)
		||	Property->IsA(UEnumProperty::StaticClass())
		||	(Property->IsA(UStrProperty::StaticClass()) && Property->HasMetaData(TEXT("Enum")))
		)
		&&	( ( ArrayIndex == -1 && Property->ArrayDim == 1 ) || ( ArrayIndex > -1 && Property->ArrayDim > 0 ) ) )
	{
		return true;
	}

	return false;
}

void SPropertyEditorCombo::Construct( const FArguments& InArgs, const TSharedPtr< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	PropertyHandle = InArgs._PropertyHandle;

	TAttribute<FText> TooltipAttribute;

	if (PropertyEditor.IsValid())
	{
		PropertyHandle = PropertyEditor->GetPropertyHandle();
		TooltipAttribute = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(PropertyEditor.ToSharedRef(), &FPropertyEditor::GetValueAsText));
	}

	TArray<TSharedPtr<FString>> ComboItems;
	TArray<bool> Restrictions;
	TArray<TSharedPtr<SToolTip>> RichToolTips;

	OnGetComboBoxStrings = InArgs._OnGetComboBoxStrings;
	OnGetComboBoxValue = InArgs._OnGetComboBoxValue;
	OnComboBoxValueSelected = InArgs._OnComboBoxValueSelected;

	GenerateComboBoxStrings(ComboItems, RichToolTips, Restrictions);

	SAssignNew(ComboBox, SPropertyComboBox)
		.Font( InArgs._Font )
		.RichToolTipList( RichToolTips )
		.ComboItemList( ComboItems )
		.RestrictedList( Restrictions )
		.OnSelectionChanged( this, &SPropertyEditorCombo::OnComboSelectionChanged )
		.OnComboBoxOpening( this, &SPropertyEditorCombo::OnComboOpening )
		.VisibleText( this, &SPropertyEditorCombo::GetDisplayValueAsString )
		.ToolTipText( TooltipAttribute );

	ChildSlot
	[
		ComboBox.ToSharedRef()
	];

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorCombo::CanEdit ) );
}

FString SPropertyEditorCombo::GetDisplayValueAsString() const
{
	if (OnGetComboBoxValue.IsBound())
	{
		return OnGetComboBoxValue.Execute();
	}
	else if (PropertyEditor.IsValid())
	{
		return (bUsesAlternateDisplayValues) ? PropertyEditor->GetValueAsDisplayString() : PropertyEditor->GetValueAsString();
	}
	else
	{
		FString ValueString;

		if (bUsesAlternateDisplayValues)
		{
			PropertyHandle->GetValueAsDisplayString(ValueString);
		}
		else
		{
			PropertyHandle->GetValueAsFormattedString(ValueString);
		}
		return ValueString;
	}
}

void SPropertyEditorCombo::GenerateComboBoxStrings( TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& RichToolTips, TArray<bool>& OutRestrictedItems )
{
	if (OnGetComboBoxStrings.IsBound())
	{
		OnGetComboBoxStrings.Execute(OutComboBoxStrings, RichToolTips, OutRestrictedItems);
		return;
	}

	TArray<FText> BasicTooltips;
	bUsesAlternateDisplayValues = PropertyHandle->GeneratePossibleValues(OutComboBoxStrings, BasicTooltips, OutRestrictedItems);

	// For enums, look for rich tooltip information
	if(PropertyHandle.IsValid())
	{
		if(const UProperty* Property = PropertyHandle->GetProperty())
		{
			UEnum* Enum = nullptr;

			if(const UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
			{
				Enum = ByteProperty->Enum;
			}
			else if(const UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
			}

			if(Enum)
			{
				TArray<FName> AllowedPropertyEnums = PropertyEditorHelpers::GetValidEnumsFromPropertyOverride(Property, Enum);

				// Get enum doc link (not just GetDocumentationLink as that is the documentation for the struct we're in, not the enum documentation)
				FString DocLink = PropertyEditorHelpers::GetEnumDocumentationLink(Property);

				for(int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; ++EnumIdx)
				{
					FString Excerpt = Enum->GetNameStringByIndex(EnumIdx);

					bool bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIdx) || Enum->HasMetaData(TEXT("Spacer"), EnumIdx);
					if(!bShouldBeHidden && AllowedPropertyEnums.Num() != 0)
					{
						bShouldBeHidden = AllowedPropertyEnums.Find(Enum->GetNameByIndex(EnumIdx)) == INDEX_NONE;
					}

					if (!bShouldBeHidden)
					{
						bShouldBeHidden = PropertyHandle->IsHidden(Excerpt);
					}
				
					if(!bShouldBeHidden)
					{
						RichToolTips.Add(IDocumentation::Get()->CreateToolTip(MoveTemp(BasicTooltips[EnumIdx]), nullptr, DocLink, MoveTemp(Excerpt)));
					}
				}
			}
		}
	}
}

void SPropertyEditorCombo::OnComboSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo )
{
	if ( NewValue.IsValid() )
	{
		SendToObjects( *NewValue );
	}
}

void SPropertyEditorCombo::OnComboOpening()
{
	TArray<TSharedPtr<FString>> ComboItems;
	TArray<TSharedPtr<SToolTip>> RichToolTips;
	TArray<bool> Restrictions;
	GenerateComboBoxStrings(ComboItems, RichToolTips, Restrictions);

	ComboBox->SetItemList(ComboItems, RichToolTips, Restrictions);

	// try and re-sync the selection in the combo list in case it was changed since Construct was called
	// this would fail if the displayed value doesn't match the equivalent value in the combo list
	FString CurrentDisplayValue = GetDisplayValueAsString();
	ComboBox->SetSelectedItem(CurrentDisplayValue);
}

void SPropertyEditorCombo::SendToObjects( const FString& NewValue )
{
	FString Value = NewValue;
	if (OnComboBoxValueSelected.IsBound())
	{
		OnComboBoxValueSelected.Execute(NewValue);
	}
	else if (PropertyHandle.IsValid())
	{
		UProperty* Property = PropertyHandle->GetProperty();

		if (bUsesAlternateDisplayValues && !Property->IsA(UStrProperty::StaticClass()))
		{
			// currently only enum properties can use alternate display values; this 
			// might change, so assert here so that if support is expanded to other 
			// property types without updating this block of code, we'll catch it quickly
			UEnum* Enum = nullptr;
			if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
			{
				Enum = ByteProperty->Enum;
			}
			else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
			}
			check(Enum != nullptr);

			const int32 Index = FindEnumValueIndex(Enum, NewValue);
			check(Index != INDEX_NONE);

			Value = Enum->GetNameStringByIndex(Index);

			FText ToolTipValue = Enum->GetToolTipTextByIndex(Index);
			FText ToolTipText = Property->GetToolTipText();
			if (!ToolTipValue.IsEmpty())
			{
				ToolTipText = FText::Format(FText::FromString(TEXT("{0}\n\n{1}")), ToolTipText, ToolTipValue);
			}
			SetToolTipText(ToolTipText);
		}

		PropertyHandle->SetValueFromFormattedString(Value);
	}
}

bool SPropertyEditorCombo::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}

#undef LOCTEXT_NAMESPACE
