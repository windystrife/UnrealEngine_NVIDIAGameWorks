// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserModule.h"
#include "ContentBrowserLog.h"
#include "ContentBrowserSingleton.h"

IMPLEMENT_MODULE( FContentBrowserModule, ContentBrowser );
DEFINE_LOG_CATEGORY(LogContentBrowser);

void FContentBrowserModule::StartupModule()
{
	ContentBrowserSingleton = new FContentBrowserSingleton();
}

void FContentBrowserModule::ShutdownModule()
{	
	if ( ContentBrowserSingleton )
	{
		delete ContentBrowserSingleton;
		ContentBrowserSingleton = NULL;
	}

}

IContentBrowserSingleton& FContentBrowserModule::Get() const
{
	check(ContentBrowserSingleton);
	return *ContentBrowserSingleton;

}
