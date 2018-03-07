// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Widgets/SToolTip.h"
#include "IDocumentationModule.h"
#include "IDocumentation.h"
#include "Documentation.h"

class FDocumentationModule : public IDocumentationModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		Documentation = FDocumentation::Create();

		FMultiBoxSettings::ToolTipConstructor = FMultiBoxSettings::FConstructToolTip::CreateRaw( this, &FDocumentationModule::ConstructDefaultToolTip );
	}

	virtual void ShutdownModule() override
	{
		if ( FModuleManager::Get().IsModuleLoaded("Slate") )
		{
			FMultiBoxSettings::ResetToolTipConstructor();
		}

		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
	}

	virtual class TSharedRef< IDocumentation > GetDocumentation() const override
	{
		return Documentation.ToSharedRef();
	}

private:

	TSharedRef< SToolTip > ConstructDefaultToolTip( const TAttribute<FText>& ToolTipText, const TSharedPtr<SWidget>& OverrideContent, const TSharedPtr<const FUICommandInfo>& Action )
	{
		if ( Action.IsValid() )
		{
			return Documentation->CreateToolTip( ToolTipText, OverrideContent, FString( TEXT("Shared/") ) + Action->GetBindingContext().ToString(), Action->GetCommandName().ToString() );
		}

		TSharedPtr< SWidget > ToolTipContent;
        if ( OverrideContent.IsValid() )
        {
            ToolTipContent = OverrideContent;
        }
        else
        {
            ToolTipContent = SNullWidget::NullWidget;
        }
        
		return SNew( SToolTip )
			   .Text( ToolTipText )
			   [
					ToolTipContent.ToSharedRef()
			   ];
	}

private:

	TSharedPtr< IDocumentation > Documentation;

};

IMPLEMENT_MODULE( FDocumentationModule, Documentation )
