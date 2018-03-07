// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/GatherableTextData.h"

FArchive& operator<<(FArchive& Archive, FTextSourceSiteContext& This)
{
	Archive << This.KeyName;
	Archive << This.SiteDescription;
	Archive << This.IsEditorOnly;
	Archive << This.IsOptional;
	Archive << This.InfoMetaData;
	Archive << This.KeyMetaData;

	return Archive;
}

FArchive& operator<<(FArchive& Archive, FTextSourceData& This)
{
	Archive << This.SourceString;
	Archive << This.SourceStringMetaData;

	return Archive;
}

FArchive& operator<<(FArchive& Archive, FGatherableTextData& This)
{
	Archive << This.NamespaceName;
	Archive << This.SourceData;
	Archive << This.SourceSiteContexts;

	return Archive;
}
