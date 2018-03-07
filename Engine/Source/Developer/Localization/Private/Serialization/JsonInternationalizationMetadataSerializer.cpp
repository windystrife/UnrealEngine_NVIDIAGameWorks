// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/JsonInternationalizationMetadataSerializer.h"
#include "Serialization/JsonSerializer.h"


struct FCompareLocMetadataValue
{
	FORCEINLINE bool operator()( TSharedPtr< FLocMetadataValue > A, TSharedPtr< FLocMetadataValue > B ) const
	{
		if( A.IsValid() && B.IsValid() )
		{
			return (*A < *B);
		}
		else if( A.IsValid() != B.IsValid() )
		{
			return B.IsValid();
		}
		return false;
	}
};


TSharedPtr<FLocMetadataValue> JSonValueToLocMetaDataValue( const TSharedRef< FJsonValue > JsonValue )
{
	switch( JsonValue->Type )
	{
	case EJson::Boolean:
		{
			return MakeShareable( new FLocMetadataValueBoolean( JsonValue->AsBool() ) );
		}
		break;
	case EJson::String:
		{
			return MakeShareable( new FLocMetadataValueString( JsonValue->AsString() ) );
		}
		break;
	case EJson::Array:
		{
			TArray< TSharedPtr< FLocMetadataValue > > MetadataArray;
			TArray< TSharedPtr< FJsonValue > > JsonArray = JsonValue->AsArray();
			for( auto ArrayIter = JsonArray.CreateConstIterator(); ArrayIter; ++ArrayIter )
			{
				const TSharedPtr< FJsonValue >& Item = *ArrayIter;
				if( Item.IsValid() )
				{
					TSharedPtr< FLocMetadataValue > Entry = JSonValueToLocMetaDataValue( Item.ToSharedRef() );
					if( Entry.IsValid() )
					{
						MetadataArray.Add( Entry );
					}
				}
			}
			if( MetadataArray.Num() > 0 )
			{
				return MakeShareable( new FLocMetadataValueArray( MetadataArray ) );
			}
		}
		break;
	case EJson::Object:
		{
			TSharedPtr< FLocMetadataObject > MetadataSubObject = MakeShareable( new FLocMetadataObject );
			TSharedPtr< FJsonObject > JsonObject = JsonValue->AsObject();
			for( auto ValueIter = JsonObject->Values.CreateConstIterator(); ValueIter; ++ValueIter )
			{
				const FString Name = (*ValueIter).Key;
				TSharedPtr< FJsonValue > Value = (*ValueIter).Value;
				if( Value.IsValid() )
				{
					TSharedPtr< FLocMetadataValue > MetadataValue = JSonValueToLocMetaDataValue( Value.ToSharedRef() );
					if( MetadataValue.IsValid() )
					{
						MetadataSubObject->SetField( Name, MetadataValue );
					}
				}
			}
			return MakeShareable( new FLocMetadataValueObject(MetadataSubObject) );
		}
		break;
	default:
		{
			// At the moment we do not support all the json types as metadata.  In the future these types will be handled in a way that they can be stored in an unprocessed way.
		}
		break;
	}

	return NULL;
}


TSharedPtr<FJsonValue> LocMetaDataValueToJsonValue( const TSharedRef< FLocMetadataValue > MetadataValue )
{
	switch( MetadataValue->GetType() )
	{
	case ELocMetadataType::Boolean:
		{
			return MakeShareable( new FJsonValueBoolean( MetadataValue->AsBool() ) );
		}
		break;

	case ELocMetadataType::String:
		{
			return MakeShareable( new FJsonValueString( MetadataValue->AsString() ) );
		}
		break;

	case ELocMetadataType::Array:
		{
			TArray< TSharedPtr<FJsonValue> > JsonArrayVals;
			TArray< TSharedPtr< FLocMetadataValue > > MetaDataArray = MetadataValue->AsArray();
			
			MetaDataArray.Sort( FCompareLocMetadataValue() );
			for( auto ArrayIter = MetaDataArray.CreateConstIterator(); ArrayIter; ++ArrayIter )
			{
				const TSharedPtr<FLocMetadataValue>& Item = *ArrayIter;
				if( Item.IsValid() )
				{
					TSharedPtr<FJsonValue> Entry = LocMetaDataValueToJsonValue( Item.ToSharedRef() );
					if( Entry.IsValid() )
					{
						JsonArrayVals.Add( Entry );
					}
				}
			}

			if( JsonArrayVals.Num() > 0 )
			{
				return MakeShareable( new FJsonValueArray( JsonArrayVals ) );
			}
		}
		break;

	case ELocMetadataType::Object:
		{
			TSharedPtr< FJsonObject > JsonSubObject = MakeShareable( new FJsonObject );
			TSharedPtr< FLocMetadataObject > MetaDataObject = MetadataValue->AsObject();
			for( auto ValueIter = MetaDataObject->Values.CreateConstIterator(); ValueIter; ++ValueIter )
			{
				const FString Name = (*ValueIter).Key;
				TSharedPtr< FLocMetadataValue > Value = (*ValueIter).Value;
				if( Value.IsValid() )
				{
					TSharedPtr< FJsonValue > JsonValue = LocMetaDataValueToJsonValue( Value.ToSharedRef() );
					if( JsonValue.IsValid() )
					{
						JsonSubObject->SetField( Name, JsonValue );
					}
				}
			}

			// Sorting by key is probably sufficient for now but ideally we would sort the resulting json object using
			//  the same logic that is in the FLocMetadata < operator
			JsonSubObject->Values.KeySort( TLess<FString>() );

			return MakeShareable( new FJsonValueObject(JsonSubObject) );
		}

	default:
		break;
	}
	return NULL;
}


void FJsonInternationalizationMetaDataSerializer::DeserializeMetadata( const TSharedRef< FJsonObject > JsonObj, TSharedPtr< FLocMetadataObject >& OutMetaDataObj )
{
	OutMetaDataObj = JSonValueToLocMetaDataValue( MakeShareable( new FJsonValueObject( JsonObj ) ) )->AsObject();
}


void FJsonInternationalizationMetaDataSerializer::SerializeMetadata( const TSharedRef< FLocMetadataObject > MetaData, TSharedPtr< FJsonObject >& OutJsonObj )
{
	OutJsonObj = LocMetaDataValueToJsonValue( MakeShareable( new FLocMetadataValueObject( MetaData ) ) )->AsObject();
}


FString FJsonInternationalizationMetaDataSerializer::MetadataToString( const TSharedPtr<FLocMetadataObject> Metadata )
{
	FString StringMetadata = TEXT("");
	if( Metadata.IsValid() )
	{
		TSharedPtr< FJsonObject > JsonKeyMetadata;
		FJsonInternationalizationMetaDataSerializer::SerializeMetadata( Metadata.ToSharedRef(), JsonKeyMetadata );

		if( JsonKeyMetadata.IsValid() )
		{
			JsonKeyMetadata->Values.KeySort( TLess<FString>() );

			TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create( &StringMetadata );
			FJsonSerializer::Serialize( JsonKeyMetadata.ToSharedRef(), Writer );
			Writer->Close();

			StringMetadata.ReplaceInline( TEXT("\t"), TEXT(" ") );
			StringMetadata.ReplaceInline( TEXT("\r\n"), TEXT(" ") );
			StringMetadata.ReplaceInline( TEXT("\n"), TEXT(" ") );
		}
	}
	return StringMetadata;
}
