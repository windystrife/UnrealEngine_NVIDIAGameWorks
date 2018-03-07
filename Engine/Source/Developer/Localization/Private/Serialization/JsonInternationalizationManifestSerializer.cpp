// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/JsonInternationalizationManifestSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonInternationalizationMetadataSerializer.h"


DEFINE_LOG_CATEGORY_STATIC(LogInternationalizationManifestSerializer, Log, All);


const FString FJsonInternationalizationManifestSerializer::TAG_FORMATVERSION = TEXT("FormatVersion");
const FString FJsonInternationalizationManifestSerializer::TAG_NAMESPACE = TEXT("Namespace");
const FString FJsonInternationalizationManifestSerializer::TAG_CHILDREN = TEXT("Children");
const FString FJsonInternationalizationManifestSerializer::TAG_SUBNAMESPACES = TEXT("Subnamespaces");
const FString FJsonInternationalizationManifestSerializer::TAG_PATH = TEXT("Path");
const FString FJsonInternationalizationManifestSerializer::TAG_OPTIONAL = TEXT("Optional");
const FString FJsonInternationalizationManifestSerializer::TAG_KEYCOLLECTION = TEXT("Keys");
const FString FJsonInternationalizationManifestSerializer::TAG_KEY = TEXT("Key");
const FString FJsonInternationalizationManifestSerializer::TAG_DEPRECATED_DEFAULTTEXT = TEXT("DefaultText");
const FString FJsonInternationalizationManifestSerializer::TAG_SOURCE = TEXT("Source");
const FString FJsonInternationalizationManifestSerializer::TAG_SOURCE_TEXT = TEXT("Text");
const FString FJsonInternationalizationManifestSerializer::TAG_METADATA = TEXT("MetaData");
const FString FJsonInternationalizationManifestSerializer::TAG_METADATA_INFO = TEXT("Info");
const FString FJsonInternationalizationManifestSerializer::TAG_METADATA_KEY = TEXT("Key");
const FString FJsonInternationalizationManifestSerializer::NAMESPACE_DELIMITER = TEXT(".");


struct FCompareManifestEntryBySource
{
	FORCEINLINE bool operator()( TSharedPtr< FManifestEntry > A, TSharedPtr< FManifestEntry > B ) const
	{
		return A->Source < B->Source;
	}
};


struct FCompareStructuredEntryByNamespace
{
	FORCEINLINE bool operator()( TSharedPtr< FStructuredEntry > A, TSharedPtr< FStructuredEntry > B ) const
	{
		return A->Namespace < B->Namespace;
	}
};


bool FJsonInternationalizationManifestSerializer::DeserializeManifest( const FString& InStr, TSharedRef< FInternationalizationManifest > Manifest )
{
	TSharedPtr<FJsonObject> JsonManifestObj;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InStr );
	bool bExecSuccessful = FJsonSerializer::Deserialize( Reader, JsonManifestObj );

	if ( bExecSuccessful && JsonManifestObj.IsValid() )
	{
		bExecSuccessful = DeserializeInternal( JsonManifestObj.ToSharedRef(), Manifest );
	}

	return bExecSuccessful;
}


bool FJsonInternationalizationManifestSerializer::DeserializeManifest( TSharedRef< FJsonObject > InJsonObj, TSharedRef< FInternationalizationManifest > Manifest )
{
	return DeserializeInternal( InJsonObj, Manifest );
}


bool FJsonInternationalizationManifestSerializer::DeserializeManifestFromFile( const FString& InJsonFile, TSharedRef< FInternationalizationManifest > Manifest )
{
	// Read in file as string
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *InJsonFile))
	{
		UE_LOG(LogInternationalizationManifestSerializer, Error, TEXT("Failed to load manifest '%s'."), *InJsonFile);
		return false;
	}

	// Parse as JSON
	TSharedPtr<FJsonObject> JsonObject;

	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogInternationalizationManifestSerializer, Error, TEXT("Failed to parse manifest '%s'. %s."), *InJsonFile, *JsonReader->GetErrorMessage());
		return false;
	}

	return DeserializeInternal(JsonObject.ToSharedRef(), Manifest);
}


bool FJsonInternationalizationManifestSerializer::SerializeManifest( TSharedRef< const FInternationalizationManifest > Manifest, FString& Str )
{
	TSharedRef< FJsonObject > JsonManifestObj = MakeShareable( new FJsonObject );
	bool bExecSuccessful = SerializeInternal( Manifest, JsonManifestObj );

	if(bExecSuccessful)
	{
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create( &Str );
		bExecSuccessful = FJsonSerializer::Serialize(JsonManifestObj, Writer);
		Writer->Close();
	}
	return bExecSuccessful;
}


bool FJsonInternationalizationManifestSerializer::SerializeManifest( TSharedRef< const FInternationalizationManifest > Manifest, TSharedRef< FJsonObject > JsonObj )
{
	return SerializeInternal( Manifest, JsonObj );
}


bool FJsonInternationalizationManifestSerializer::SerializeManifestToFile( TSharedRef< const FInternationalizationManifest > Manifest, const FString& InJsonFile )
{
	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	if (!SerializeManifest(Manifest, JsonObject))
	{
		UE_LOG(LogInternationalizationManifestSerializer, Error, TEXT("Failed to serialize manifest '%s'."), *InJsonFile);
		return false;
	}

	// Print the JSON data to a string
	FString OutputJsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputJsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter);

	// Save the JSON string (force Unicode for our manifest and archive files)
	if (!FFileHelper::SaveStringToFile(OutputJsonString, *InJsonFile, FFileHelper::EEncodingOptions::ForceUnicode))
	{
		UE_LOG(LogInternationalizationManifestSerializer, Error, TEXT("Failed to save manifest '%s'."), *InJsonFile);
		return false;
	}

	return true;
}


bool FJsonInternationalizationManifestSerializer::DeserializeInternal( TSharedRef< FJsonObject > InJsonObj, TSharedRef< FInternationalizationManifest > Manifest )
{
	if( InJsonObj->HasField( TAG_FORMATVERSION ) )
	{
		const int32 FormatVersion = static_cast<int32>(InJsonObj->GetNumberField( TAG_FORMATVERSION ));
		if (FormatVersion > (int32)FInternationalizationManifest::EFormatVersion::Latest)
		{
			// Manifest is too new to be loaded!
			return false;
		}

		Manifest->SetFormatVersion(static_cast<FInternationalizationManifest::EFormatVersion>(FormatVersion));
	}
	else
	{
		Manifest->SetFormatVersion(FInternationalizationManifest::EFormatVersion::Initial);
	}

	return JsonObjToManifest(InJsonObj, TEXT(""), Manifest );
}


bool FJsonInternationalizationManifestSerializer::SerializeInternal( TSharedRef< const FInternationalizationManifest > InManifest, TSharedRef< FJsonObject > JsonObj )
{
	TSharedPtr< FStructuredEntry > RootElement = MakeShareable( new FStructuredEntry( TEXT("") ) );

	// Condition the data so that it exists in a structured hierarchy for easy population of the JSON object.
	GenerateStructuredData( InManifest, RootElement );

	// Arrange the entries in non-cultural format so that diffs are easier to read.
	SortStructuredData( RootElement );

	//Clear out anything that may be in the JSON object
	JsonObj->Values.Empty();

	// Set format version.
	JsonObj->SetNumberField(TAG_FORMATVERSION, static_cast<double>(InManifest->GetFormatVersion()));

	// Setup the JSON object using the structured data created
	StructuredDataToJsonObj( RootElement, JsonObj );

	return true;
}


bool FJsonInternationalizationManifestSerializer::JsonObjToManifest( TSharedRef< FJsonObject > InJsonObj, FString ParentNamespace, TSharedRef< FInternationalizationManifest > Manifest )
{
	bool bConvertSuccess = true;
	FString AccumulatedNamespace = ParentNamespace;
	if( InJsonObj->HasField( TAG_NAMESPACE) )
	{
		if( !( AccumulatedNamespace.IsEmpty() ) )
		{
			AccumulatedNamespace += NAMESPACE_DELIMITER;
		}
		AccumulatedNamespace += InJsonObj->GetStringField( TAG_NAMESPACE );
	}
	else
	{
		// We found an entry with a missing namespace
		bConvertSuccess = false;
	}

	// Process all the child objects
	if( bConvertSuccess && InJsonObj->HasField( TAG_CHILDREN ) )
	{
		const TArray< TSharedPtr< FJsonValue> > ChildrenArray = InJsonObj->GetArrayField( TAG_CHILDREN );

		for(TArray< TSharedPtr< FJsonValue > >::TConstIterator ChildIter( ChildrenArray.CreateConstIterator() ); ChildIter && bConvertSuccess; ++ChildIter)
		{
			const TSharedPtr< FJsonValue >  ChildEntry = *ChildIter;
			const TSharedPtr< FJsonObject > ChildJSONObject = ChildEntry->AsObject();

			
			FString SourceText;
			TSharedPtr< FLocMetadataObject > SourceMetadata;
			if( ChildJSONObject->HasTypedField< EJson::String>( TAG_DEPRECATED_DEFAULTTEXT) )
			{
				SourceText = ChildJSONObject->GetStringField( TAG_DEPRECATED_DEFAULTTEXT );
			}
			else if( ChildJSONObject->HasTypedField< EJson::Object>( TAG_SOURCE ) )
			{
				const TSharedPtr< FJsonObject > SourceJSONObject = ChildJSONObject->GetObjectField( TAG_SOURCE );
				if( SourceJSONObject->HasTypedField< EJson::String >( TAG_SOURCE_TEXT ) )
				{
					SourceText = SourceJSONObject->GetStringField( TAG_SOURCE_TEXT );

					// Source meta data is mixed in with the source text, we'll process metadata if the source json object has more than one entry
					if( SourceJSONObject->Values.Num() > 1 )
					{
						// We load in the entire source object as metadata and remove the source object
						FJsonInternationalizationMetaDataSerializer::DeserializeMetadata( SourceJSONObject.ToSharedRef(), SourceMetadata );
						if( SourceMetadata.IsValid() )
						{
							SourceMetadata->Values.Remove( TAG_SOURCE_TEXT );
						}
					}
				}
				else
				{
					bConvertSuccess = false;
				}
			}
			else
			{
				bConvertSuccess = false;
			}

			FLocItem Source(SourceText);
			Source.MetadataObj = SourceMetadata;

			if( bConvertSuccess && ChildJSONObject->HasField( TAG_KEYCOLLECTION ) )
			{
				
				const TArray< TSharedPtr<FJsonValue> > ContextArray = ChildJSONObject->GetArrayField( TAG_KEYCOLLECTION);

				for(TArray< TSharedPtr< FJsonValue > >::TConstIterator ContextIter( ContextArray.CreateConstIterator() ); ContextIter && bConvertSuccess; ++ContextIter)
				{
					const TSharedPtr< FJsonValue > ContextEntry = *ContextIter;
					const TSharedPtr< FJsonObject > ContextJSONObject = ContextEntry->AsObject();

					if( ContextJSONObject->HasTypedField< EJson::String >( TAG_KEY ) )
					{
						const FString Key = ContextJSONObject->GetStringField( TAG_KEY );
						const FString SourceLocation = ContextJSONObject->HasField( TAG_PATH ) ? ContextJSONObject->GetStringField( TAG_PATH ) : FString();

						FManifestContext CommandContext;
						CommandContext.Key = Key;
						CommandContext.SourceLocation = SourceLocation;

						if( ContextJSONObject->HasTypedField< EJson::Boolean >( TAG_OPTIONAL ) )
						{
							CommandContext.bIsOptional = ContextJSONObject->GetBoolField( TAG_OPTIONAL );
						}

						if( ContextJSONObject->HasTypedField< EJson::Object >( TAG_METADATA ) )
						{
							const TSharedPtr< FJsonObject > MetaDataJSONObject = ContextJSONObject->GetObjectField( TAG_METADATA );

							if( MetaDataJSONObject->HasTypedField< EJson::Object >( TAG_METADATA_INFO ) )
							{
								const TSharedPtr< FJsonObject > MetaDataInfoJSONObject = MetaDataJSONObject->GetObjectField( TAG_METADATA_INFO );

								TSharedPtr< FLocMetadataObject > MetadataNode;
								FJsonInternationalizationMetaDataSerializer::DeserializeMetadata( MetaDataInfoJSONObject.ToSharedRef(), MetadataNode );
								if( MetadataNode.IsValid() )
								{
									CommandContext.InfoMetadataObj = MetadataNode;
								}
							}

							if( MetaDataJSONObject->HasTypedField< EJson::Object >( TAG_METADATA_KEY ) )
							{
								const TSharedPtr< FJsonObject > MetaDataKeyJSONObject = MetaDataJSONObject->GetObjectField( TAG_METADATA_KEY );

								TSharedPtr< FLocMetadataObject > MetadataNode;
								FJsonInternationalizationMetaDataSerializer::DeserializeMetadata( MetaDataKeyJSONObject.ToSharedRef(), MetadataNode );
								if( MetadataNode.IsValid() )
								{
									CommandContext.KeyMetadataObj = MetadataNode;
								}
							}
						}
						bool bAddSuccessful = Manifest->AddSource( AccumulatedNamespace, Source, CommandContext );
						if(!bAddSuccessful)
						{
							UE_LOG( LogInternationalizationManifestSerializer, Warning,TEXT("Could not add JSON entry to the Internationalization manifest: Namespace:%s SourceText:%s SourceData:%s"), 
								*AccumulatedNamespace, 
								*Source.Text, 
								*FJsonInternationalizationMetaDataSerializer::MetadataToString(Source.MetadataObj) );
						}
					}
					else
					{
						//We found a context entry that is missing a identifier/key or a path
						bConvertSuccess = false;
						break;
					}

				}
			}
			else
			{
				// We have an entry that is missing a key/context collection or default text entry.
				bConvertSuccess = false;
				break;
			}

		}
	}

	if( bConvertSuccess && InJsonObj->HasField( TAG_SUBNAMESPACES ) )
	{
		const TArray< TSharedPtr<FJsonValue> > SubnamespaceArray = InJsonObj->GetArrayField( TAG_SUBNAMESPACES );

		for(TArray< TSharedPtr< FJsonValue > >::TConstIterator SubnamespaceIter( SubnamespaceArray.CreateConstIterator() ); SubnamespaceIter; ++SubnamespaceIter )
		{
			const TSharedPtr< FJsonValue >  SubnamespaceEntry = *SubnamespaceIter;
			const TSharedPtr< FJsonObject > SubnamespaceJSONObject = SubnamespaceEntry->AsObject();

			if( !JsonObjToManifest( SubnamespaceJSONObject.ToSharedRef(), AccumulatedNamespace, Manifest ) )
			{
				bConvertSuccess = false;
				break;
			}
		}
	}
	
	return bConvertSuccess;
}


void FJsonInternationalizationManifestSerializer::GenerateStructuredData( TSharedRef< const FInternationalizationManifest > InManifest, TSharedPtr< FStructuredEntry > RootElement )
{
	//Loop through all the unstructured manifest entries and build up our structured hierarchy
	for( FManifestEntryByStringContainer::TConstIterator It( InManifest->GetEntriesByKeyIterator() ); It; ++It )
	{
		const TSharedRef< FManifestEntry > UnstructuredManifestEntry = It.Value();

		TArray< FString > NamespaceTokens;

		// Tokenize the namespace by using '.' as a delimiter
		int32 NamespaceTokenCount = UnstructuredManifestEntry->Namespace.ParseIntoArray( NamespaceTokens, *NAMESPACE_DELIMITER, true );

		TSharedPtr< FStructuredEntry > StructuredManifestEntry = RootElement;
		//Loop through all the namespace tokens and find the appropriate structured entry, if it does not exist add it.  At the end StructuredManifestEntry
		//  will point to the correct hierarchy entry for a given namespace
		for( int32 TokenIndex = 0; TokenIndex < NamespaceTokenCount; ++TokenIndex )
		{
			TSharedPtr< FStructuredEntry > FoundNamespaceEntry;
			for( int SubNamespaceIndex = 0; SubNamespaceIndex < StructuredManifestEntry->SubNamespaces.Num(); SubNamespaceIndex++ )
			{
				if( StructuredManifestEntry->SubNamespaces[SubNamespaceIndex]->Namespace.Equals(NamespaceTokens[TokenIndex], ESearchCase::CaseSensitive) )
				{
					FoundNamespaceEntry = StructuredManifestEntry->SubNamespaces[SubNamespaceIndex];
					break;
				}
			}

			if( !FoundNamespaceEntry.IsValid() )
			{
				int32 index = StructuredManifestEntry->SubNamespaces.Add( MakeShareable( new FStructuredEntry( NamespaceTokens[TokenIndex] ) ) );
				FoundNamespaceEntry = StructuredManifestEntry->SubNamespaces[index];
			}
			StructuredManifestEntry = FoundNamespaceEntry;
		}

		// We add the unstructured manifest entry to the hierarchy
		StructuredManifestEntry->ManifestEntries.AddUnique( UnstructuredManifestEntry );
	}
}


void FJsonInternationalizationManifestSerializer::SortStructuredData( TSharedPtr< FStructuredEntry > InElement )
{
	if( !InElement.IsValid() )
	{
		return;
	}

	// Sort the manifest entries by source text.
	InElement->ManifestEntries.Sort( FCompareManifestEntryBySource() );

	// Sort the manifest entry contexts by key/identifier
	for( TArray< TSharedPtr< FManifestEntry > >::TIterator Iter( InElement->ManifestEntries.CreateIterator() ); Iter; ++Iter)
	{
		TSharedPtr< FManifestEntry > SubEntry = *Iter;
		if( SubEntry.IsValid())
		{
			SubEntry->Contexts.Sort( );
		}
	}
	
	// Sort the subnamespaces by namespace string
	InElement->SubNamespaces.Sort( FCompareStructuredEntryByNamespace() );

	// Do the sorting for each of the subnamespaces
	for( TArray< TSharedPtr< FStructuredEntry > >::TIterator Iter( InElement->SubNamespaces.CreateIterator() ); Iter; ++Iter )
	{
		TSharedPtr< FStructuredEntry > SubElement = *Iter;

		SortStructuredData( SubElement );
	}
}


void FJsonInternationalizationManifestSerializer::StructuredDataToJsonObj( TSharedPtr< const FStructuredEntry > InElement, TSharedRef< FJsonObject > JsonObj )
{

	JsonObj->SetStringField( TAG_NAMESPACE, InElement->Namespace );

	TArray< TSharedPtr< FJsonValue > > NamespaceArray;
	TArray< TSharedPtr< FJsonValue > > EntryArray;

	//Write namespace content entries
	for( TArray< TSharedPtr< FManifestEntry > >::TConstIterator Iter( InElement->ManifestEntries ); Iter; ++Iter )
	{
		const TSharedPtr< FManifestEntry > Entry = *Iter;
		TSharedPtr< FJsonObject > EntryNode = MakeShareable( new FJsonObject );

		TSharedPtr<FJsonObject> SourceNode;

		if( Entry->Source.MetadataObj.IsValid() )
		{
			FJsonInternationalizationMetaDataSerializer::SerializeMetadata( Entry->Source.MetadataObj.ToSharedRef(), SourceNode );
		}
		
		if( !SourceNode.IsValid() )
		{
			SourceNode = MakeShareable( new FJsonObject );
		}

		// Add escapes for special chars - doesn't do backslash
		{
			FString ProcessedText = Entry->Source.Text;
			SourceNode->SetStringField(TAG_SOURCE_TEXT, ProcessedText);
		}

		EntryNode->SetObjectField( TAG_SOURCE, SourceNode );

		TArray< TSharedPtr< FJsonValue > > KeyArray;

		for(auto ContextIter = Entry->Contexts.CreateConstIterator(); ContextIter; ++ContextIter)
		{
			const FManifestContext& AContext = *ContextIter;

			FString ProcessedText = AContext.SourceLocation;
			ProcessedText.ReplaceInline( TEXT("\\"), TEXT("/"));
			ProcessedText.ReplaceInline( *FPaths::RootDir(), TEXT("/"));

			TSharedPtr<FJsonObject> KeyNode = MakeShareable( new FJsonObject );
			KeyNode->SetStringField( TAG_KEY, AContext.Key );
			KeyNode->SetStringField( TAG_PATH, ProcessedText );

			// We only add the optional field if it is true, it is assumed to be false otherwise.
			if( AContext.bIsOptional == true )
			{
				KeyNode->SetBoolField( TAG_OPTIONAL, AContext.bIsOptional);
			}

			TSharedPtr< FJsonObject > MetaDataNode = MakeShareable( new FJsonObject );

			if( AContext.InfoMetadataObj.IsValid() )
			{
				TSharedPtr< FJsonObject > InfoDataNode;
				FJsonInternationalizationMetaDataSerializer::SerializeMetadata( AContext.InfoMetadataObj.ToSharedRef(), InfoDataNode );
				if( InfoDataNode.IsValid() )
				{
					MetaDataNode->SetObjectField( TAG_METADATA_INFO, InfoDataNode );
				}
			}

			if( AContext.KeyMetadataObj.IsValid() )
			{
				TSharedPtr< FJsonObject > KeyDataNode;
				FJsonInternationalizationMetaDataSerializer::SerializeMetadata( AContext.KeyMetadataObj.ToSharedRef(), KeyDataNode );
				if( KeyDataNode.IsValid() )
				{
					MetaDataNode->SetObjectField( TAG_METADATA_KEY, KeyDataNode );
				}
			}

			if( MetaDataNode->Values.Num() > 0 )
			{
				KeyNode->SetObjectField( TAG_METADATA, MetaDataNode );
			}
			
			KeyArray.Add( MakeShareable( new FJsonValueObject( KeyNode ) ) );
		}

		EntryNode->SetArrayField( TAG_KEYCOLLECTION, KeyArray);

		EntryArray.Add( MakeShareable( new FJsonValueObject( EntryNode ) ) );
	}

	//Write the subnamespaces
	for( TArray< TSharedPtr< FStructuredEntry > >::TConstIterator Iter( InElement->SubNamespaces.CreateConstIterator() ); Iter; ++Iter )
	{
		const TSharedPtr< FStructuredEntry > SubElement = *Iter;

		if(SubElement.IsValid())
		{
			TSharedRef< FJsonObject > SubObject = MakeShareable( new FJsonObject );
			StructuredDataToJsonObj( SubElement, SubObject );

			NamespaceArray.Add( MakeShareable( new FJsonValueObject( SubObject ) ) );
		}
	}

	if( EntryArray.Num() > 0 )
	{
		JsonObj->SetArrayField( TAG_CHILDREN, EntryArray );
	}

	if( NamespaceArray.Num() > 0 )
	{
		JsonObj->SetArrayField( TAG_SUBNAMESPACES, NamespaceArray );
	}
}
