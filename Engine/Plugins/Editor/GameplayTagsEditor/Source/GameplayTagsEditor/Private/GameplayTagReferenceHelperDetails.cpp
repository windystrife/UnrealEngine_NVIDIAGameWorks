// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagReferenceHelperDetails.h"
#include "AssetRegistryModule.h"
#include "SHyperlink.h"
#include "Toolkits/AssetEditorManager.h"
#include "ObjectTools.h"
#include "GameplayTagContainer.h"
#include "UObjectHash.h"
#include "UnrealType.h"
#include "GameplayTagsManager.h"
#include "SGameplayTagWidget.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "GameplayTagReferenceHelperDetails"

static const FName GameplayTagColumnName("GameplayTagColumn");

TSharedRef<IPropertyTypeCustomization> FGameplayTagReferenceHelperDetails::MakeInstance()
{
	return MakeShareable(new FGameplayTagReferenceHelperDetails());
}

void FGameplayTagReferenceHelperDetails::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyHandle = StructPropertyHandle;

	TreeItems.Reset();
	if (FGameplayTagReferenceHelper* Helper = GetValue())
	{
		// We need the raw data pointer to the struct (UStruct or UClass) that owns the FGameplayTagReferenceHelper property.
		// Its not enough to just bind the raw 'this' pointer in the owning struct's cstor, since lists or data tables of structs
		// will be copied around as the list changes sizes (overloading copy and assignment operators on the owning struct to clean/update 
		// the delegate is also a major pain).
		// 
		// We cheat a bit here and use GetOffset_ForGC to work backwards up the property change and get the raw, castable, address of the owning
		// structure so that the delegate can just do a static_cast and do whatever they want.
		//
		//
		// Note: this currently does NOT handle the owning struct changing and auto updating. This is a bit tricky since we don't know, in this context, when
		// an update has to happen, since this thing is not tied directly to a tag uproperty. (E.g, a data table row where the row's key name is the tag tag name)

		void* OwnerStructRawData = nullptr;
		if (UProperty* MyProperty = PropertyHandle->GetProperty())
		{
			if (UStruct* OwnerStruct = MyProperty->GetOwnerStruct())
			{				
				// Manually calculate the owning struct raw data pointer
				checkf(MyProperty->ArrayDim == 1, TEXT("FGameplayTagReferenceHelper should never be in an array"));
				uint8* PropertyRawData = (uint8*)GetValue();
				OwnerStructRawData = (void*) (PropertyRawData - MyProperty->GetOffset_ForGC());
			}
		}
		
		if (ensureMsgf(OwnerStructRawData != nullptr, TEXT("Unable to get outer struct's raw data")))
		{
			FName TagName = Helper->OnGetGameplayTagName.Execute(OwnerStructRawData);
		
			FAssetIdentifier TagId = FAssetIdentifier(FGameplayTag::StaticStruct(), TagName);
			TArray<FAssetIdentifier> Referencers;

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, EAssetRegistryDependencyType::SearchableName);

			for (FAssetIdentifier& AssetIdentifier : Referencers)
			{
				TSharedPtr<FGameplayTagReferenceTreeItem> Item =  TSharedPtr<FGameplayTagReferenceTreeItem>(new FGameplayTagReferenceTreeItem());
				Item->GameplayTagName = TagName;
				Item->AssetIdentifier = AssetIdentifier;
				TreeItems.Add(Item);
			}
		}
	}

	HeaderRow
	.NameContent()
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolBar.Background"))
		[
			SNew(SGameplayTagReferenceTree)
			.ItemHeight(24)
			.TreeItemsSource(&TreeItems)
			.OnGenerateRow(this, &FGameplayTagReferenceHelperDetails::OnGenerateWidgetForGameplayCueListView)
			.OnGetChildren( SGameplayTagReferenceTree::FOnGetChildren::CreateLambda([](TSharedPtr<FGameplayTagReferenceTreeItem> Item, TArray< TSharedPtr<FGameplayTagReferenceTreeItem> >& Children) { } ) )
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(GameplayTagColumnName)
				.DefaultLabel(NSLOCTEXT("GameplayTagReferenceHelper", "GameplayTagReferenceHelperColumn", "GameplayTag Referencers (does not include native code)"))
				.FillWidth(0.50)
			)
		]
	];
}

void FGameplayTagReferenceHelperDetails::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	
}

FGameplayTagReferenceHelper* FGameplayTagReferenceHelperDetails::GetValue()
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	if (RawData.Num() != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Unexpected raw data count of %d"), RawData.Num());
		return nullptr;
	}
	return static_cast<FGameplayTagReferenceHelper*>(RawData[0]);
}

/** Builds widget for rows in the GameplayCue Editor tab */
TSharedRef<ITableRow> FGameplayTagReferenceHelperDetails::OnGenerateWidgetForGameplayCueListView(TSharedPtr< FGameplayTagReferenceTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	class SGameplayTagWidgetItem : public SMultiColumnTableRow< TSharedPtr< FGameplayTagReferenceTreeItem > >
	{
	public:
		SLATE_BEGIN_ARGS(SGameplayTagWidgetItem){}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FGameplayTagReferenceTreeItem> InListItem)
		{
			Item = InListItem;
			SMultiColumnTableRow< TSharedPtr< FGameplayTagReferenceTreeItem > >::Construct(
				FSuperRowType::FArguments()
			, InOwnerTable);
		}
	private:

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == GameplayTagColumnName)
			{
				if (Item->AssetIdentifier.ToString().IsEmpty() == false)
				{
					return
					SNew(SBox)
					.HAlign(HAlign_Left)
					[
						SNew(SHyperlink)
						.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
						.Text(FText::FromString(Item->AssetIdentifier.ToString()))
						.OnNavigate(this, &SGameplayTagWidgetItem::NavigateToReference)
					];
				}
				else
				{
					return
					SNew(SBox)
					.HAlign(HAlign_Left);
				}
			}
			else
			{
				return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
			}
		}

		void NavigateToReference()
		{
			UPackage* Pkg = FindPackage(nullptr, *Item->AssetIdentifier.PackageName.ToString());
			if (Pkg == nullptr)
			{
				Pkg = LoadPackage(nullptr, *Item->AssetIdentifier.PackageName.ToString(), LOAD_None);
			}

			if (Pkg)
			{
				ForEachObjectWithOuter(Pkg,[](UObject* Obj)
				{
					if (ObjectTools::IsObjectBrowsable(Obj))
					{
						FAssetEditorManager::Get().OpenEditorForAsset(Obj);
					}
				});
			}
		}

		TSharedPtr< FGameplayTagReferenceTreeItem > Item;
	};

	if ( InItem.IsValid() )
	{
		return SNew(SGameplayTagWidgetItem, OwnerTable, InItem);
	}
	else
	{
		return
		SNew(STableRow< TSharedPtr<FGameplayTagReferenceTreeItem> >, OwnerTable)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UnknownItemType", "Unknown Item Type"))
		];
	}
}

// --------------------------------------------------------------------------------------

TSharedRef<IPropertyTypeCustomization> FGameplayTagCreationWidgetHelperDetails::MakeInstance()
{
	return MakeShareable(new FGameplayTagCreationWidgetHelperDetails());
}

void FGameplayTagCreationWidgetHelperDetails::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{

}

void FGameplayTagCreationWidgetHelperDetails::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	FString FilterString = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);
	const float MaxPropertyWidth = 480.0f;
	const float MaxPropertyHeight = 240.0f;

	StructBuilder.AddCustomRow( LOCTEXT("NewTag", "NewTag") )
	.ValueContent()
	.MaxDesiredWidth(MaxPropertyWidth)
	[
		SAssignNew(TagWidget, SGameplayTagWidget, TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum>())
		.Filter(FilterString)
		.NewTagName(FilterString)
		.MultiSelect(false)
		.GameplayTagUIMode(EGameplayTagUIMode::ManagementMode)
		.MaxHeight(MaxPropertyHeight)
		.NewTagControlsInitiallyExpanded(true)
		//.OnTagChanged(this, &FGameplayTagsSettingsCustomization::OnTagChanged)
	];
	
}



#undef LOCTEXT_NAMESPACE