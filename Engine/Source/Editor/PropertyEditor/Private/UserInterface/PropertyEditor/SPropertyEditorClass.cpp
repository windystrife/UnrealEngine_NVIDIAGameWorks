// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorClass.h"
#include "Engine/Blueprint.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"

#include "DragAndDrop/ClassDragDropOp.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"


#define LOCTEXT_NAMESPACE "PropertyEditor"

class FPropertyEditorClassFilter : public IClassViewerFilter
{
public:
	/** The meta class for the property that classes must be a child-of. */
	const UClass* ClassPropertyMetaClass;

	/** The interface that must be implemented. */
	const UClass* InterfaceThatMustBeImplemented;

	/** Whether or not abstract classes are allowed. */
	bool bAllowAbstract;

	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated) &&
			(bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract));

		if(bMatchesFlags && InClass->IsChildOf(ClassPropertyMetaClass)
			&& (!InterfaceThatMustBeImplemented || InClass->ImplementsInterface(InterfaceThatMustBeImplemented)))
		{
			return true;
		}

		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated) &&
			(bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract));

		if(bMatchesFlags && InClass->IsChildOf(ClassPropertyMetaClass)
			&& (!InterfaceThatMustBeImplemented || InClass->ImplementsInterface(InterfaceThatMustBeImplemented)))
		{
			return true;
		}

		return false;
	}
};

void SPropertyEditorClass::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 400.0f;
}

bool SPropertyEditorClass::Supports(const TSharedRef< class FPropertyEditor >& InPropertyEditor)
{
	if(InPropertyEditor->IsEditConst())
	{
		return false;
	}

	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();
	int32 ArrayIndex = PropertyNode->GetArrayIndex();

	if ((Property->IsA(UClassProperty::StaticClass()) || Property->IsA(USoftClassProperty::StaticClass())) 
		&& ((ArrayIndex == -1 && Property->ArrayDim == 1) || (ArrayIndex > -1 && Property->ArrayDim > 0)))
	{
		return true;
	}

	return false;
}

void SPropertyEditorClass::Construct(const FArguments& InArgs, const TSharedPtr< class FPropertyEditor >& InPropertyEditor)
{
	PropertyEditor = InPropertyEditor;
	
	if (PropertyEditor.IsValid())
	{
		const TSharedRef<FPropertyNode> PropertyNode = PropertyEditor->GetPropertyNode();
		UProperty* const Property = PropertyNode->GetProperty();
		if (UClassProperty* const ClassProp = Cast<UClassProperty>(Property))
		{
			MetaClass = ClassProp->MetaClass;
		}
		else if (USoftClassProperty* const SoftClassProperty = Cast<USoftClassProperty>(Property))
		{
			MetaClass = SoftClassProperty->MetaClass;
		}
		else
		{
			check(false);
		}
		
		bAllowAbstract = Property->GetOwnerProperty()->HasMetaData(TEXT("AllowAbstract"));
		bAllowOnlyPlaceable = Property->GetOwnerProperty()->HasMetaData(TEXT("OnlyPlaceable"));
		bIsBlueprintBaseOnly = Property->GetOwnerProperty()->HasMetaData(TEXT("BlueprintBaseOnly"));
		RequiredInterface = Property->GetOwnerProperty()->GetClassMetaData(TEXT("MustImplement"));
		bAllowNone = !(Property->PropertyFlags & CPF_NoClear);
		bShowViewOptions = Property->GetOwnerProperty()->HasMetaData(TEXT("HideViewOptions")) ? false : true;
		bShowTree = Property->GetOwnerProperty()->HasMetaData(TEXT("ShowTreeView"));
	}
	else
	{
		check(InArgs._MetaClass);
		check(InArgs._SelectedClass.IsSet());
		check(InArgs._OnSetClass.IsBound());

		MetaClass = InArgs._MetaClass;
		RequiredInterface = InArgs._RequiredInterface;
		bAllowAbstract = InArgs._AllowAbstract;
		bIsBlueprintBaseOnly = InArgs._IsBlueprintBaseOnly;
		bAllowNone = InArgs._AllowNone;
		bAllowOnlyPlaceable = false;
		bShowViewOptions = InArgs._ShowViewOptions;
		bShowTree = InArgs._ShowTree;

		SelectedClass = InArgs._SelectedClass;
		OnSetClass = InArgs._OnSetClass;


	}
	
	SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SPropertyEditorClass::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SPropertyEditorClass::GetDisplayValueAsString)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SPropertyEditorClass::GetDisplayValueAsString)
			.Font(InArgs._Font)
		];

	ChildSlot
	[
		ComboButton.ToSharedRef()
	];
}

/** Util to give better names for BP generated classes */
static FString GetClassDisplayName(const UObject* Object)
{
	const UClass* Class = Cast<UClass>(Object);
	if (Class != NULL)
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		if(BP != NULL)
		{
			return BP->GetName();
		}
	}
	return (Object) ? Object->GetName() : "None";
}

FText SPropertyEditorClass::GetDisplayValueAsString() const
{
	static bool bIsReentrant = false;

	// Guard against re-entrancy which can happen if the delegate executed below (SelectedClass.Get()) forces a slow task dialog to open, thus causing this to lose context and regain focus later starting the loop over again
	if( !bIsReentrant )
	{
		TGuardValue<bool>( bIsReentrant, true );
		if(PropertyEditor.IsValid())
		{
			UObject* ObjectValue = NULL;
			FPropertyAccess::Result Result = PropertyEditor->GetPropertyHandle()->GetValue(ObjectValue);

			if(Result == FPropertyAccess::Success && ObjectValue != NULL)
			{
				return FText::FromString(GetClassDisplayName(ObjectValue));
			}

			return FText::FromString(FPaths::GetBaseFilename(PropertyEditor->GetValueAsString()));
		}

		return FText::FromString(GetClassDisplayName(SelectedClass.Get()));
	}
	else
	{
		return FText::GetEmpty();
	}
	
}

TSharedRef<SWidget> SPropertyEditorClass::GenerateClassPicker()
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowNoneOption = bAllowNone; 

	if(PropertyEditor.IsValid())
	{
		Options.PropertyHandle = PropertyEditor->GetPropertyHandle();
	}

	TSharedPtr<FPropertyEditorClassFilter> ClassFilter = MakeShareable(new FPropertyEditorClassFilter);
	Options.ClassFilter = ClassFilter;
	ClassFilter->ClassPropertyMetaClass = MetaClass;
	ClassFilter->InterfaceThatMustBeImplemented = RequiredInterface;
	ClassFilter->bAllowAbstract = bAllowAbstract;
	Options.bIsBlueprintBaseOnly = bIsBlueprintBaseOnly;
	Options.bIsPlaceableOnly = bAllowOnlyPlaceable;
	Options.DisplayMode = bShowTree ? EClassViewerDisplayMode::TreeView : EClassViewerDisplayMode::ListView;
	Options.bAllowViewOptions = bShowViewOptions;

	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SPropertyEditorClass::OnClassPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked)
			]			
		];
}

void SPropertyEditorClass::OnClassPicked(UClass* InClass)
{
	if(!InClass)
	{
		SendToObjects(TEXT("None"));
	}
	else
	{
		SendToObjects(InClass->GetPathName());
	}

	ComboButton->SetIsOpen(false);
}

void SPropertyEditorClass::SendToObjects(const FString& NewValue)
{
	if(PropertyEditor.IsValid())
	{
		const TSharedRef<IPropertyHandle> PropertyHandle = PropertyEditor->GetPropertyHandle();
		PropertyHandle->SetValueFromFormattedString(NewValue);
	}
	else
	{
		UClass* NewClass = FindObject<UClass>(ANY_PACKAGE, *NewValue);
		if(!NewClass)
		{
			NewClass = LoadObject<UClass>(nullptr, *NewValue);
		}
		OnSetClass.Execute(NewClass);
	}
}

FReply SPropertyEditorClass::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FClassDragDropOp> ClassOperation = DragDropEvent.GetOperationAs<FClassDragDropOp>();
	if (ClassOperation.IsValid())
	{
		// We can only drop one item into the combo box, so drop the first one.
		FString AssetName = ClassOperation->ClassesToDrop[0]->GetName();

		// Set the property, it will be verified as valid.
		SendToObjects(AssetName);

		return FReply::Handled();
	}
	
	TSharedPtr<FUnloadedClassDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FUnloadedClassDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		// Check if the asset is loaded, used to see if the context menu should be available
		bool bAllAssetWereLoaded = true;

		TArray<FClassPackageData>& AssetArray = *(UnloadedClassOp->AssetsToDrop.Get());

		// We can only drop one item into the combo box, so drop the first one.
		FString& AssetName = AssetArray[0].AssetName;

		// Check to see if the asset can be found, otherwise load it.
		UObject* Object = FindObject<UObject>(NULL, *AssetName);
		if(Object == NULL)
		{
			// Check to see if the dropped asset was a blueprint
			const FString& PackageName = AssetArray[0].GeneratedPackageName;
			Object = FindObject<UObject>(NULL, *FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName));

			if(Object == NULL)
			{
				// Load the package.
				GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);
				UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_NoRedirects );
				if(Package)
				{
					Package->FullyLoad();
				}
				GWarn->EndSlowTask();

				Object = FindObject<UObject>(Package, *AssetName);
			}

			if(Object->IsA(UBlueprint::StaticClass()))
			{
				// Get the default object from the generated class.
				Object = Cast<UBlueprint>(Object)->GeneratedClass->GetDefaultObject();
			}
		}

		// Set the property, it will be verified as valid.
		SendToObjects(AssetName);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
