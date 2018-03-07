// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PropertyNode.h"
#include "ObjectPropertyNode.h"
#include "PropertyHandle.h"
#include "IPropertyTableCell.h"
#include "IPropertyTableRow.h"

class FPropertyTableObjectNameColumn;

class FPropertyTableObjectNameCell : public TSharedFromThis< FPropertyTableObjectNameCell >, public IPropertyTableCell
{
public:

	FPropertyTableObjectNameCell( const TSharedRef< class FPropertyTableObjectNameColumn >& InColumn, const TSharedRef< class IPropertyTableRow >& InRow );
	virtual ~FPropertyTableObjectNameCell() {}

	virtual void Refresh() override;

	virtual bool IsReadOnly() const override;
	virtual bool IsBound() const override { return bIsBound; }
	virtual bool InEditMode() const override { return bInEditMode; }
	virtual bool IsValid() const override;

	virtual FString GetValueAsString() const override;
	virtual FText GetValueAsText() const override;
	virtual void SetValueFromString( const FString& InString ) override {}

	virtual TWeakObjectPtr< UObject > GetObject() const override;

	virtual TSharedPtr< class FPropertyNode > GetNode() const override { return NULL; }
	virtual TSharedRef< class IPropertyTableColumn > GetColumn() const override;
	virtual TSharedRef< class IPropertyTableRow > GetRow() const override;
	virtual TSharedRef< class IPropertyTable > GetTable() const override;
	virtual TSharedPtr< class IPropertyHandle > GetPropertyHandle() const override { return NULL; }

	virtual void EnterEditMode() override;
	virtual void ExitEditMode() override;

	DECLARE_DERIVED_EVENT( FPropertyTableObjectNameCell, IPropertyTableCell::FEnteredEditModeEvent, FEnteredEditModeEvent );
	virtual FEnteredEditModeEvent& OnEnteredEditMode() override { return EnteredEditModeEvent; }

	DECLARE_DERIVED_EVENT( FPropertyTableObjectNameCell, IPropertyTableCell::FExitedEditModeEvent, FExitedEditModeEvent  );
	virtual FExitedEditModeEvent& OnExitedEditMode() override { return ExitedEditModeEvent; }


private:

	FEnteredEditModeEvent EnteredEditModeEvent;
	FExitedEditModeEvent ExitedEditModeEvent;

	TWeakPtr< class FPropertyTableObjectNameColumn > Column;
	TWeakPtr< class IPropertyTableRow > Row;

	TSharedPtr< class FObjectPropertyNode > ObjectNode;
	FString ObjectName;

	bool bIsBound;
	bool bInEditMode;
};
