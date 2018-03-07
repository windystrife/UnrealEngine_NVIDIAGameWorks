// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "Presentation/PropertyTable/PropertyTableCell.h"
#include "IPropertyTable.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"

FPropertyTableCell::FPropertyTableCell( const TSharedRef< class IPropertyTableColumn >& InColumn, const TSharedRef< class IPropertyTableRow >& InRow )
	: bIsBound( false )
	, bInEditMode( false )
	, EnteredEditModeEvent()
	, ExitedEditModeEvent()
	, Column( InColumn )
	, Row( InRow )
	, PropertyNode( NULL )
	, ObjectNode( NULL )
	, PropertyEditor( NULL )
{
	Refresh();
}

void FPropertyTableCell::Refresh()
{
	const TSharedRef< IPropertyTableColumn > ColumnRef = Column.Pin().ToSharedRef();
	const TSharedRef< IPropertyTableRow > RowRef = Row.Pin().ToSharedRef();

	bIsBound = false;
	ObjectNode = GetTable()->GetObjectPropertyNode( ColumnRef, RowRef );

	if ( !ObjectNode.IsValid() )
	{
		return;
	}

	TSharedRef< IDataSource > ColumnBoundData = ColumnRef->GetDataSource();
	TSharedRef< IDataSource > RowBoundData = RowRef->GetDataSource();

	if ( !ColumnBoundData->IsValid() || !RowBoundData->IsValid() )
	{
		return;
	}

	TSharedPtr< FPropertyPath > PropertyPath;
	TWeakObjectPtr< UObject > Item;

	Item = ColumnBoundData->AsUObject();
	if ( !Item.IsValid() )
	{
		Item = RowBoundData->AsUObject();
	}

	if ( !Item.IsValid() )
	{
		// Must have a valid non-UProperty UObject bound to the column or row
		return;
	}

	PropertyPath = ColumnBoundData->AsPropertyPath();
	if ( !PropertyPath.IsValid() )
	{
		PropertyPath = RowBoundData->AsPropertyPath();
	}

	if ( !PropertyPath.IsValid() )
	{
		// By this point a valid PropertyPath must have been found or created
		return;
	}

	PropertyNode = FPropertyNode::FindPropertyNodeByPath( GetTable()->GetRootPath(), ObjectNode.ToSharedRef() );

	if ( !PropertyNode.IsValid() )
	{
		return;
	}

	PropertyNode = FPropertyNode::FindPropertyNodeByPath( RowRef->GetPartialPath(), PropertyNode.ToSharedRef() );

	if ( !PropertyNode.IsValid() )
	{
		return;
	}

	PropertyNode = FPropertyNode::FindPropertyNodeByPath( ColumnRef->GetPartialPath(), PropertyNode.ToSharedRef() );

	if ( !PropertyNode.IsValid() )
	{
		return;
	}

	PropertyNode = FPropertyNode::FindPropertyNodeByPath( PropertyPath, PropertyNode.ToSharedRef() );

	if ( PropertyNode.IsValid() )
	{
		bIsBound = true;
		PropertyEditor = FPropertyEditor::Create( PropertyNode.ToSharedRef(), GetTable() );
	}
}

bool FPropertyTableCell::IsReadOnly() const 
{ 
	return !IsBound() || PropertyEditor->IsEditConst() || ( PropertyEditor->HasEditCondition() && !PropertyEditor->IsEditConditionMet() );
}

bool FPropertyTableCell::IsValid() const
{
	return !IsBound() || (PropertyEditor->GetPropertyHandle()->GetProperty() != NULL);
}

FString FPropertyTableCell::GetValueAsString() const
{
	return IsBound() ? PropertyEditor->GetValueAsString() : FString();
}

FText FPropertyTableCell::GetValueAsText() const
{
	return IsBound() ? PropertyEditor->GetValueAsText() : FText::GetEmpty();
}

void FPropertyTableCell::SetValueFromString( const FString& InString )
{
	if ( IsReadOnly() )
	{
		return;
	}

	TSharedRef< IPropertyHandle > Handle = PropertyEditor->GetPropertyHandle();
	Handle->SetValueFromFormattedString( InString );
}

TWeakObjectPtr< UObject > FPropertyTableCell::GetObject() const
{
	if ( !ObjectNode.IsValid() )
	{
		return NULL;
	}

	return ObjectNode->GetUObject( 0 );
}

TSharedRef< class IPropertyTable > FPropertyTableCell::GetTable() const
{
	return Column.Pin()->GetTable();
}

TSharedPtr< class IPropertyHandle > FPropertyTableCell::GetPropertyHandle() const 
{ 
	if( IsBound() ) 
	{
		return PropertyEditor->GetPropertyHandle(); 
	}
	return NULL;
}

void FPropertyTableCell::EnterEditMode() 
{
	if ( !bInEditMode )
	{
		TSharedRef< IPropertyTable > Table = GetTable();
		Table->SetCurrentCell( SharedThis( this ) );
		bInEditMode = true;
		EnteredEditModeEvent.Broadcast();
	}
}

void FPropertyTableCell::ExitEditMode()
{
	if ( bInEditMode )
	{
		bInEditMode = false;
		ExitedEditModeEvent.Broadcast();
	}
}
