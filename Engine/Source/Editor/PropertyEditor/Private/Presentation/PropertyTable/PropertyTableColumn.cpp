// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Presentation/PropertyTable/PropertyTableColumn.h"
#include "Editor/EditorEngine.h"
#include "IPropertyTableCell.h"
#include "UObject/TextProperty.h"
#include "ObjectPropertyNode.h"
#include "Presentation/PropertyTable/PropertyTableCell.h"
#include "Presentation/PropertyTable/DataSource.h"
#include "PropertyEditorHelpers.h"

#define LOCTEXT_NAMESPACE "PropertyTableColumn"

template< typename UPropertyType >
struct FCompareRowByColumnAscending
{
public:
	FCompareRowByColumnAscending( const TSharedRef< IPropertyTableColumn >& InColumn, const TWeakObjectPtr< UPropertyType >& InUProperty )
		: Property( InUProperty )
		, Column( InColumn )
	{

	}

	FORCEINLINE bool operator()( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
	{
		const TSharedRef< IPropertyTableCell > LhsCell = Column->GetCell( Lhs );
		const TSharedRef< IPropertyTableCell > RhsCell = Column->GetCell( Rhs );

		const TSharedPtr< FPropertyNode > LhsPropertyNode = LhsCell->GetNode();
		if ( !LhsPropertyNode.IsValid() )
		{
			return true;
		}

		const TSharedPtr< FPropertyNode > RhsPropertyNode = RhsCell->GetNode();
		if ( !RhsPropertyNode.IsValid() )
		{
			return false;
		}

		const TSharedPtr< IPropertyHandle > LhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( LhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !LhsPropertyHandle.IsValid() )
		{
			return true;
		}

		const TSharedPtr< IPropertyHandle > RhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( RhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !RhsPropertyHandle.IsValid() )
		{
			return false;
		}

		return ComparePropertyValue( LhsPropertyHandle, RhsPropertyHandle );
	}

private:

	FORCEINLINE bool ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
	{
		typename UPropertyType::TCppType LhsValue; 
		LhsPropertyHandle->GetValue( LhsValue );

		typename UPropertyType::TCppType RhsValue; 
		RhsPropertyHandle->GetValue( RhsValue );

		return LhsValue < RhsValue;
	}

private:

	const TWeakObjectPtr< UPropertyType > Property;
	TSharedRef< IPropertyTableColumn > Column;
};

template< typename UPropertyType >
struct FCompareRowByColumnDescending
{
public:
	FCompareRowByColumnDescending( const TSharedRef< IPropertyTableColumn >& InColumn, const TWeakObjectPtr< UPropertyType >& InUProperty )
		: Comparer( InColumn, InUProperty )
	{

	}

	FORCEINLINE bool operator()( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
	{
		return !Comparer( Lhs, Rhs );
	}


private:

	const FCompareRowByColumnAscending< UPropertyType > Comparer;
};


template<>
FORCEINLINE bool FCompareRowByColumnAscending<UEnumProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	// Only Bytes work right now

	// Get the basic uint8 values
	uint8 LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	uint8 RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	// Bytes are trivially sorted numerically
	UEnum* PropertyEnum = Property->GetEnum();

	// Enums are sorted alphabetically based on the full enum entry name - must be sure that values are within Enum bounds!
	int32 LhsIndex = PropertyEnum->GetIndexByValue(LhsValue);
	int32 RhsIndex = PropertyEnum->GetIndexByValue(RhsValue);
	bool bLhsEnumValid = LhsIndex != INDEX_NONE;
	bool bRhsEnumValid = RhsIndex != INDEX_NONE;
	if (bLhsEnumValid && bRhsEnumValid)
	{
		FName LhsEnumName(PropertyEnum->GetNameByIndex(LhsIndex));
		FName RhsEnumName(PropertyEnum->GetNameByIndex(RhsIndex));
		return LhsEnumName.Compare(RhsEnumName) < 0;
	}
	else if(bLhsEnumValid)
	{
		return true;
	}
	else if(bRhsEnumValid)
	{
		return false;
	}
	else
	{
		return LhsValue < RhsValue;
	}
}

// UByteProperty objects may in fact represent Enums - so they need special handling for alphabetic Enum vs. numerical Byte sorting.
template<>
FORCEINLINE bool FCompareRowByColumnAscending<UByteProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	// Get the basic uint8 values
	uint8 LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	uint8 RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	// Bytes are trivially sorted numerically
	UEnum* PropertyEnum = Property->GetIntPropertyEnum();
	if(PropertyEnum == nullptr)
	{
		return LhsValue < RhsValue;
	}
	else
	{
		int32 LhsIndex = PropertyEnum->GetIndexByValue(LhsValue);
		int32 RhsIndex = PropertyEnum->GetIndexByValue(RhsValue);
		// But Enums are sorted alphabetically based on the full enum entry name - must be sure that values are within Enum bounds!
		bool bLhsEnumValid = LhsIndex != INDEX_NONE;
		bool bRhsEnumValid = RhsIndex != INDEX_NONE;
		if(bLhsEnumValid && bRhsEnumValid)
		{
			FName LhsEnumName(PropertyEnum->GetNameByIndex(LhsIndex));
			FName RhsEnumName(PropertyEnum->GetNameByIndex(RhsIndex));
			return LhsEnumName.Compare(RhsEnumName) < 0;
		}
		else if(bLhsEnumValid)
		{
			return true;
		}
		else if(bRhsEnumValid)
		{
			return false;
		}
		else
		{
			return LhsValue < RhsValue;
		}
	}
}

template<>
FORCEINLINE bool FCompareRowByColumnAscending<UNameProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	FName LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	FName RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	return LhsValue.Compare( RhsValue ) < 0;
}

template<>
FORCEINLINE bool FCompareRowByColumnAscending<UObjectPropertyBase>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	UObject* LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	if ( LhsValue == NULL )
	{
		return true;
	}

	UObject* RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	if ( RhsValue == NULL )
	{
		return false;
	}

	return LhsValue->GetName() < RhsValue->GetName();
}

template<>
FORCEINLINE bool FCompareRowByColumnAscending<UStructProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	if ( !FPropertyTableColumn::IsSupportedStructProperty(LhsPropertyHandle->GetProperty() ) )
	{
		return true;
	}

	if ( !FPropertyTableColumn::IsSupportedStructProperty(RhsPropertyHandle->GetProperty() ) )
	{
		return false;
	}

	{
		FVector LhsVector;
		FVector RhsVector;

		if ( LhsPropertyHandle->GetValue(LhsVector) != FPropertyAccess::Fail && RhsPropertyHandle->GetValue(RhsVector) != FPropertyAccess::Fail )
		{
			return LhsVector.SizeSquared() < RhsVector.SizeSquared();
		}

		FVector2D LhsVector2D;
		FVector2D RhsVector2D;

		if ( LhsPropertyHandle->GetValue(LhsVector2D) != FPropertyAccess::Fail && RhsPropertyHandle->GetValue(RhsVector2D) != FPropertyAccess::Fail )
		{
			return LhsVector2D.SizeSquared()  < RhsVector2D.SizeSquared();
		}

		FVector4 LhsVector4;
		FVector4 RhsVector4;

		if ( LhsPropertyHandle->GetValue(LhsVector4) != FPropertyAccess::Fail && RhsPropertyHandle->GetValue(RhsVector4) != FPropertyAccess::Fail )
		{
			return LhsVector4.SizeSquared()  < RhsVector4.SizeSquared();
		}
	}

	ensureMsgf(false, TEXT("A supported struct property does not have a defined implementation for sorting a property column."));
	return false;
}


FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject )
	: Cells()
	, DataSource( MakeShareable( new UObjectDataSource( InObject.Get() ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( FPropertyPath::CreateEmpty() )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
	GenerateColumnDisplayName();
}

FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TSharedRef< FPropertyPath >& InPropertyPath )
	: Cells()
	, DataSource( MakeShareable( new PropertyPathDataSource( InPropertyPath ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( FPropertyPath::CreateEmpty() )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
	GenerateColumnDisplayName();
}

FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< class IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject, const TSharedRef< FPropertyPath >& InPartialPropertyPath )
	: Cells()
	, DataSource( MakeShareable( new UObjectDataSource( InObject.Get() ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( InPartialPropertyPath )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
}


void FPropertyTableColumn::GenerateColumnId()
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedPtr< FPropertyPath > PropertyPath = DataSource->AsPropertyPath();

	// Use partial path for a valid column ID if we have one. We are pointing to a container with an array, but all columns must be unique
	if ( PartialPath->GetNumProperties() > 0 )
	{
		Id = FName( *PartialPath->ToString());
	}
	else if ( Object.IsValid() )
	{
		Id = Object->GetFName();
	}
	else if ( PropertyPath.IsValid() )
	{
		Id = FName( *PropertyPath->ToString() );
	}
	else
	{
		Id = NAME_None;
	}
}

void FPropertyTableColumn::GenerateColumnDisplayName()
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedPtr< FPropertyPath > PropertyPath = DataSource->AsPropertyPath();

	if ( Object.IsValid() )
	{
		if ( Object->IsA( UProperty::StaticClass() ) )
		{
			const UProperty* Property = Cast< UProperty >( Object.Get() );
			DisplayName = FText::FromString(UEditorEngine::GetFriendlyName(Property)); 
		}
		else
		{
			DisplayName = FText::FromString(Object->GetFName().ToString());
		}
	}
	else if ( PropertyPath.IsValid() )
	{
		//@todo unify this logic with all the property editors [12/11/2012 Justin.Sargent]
		FString NewName;
		bool FirstAddition = true;
		const FPropertyInfo* PreviousPropInfo = NULL;
		for (int PropertyIndex = 0; PropertyIndex < PropertyPath->GetNumProperties(); PropertyIndex++)
		{
			const FPropertyInfo& PropInfo = PropertyPath->GetPropertyInfo( PropertyIndex );

			if ( !(PropInfo.Property->IsA( UArrayProperty::StaticClass() ) && PropertyIndex != PropertyPath->GetNumProperties() - 1 ) )
			{
				if ( !FirstAddition )
				{
					NewName += TEXT( "->" );
				}

				FString PropertyName = PropInfo.Property->GetDisplayNameText().ToString();

				if ( PropertyName.IsEmpty() )
				{
					PropertyName = PropInfo.Property->GetName();

					const bool bIsBoolProperty = Cast<const UBoolProperty>( PropInfo.Property.Get() ) != NULL;

					if ( PreviousPropInfo != NULL )
					{
						const UStructProperty* ParentStructProperty = Cast<const UStructProperty>( PreviousPropInfo->Property.Get() );
						if( ParentStructProperty && ParentStructProperty->Struct->GetFName() == NAME_Rotator )
						{
							if( PropInfo.Property->GetFName() == "Roll" )
							{
								PropertyName = TEXT("X");
							}
							else if( PropInfo.Property->GetFName() == "Pitch" )
							{
								PropertyName = TEXT("Y");
							}
							else if( PropInfo.Property->GetFName() == "Yaw" )
							{
								PropertyName = TEXT("Z");
							}
							else
							{
								check(0);
							}
						}
					}

					PropertyName = FName::NameToDisplayString( PropertyName, bIsBoolProperty );
				}

				NewName += PropertyName;

				if ( PropInfo.ArrayIndex != INDEX_NONE )
				{
					NewName += FString::Printf( TEXT( "[%d]" ), PropInfo.ArrayIndex );
				}

				PreviousPropInfo = &PropInfo;
				FirstAddition = false;
			}
		}

		DisplayName = FText::FromString(*NewName);
	}
	else
	{
		DisplayName = LOCTEXT( "InvalidColumnName", "Invalid Property" );
	}
}

FName FPropertyTableColumn::GetId() const 
{ 
	return Id;
}

FText FPropertyTableColumn::GetDisplayName() const 
{ 
	return DisplayName;
}

TSharedRef< IPropertyTableCell > FPropertyTableColumn::GetCell( const TSharedRef< class IPropertyTableRow >& Row ) 
{
	//@todo Clean Cells cache when rows get updated [11/27/2012 Justin.Sargent]
	TSharedRef< IPropertyTableCell >* CellPtr = Cells.Find( Row );

	if( CellPtr != NULL )
	{
		return *CellPtr;
	}

	TSharedRef< IPropertyTableCell > Cell = MakeShareable( new FPropertyTableCell( SharedThis( this ), Row ) );
	Cells.Add( Row, Cell );

	return Cell;
}

void FPropertyTableColumn::RemoveCellsForRow( const TSharedRef< class IPropertyTableRow >& Row )
{
	Cells.Remove( Row );
}

TSharedRef< class IPropertyTable > FPropertyTableColumn::GetTable() const
{
	return Table.Pin().ToSharedRef();
}

bool FPropertyTableColumn::CanSortBy() const
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	UProperty* Property = Cast< UProperty >( Object.Get() );

	TSharedPtr< FPropertyPath > Path = DataSource->AsPropertyPath();
	if ( Property == NULL && Path.IsValid() )
	{
		Property = Path->GetLeafMostProperty().Property.Get();
	}

	if ( Property != NULL )
	{
		return Property->IsA( UByteProperty::StaticClass() )  ||
			Property->IsA( UIntProperty::StaticClass() )   ||
			Property->IsA( UBoolProperty::StaticClass() )  ||
			Property->IsA( UEnumProperty::StaticClass() )  ||
			Property->IsA( UFloatProperty::StaticClass() ) ||
			Property->IsA( UNameProperty::StaticClass() )  ||
			Property->IsA( UStrProperty::StaticClass() )   ||
			IsSupportedStructProperty( Property )		   ||
			( Property->IsA( UObjectPropertyBase::StaticClass() ) && !Property->HasAnyPropertyFlags(CPF_InstancedReference) );
			//Property->IsA( UTextProperty::StaticClass() );
	}

	return false;
}

void FPropertyTableColumn::Sort( TArray< TSharedRef< class IPropertyTableRow > >& Rows, const EColumnSortMode::Type SortMode )
{
	if ( SortMode == EColumnSortMode::None )
	{
		return;
	}

	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	UProperty* Property = Cast< UProperty >( Object.Get() );

	TSharedPtr< FPropertyPath > Path = DataSource->AsPropertyPath();
	if ( Property == NULL && Path.IsValid() )
	{
		Property = Path->GetLeafMostProperty().Property.Get();
	}

	if (!Property)
	{
		return;
	}

	if ( Property->IsA( UEnumProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UEnumProperty> EnumProperty( Cast< UEnumProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UEnumProperty >( SharedThis( this ), EnumProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UEnumProperty >( SharedThis( this ), EnumProperty ) );
		}
	}
	else if ( Property->IsA( UByteProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UByteProperty> ByteProperty( Cast< UByteProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UByteProperty >( SharedThis( this ), ByteProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UByteProperty >( SharedThis( this ), ByteProperty ) );
		}
	}
	else if ( Property->IsA( UIntProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UIntProperty> IntProperty( Cast< UIntProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UIntProperty >( SharedThis( this ), IntProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UIntProperty >( SharedThis( this ), IntProperty ) );
		}
	}
	else if ( Property->IsA( UBoolProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UBoolProperty> BoolProperty( Cast< UBoolProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UBoolProperty >( SharedThis( this ), BoolProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UBoolProperty >( SharedThis( this ), BoolProperty ) );
		}
	}
	else if ( Property->IsA( UFloatProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UFloatProperty> FloatProperty( Cast< UFloatProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UFloatProperty >( SharedThis( this ), FloatProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UFloatProperty >( SharedThis( this ), FloatProperty ) );
		}
	}
	else if ( Property->IsA( UNameProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UNameProperty> NameProperty( Cast< UNameProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UNameProperty >( SharedThis( this ), NameProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UNameProperty >( SharedThis( this ), NameProperty ) );
		}
	}
	else if ( Property->IsA( UStrProperty::StaticClass() ) )
	{
		TWeakObjectPtr<UStrProperty> StrProperty( Cast< UStrProperty >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UStrProperty >( SharedThis( this ), StrProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UStrProperty >( SharedThis( this ), StrProperty ) );
		}
	}
	else if ( Property->IsA( UObjectPropertyBase::StaticClass() ) )
	{
		TWeakObjectPtr<UObjectPropertyBase> ObjectProperty( Cast< UObjectPropertyBase >( Property ) );

		if ( SortMode == EColumnSortMode::Ascending )
		{
			Rows.Sort( FCompareRowByColumnAscending< UObjectPropertyBase >( SharedThis( this ), ObjectProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UObjectPropertyBase >( SharedThis( this ), ObjectProperty ) );
		}
	}
	else if ( IsSupportedStructProperty( Property ) )
	{
		TWeakObjectPtr<UStructProperty> StructProperty(Cast < UStructProperty >( Property ) );

		if (SortMode == EColumnSortMode::Ascending)
		{
			Rows.Sort( FCompareRowByColumnAscending< UStructProperty >( SharedThis(this), StructProperty ) );
		}
		else
		{
			Rows.Sort( FCompareRowByColumnDescending< UStructProperty >( SharedThis(this), StructProperty ) );
		}
	}
	//else if ( Property->IsA( UTextProperty::StaticClass() ) )
	//{
	//	if ( SortMode == EColumnSortMode::Ascending )
	//	{
	//		Rows.Sort( FCompareRowByColumnAscending< UTextProperty >( SharedThis( this ) ) );
	//	}
	//	else
	//	{
	//		Rows.Sort( FCompareRowByColumnDescending< UTextProperty >( SharedThis( this ) ) );
	//	}
	//}
	else
	{
		check( false && "Cannot Sort Rows by this Column!" );
	}
}

void FPropertyTableColumn::Tick()
{
	if ( !DataSource->AsPropertyPath().IsValid() )
	{
		const TSharedRef< IPropertyTable > TableRef = GetTable();
		const TWeakObjectPtr< UObject > Object = DataSource->AsUObject();

		if ( !Object.IsValid() )
		{
			TableRef->RemoveColumn( SharedThis( this ) );
		}
		else
		{
			const TSharedRef< FObjectPropertyNode > Node = TableRef->GetObjectPropertyNode( Object );
			EPropertyDataValidationResult Result = Node->EnsureDataIsValid();

			if ( Result == EPropertyDataValidationResult::ObjectInvalid )
			{
				TableRef->RemoveColumn( SharedThis( this ) );
			}
			else if ( Result == EPropertyDataValidationResult::ArraySizeChanged )
			{
				TableRef->RequestRefresh();
			}
		}
	}
}

void FPropertyTableColumn::SetFrozen(bool InIsFrozen)
{
	bIsFrozen = InIsFrozen;
	FrozenStateChanged.Broadcast( SharedThis(this) );
}

bool FPropertyTableColumn::IsSupportedStructProperty(const UProperty* InProperty)
{
	if ( InProperty != nullptr && Cast<UStructProperty>(InProperty) != nullptr)
	{
		const UStructProperty* StructProp = Cast<UStructProperty>(InProperty);
		FName StructName = StructProp->Struct->GetFName();

		return StructName == NAME_Vector ||
			StructName == NAME_Vector2D	 ||
			StructName == NAME_Vector4;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
