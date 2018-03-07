// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Presentation/PropertyTable/PropertyTableRow.h"
#include "PropertyNode.h"
#include "ObjectPropertyNode.h"
#include "IPropertyTableColumn.h"
#include "IPropertyTableCell.h"
#include "Presentation/PropertyTable/DataSource.h"

FPropertyTableRow::FPropertyTableRow( const TSharedRef< class IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject )
	: DataSource( MakeShareable( new UObjectDataSource( InObject ) ) )
	, Table( InTable )
	, Children()
	, PartialPath( FPropertyPath::CreateEmpty() )
{
	GenerateChildren();
}

FPropertyTableRow::FPropertyTableRow( const TSharedRef< class IPropertyTable >& InTable, const TSharedRef< FPropertyPath >& InPropertyPath )
	: DataSource( MakeShareable( new PropertyPathDataSource( InPropertyPath ) ) )
	, Table( InTable )
	, Children()
	, PartialPath( FPropertyPath::CreateEmpty() )
{
	GenerateChildren();
}

FPropertyTableRow::FPropertyTableRow( const TSharedRef< class IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject, const TSharedRef< FPropertyPath >& InPartialPropertyPath )
	: DataSource( MakeShareable( new UObjectDataSource( InObject ) ) )
	, Table( InTable )
	, Children()
	, PartialPath( InPartialPropertyPath )
{
	GenerateChildren();
}

FPropertyTableRow::FPropertyTableRow( const TSharedRef< class IPropertyTable >& InTable, const TSharedRef< FPropertyPath >& InPropertyPath, const TSharedRef< FPropertyPath >& InPartialPropertyPath )
	: DataSource( MakeShareable( new PropertyPathDataSource( InPropertyPath ) ) )
	, Table( InTable )
	, Children()
	, PartialPath( InPartialPropertyPath )
{
	GenerateChildren();
}

bool FPropertyTableRow::HasChildren() const
{
	return Children.Num() > 0;
}

void FPropertyTableRow::GetChildRows( TArray< TSharedRef< IPropertyTableRow > >& OutChildren ) const 
{
	OutChildren.Append( Children );
}

void FPropertyTableRow::GenerateChildren()
{
	if ( !DataSource->IsValid() || DataSource->AsPropertyPath().IsValid() )
	{
		return;
	}

	const TSharedRef< class IPropertyTable > TableRef = GetTable();
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedRef< FObjectPropertyNode > ObjectNode = TableRef->GetObjectPropertyNode( Object );
	TSharedRef< FPropertyPath > RootPath = TableRef->GetRootPath();

	TSharedPtr< FPropertyNode > PropertyNode = FPropertyNode::FindPropertyNodeByPath( RootPath, ObjectNode );

	if ( !PropertyNode.IsValid() )
	{
		return;
	}

	PropertyNode = FPropertyNode::FindPropertyNodeByPath( PartialPath, PropertyNode.ToSharedRef() );

	if ( !PropertyNode.IsValid() )
	{
		return;
	}

	UProperty* Property = PropertyNode->GetProperty();

	if ( Property != NULL && Property->IsA( UArrayProperty::StaticClass() ) )
	{
		for (int Index = 0; Index < PropertyNode->GetNumChildNodes(); Index++)
		{
			TSharedPtr< FPropertyNode > ChildNode = PropertyNode->GetChildNode( Index );

			FPropertyInfo Extension;
			Extension.Property = ChildNode->GetProperty();
			Extension.ArrayIndex = ChildNode->GetArrayIndex();

			Children.Add( MakeShareable( new FPropertyTableRow( TableRef, Object, RootPath->ExtendPath( Extension ) ) ) );
		}
	}
}

TSharedRef< class IPropertyTable > FPropertyTableRow::GetTable() const 
{
	return Table.Pin().ToSharedRef();
}

void FPropertyTableRow::Tick() 
{
	//@todo Create similar logic to handle if the column is the object and row is the property [12/7/2012 Justin.Sargent]
	if ( !HasChildren() && !DataSource->AsPropertyPath().IsValid() )
	{
		const TSharedRef< IPropertyTable > TableRef = GetTable();
		const TWeakObjectPtr< UObject > Object = DataSource->AsUObject();

		if ( !Object.IsValid() )
		{
			TableRef->RemoveRow( SharedThis( this ) );
		}
		else
		{
			TSharedPtr< FPropertyNode > Node = TableRef->GetObjectPropertyNode( Object );
			if(!Node.IsValid())
			{
				TableRef->RemoveRow( SharedThis( this ) );
				return;
			}

			Node = FPropertyNode::FindPropertyNodeByPath( GetTable()->GetRootPath(), Node.ToSharedRef() );
			if ( !Node.IsValid() )
			{
				TableRef->RemoveRow( SharedThis( this ) );
				return;
			}

			Node = FPropertyNode::FindPropertyNodeByPath( GetPartialPath(), Node.ToSharedRef() );

			if ( !Node.IsValid() )
			{
				TableRef->RemoveRow( SharedThis( this ) );
				return;
			}

			EPropertyDataValidationResult Result = Node->EnsureDataIsValid();

			if ( Result == EPropertyDataValidationResult::ObjectInvalid )
			{
				TableRef->RemoveRow( SharedThis( this ) );
			}
			else if ( Result == EPropertyDataValidationResult::ArraySizeChanged )
			{
				TableRef->RequestRefresh();
			}
			else if ( Result != EPropertyDataValidationResult::DataValid )
			{
				Refresh();
			}
		}
	}
}

void FPropertyTableRow::Refresh()
{
	const TSharedRef< IPropertyTable > TableRef = GetTable();
	const TSharedRef< IPropertyTableRow > SelfRef = SharedThis( this );
	for( auto ColumnIter = TableRef->GetColumns().CreateConstIterator(); ColumnIter; ++ColumnIter )
	{
		const TSharedRef< IPropertyTableColumn >& CurrentColumn = *ColumnIter;

		CurrentColumn->GetCell( SelfRef )->Refresh();
	}

	Refreshed.Broadcast();
}
