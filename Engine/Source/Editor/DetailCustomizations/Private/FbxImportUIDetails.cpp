// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FbxImportUIDetails.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxTextureImportData.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/SToolTip.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FbxImportUIDetails"

//If the String is contain in the StringArray, it return the index. Otherwise return INDEX_NONE
int FindString(const TArray<TSharedPtr<FString>> &StringArray, const FString &String) {
	for (int i = 0; i < StringArray.Num(); i++)
	{
		if (String.Equals(*StringArray[i].Get()))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FFbxImportUIDetails::FFbxImportUIDetails()
{
	CachedDetailBuilder = nullptr;

	LODGroupNames.Reset();
	UStaticMesh::GetLODGroups(LODGroupNames);
	for (int32 GroupIndex = 0; GroupIndex < LODGroupNames.Num(); ++GroupIndex)
	{
		LODGroupOptions.Add(MakeShareable(new FString(LODGroupNames[GroupIndex].GetPlainNameString())));
	}

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}
}

FFbxImportUIDetails::~FFbxImportUIDetails()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->UnregisterForUndo(this);
	}
}

void FFbxImportUIDetails::RefreshCustomDetail()
{
	if (CachedDetailBuilder)
	{
		CachedDetailBuilder->ForceRefreshDetails();
	}
}

void FFbxImportUIDetails::PostUndo(bool bSuccess)
{
	//Refresh the UI
	RefreshCustomDetail();
}

void FFbxImportUIDetails::PostRedo(bool bSuccess)
{
	//Refresh the UI
	RefreshCustomDetail();
}


TSharedRef<IDetailCustomization> FFbxImportUIDetails::MakeInstance()
{
	return MakeShareable( new FFbxImportUIDetails );
}

void FFbxImportUIDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	CachedDetailBuilder = &DetailBuilder;
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);
	check(EditingObjects.Num() == 1);

	ImportUI = Cast<UFbxImportUI>(EditingObjects[0].Get());

	// Handle mesh category
	IDetailCategoryBuilder& MeshCategory = DetailBuilder.EditCategory("Mesh", FText::GetEmpty(), ECategoryPriority::Important);
	IDetailCategoryBuilder& TransformCategory = DetailBuilder.EditCategory("Transform");
	TArray<TSharedRef<IPropertyHandle>> CategoryDefaultProperties;
	TArray<TSharedPtr<IPropertyHandle>> ExtraProperties;

	// Grab and hide per-type import options
	TSharedRef<IPropertyHandle> StaticMeshDataProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, StaticMeshImportData));
	TSharedRef<IPropertyHandle> SkeletalMeshDataProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, SkeletalMeshImportData));
	TSharedRef<IPropertyHandle> AnimSequenceDataProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, AnimSequenceImportData));
	DetailBuilder.HideProperty(StaticMeshDataProp);
	DetailBuilder.HideProperty(SkeletalMeshDataProp);
	DetailBuilder.HideProperty(AnimSequenceDataProp);

	TSharedRef<IPropertyHandle> ImportMaterialsProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, bImportMaterials));
	ImportMaterialsProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFbxImportUIDetails::ImportMaterialsChanged));

	TSharedRef<IPropertyHandle> ImportAutoComputeLodDistancesProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, bAutoComputeLodDistances));
	ImportAutoComputeLodDistancesProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFbxImportUIDetails::ImportAutoComputeLodDistancesChanged));

	MeshCategory.GetDefaultProperties(CategoryDefaultProperties);

	
	switch(ImportUI->MeshTypeToImport)
	{
		case FBXIT_StaticMesh:
			CollectChildPropertiesRecursive(StaticMeshDataProp, ExtraProperties);
			break;
		case FBXIT_SkeletalMesh:
			if(ImportUI->bImportMesh)
			{
				CollectChildPropertiesRecursive(SkeletalMeshDataProp, ExtraProperties);
			}
			else
			{
				ImportUI->MeshTypeToImport = FBXIT_Animation;
			}
			break;
		default:
			break;
	}
	EFBXImportType ImportType = ImportUI->MeshTypeToImport;

	//Hide LodDistance property if we do not need them
	if (ImportType == FBXIT_StaticMesh && ImportUI->bAutoComputeLodDistances)
	{
		for (int32 LodIndex = 0; LodIndex < 8; ++LodIndex)
		{
			TArray<FStringFormatArg> Args;
			Args.Add(TEXT("LodDistance"));
			Args.Add(FString::FromInt(LodIndex));
			FString LodDistancePropertyName = FString::Format(TEXT("{0}{1}"), Args);
			TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(FName(*LodDistancePropertyName));
			UProperty* Property = Handle->GetProperty();
			if (Property != nullptr && Property->GetName().Compare(LodDistancePropertyName) == 0)
			{
				DetailBuilder.HideProperty(Handle);
			}
		}
	}
	else if (ImportType != FBXIT_StaticMesh)
	{
		DetailBuilder.HideCategory(FName(TEXT("LodSettings")));
	}
	

	if(ImportType != FBXIT_Animation)
	{
		{
			TSharedRef<IPropertyHandle> Prop = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, bImportAsSkeletal));
			if (!ImportUI->bIsReimport)
			{
				Prop->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFbxImportUIDetails::MeshImportModeChanged));
				MeshCategory.AddProperty(Prop);
			}
			else
			{
				DetailBuilder.HideProperty(Prop);
			}
		}
	}

	TSharedRef<IPropertyHandle> ImportMeshProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, bImportMesh));
	if(ImportUI->OriginalImportType == FBXIT_SkeletalMesh && ImportType != FBXIT_StaticMesh && !ImportUI->bIsReimport)
	{
		ImportMeshProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFbxImportUIDetails::ImportMeshToggleChanged));
		MeshCategory.AddProperty(ImportMeshProp);
	}
	else
	{
		DetailBuilder.HideProperty(ImportMeshProp);
	}

	for(TSharedRef<IPropertyHandle> Handle : CategoryDefaultProperties)
	{
		FString MetaData = Handle->GetMetaData(TEXT("ImportType"));
		if(!IsImportTypeMetaDataValid(ImportType, MetaData))
		{
			DetailBuilder.HideProperty(Handle);
		}
	}

	for(TSharedPtr<IPropertyHandle> Handle : ExtraProperties)
	{
		FString ImportTypeMetaData = Handle->GetMetaData(TEXT("ImportType"));
		const FString& CategoryMetaData = Handle->GetMetaData(TEXT("ImportCategory"));
		if(IsImportTypeMetaDataValid(ImportType, ImportTypeMetaData))
		{
			// Decide on category
			if(!CategoryMetaData.IsEmpty())
			{
				// Populate custom categories.
				IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory(*CategoryMetaData);
				CustomCategory.AddProperty(Handle);
			}
			else
			{
				// No override, add to default mesh category
				IDetailPropertyRow& PropertyRow = MeshCategory.AddProperty(Handle);

				UProperty* Property = Handle->GetProperty();
				if (Property != nullptr)
				{
					if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UFbxStaticMeshImportData, StaticMeshLODGroup))
					{
						SetStaticMeshLODGroupWidget(PropertyRow, Handle);
					}

					if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UFbxStaticMeshImportData, VertexOverrideColor))
					{
						// Cache the VertexColorImportOption property
						VertexColorImportOptionHandle = StaticMeshDataProp->GetChildHandle(GET_MEMBER_NAME_CHECKED(UFbxStaticMeshImportData, VertexColorImportOption));

						PropertyRow.IsEnabled(TAttribute<bool>(this, &FFbxImportUIDetails::GetVertexOverrideColorEnabledState));
					}
				}
			}
		}
	}

	// Animation Category
	IDetailCategoryBuilder& AnimCategory = DetailBuilder.EditCategory("Animation", FText::GetEmpty(), ECategoryPriority::Important);

	CategoryDefaultProperties.Empty();
	AnimCategory.GetDefaultProperties(CategoryDefaultProperties);
	for(TSharedRef<IPropertyHandle> Handle : CategoryDefaultProperties)
	{
		FString MetaData = Handle->GetMetaData(TEXT("ImportType"));
		if(!IsImportTypeMetaDataValid(ImportType, MetaData))
		{
			DetailBuilder.HideProperty(Handle);
		}
	}

	if(ImportType == FBXIT_Animation || ImportType == FBXIT_SkeletalMesh)
	{
		ExtraProperties.Empty();
		CollectChildPropertiesRecursive(AnimSequenceDataProp, ExtraProperties);

		// Before we add the import data properties we need to re-add any properties we want to appear above them in the UI
		TSharedRef<IPropertyHandle> ImportAnimProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, bImportAnimations));
		// If we're importing an animation file we really don't need to ask this
		DetailBuilder.HideProperty(ImportAnimProp);
		if(ImportType == FBXIT_Animation)
		{
			ImportUI->bImportAnimations = true;
		}
		else
		{
			AnimCategory.AddProperty(ImportAnimProp);
		}

		for(TSharedPtr<IPropertyHandle> Handle : ExtraProperties)
		{
			const FString& CategoryMetaData = Handle->GetMetaData(TEXT("ImportCategory"));
			if(Handle->GetProperty()->GetOuter() == UFbxAnimSequenceImportData::StaticClass()
			   && CategoryMetaData.IsEmpty())
			{
				// Add to default anim category if no override specified
				IDetailPropertyRow& PropertyRow = AnimCategory.AddProperty(Handle);
			}
			else if(ImportType == FBXIT_Animation && !CategoryMetaData.IsEmpty())
			{
				// Override category is available
				IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory(*CategoryMetaData);
				CustomCategory.AddProperty(Handle);
			}
		}
	}
	else
	{
		// Hide animation options
		CategoryDefaultProperties.Empty();
		AnimCategory.GetDefaultProperties(CategoryDefaultProperties);

		for(TSharedRef<IPropertyHandle> Handle : CategoryDefaultProperties)
		{
			DetailBuilder.HideProperty(Handle);
		}
	}

	// Material Category
	IDetailCategoryBuilder& MaterialCategory = DetailBuilder.EditCategory("Material");
	if(ImportType == FBXIT_Animation)
	{
		// In animation-only mode, hide the material display
		CategoryDefaultProperties.Empty();
		MaterialCategory.GetDefaultProperties(CategoryDefaultProperties);

		for(TSharedRef<IPropertyHandle> Handle : CategoryDefaultProperties)
		{
			DetailBuilder.HideProperty(Handle);
		}
	}
	else
	{
		//Show the reset Material slot only when re importing
		TSharedRef<IPropertyHandle> ResetMaterialSlotHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, bResetMaterialSlots));
		if (!ImportUI->bIsReimport)
		{
			DetailBuilder.HideProperty(ResetMaterialSlotHandle);
		}

		TSharedRef<IPropertyHandle> TextureDataProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFbxImportUI, TextureImportData));
		DetailBuilder.HideProperty(TextureDataProp);

		ExtraProperties.Empty();
		CollectChildPropertiesRecursive(TextureDataProp, ExtraProperties);

		for(TSharedPtr<IPropertyHandle> Handle : ExtraProperties)
		{
			// We ignore base import data for this window.
			if(Handle->GetProperty()->GetOuter() == UFbxTextureImportData::StaticClass())
			{
				if (Handle->GetPropertyDisplayName().ToString() == FString(TEXT("Base Material Name")))
				{
					if (ImportUI->bImportMaterials)
					{
						ConstructBaseMaterialUI(Handle, MaterialCategory);
					}
				}
				else
				{
					MaterialCategory.AddProperty(Handle);
				}
			}
		}
	}
}


void FFbxImportUIDetails::ConstructBaseMaterialUI(TSharedPtr<IPropertyHandle> Handle, IDetailCategoryBuilder& MaterialCategory)
{
	IDetailPropertyRow &MaterialPropertyRow = MaterialCategory.AddProperty(Handle);
	Handle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFbxImportUIDetails::BaseMaterialChanged));
	UMaterialInterface *MaterialInstanceProperty = Cast<UMaterialInterface>(ImportUI->TextureImportData->BaseMaterialName.TryLoad());
	if (MaterialInstanceProperty == nullptr)
	{
		return;
	}
	UMaterial *Material = MaterialInstanceProperty->GetMaterial();
	if (Material == nullptr)
	{
		return;
	}

	BaseColorNames.Empty();
	BaseTextureNames.Empty();
	BaseColorNames.Add(MakeShareable(new FString()));
	BaseTextureNames.Add(MakeShareable(new FString()));
	TArray<FName> ParameterNames;
	TArray<FGuid> Guids;
	float MinDesiredWidth = 150.0f;
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;
	MaterialPropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

	// base color properties, only used when there is no texture in the diffuse map
	Material->GetAllVectorParameterNames(ParameterNames, Guids);
	for (FName &ParameterName : ParameterNames)
	{
		BaseColorNames.Add(MakeShareable(new FString(ParameterName.ToString())));
	}
	int InitialSelect = FindString(BaseColorNames, ImportUI->TextureImportData->BaseColorName);
	InitialSelect = InitialSelect == INDEX_NONE ? 0 : InitialSelect; // default to the empty string located at index 0
	MaterialCategory.AddCustomRow(LOCTEXT("BaseColorProperty", "Base Color Property"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BaseColorProperty", "Base Color Property"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth(MinDesiredWidth)
			[
				SNew(STextComboBox)
				.OptionsSource(&BaseColorNames)
				.ToolTip(SNew(SToolTip).Text(LOCTEXT("BaseColorFBXImportToolTip", "When there is no diffuse texture in the imported material this color property will be used to fill a contant color value instead.")))
				.OnSelectionChanged(this, &FFbxImportUIDetails::OnBaseColor)
				.InitiallySelectedItem(BaseColorNames[InitialSelect])
			]
		]
	];

	// base texture properties
	ParameterNames.Empty();
	Guids.Empty();
	Material->GetAllTextureParameterNames(ParameterNames, Guids);
	for (FName &ParameterName : ParameterNames)
	{
		BaseTextureNames.Add(MakeShareable(new FString(ParameterName.ToString())));
	}
	InitialSelect = FindString(BaseTextureNames, ImportUI->TextureImportData->BaseDiffuseTextureName);
	InitialSelect = InitialSelect == INDEX_NONE ? 0 : InitialSelect; // default to the empty string located at index 0
	MaterialCategory.AddCustomRow(LOCTEXT("BaseTextureProperty", "Base Texture Property")).NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BaseTextureProperty", "Base Texture Property"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth(MinDesiredWidth)
			[
				SNew(STextComboBox)
				.OptionsSource(&BaseTextureNames)
				.OnSelectionChanged(this, &FFbxImportUIDetails::OnDiffuseTextureColor)
				.InitiallySelectedItem(BaseTextureNames[InitialSelect])
			]
		]
	];

	// base normal properties
	InitialSelect = FindString(BaseTextureNames, ImportUI->TextureImportData->BaseNormalTextureName);
	InitialSelect = InitialSelect == INDEX_NONE ? 0 : InitialSelect; // default to the empty string located at index 0
	MaterialCategory.AddCustomRow(LOCTEXT("BaseNormalTextureProperty", "Base Normal Texture Property")).NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BaseNormalTextureProperty", "Base Normal Texture Property"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth(MinDesiredWidth)
			[
				SNew(STextComboBox)
				.OptionsSource(&BaseTextureNames)
				.OnSelectionChanged(this, &FFbxImportUIDetails::OnNormalTextureColor)
				.InitiallySelectedItem(BaseTextureNames[InitialSelect])
			]
		]
	];

	// base emissive color properties, only used when there is no texture in the emissive map
	InitialSelect = FindString(BaseColorNames, ImportUI->TextureImportData->BaseEmissiveColorName);
	InitialSelect = InitialSelect == INDEX_NONE ? 0 : InitialSelect; // default to the empty string located at index 0
	MaterialCategory.AddCustomRow(LOCTEXT("BaseEmissiveColorProperty", "Base Emissive Color Property"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BaseEmissiveColorProperty", "Base Emissive Color Property"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth(MinDesiredWidth)
			[
				SNew(STextComboBox)
				.OptionsSource(&BaseColorNames)
				.ToolTip(SNew(SToolTip).Text(LOCTEXT("BaseEmissiveColorFBXImportToolTip", "When there is no emissive texture in the imported material this emissive color property will be used to fill a contant color value instead.")))
				.OnSelectionChanged(this, &FFbxImportUIDetails::OnEmissiveColor)
				.InitiallySelectedItem(BaseColorNames[InitialSelect])
			]
		]
	];

	// base emmisive properties
	InitialSelect = FindString(BaseTextureNames, ImportUI->TextureImportData->BaseEmmisiveTextureName);
	InitialSelect = InitialSelect == INDEX_NONE ? 0 : InitialSelect; // default to the empty string located at index 0
	MaterialCategory.AddCustomRow(LOCTEXT("BaseEmmisiveTextureProperty", "Base Emmisive Texture Property")).NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BaseEmmisiveTextureProperty", "Base Emmisive Texture Property"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth(MinDesiredWidth)
			[
				SNew(STextComboBox)
				.OptionsSource(&BaseTextureNames)
				.OnSelectionChanged(this, &FFbxImportUIDetails::OnEmmisiveTextureColor)
				.InitiallySelectedItem(BaseTextureNames[InitialSelect])
			]
		]
	];

	// base specular properties
	InitialSelect = FindString(BaseTextureNames, ImportUI->TextureImportData->BaseSpecularTextureName);
	InitialSelect = InitialSelect == INDEX_NONE ? 0 : InitialSelect; // default to the empty string located at index 0
	MaterialCategory.AddCustomRow(LOCTEXT("BaseSpecularTextureProperty", "Base Specular Texture Property")).NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BaseSpecularTextureProperty", "Base Specular Texture Property"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth(MinDesiredWidth)
			[
				SNew(STextComboBox)
				.OptionsSource(&BaseTextureNames)
				.OnSelectionChanged(this, &FFbxImportUIDetails::OnSpecularTextureColor)
				.InitiallySelectedItem(BaseTextureNames[InitialSelect])
			]
		]
	];
}

void FFbxImportUIDetails::SetStaticMeshLODGroupWidget(IDetailPropertyRow& PropertyRow, const TSharedPtr<IPropertyHandle>& Handle)
{
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;
	PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

	FName InitialValue;
	ensure(Handle->GetValue(InitialValue) == FPropertyAccess::Success);
	int32 GroupIndex = LODGroupNames.Find(InitialValue);
	if (GroupIndex == INDEX_NONE && LODGroupNames.Num() > 0)
	{
		GroupIndex = 0;
	}
	check(GroupIndex != INDEX_NONE);
	StaticMeshLODGroupPropertyHandle = Handle;
	TWeakPtr<IPropertyHandle> HandlePtr = Handle;

	const bool bShowChildren = true;
	PropertyRow.CustomWidget(bShowChildren)
		.NameContent()
		.MinDesiredWidth(Row.NameWidget.MinWidth)
		.MaxDesiredWidth(Row.NameWidget.MaxWidth)
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(Row.ValueWidget.MinWidth)
		.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
		.VAlign(VAlign_Center)
		[
			SNew(STextComboBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OptionsSource(&LODGroupOptions)
			.InitiallySelectedItem(LODGroupOptions[GroupIndex])
			.OnSelectionChanged(this, &FFbxImportUIDetails::OnLODGroupChanged, HandlePtr)
		];
}

void FFbxImportUIDetails::OnLODGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TWeakPtr<IPropertyHandle> HandlePtr)
{
	TSharedPtr<IPropertyHandle> Handle = HandlePtr.Pin();
	if (Handle.IsValid())
	{
		int32 GroupIndex = LODGroupOptions.Find(NewValue);
		check(GroupIndex != INDEX_NONE);
		ensure(Handle->SetValue(LODGroupNames[GroupIndex]) == FPropertyAccess::Success);
	}
}

bool FFbxImportUIDetails::GetVertexOverrideColorEnabledState() const
{
	uint8 VertexColorImportOption;
	check(VertexColorImportOptionHandle.IsValid())
	ensure(VertexColorImportOptionHandle->GetValue(VertexColorImportOption) == FPropertyAccess::Success);

	return (VertexColorImportOption == EVertexColorImportOption::Override);
}

void FFbxImportUIDetails::CollectChildPropertiesRecursive(TSharedPtr<IPropertyHandle> Node, TArray<TSharedPtr<IPropertyHandle>>& OutProperties)
{
	uint32 NodeNumChildren = 0;
	Node->GetNumChildren(NodeNumChildren);

	for(uint32 ChildIdx = 0 ; ChildIdx < NodeNumChildren ; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = Node->GetChildHandle(ChildIdx);
		CollectChildPropertiesRecursive(ChildHandle, OutProperties);

		if(ChildHandle->GetProperty())
		{
			OutProperties.AddUnique(ChildHandle);
		}
	}
}

bool FFbxImportUIDetails::IsImportTypeMetaDataValid(EFBXImportType& ImportType, FString& MetaData)
{
	TArray<FString> Types;
	MetaData.ParseIntoArray(Types, TEXT("|"), 1);
	switch(ImportType)
	{
		case FBXIT_StaticMesh:
			return Types.Contains(TEXT("StaticMesh")) || Types.Contains(TEXT("Mesh"));
		case FBXIT_SkeletalMesh:
			return Types.Contains(TEXT("SkeletalMesh")) || Types.Contains(TEXT("Mesh"));
		case FBXIT_Animation:
			return Types.Contains(TEXT("Animation"));
		default:
			return false;
	}
}

void FFbxImportUIDetails::ImportAutoComputeLodDistancesChanged()
{
	//We need to update the LOD distance UI
	RefreshCustomDetail();
}

void FFbxImportUIDetails::ImportMaterialsChanged()
{
	//We need to update the Base Material UI
	RefreshCustomDetail();
}

void FFbxImportUIDetails::MeshImportModeChanged()
{
	ImportUI->SetMeshTypeToImport();
	RefreshCustomDetail();
}

void FFbxImportUIDetails::ImportMeshToggleChanged()
{
	if(ImportUI->bImportMesh)
	{
		ImportUI->SetMeshTypeToImport();
	}
	else
	{
		ImportUI->MeshTypeToImport = FBXIT_Animation;
	}
	RefreshCustomDetail();
}

void FFbxImportUIDetails::BaseMaterialChanged()
{
	RefreshCustomDetail();
}

void FFbxImportUIDetails::OnBaseColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) {
	ImportUI->TextureImportData->BaseColorName = *Selection.Get();
}
void FFbxImportUIDetails::OnDiffuseTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) {
	ImportUI->TextureImportData->BaseDiffuseTextureName = *Selection.Get();
}
void FFbxImportUIDetails::OnNormalTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) {
	ImportUI->TextureImportData->BaseNormalTextureName = *Selection.Get();
}
void FFbxImportUIDetails::OnEmmisiveTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) {
	ImportUI->TextureImportData->BaseEmmisiveTextureName = *Selection.Get();
}
void FFbxImportUIDetails::OnEmissiveColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) {
	ImportUI->TextureImportData->BaseEmissiveColorName = *Selection.Get();
}
void FFbxImportUIDetails::OnSpecularTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) {
	ImportUI->TextureImportData->BaseSpecularTextureName = *Selection.Get();
}

#undef LOCTEXT_NAMESPACE
