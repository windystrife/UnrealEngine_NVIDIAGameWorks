// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorDetailCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameterCollection.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Materials/MaterialExpressionScalarParameter.h"

#define LOCTEXT_NAMESPACE "MaterialEditor"



TSharedRef<IDetailCustomization> FMaterialExpressionParameterDetails::MakeInstance(FOnCollectParameterGroups InCollectGroupsDelegate)
{
	return MakeShareable( new FMaterialExpressionParameterDetails(InCollectGroupsDelegate) );
}

FMaterialExpressionParameterDetails::FMaterialExpressionParameterDetails(FOnCollectParameterGroups InCollectGroupsDelegate)
	: CollectGroupsDelegate(InCollectGroupsDelegate)
{
}

void FMaterialExpressionParameterDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// for expression parameters all their properties are in one category based on their class name.
	FName DefaultCategory = NAME_None;
	IDetailCategoryBuilder& Category = DetailLayout.EditCategory( DefaultCategory );

	TArray< TWeakObjectPtr<UObject> > Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);

	DefaultValueHandles.Reset();
	ScalarParameterObjects.Reset();

	for (const auto& WeakObjectPtr : Objects)
	{
		UObject* Object = WeakObjectPtr.Get();

		UMaterialExpressionScalarParameter* ScalarParameter = Cast<UMaterialExpressionScalarParameter>(Object);

		if (ScalarParameter)
		{
			// Store these for OnSliderMinMaxEdited
			ScalarParameterObjects.Emplace(WeakObjectPtr);
			DefaultValueHandles.Emplace(DetailLayout.GetProperty("DefaultValue", UMaterialExpressionScalarParameter::StaticClass()));

			TSharedPtr<IPropertyHandle> SliderMinHandle = DetailLayout.GetProperty("SliderMin", UMaterialExpressionScalarParameter::StaticClass());

			if (SliderMinHandle.IsValid() && SliderMinHandle->IsValidHandle())
			{
				// Setup a callback when SliderMin changes to update the DefaultValue slider
				FSimpleDelegate OnSliderMinMaxEditedDelegate = FSimpleDelegate::CreateSP(this, &FMaterialExpressionParameterDetails::OnSliderMinMaxEdited);
				SliderMinHandle->SetOnPropertyValueChanged(OnSliderMinMaxEditedDelegate);
			}

			TSharedPtr<IPropertyHandle> SliderMaxHandle = DetailLayout.GetProperty("SliderMax", UMaterialExpressionScalarParameter::StaticClass());

			if (SliderMaxHandle.IsValid() && SliderMaxHandle->IsValidHandle())
			{
				FSimpleDelegate OnSliderMinMaxEditedDelegate = FSimpleDelegate::CreateSP(this, &FMaterialExpressionParameterDetails::OnSliderMinMaxEdited);
				SliderMaxHandle->SetOnPropertyValueChanged(OnSliderMinMaxEditedDelegate);
			}

			OnSliderMinMaxEdited();
		}
	}

	check(ScalarParameterObjects.Num() == DefaultValueHandles.Num());
	
	Category.AddProperty("ParameterName");
	
	// Get a handle to the property we are about to edit
	GroupPropertyHandle = DetailLayout.GetProperty( "Group" );

	GroupPropertyHandle->MarkHiddenByCustomization();

	PopulateGroups();

	TSharedPtr<SComboButton> NewComboButton;
	TSharedPtr<SEditableText> NewEditBox;
	TSharedPtr<SListView<TSharedPtr<FString>>> NewListView;
	
	Category.AddCustomRow( GroupPropertyHandle->GetPropertyDisplayName() )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( GroupPropertyHandle->GetPropertyDisplayName() )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	[
		SAssignNew(NewComboButton, SComboButton)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ButtonContent()
		[
			SAssignNew(NewEditBox, SEditableText)
				.Text(this, &FMaterialExpressionParameterDetails::OnGetText)
				.OnTextCommitted(this, &FMaterialExpressionParameterDetails::OnTextCommitted)
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SAssignNew(NewListView, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&GroupsSource)
					.OnGenerateRow(this, &FMaterialExpressionParameterDetails::MakeDetailsGroupViewWidget)
					.OnSelectionChanged(this, &FMaterialExpressionParameterDetails::OnSelectionChanged)
			]
		]
	];


	Category.AddProperty("SortPriority");

	GroupComboButton = NewComboButton;
	GroupEditBox = NewEditBox;
	GroupListView = NewListView;
}

void FMaterialExpressionParameterDetails::PopulateGroups()
{
	TArray<FString> Groups;
	CollectGroupsDelegate.ExecuteIfBound(&Groups);
	Groups.Sort([&](const FString& A, const FString& B) {
		return A.ToLower() < B.ToLower();
	});

	GroupsSource.Empty();
	for (int32 GroupIdx = 0; GroupIdx < Groups.Num(); ++GroupIdx)
	{
		GroupsSource.Add(MakeShareable(new FString(Groups[GroupIdx])));
	}
}

TSharedRef< ITableRow > FMaterialExpressionParameterDetails::MakeDetailsGroupViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(FText::FromString(*Item.Get()))
		];
}

void FMaterialExpressionParameterDetails::OnSelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if (ProposedSelection.IsValid())
	{
		GroupPropertyHandle->SetValue(*ProposedSelection.Get());
		GroupListView.Pin()->ClearSelection();
		GroupComboButton.Pin()->SetIsOpen(false);
	}
}

void FMaterialExpressionParameterDetails::OnTextCommitted( const FText& InText, ETextCommit::Type /*CommitInfo*/)
{
	GroupPropertyHandle->SetValue(InText.ToString());
	PopulateGroups();
}

FString FMaterialExpressionParameterDetails::OnGetString() const
{
	FString OutText;
	if (GroupPropertyHandle->GetValue(OutText) == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values").ToString();
	}
	return OutText;
}

FText FMaterialExpressionParameterDetails::OnGetText() const
{
	FString NewString = OnGetString();
	return FText::FromString(NewString);
}

void FMaterialExpressionParameterDetails::OnSliderMinMaxEdited()
{
	check(ScalarParameterObjects.Num() == DefaultValueHandles.Num());
	for (int32 Index = 0; Index < ScalarParameterObjects.Num(); Index++)
	{
		UMaterialExpressionScalarParameter* ScalarParameter = Cast<UMaterialExpressionScalarParameter>(ScalarParameterObjects[Index].Get());

		if (ScalarParameter && DefaultValueHandles[Index].IsValid() && DefaultValueHandles[Index]->IsValidHandle())
		{
			if (ScalarParameter->SliderMax > ScalarParameter->SliderMin)
			{
				// Update the values that SPropertyEditorNumeric reads
				// Unfortuantly there is no way to recreate the widget to actually update the UI with these new values
				DefaultValueHandles[Index]->SetInstanceMetaData("UIMin", FString::Printf(TEXT("%f"), ScalarParameter->SliderMin));
				DefaultValueHandles[Index]->SetInstanceMetaData("UIMax", FString::Printf(TEXT("%f"), ScalarParameter->SliderMax));
			}
			else
			{
				DefaultValueHandles[Index]->SetInstanceMetaData("UIMin", TEXT(""));
				DefaultValueHandles[Index]->SetInstanceMetaData("UIMax", TEXT(""));
			}
		}
	}
}

TSharedRef<IDetailCustomization> FMaterialExpressionCollectionParameterDetails::MakeInstance()
{
	return MakeShareable( new FMaterialExpressionCollectionParameterDetails() );
}

FMaterialExpressionCollectionParameterDetails::FMaterialExpressionCollectionParameterDetails()
{
}

FText FMaterialExpressionCollectionParameterDetails::GetToolTipText() const
{
	if (ParametersSource.Num() == 1)
	{
		return LOCTEXT("SpecifyCollection", "Specify a Collection to get parameter options");
	}
	else
	{
		return LOCTEXT("ChooseParameter", "Choose a parameter from the collection");
	}
}

FText FMaterialExpressionCollectionParameterDetails::GetParameterNameString() const
{
	FString CurrentParameterName;

	FPropertyAccess::Result Result = ParameterNamePropertyHandle->GetValue(CurrentParameterName);
	if( Result == FPropertyAccess::MultipleValues )
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return FText::FromString(CurrentParameterName);
}

bool FMaterialExpressionCollectionParameterDetails::IsParameterNameComboEnabled() const
{
	UMaterialParameterCollection* Collection = nullptr;
	
	if (CollectionPropertyHandle->IsValidHandle())
	{
		UObject* CollectionObject = nullptr;
		verify(CollectionPropertyHandle->GetValue(CollectionObject) == FPropertyAccess::Success);
		Collection = Cast<UMaterialParameterCollection>(CollectionObject);
	}

	return Collection != nullptr;
}

void FMaterialExpressionCollectionParameterDetails::OnCollectionChanged()
{
	PopulateParameters();
}

void FMaterialExpressionCollectionParameterDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// for expression parameters all their properties are in one category based on their class name.
	FName DefaultCategory = NAME_None;
	IDetailCategoryBuilder& Category = DetailLayout.EditCategory( DefaultCategory );

	// Get a handle to the property we are about to edit
	ParameterNamePropertyHandle = DetailLayout.GetProperty( "ParameterName" );
	check(ParameterNamePropertyHandle.IsValid());
	CollectionPropertyHandle = DetailLayout.GetProperty( "Collection" );
	check(CollectionPropertyHandle.IsValid());

	// Register a changed callback on the collection property since we need to update the PropertyName vertical box when it changes
	FSimpleDelegate OnCollectionChangedDelegate = FSimpleDelegate::CreateSP( this, &FMaterialExpressionCollectionParameterDetails::OnCollectionChanged );
	CollectionPropertyHandle->SetOnPropertyValueChanged( OnCollectionChangedDelegate );

	ParameterNamePropertyHandle->MarkHiddenByCustomization();
	CollectionPropertyHandle->MarkHiddenByCustomization();

	PopulateParameters();

	TSharedPtr<SComboButton> NewComboButton;
	TSharedPtr<SListView<TSharedPtr<FString>>> NewListView;

	// This isn't strictly speaking customized, but we need it to appear before the "Parameter Name" property, 
	// so we manually add it and set MarkHiddenByCustomization on it to avoid it being automatically added
	Category.AddProperty(CollectionPropertyHandle);

	Category.AddCustomRow( ParameterNamePropertyHandle->GetPropertyDisplayName() )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( ParameterNamePropertyHandle->GetPropertyDisplayName() )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	[
		SAssignNew(NewComboButton, SComboButton)
		.IsEnabled(this, &FMaterialExpressionCollectionParameterDetails::IsParameterNameComboEnabled)
		.ContentPadding(0)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FMaterialExpressionCollectionParameterDetails::GetParameterNameString )
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SAssignNew(NewListView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&ParametersSource)
				.OnGenerateRow(this, &FMaterialExpressionCollectionParameterDetails::MakeDetailsGroupViewWidget)
				.OnSelectionChanged(this, &FMaterialExpressionCollectionParameterDetails::OnSelectionChanged)
			]
		]
	];

	ParameterComboButton = NewComboButton;
	ParameterListView = NewListView;

	NewComboButton->SetToolTipText(GetToolTipText());
}

void FMaterialExpressionCollectionParameterDetails::PopulateParameters()
{
	UMaterialParameterCollection* Collection = nullptr;
	
	if (CollectionPropertyHandle->IsValidHandle())
	{
		UObject* CollectionObject = nullptr;
		verify(CollectionPropertyHandle->GetValue(CollectionObject) == FPropertyAccess::Success);
		Collection = Cast<UMaterialParameterCollection>(CollectionObject);
	}

	ParametersSource.Empty();

	if (Collection)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < Collection->ScalarParameters.Num(); ++ParameterIndex)
		{
			ParametersSource.Add(MakeShareable(new FString(Collection->ScalarParameters[ParameterIndex].ParameterName.ToString())));
		}

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->VectorParameters.Num(); ++ParameterIndex)
		{
			ParametersSource.Add(MakeShareable(new FString(Collection->VectorParameters[ParameterIndex].ParameterName.ToString())));
		}
	}

	if (ParametersSource.Num() == 0)
	{
		ParametersSource.Add(MakeShareable(new FString(LOCTEXT("NoParameter", "None").ToString())));
	}
}

TSharedRef< ITableRow > FMaterialExpressionCollectionParameterDetails::MakeDetailsGroupViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(FText::FromString(*Item.Get()))
		];
}

void FMaterialExpressionCollectionParameterDetails::OnSelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if (ProposedSelection.IsValid())
	{
		ParameterNamePropertyHandle->SetValue(*ProposedSelection.Get());
		ParameterListView.Pin()->ClearSelection();
		ParameterComboButton.Pin()->SetIsOpen(false);
	}
}

TSharedRef<class IDetailCustomization> FMaterialDetailCustomization::MakeInstance()
{
	return MakeShareable( new FMaterialDetailCustomization );
}

void FMaterialDetailCustomization::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	TArray<TWeakObjectPtr<UObject> > Objects;
	DetailLayout.GetObjectsBeingCustomized( Objects );

	bool bUIMaterial = true;
	for( TWeakObjectPtr<UObject>& Object : Objects )
	{
		UMaterial* Material = Cast<UMaterial>( Object.Get() );
		if( Material )
		{
			bUIMaterial &= Material->IsUIMaterial();
		}
		else
		{
			// this shouldn't happen but just in case, let all properties through
			bUIMaterial = false;
		}
	}

	if( bUIMaterial )
	{
		DetailLayout.HideCategory( TEXT("TranslucencySelfShadowing") );
		DetailLayout.HideCategory( TEXT("Translucency") );
		DetailLayout.HideCategory( TEXT("Tessellation") );
		DetailLayout.HideCategory( TEXT("PostProcessMaterial") );
		DetailLayout.HideCategory( TEXT("Lightmass") );
		DetailLayout.HideCategory( TEXT("Thumbnail") );
		DetailLayout.HideCategory( TEXT("MaterialInterface") );
		DetailLayout.HideCategory( TEXT("PhysicalMaterial") );
		DetailLayout.HideCategory( TEXT("Usage") );

		// Materail category
		{
			IDetailCategoryBuilder& MaterialCategory = DetailLayout.EditCategory( TEXT("Material") );

			TArray<TSharedRef<IPropertyHandle>> AllProperties;
			MaterialCategory.GetDefaultProperties( AllProperties );

			for( TSharedRef<IPropertyHandle>& PropertyHandle : AllProperties )
			{
				UProperty* Property = PropertyHandle->GetProperty();
				FName PropertyName = Property->GetFName();

				if(		PropertyName != GET_MEMBER_NAME_CHECKED(UMaterial, MaterialDomain) 
					&&	PropertyName != GET_MEMBER_NAME_CHECKED(UMaterial, BlendMode) 
					&&	PropertyName != GET_MEMBER_NAME_CHECKED(UMaterial, OpacityMaskClipValue) 
					&&  	PropertyName != GET_MEMBER_NAME_CHECKED(UMaterial, NumCustomizedUVs) )
				{
					DetailLayout.HideProperty( PropertyHandle );
				}
			}
		}

		// Mobile category
		{
			IDetailCategoryBuilder& MobileCategory = DetailLayout.EditCategory(TEXT("Mobile"));
			
			TArray<TSharedRef<IPropertyHandle>> AllProperties;
			MobileCategory.GetDefaultProperties(AllProperties);

			for (TSharedRef<IPropertyHandle>& PropertyHandle : AllProperties)
			{
				UProperty* Property = PropertyHandle->GetProperty();
				FName PropertyName = Property->GetFName();

				if (PropertyName != GET_MEMBER_NAME_CHECKED(UMaterial, bUseFullPrecision)) 
				{
					DetailLayout.HideProperty(PropertyHandle);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
