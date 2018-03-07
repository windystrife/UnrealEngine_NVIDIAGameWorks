// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/NamedSlotInterface.h"
#include "Blueprint/UserWidget.h"

#include "WidgetTree.generated.h"

/** The widget tree manages the collection of widgets in a blueprint widget. */
UCLASS()
class UMG_API UWidgetTree : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Begin UObject
	virtual UWorld* GetWorld() const override;
	// End UObject

	/** Finds the widget in the tree by name. */
	UWidget* FindWidget(const FName& Name) const;

	/** Finds a widget in the tree using the native widget as the key. */
	UWidget* FindWidget(TSharedRef<SWidget> InWidget) const;

	/** Finds the widget in the tree by name and casts the return to the desired type. */
	template<class WidgetClass>
	FORCEINLINE WidgetClass* FindWidget(const FName& Name) const
	{
		return Cast<WidgetClass>(FindWidget(Name));
	}

	/** Removes the widget from the hierarchy and all sub widgets. */
	bool RemoveWidget(UWidget* Widget);

	/** Gets the parent widget of a given widget, and potentially the child index. */
	class UPanelWidget* FindWidgetParent(UWidget* Widget, int32& OutChildIndex);

	/** Gathers all the widgets in the tree recursively */
	void GetAllWidgets(TArray<UWidget*>& Widgets) const;

	/** Gathers descendant child widgets of a parent widget. */
	static void GetChildWidgets(UWidget* Parent, TArray<UWidget*>& Widgets);

	/** Attempts to move a constructed Widget to another tree. Returns true on a successful move. */
	static bool TryMoveWidgetToNewTree(UWidget* Widget, UWidgetTree* DestinationTree);

	/**
	 * Iterates through all widgets including widgets contained in named slots, other than
	 * investigating named slots, this code does not dive into foreign WidgetTrees, as would exist
	 * inside another user widget.
	 */
	void ForEachWidget(TFunctionRef<void(UWidget*)> Predicate) const;

	/**
	 * Iterates through all widgets including widgets contained in named slots, other than
	 * investigating named slots.  This includes foreign widget trees inside of other UserWidgets.
	 */
	void ForEachWidgetAndDescendants(TFunctionRef<void(UWidget*)> Predicate) const;

	/**
	 * Iterates through all child widgets including widgets contained in named slots, other than
	 * investigating named slots, this code does not dive into foreign WidgetTrees, as would exist
	 * inside another user widget.
	 */
	static void ForWidgetAndChildren(UWidget* Widget, TFunctionRef<void(UWidget*)> Predicate);

	/** Constructs the widget, and adds it to the tree. */
	template< class T >
	FORCEINLINE T* ConstructWidget(TSubclassOf<UWidget> WidgetType = T::StaticClass(), FName WidgetName = NAME_None)
	{
		EObjectFlags NewObjectFlags = RF_Transactional;
		if (HasAnyFlags(RF_Transient))
		{
			NewObjectFlags |= RF_Transient;
		}

		if ( WidgetType->IsChildOf(UUserWidget::StaticClass()) )
		{
			UUserWidget* Widget = UUserWidget::NewWidgetObject(this, WidgetType, WidgetName);
			Widget->Initialize();
			return (T*)Widget;
		}
		else
		{
			UWidget* Widget = (UWidget*)NewObject<UWidget>(this, WidgetType, WidgetName, NewObjectFlags);
			return (T*)Widget;
		}
	}

	// UObject interface
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostLoad() override;
	// End of UObject interface

public:
	/** The root widget of the tree */
	UPROPERTY(Instanced)
	UWidget* RootWidget;

protected:

	UPROPERTY(Instanced)
	TArray< UWidget* > AllWidgets;
};
