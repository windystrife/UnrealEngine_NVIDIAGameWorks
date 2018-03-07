// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WidgetBlueprintFactory.h"
#include "UObject/Interface.h"
#include "Misc/MessageDialog.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"



#include "Components/CanvasPanel.h"
#include "WidgetTree.h"

#define LOCTEXT_NAMESPACE "UWidgetBlueprintFactory"

/*------------------------------------------------------------------------------
	UWidgetBlueprintFactory implementation.
------------------------------------------------------------------------------*/

UWidgetBlueprintFactory::UWidgetBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UWidgetBlueprint::StaticClass();
	ParentClass = UUserWidget::StaticClass();
}

bool UWidgetBlueprintFactory::ConfigureProperties()
{
	// TODO Make config dialog like the anim blueprints.
	return true;
}

bool UWidgetBlueprintFactory::ShouldShowInNewMenu() const
{
	return true;
}

UObject* UWidgetBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Anim Blueprint, then create and init one
	check(Class->IsChildOf(UWidgetBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}

	if ( ( ParentClass == NULL ) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UUserWidget::StaticClass()) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateWidgetBlueprint", "Cannot create a Widget Blueprint based on the class '{ClassName}'."), Args ) );
		return NULL;
	}
	else
	{
		UWidgetBlueprint* NewBP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(), CallingContext));

		// Create a CanvasPanel to use as the default root widget
		if ( NewBP->WidgetTree->RootWidget == nullptr )
		{
			UWidget* Root = NewBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
			NewBP->WidgetTree->RootWidget = Root;
		}

		return NewBP;
	}
}

UObject* UWidgetBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE
