// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Details/SPropertyBinding.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"

#if WITH_EDITOR
	#include "Components/PrimitiveComponent.h"
	#include "Components/StaticMeshComponent.h"
	#include "Engine/BlueprintGeneratedClass.h"
#endif // WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Animation/WidgetAnimation.h"
#include "WidgetBlueprint.h"

#include "DetailLayoutBuilder.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"
#include "WidgetGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Components/WidgetComponent.h"
#include "Binding/PropertyBinding.h"


#define LOCTEXT_NAMESPACE "UMG"


/////////////////////////////////////////////////////
// SPropertyBinding

void SPropertyBinding::Construct(const FArguments& InArgs, TSharedRef<FWidgetBlueprintEditor> InEditor, UDelegateProperty* DelegateProperty, TSharedRef<IPropertyHandle> Property)
{
	Editor = InEditor;
	Blueprint = InEditor->GetWidgetBlueprintObj();

	GeneratePureBindings = InArgs._GeneratePureBindings;
	BindableSignature = DelegateProperty->SignatureFunction;

	TArray<UObject*> Objects;
	Property->GetOuterObjects(Objects);

	UWidget* Widget = CastChecked<UWidget>(Objects[0]);

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &SPropertyBinding::OnGenerateDelegateMenu, Widget, Property)
			.ContentPadding(1)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(this, &SPropertyBinding::GetCurrentBindingImage, Property)
					.ColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 1, 0, 0)
				[
					SNew(STextBlock)
					.Text(this, &SPropertyBinding::GetCurrentBindingText, Property)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.Visibility(this, &SPropertyBinding::GetGotoBindingVisibility, Property)
			.OnClicked(this, &SPropertyBinding::HandleGotoBindingClicked, Property)
			.VAlign(VAlign_Center)
			.ToolTipText(LOCTEXT("GotoFunction", "Goto Function"))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Browse"))
			]
		]
	];
}

static bool IsClassBlackListed(UClass* OwnerClass)
{
	if ( OwnerClass == UUserWidget::StaticClass() ||
		OwnerClass == AActor::StaticClass() ||
		OwnerClass == APawn::StaticClass() ||
		OwnerClass == UObject::StaticClass() ||
		OwnerClass == UPrimitiveComponent::StaticClass() ||
		OwnerClass == USceneComponent::StaticClass() ||
		OwnerClass == UActorComponent::StaticClass() ||
		OwnerClass == UWidgetComponent::StaticClass() ||
		OwnerClass == UStaticMeshComponent::StaticClass() ||
		OwnerClass == UWidgetAnimation::StaticClass() )
	{
		return true;
	}

	return false;
}

static bool IsFieldFromBlackListedClass(UField* Field)
{
	return IsClassBlackListed(Field->GetOwnerClass());
}

static bool HasFunctionBinder(UFunction* Function, UFunction* BindableSignature)
{
	if ( Function->NumParms == 1 && BindableSignature->NumParms == 1 )
	{
		if ( UProperty* FunctionReturn = Function->GetReturnProperty() )
		{
			if ( UProperty* DelegateReturn = BindableSignature->GetReturnProperty() )
			{
				// Find the binder that can handle the delegate return type.
				TSubclassOf<UPropertyBinding> Binder = UWidget::FindBinderClassForDestination(DelegateReturn);
				if ( Binder != nullptr )
				{
					// Ensure that the binder also can handle binding from the property we care about.
					if ( Binder->GetDefaultObject<UPropertyBinding>()->IsSupportedSource(FunctionReturn) )
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

template <typename Predicate>
void SPropertyBinding::ForEachBindableFunction(UClass* FromClass, Predicate Pred) const
{
	const UWidgetGraphSchema* Schema = GetDefault<UWidgetGraphSchema>();
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	UBlueprintGeneratedClass* SkeletonClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);

	// Walk up class hierarchy for native functions and properties
	for ( TFieldIterator<UFunction> FuncIt(FromClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt )
	{
		UFunction* Function = *FuncIt;

		// Stop processing functions after reaching a base class that it doesn't make sense to go beyond.
		if ( IsFieldFromBlackListedClass(Function) )
		{
			break;
		}

		// Only allow binding pure functions if we're limited to pure function bindings.
		if ( GeneratePureBindings && !Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) )
		{
			continue;
		}

		// Only bind to functions that are callable from blueprints
		if ( !UEdGraphSchema_K2::CanUserKismetCallFunction(Function) )
		{
			continue;
		}

		// We ignore CPF_ReturnParm because all that matters for binding to script functions is that the number of out parameters match.
		if ( Function->IsSignatureCompatibleWith(BindableSignature, UFunction::GetDefaultIgnoredSignatureCompatibilityFlags() | CPF_ReturnParm) ||
			 HasFunctionBinder(Function, BindableSignature) )
		{
			TSharedPtr<FFunctionInfo> Info = MakeShareable(new FFunctionInfo());
			Info->DisplayName = FText::FromName(Function->GetFName());
			Info->Tooltip = Function->GetMetaData("Tooltip");
			Info->FuncName = Function->GetFName();
			Info->Function = Function;

			Pred(Info);
		}
	}
}

template <typename Predicate>
void SPropertyBinding::ForEachBindableProperty(UStruct* InStruct, Predicate Pred) const
{
	UBlueprintGeneratedClass* SkeletonClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);

	for ( TFieldIterator<UProperty> PropIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt )
	{
		UProperty* Property = *PropIt;

		// Stop processing properties after reaching the stoppped base class
		if ( IsFieldFromBlackListedClass(Property) )
		{
			break;
		}

		if ( !UEdGraphSchema_K2::CanUserKismetAccessVariable(Property, SkeletonClass, UEdGraphSchema_K2::CannotBeDelegate) )
		{
			continue;
		}

		// Also ignore advanced properties
		if ( Property->HasAnyPropertyFlags(CPF_AdvancedDisplay | CPF_EditorOnly) )
		{
			continue;
		}

		// Add matching properties, ensure they return the same type as the property.
		if ( UProperty* ReturnProperty = BindableSignature->GetReturnProperty() )
		{
			// Find the binder that can handle the delegate return type.
			TSubclassOf<UPropertyBinding> Binder = UWidget::FindBinderClassForDestination(ReturnProperty);
			if ( Binder != nullptr )
			{
				// Ensure that the binder also can handle binding from the property we care about.
				if ( Binder->GetDefaultObject<UPropertyBinding>()->IsSupportedSource(Property) )
				{
					Pred(Property);
				}
			}
		}
	}
}

TSharedRef<SWidget> SPropertyBinding::OnGenerateDelegateMenu(UWidget* Widget, TSharedRef<IPropertyHandle> PropertyHandle)
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr);

	MenuBuilder.BeginSection("BindingActions");
	{
		if ( CanRemoveBinding(PropertyHandle) )
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RemoveBinding", "Remove Binding"),
				LOCTEXT("RemoveBindingToolTip", "Removes the current binding"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Cross"),
				FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleRemoveBinding, PropertyHandle))
				);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateBinding", "Create Binding"),
			LOCTEXT("CreateBindingToolTip", "Creates a new function on the widget blueprint that will return the binding data for this property."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Plus"),
			FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleCreateAndAddBinding, Widget, PropertyHandle))
			);
	}
	MenuBuilder.EndSection(); //CreateBinding

	// Properties
	{
		// Get the current skeleton class, think header for the blueprint.
		UBlueprintGeneratedClass* SkeletonClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);

		TArray<UField*> BindingChain;
		FillPropertyMenu(MenuBuilder, PropertyHandle, SkeletonClass, BindingChain);
	}

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

	return
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.MaxHeight(DisplayMetrics.PrimaryDisplayHeight * 0.5)
		[
			MenuBuilder.MakeWidget()
		];
}

void SPropertyBinding::FillPropertyMenu(FMenuBuilder& MenuBuilder, TSharedRef<IPropertyHandle> PropertyHandle, UStruct* OwnerStruct, TArray<UField*> BindingChain)
{
	bool bFoundEntry = false;

	//---------------------------------------
	// Function Bindings

	if ( UClass* OwnerClass = Cast<UClass>(OwnerStruct) )
	{
		static FName FunctionIcon(TEXT("GraphEditor.Function_16x"));

		MenuBuilder.BeginSection("Functions", LOCTEXT("Functions", "Functions"));
		{
			ForEachBindableFunction(OwnerClass, [&] (TSharedPtr<FFunctionInfo> Info) {
				TArray<UField*> NewBindingChain(BindingChain);
				NewBindingChain.Add(Info->Function);

				bFoundEntry = true;

				MenuBuilder.AddMenuEntry(
					Info->DisplayName,
					FText::FromString(Info->Tooltip),
					FSlateIcon(FEditorStyle::GetStyleSetName(), FunctionIcon),
					FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleAddFunctionBinding, PropertyHandle, Info, NewBindingChain))
					);
			});
		}
		MenuBuilder.EndSection(); //Functions
	}

	//---------------------------------------
	// Property Bindings

	// Get the current skeleton class, think header for the blueprint.
	UBlueprintGeneratedClass* SkeletonClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);

	// Only show bindable subobjects and variables if we're generating pure bindings.
	if ( GeneratePureBindings )
	{
		UProperty* ReturnProperty = BindableSignature->GetReturnProperty();

		// Find the binder that can handle the delegate return type, don't bother allowing people 
		// to look for bindings that we don't support
		if ( ensure(UWidget::FindBinderClassForDestination(ReturnProperty) != nullptr) )
		{
			static FName PropertyIcon(TEXT("Kismet.Tabs.Variables"));

			MenuBuilder.BeginSection("Properties", LOCTEXT("Properties", "Properties"));
			{
				ForEachBindableProperty(OwnerStruct, [&] (UProperty* Property) {
					TArray<UField*> NewBindingChain(BindingChain);
					NewBindingChain.Add(Property);

					bFoundEntry = true;

					MenuBuilder.AddMenuEntry(
						Property->GetDisplayNameText(),
						Property->GetToolTipText(),
						FSlateIcon(FEditorStyle::GetStyleSetName(), PropertyIcon),
						FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleAddPropertyBinding, PropertyHandle, Property, NewBindingChain))
						);
				});
			}
			MenuBuilder.EndSection(); //Properties

			MenuBuilder.BeginSection("SubObjectProperties", LOCTEXT("SubObjectProperties", "Sub-Object Properties"));
			{
				// Add all the properties that are not bindable, but are object or struct members that could contain children
				// properties that are bindable.
				for ( TFieldIterator<UProperty> PropIt(OwnerStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt )
				{
					UProperty* Property = *PropIt;

					// Stop processing properties after reaching the user widget properties.
					if ( IsFieldFromBlackListedClass(Property) )
					{
						break;
					}

					// If the owner is a class then use the blueprint scheme to determine if it's visible.
					if ( !UEdGraphSchema_K2::CanUserKismetAccessVariable(Property, SkeletonClass, UEdGraphSchema_K2::CannotBeDelegate) )
					{
						continue;
					}

					if ( Property->HasAllPropertyFlags(CPF_BlueprintVisible) )
					{
						UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
						UWeakObjectProperty* WeakObjectProperty = Cast<UWeakObjectProperty>(Property);
						UStructProperty* StructProperty = Cast<UStructProperty>(Property);

						UStruct* Struct = nullptr;
						UClass* Class = nullptr;

						if ( ObjectProperty )
						{
							Struct = Class = ObjectProperty->PropertyClass;
						}
						else if ( WeakObjectProperty )
						{
							Struct = Class = WeakObjectProperty->PropertyClass;
						}
						else if ( StructProperty )
						{
							Struct = StructProperty->Struct;
						}

						if ( Struct )
						{
							if ( Class )
							{
								// Ignore any properties that are widgets, we don't want users binding widgets to other widgets.
								// also ignore any class that is explicitly on the black list.
								if ( IsClassBlackListed(Class) || Class->IsChildOf(UWidget::StaticClass()) )
								{
									continue;
								}
							}

							// Stop processing properties after reaching the user widget properties.
							if ( IsFieldFromBlackListedClass(Property) )
							{
								break;
							}

							TArray<UField*> NewBindingChain(BindingChain);
							NewBindingChain.Add(Property);

							bFoundEntry = true;

							MenuBuilder.AddSubMenu(
								Property->GetDisplayNameText(),
								Property->GetToolTipText(),
								FNewMenuDelegate::CreateSP(this, &SPropertyBinding::FillPropertyMenu, PropertyHandle, Struct, NewBindingChain)
								);
						}
					}
				}
			}
			MenuBuilder.EndSection(); //SubObjectProperties
		}
	}

	if ( bFoundEntry == false && OwnerStruct != SkeletonClass )
	{
		MenuBuilder.BeginSection("None", OwnerStruct->GetDisplayNameText());
		MenuBuilder.AddWidget(SNew(STextBlock).Text(LOCTEXT("None", "None")), FText::GetEmpty());
		MenuBuilder.EndSection(); //None
	}
}

const FSlateBrush* SPropertyBinding::GetCurrentBindingImage(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	static FName PropertyIcon(TEXT("Kismet.Tabs.Variables"));
	static FName FunctionIcon(TEXT("GraphEditor.Function_16x"));

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	//TODO UMG O(N) Isn't good for this, needs to be map, but map isn't serialized, need cached runtime map for fast lookups.

	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	for ( int32 ObjectIndex = 0; ObjectIndex < OuterObjects.Num(); ObjectIndex++ )
	{
		// Ignore null outer objects
		if ( OuterObjects[ObjectIndex] == NULL )
		{
			continue;
		}

		//TODO UMG handle multiple things selected

		for ( const FDelegateEditorBinding& Binding : Blueprint->Bindings )
		{
			if ( Binding.ObjectName == OuterObjects[ObjectIndex]->GetName() && Binding.PropertyName == PropertyName )
			{
				if ( Binding.Kind == EBindingKind::Function )
				{
					return FEditorStyle::GetBrush(FunctionIcon);
				}
				else // Property
				{
					return FEditorStyle::GetBrush(PropertyIcon);
				}
			}
		}
	}

	return nullptr;
}

FText SPropertyBinding::GetCurrentBindingText(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	//TODO UMG O(N) Isn't good for this, needs to be map, but map isn't serialized, need cached runtime map for fast lookups.

	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	for ( int32 ObjectIndex = 0; ObjectIndex < OuterObjects.Num(); ObjectIndex++ )
	{
		// Ignore null outer objects
		if ( OuterObjects[ObjectIndex] == nullptr )
		{
			continue;
		}

		//TODO UMG handle multiple things selected

		for ( const FDelegateEditorBinding& Binding : Blueprint->Bindings )
		{
			if ( Binding.ObjectName == OuterObjects[ObjectIndex]->GetName() && Binding.PropertyName == PropertyName )
			{
				if ( !Binding.SourcePath.IsEmpty() )
				{
					return Binding.SourcePath.GetDisplayText();
				}
				else
				{
					if ( Binding.Kind == EBindingKind::Function )
					{
						if ( Binding.MemberGuid.IsValid() )
						{
							// Graph function, look up by Guid
							FName FoundName = Blueprint->GetFieldNameFromClassByGuid<UFunction>(Blueprint->GeneratedClass, Binding.MemberGuid);
							return FText::FromString(FName::NameToDisplayString(FoundName.ToString(), false));
						}
						else
						{
							// No GUID, native function, return function name.
							return FText::FromName(Binding.FunctionName);
						}
					}
					else // Property
					{
						if ( Binding.MemberGuid.IsValid() )
						{
							FName FoundName = Blueprint->GetFieldNameFromClassByGuid<UProperty>(Blueprint->GeneratedClass, Binding.MemberGuid);
							return FText::FromString(FName::NameToDisplayString(FoundName.ToString(), false));
						}
						else
						{
							// No GUID, native property, return source property.
							return FText::FromName(Binding.SourceProperty);
						}
					}
				}
			}
		}

		//TODO UMG Do something about missing functions, little exclamation points if they're missing and such.

		break;
	}

	return LOCTEXT("Bind", "Bind");
}

bool SPropertyBinding::CanRemoveBinding(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FName PropertyName = PropertyHandle->GetProperty()->GetFName();

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for ( UObject* SelectedObject : OuterObjects )
	{
		for ( const FDelegateEditorBinding& Binding : Blueprint->Bindings )
		{
			if ( Binding.ObjectName == SelectedObject->GetName() && Binding.PropertyName == PropertyName )
			{
				return true;
			}
		}
	}

	return false;
}

void SPropertyBinding::HandleRemoveBinding(TSharedRef<IPropertyHandle> PropertyHandle)
{
	const FScopedTransaction Transaction(LOCTEXT("UnbindDelegate", "Remove Binding"));

	Blueprint->Modify();

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for ( UObject* SelectedObject : OuterObjects )
	{
		FDelegateEditorBinding Binding;
		Binding.ObjectName = SelectedObject->GetName();
		Binding.PropertyName = PropertyHandle->GetProperty()->GetFName();

		Blueprint->Bindings.Remove(Binding);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void SPropertyBinding::HandleAddFunctionBinding(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<FFunctionInfo> SelectedFunction, TArray<UField*> BindingChain)
{
	FEditorPropertyPath BindingPath(BindingChain);
	HandleAddFunctionBinding(PropertyHandle, SelectedFunction, BindingPath);
}

void SPropertyBinding::HandleAddFunctionBinding(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<FFunctionInfo> SelectedFunction, FEditorPropertyPath& BindingPath)
{
	const FScopedTransaction Transaction(LOCTEXT("BindDelegate", "Set Binding"));

	Blueprint->Modify();

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for ( UObject* SelectedObject : OuterObjects )
	{
		FDelegateEditorBinding Binding;
		Binding.ObjectName = SelectedObject->GetName();
		Binding.PropertyName = PropertyHandle->GetProperty()->GetFName();
		Binding.FunctionName = SelectedFunction->FuncName;

		Binding.SourcePath = BindingPath;

		if ( SelectedFunction->Function )
		{
			UBlueprint::GetGuidFromClassByFieldName<UFunction>(
				SelectedFunction->Function->GetOwnerClass(),
				SelectedFunction->Function->GetFName(),
				Binding.MemberGuid);
		}

		Binding.Kind = EBindingKind::Function;

		Blueprint->Bindings.Remove(Binding);
		Blueprint->Bindings.AddUnique(Binding);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void SPropertyBinding::HandleAddPropertyBinding(TSharedRef<IPropertyHandle> PropertyHandle, UProperty* SelectedProperty, TArray<UField*> BindingChain)
{
	const FScopedTransaction Transaction(LOCTEXT("BindDelegate", "Set Binding"));

	// Get the current skeleton class, think header for the blueprint.
	UBlueprintGeneratedClass* SkeletonClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);

	Blueprint->Modify();

	FGuid MemberGuid;
	UBlueprint::GetGuidFromClassByFieldName<UProperty>(SkeletonClass, SelectedProperty->GetFName(), MemberGuid);

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for ( UObject* SelectedObject : OuterObjects )
	{
		FDelegateEditorBinding Binding;
		Binding.ObjectName = SelectedObject->GetName();
		Binding.PropertyName = PropertyHandle->GetProperty()->GetFName();
		Binding.SourceProperty = SelectedProperty->GetFName();
		Binding.SourcePath = FEditorPropertyPath(BindingChain);
		Binding.MemberGuid = MemberGuid;
		Binding.Kind = EBindingKind::Property;

		Blueprint->Bindings.Remove(Binding);
		Blueprint->Bindings.AddUnique(Binding);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void SPropertyBinding::HandleCreateAndAddBinding(UWidget* Widget, TSharedRef<IPropertyHandle> PropertyHandle)
{
	const FScopedTransaction Transaction(LOCTEXT("CreateDelegate", "Create Binding"));

	Blueprint->Modify();

	FString Pre = GeneratePureBindings ? FString(TEXT("Get")) : FString(TEXT("On"));

	FString WidgetName;
	if ( Widget && !Widget->IsGeneratedName() )
	{
		WidgetName = TEXT("_") + Widget->GetName() + TEXT("_");
	}

	FString Post = PropertyHandle->GetProperty()->GetName();
	Post.RemoveFromStart(TEXT("On"));
	Post.RemoveFromEnd(TEXT("Event"));

	// Create the function graph.
	FString FunctionName = Pre + WidgetName + Post;
	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, 
		FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	
	// Add the binding to the blueprint
	TSharedPtr<FFunctionInfo> SelectedFunction = MakeShareable(new FFunctionInfo());
	SelectedFunction->FuncName = FunctionGraph->GetFName();

	FEditorPropertyPath BindingPath;
	BindingPath.Segments.Add(FEditorPropertyPathSegment(FunctionGraph));

	HandleAddFunctionBinding(PropertyHandle, SelectedFunction, BindingPath);

	const bool bUserCreated = true;
	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FunctionGraph, bUserCreated, BindableSignature);

	// Only mark bindings as pure that need to be.
	if ( GeneratePureBindings )
	{
		const UEdGraphSchema_K2* Schema_K2 = Cast<UEdGraphSchema_K2>(FunctionGraph->GetSchema());
		Schema_K2->AddExtraFunctionFlags(FunctionGraph, FUNC_BlueprintPure);
	}

	GotoFunction(FunctionGraph);
}

EVisibility SPropertyBinding::GetGotoBindingVisibility(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	//TODO UMG O(N) Isn't good for this, needs to be map, but map isn't serialized, need cached runtime map for fast lookups.

	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	for ( int32 ObjectIndex = 0; ObjectIndex < OuterObjects.Num(); ObjectIndex++ )
	{
		// Ignore null outer objects
		if ( OuterObjects[ObjectIndex] == nullptr )
		{
			continue;
		}

		//TODO UMG handle multiple things selected

		for ( const FDelegateEditorBinding& Binding : Blueprint->Bindings )
		{
			if ( Binding.ObjectName == OuterObjects[ObjectIndex]->GetName() && Binding.PropertyName == PropertyName )
			{
				if ( Binding.Kind == EBindingKind::Function )
				{
					return EVisibility::Visible;
				}
			}
		}
	}

	return EVisibility::Collapsed;
}

FReply SPropertyBinding::HandleGotoBindingClicked(TSharedRef<IPropertyHandle> PropertyHandle)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	//TODO UMG O(N) Isn't good for this, needs to be map, but map isn't serialized, need cached runtime map for fast lookups.

	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	for ( int32 ObjectIndex = 0; ObjectIndex < OuterObjects.Num(); ObjectIndex++ )
	{
		// Ignore null outer objects
		if ( OuterObjects[ObjectIndex] == nullptr )
		{
			continue;
		}

		//TODO UMG handle multiple things selected

		for ( const FDelegateEditorBinding& Binding : Blueprint->Bindings )
		{
			if ( Binding.ObjectName == OuterObjects[ObjectIndex]->GetName() && Binding.PropertyName == PropertyName )
			{
				if ( Binding.Kind == EBindingKind::Function )
				{
					TArray<UEdGraph*> AllGraphs;
					Blueprint->GetAllGraphs(AllGraphs);

					FGuid SearchForGuid = Binding.MemberGuid;
					if ( !Binding.SourcePath.IsEmpty() )
					{
						SearchForGuid = Binding.SourcePath.Segments.Last().GetMemberGuid();
					}

					for ( UEdGraph* Graph : AllGraphs )
					{
						if ( Graph->GraphGuid == SearchForGuid )
						{
							GotoFunction(Graph);
						}
					}

					// Either way return
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

void SPropertyBinding::GotoFunction(UEdGraph* FunctionGraph)
{
	Editor.Pin()->SetCurrentMode(FWidgetBlueprintApplicationModes::GraphMode);

	Editor.Pin()->OpenDocument(FunctionGraph, FDocumentTracker::OpenNewDocument);
}


#undef LOCTEXT_NAMESPACE
