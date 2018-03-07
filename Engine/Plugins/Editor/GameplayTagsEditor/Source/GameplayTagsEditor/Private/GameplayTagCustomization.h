// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Editor/PropertyEditor/Public/IPropertyTypeCustomization.h"
#include "SGameplayTagWidget.h"
#include "EditorUndoClient.h"

class IPropertyHandle;

/** Customization for the gameplay tag struct */
class FGameplayTagCustomization : public IPropertyTypeCustomization, public FEditorUndoClient
{
public:

	~FGameplayTagCustomization();

	/** Overridden to show an edit button to launch the gameplay tag editor */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	/** Overridden to do nothing */
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FEditorUndoClient Interface

private:

	/** Updates the selected tag*/
	void OnTagChanged();

	/** Updates the selected tag*/
	void OnPropertyValueChanged();

	/** Build Editable Container */
	void BuildEditableContainerList();

	/** Callback function to create content for the combo button. */
	TSharedRef<SWidget> GetListContent();

	/** Returns Tag name currently selected*/
	FText SelectedTag() const;

	/** Cached property handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Tag Container used for the GameplayTagWidget. */
	TSharedPtr<FGameplayTagContainer> TagContainer;

	/** Editable Container for holding our tag */
	TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;

	/** Tag name selected*/
	FString TagName;

	void OnTagDoubleClicked();
	EVisibility GetVisibilityForTagTextBlockWidget(bool ForTextWidget) const;
};

