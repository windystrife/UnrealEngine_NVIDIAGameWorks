// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "Framework/Docking/TabManager.h"
#include "IMessageBus.h"
#include "IMessagingModule.h"
#include "Models/MessagingDebuggerCommands.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Styles/MessagingDebuggerStyle.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SMessagingDebugger.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/NameTypes.h"

#if WITH_EDITOR
	#include "WorkspaceMenuStructure.h"
	#include "WorkspaceMenuStructureModule.h"
#endif


#define LOCTEXT_NAMESPACE "FMessagingDebuggerModule"

static const FName MessagingDebuggerTabName("MessagingDebugger");


/**
 * Implements the MessagingDebugger module.
 */
class FMessagingDebuggerModule
	: public IModuleInterface
	, public IModularFeature
{
public:

	//~ IModuleInterface interface
	
	virtual void StartupModule() override
	{
		Style = MakeShareable(new FMessagingDebuggerStyle());

		FMessagingDebuggerCommands::Register();
		IModularFeatures::Get().RegisterModularFeature("MessagingDebugger", this);
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MessagingDebuggerTabName, FOnSpawnTab::CreateRaw(this, &FMessagingDebuggerModule::SpawnMessagingDebuggerTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Messaging Debugger"))
#if WITH_EDITOR
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
#else

#endif
			.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MessagingDebuggerTabIcon"))
			.SetTooltipText(LOCTEXT("TooltipText", "Visual debugger for the messaging sub-system."));
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MessagingDebuggerTabName);
		IModularFeatures::Get().UnregisterModularFeature("MessagingDebugger", this);
		FMessagingDebuggerCommands::Unregister();
	}

private:

	/**
	 * Create a new messaging debugger tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnMessagingDebuggerTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
			.TabRole(ETabRole::MajorTab);

		TSharedPtr<SWidget> TabContent;
		TSharedPtr<IMessageBus, ESPMode::ThreadSafe> MessageBus = IMessagingModule::Get().GetDefaultBus();

		if (MessageBus.IsValid())
		{
			TabContent = SNew(SMessagingDebugger, MajorTab, SpawnTabArgs.GetOwnerWindow(), MessageBus->GetTracer(), Style.ToSharedRef());
		}
		else
		{
			TabContent = SNew(STextBlock)
				.Text(LOCTEXT("MessagingSystemUnavailableError", "Sorry, the Messaging system is not available right now"));
		}

		MajorTab->SetContent(TabContent.ToSharedRef());

		return MajorTab;
	}

private:

	/** The plug-ins style set. */
	TSharedPtr<ISlateStyle> Style;
};


IMPLEMENT_MODULE(FMessagingDebuggerModule, MessagingDebugger);


#undef LOCTEXT_NAMESPACE
