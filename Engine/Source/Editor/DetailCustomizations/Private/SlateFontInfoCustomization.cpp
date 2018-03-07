// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/SlateFontInfoCustomization.h"
#include "Engine/Font.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "AssetData.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "SlateFontInfo"

TSharedRef<IPropertyTypeCustomization> FSlateFontInfoStructCustomization::MakeInstance() 
{
	return MakeShareable(new FSlateFontInfoStructCustomization());
}

void FSlateFontInfoStructCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	static const FName FontObjectPropertyName = GET_MEMBER_NAME_CHECKED(FSlateFontInfo, FontObject);
	static const FName TypefaceFontNamePropertyName = GET_MEMBER_NAME_CHECKED(FSlateFontInfo, TypefaceFontName);
	static const FName SizePropertyName = GET_MEMBER_NAME_CHECKED(FSlateFontInfo, Size);

	FontObjectProperty = StructPropertyHandle->GetChildHandle(FontObjectPropertyName);
	check(FontObjectProperty.IsValid());

	TypefaceFontNameProperty = StructPropertyHandle->GetChildHandle(TypefaceFontNamePropertyName);
	check(TypefaceFontNameProperty.IsValid());

	FontSizeProperty = StructPropertyHandle->GetChildHandle(SizePropertyName);
	check(FontSizeProperty.IsValid());

	InHeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(0)
	.MaxDesiredWidth(0)
	[
		InStructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FSlateFontInfoStructCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
{
	IDetailPropertyRow& FontObjectRow = InStructBuilder.AddProperty(FontObjectProperty.ToSharedRef());

	FontObjectRow.CustomWidget()
		.NameContent()
		[
			FontObjectProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.f)
		.MaxDesiredWidth(300.f)
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(FontObjectProperty)
			.AllowedClass(UFont::StaticClass())
			.OnShouldFilterAsset(FOnShouldFilterAsset::CreateStatic(&FSlateFontInfoStructCustomization::OnFilterFontAsset))
			.OnObjectChanged(this, &FSlateFontInfoStructCustomization::OnFontChanged)
			.DisplayUseSelected(true)
			.DisplayBrowse(true)
		];

	IDetailPropertyRow& TypefaceRow = InStructBuilder.AddProperty(TypefaceFontNameProperty.ToSharedRef());

	TypefaceRow.CustomWidget()
	.NameContent()
	[
		TypefaceFontNameProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SAssignNew(FontEntryCombo, SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&FontEntryComboData)
		.IsEnabled(this, &FSlateFontInfoStructCustomization::IsFontEntryComboEnabled)
		.OnComboBoxOpening(this, &FSlateFontInfoStructCustomization::OnFontEntryComboOpening)
		.OnSelectionChanged(this, &FSlateFontInfoStructCustomization::OnFontEntrySelectionChanged)
		.OnGenerateWidget(this, &FSlateFontInfoStructCustomization::MakeFontEntryWidget)
		[
			SNew(STextBlock)
			.Text(this, &FSlateFontInfoStructCustomization::GetFontEntryComboText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	];

	InStructBuilder.AddProperty(FontSizeProperty.ToSharedRef());

	InStructBuilder.AddProperty(InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSlateFontInfo, FontMaterial)).ToSharedRef());

	InStructBuilder.AddProperty(InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSlateFontInfo, OutlineSettings)).ToSharedRef());
}

bool FSlateFontInfoStructCustomization::OnFilterFontAsset(const FAssetData& InAssetData)
{
	// We want to filter font assets that aren't valid to use with Slate/UMG
	return Cast<const UFont>(InAssetData.GetAsset())->FontCacheType != EFontCacheType::Runtime;
}

void FSlateFontInfoStructCustomization::OnFontChanged(const FAssetData& InAssetData)
{
	const UFont* const FontAsset = Cast<const UFont>(InAssetData.GetAsset());
	const FName FirstFontName = (FontAsset && FontAsset->CompositeFont.DefaultTypeface.Fonts.Num()) ? FontAsset->CompositeFont.DefaultTypeface.Fonts[0].Name : NAME_None;

	TArray<FSlateFontInfo*> SlateFontInfoStructs = GetFontInfoBeingEdited();
	for(FSlateFontInfo* FontInfo : SlateFontInfoStructs)
	{
		// The font has been updated in the editor, so clear the non-UObject pointer so that the two don't conflict
		FontInfo->CompositeFont.Reset();

		// We've changed (or cleared) the font asset, so make sure and update the typeface entry name being used by the font info
		TypefaceFontNameProperty->SetValue(FirstFontName);
	}

	if(!FontAsset)
	{
		const FString PropertyPath = FontObjectProperty->GeneratePathToProperty();
		TArray<UObject*> PropertyOuterObjects;
		FontObjectProperty->GetOuterObjects(PropertyOuterObjects);
		for(const UObject* OuterObject : PropertyOuterObjects)
		{
			UE_LOG(LogSlate, Warning, TEXT("FSlateFontInfo property '%s' on object '%s' was set to use a null UFont. Slate will be forced to use the fallback font path which may be slower."), *PropertyPath, *OuterObject->GetPathName());
		}
	}
}

bool FSlateFontInfoStructCustomization::IsFontEntryComboEnabled() const
{
	TArray<const FSlateFontInfo*> SlateFontInfoStructs = GetFontInfoBeingEdited();
	if(SlateFontInfoStructs.Num() == 0)
	{
		return false;
	}

	const FSlateFontInfo* const FirstSlateFontInfo = SlateFontInfoStructs[0];
	const UFont* const FontObject = Cast<const UFont>(FirstSlateFontInfo->FontObject);
	if(!FontObject)
	{
		return false;
	}

	// Only let people pick an entry if every struct being edited is using the same font object
	for(int32 FontInfoIndex = 1; FontInfoIndex < SlateFontInfoStructs.Num(); ++FontInfoIndex)
	{
		const FSlateFontInfo* const OtherSlateFontInfo = SlateFontInfoStructs[FontInfoIndex];
		const UFont* const OtherFontObject = Cast<const UFont>(OtherSlateFontInfo->FontObject);
		if(FontObject != OtherFontObject)
		{
			return false;
		}
	}

	return true;
}

void FSlateFontInfoStructCustomization::OnFontEntryComboOpening()
{
	TArray<FSlateFontInfo*> SlateFontInfoStructs = GetFontInfoBeingEdited();

	FontEntryComboData.Empty();

	if(SlateFontInfoStructs.Num() > 0)
	{
		const FSlateFontInfo* const FirstSlateFontInfo = SlateFontInfoStructs[0];
		const UFont* const FontObject = Cast<const UFont>(FirstSlateFontInfo->FontObject);
		check(FontObject);

		const FName ActiveFontEntry = GetActiveFontEntry();
		TSharedPtr<FName> SelectedNamePtr;

		for(const FTypefaceEntry& TypefaceEntry : FontObject->CompositeFont.DefaultTypeface.Fonts)
		{
			TSharedPtr<FName> NameEntryPtr = MakeShareable(new FName(TypefaceEntry.Name));
			FontEntryComboData.Add(NameEntryPtr);

			if(!TypefaceEntry.Name.IsNone() && TypefaceEntry.Name == ActiveFontEntry)
			{
				SelectedNamePtr = NameEntryPtr;
			}
		}

		FontEntryComboData.Sort([](const TSharedPtr<FName>& One, const TSharedPtr<FName>& Two) -> bool
		{
			return One->ToString() < Two->ToString();
		});

		FontEntryCombo->ClearSelection();
		FontEntryCombo->RefreshOptions();
		FontEntryCombo->SetSelectedItem(SelectedNamePtr);
	}
	else
	{
		FontEntryCombo->ClearSelection();
		FontEntryCombo->RefreshOptions();
	}
}

void FSlateFontInfoStructCustomization::OnFontEntrySelectionChanged(TSharedPtr<FName> InNewSelection, ESelectInfo::Type)
{
	if(InNewSelection.IsValid())
	{
		TArray<FSlateFontInfo*> SlateFontInfoStructs = GetFontInfoBeingEdited();
		if(SlateFontInfoStructs.Num() > 0)
		{
			const FSlateFontInfo* const FirstSlateFontInfo = SlateFontInfoStructs[0];
			if(FirstSlateFontInfo->TypefaceFontName != *InNewSelection)
			{
				TypefaceFontNameProperty->SetValue(*InNewSelection);
			}
		}
	}
}

TSharedRef<SWidget> FSlateFontInfoStructCustomization::MakeFontEntryWidget(TSharedPtr<FName> InFontEntry)
{
	return
		SNew(STextBlock)
		.Text(FText::FromName(*InFontEntry))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}

FText FSlateFontInfoStructCustomization::GetFontEntryComboText() const
{
	return FText::FromName(GetActiveFontEntry());
}

FName FSlateFontInfoStructCustomization::GetActiveFontEntry() const
{
	TArray<const FSlateFontInfo*> SlateFontInfoStructs = GetFontInfoBeingEdited();
	if(SlateFontInfoStructs.Num() > 0)
	{
		const FSlateFontInfo* const FirstSlateFontInfo = SlateFontInfoStructs[0];
		const UFont* const FontObject = Cast<const UFont>(FirstSlateFontInfo->FontObject);
		if(FontObject)
		{
			return (FirstSlateFontInfo->TypefaceFontName.IsNone() && FontObject->CompositeFont.DefaultTypeface.Fonts.Num())
				? FontObject->CompositeFont.DefaultTypeface.Fonts[0].Name
				: FirstSlateFontInfo->TypefaceFontName;
		}
	}

	return NAME_None;
}

TArray<FSlateFontInfo*> FSlateFontInfoStructCustomization::GetFontInfoBeingEdited()
{
	TArray<FSlateFontInfo*> SlateFontInfoStructs;

	if(StructPropertyHandle->IsValidHandle())
	{
		TArray<void*> StructPtrs;
		StructPropertyHandle->AccessRawData(StructPtrs);
		SlateFontInfoStructs.Reserve(StructPtrs.Num());

		for(auto It = StructPtrs.CreateConstIterator(); It; ++It)
		{
			void* RawPtr = *It;
			if(RawPtr)
			{
				FSlateFontInfo* const SlateFontInfo = reinterpret_cast<FSlateFontInfo*>(RawPtr);
				SlateFontInfoStructs.Add(SlateFontInfo);
			}
		}
	}

	return SlateFontInfoStructs;
}

TArray<const FSlateFontInfo*> FSlateFontInfoStructCustomization::GetFontInfoBeingEdited() const
{
	TArray<const FSlateFontInfo*> SlateFontInfoStructs;

	if(StructPropertyHandle->IsValidHandle())
	{
		TArray<const void*> StructPtrs;
		StructPropertyHandle->AccessRawData(StructPtrs);
		SlateFontInfoStructs.Reserve(StructPtrs.Num());

		for(auto It = StructPtrs.CreateConstIterator(); It; ++It)
		{
			const void* RawPtr = *It;
			if(RawPtr)
			{
				const FSlateFontInfo* const SlateFontInfo = reinterpret_cast<const FSlateFontInfo*>(RawPtr);
				SlateFontInfoStructs.Add(SlateFontInfo);
			}
		}
	}

	return SlateFontInfoStructs;
}

#undef LOCTEXT_NAMESPACE
