// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginWardenModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

#include "SAuthorizingPlugin.h"

IMPLEMENT_MODULE( FPluginWardenModule, PluginWarden );

#define LOCTEXT_NAMESPACE "PluginWarden"

TSet<FString> AuthorizedPlugins;

void FPluginWardenModule::StartupModule()
{
	
}

void FPluginWardenModule::ShutdownModule()
{

}

void FPluginWardenModule::CheckEntitlementForPlugin(const FText& PluginFriendlyName, const FString& PluginItemId, const FString& PluginOfferId, TFunction<void()> AuthorizedCallback)
{
	// If we've previously authorized the plug-in, just immediately verify access.
	if ( AuthorizedPlugins.Contains(PluginItemId) )
	{
		AuthorizedCallback();
		return;
	}

	// Create the window
	TSharedRef<SWindow> AuthorizingPluginWindow = SNew(SWindow)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::Autosized)
		.Title(FText::Format(LOCTEXT("EntitlementCheckFormat", "{0} - Entitlement Check"), PluginFriendlyName));

	TSharedRef<SAuthorizingPlugin> PluginAuthPanel = SNew(SAuthorizingPlugin, AuthorizingPluginWindow, PluginFriendlyName, PluginItemId, PluginOfferId, AuthorizedCallback);

	AuthorizingPluginWindow->SetContent(PluginAuthPanel);

	FSlateApplication::Get().AddModalWindow(AuthorizingPluginWindow, nullptr);
}

#undef LOCTEXT_NAMESPACE
