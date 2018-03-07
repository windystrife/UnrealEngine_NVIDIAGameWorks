// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "PanelWidget.generated.h"

/** The base class for all UMG panel widgets.  Panel widgets layout a collection of child widgets. */
UCLASS(Abstract)
class UMG_API UPanelWidget : public UWidget
{
	GENERATED_UCLASS_BODY()

protected:

	/** The slots in the widget holding the child widgets of this panel. */
	UPROPERTY(Instanced)
	TArray<UPanelSlot*> Slots;

public:

	/** Gets number of child widgets in the container. */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	int32 GetChildrenCount() const;

	/**
	 * Gets the widget at an index.
	 * @param Index The index of the widget.
	 * @return The widget at the given index, or nothing if there is no widget there.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	UWidget* GetChildAt(int32 Index) const;

	/** Gets the index of a specific child widget */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	int32 GetChildIndex(UWidget* Content) const;

	/** @return true if panel contains this widget */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	bool HasChild(UWidget* Content) const;

	/** Removes a child by it's index. */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	bool RemoveChildAt(int32 Index);

	/**
	 * Adds a new child widget to the container.  Returns the base slot type, 
	 * requires casting to turn it into the type specific to the container.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	UPanelSlot* AddChild(UWidget* Content);

	/**
	 * Swaps the widget out of the slot at the given index, replacing it with a different widget.
	 *
	 * @param Index the index at which to replace the child content with this new Content.
	 * @param Content The content to replace the child with.
	 * @return true if the index existed and the child could be replaced.
	 */
	bool ReplaceChildAt(int32 Index, UWidget* Content);

#if WITH_EDITOR

	/**
	 * Swaps the child widget out of the slot, and replaces it with the new child widget.
	 * @param CurrentChild The existing child widget being removed.
	 * @param NewChild The new child widget being inserted.
	 * @return true if the CurrentChild was found and the swap occurred, otherwise false.
	 */
	virtual bool ReplaceChild(UWidget* CurrentChild, UWidget* NewChild);

	/**
	 * Inserts a widget at a specific index.  This does not update the live slate version, it requires
	 * a rebuild of the whole UI to see a change.
	 */
	UPanelSlot* InsertChildAt(int32 Index, UWidget* Content);

	/**
	 * Moves the child widget from its current index to the new index provided.
	 */
	void ShiftChild(int32 Index, UWidget* Child);

	void SetDesignerFlags(EWidgetDesignFlags::Type NewFlags);

#endif

	/**
	 * Removes a specific widget from the container.
	 * @return true if the widget was found and removed.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	bool RemoveChild(UWidget* Content);

	/** @return true if there are any child widgets in the panel */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	bool HasAnyChildren() const;

	/** Remove all child widgets from the panel widget. */
	UFUNCTION(BlueprintCallable, Category="Widget|Panel")
	void ClearChildren();

	/** The slots in the widget holding the child widgets of this panel. */
	const TArray<UPanelSlot*>& GetSlots() const;

	/** @returns true if the panel supports more than one child. */
	bool CanHaveMultipleChildren() const
	{
		return bCanHaveMultipleChildren;
	}

	/** @returns true if the panel can accept another child widget. */
	bool CanAddMoreChildren() const
	{
		return CanHaveMultipleChildren() || GetChildrenCount() == 0;
	}

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual bool LockToPanelOnDrag() const
	{
		return false;
	}

	virtual void ConnectEditorData() override
	{
		for ( UPanelSlot* PanelSlot : Slots )
		{
			PanelSlot->Parent = this;
			if ( PanelSlot->Content )
			{
				PanelSlot->Content->Slot = PanelSlot;
			}
		}
	}
#endif

	// Begin UObject
	virtual void PostLoad() override;
	// End UObject

protected:

#if WITH_EDITOR
	virtual TSharedRef<SWidget> RebuildDesignWidget(TSharedRef<SWidget> Content) override;
#endif

	virtual UClass* GetSlotClass() const
	{
		return UPanelSlot::StaticClass();
	}

	virtual void OnSlotAdded(UPanelSlot* InSlot)
	{

	}

	virtual void OnSlotRemoved(UPanelSlot* InSlot)
	{

	}

protected:
	/** Can this panel allow for multiple children? */
	bool bCanHaveMultipleChildren;
};
