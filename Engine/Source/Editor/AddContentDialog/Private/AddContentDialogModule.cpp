// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "IAddContentDialogModule.h"
#include "ContentSourceProviderManager.h"
#include "AddContentDialogStyle.h"
#include "SAddContentDialog.h"

#include "ContentSourceProviders/FeaturePack/FeaturePackContentSourceProvider.h"
#include "WidgetCarouselModule.h"
#include "WidgetCarouselStyle.h"

#define LOCTEXT_NAMESPACE "AddContentDialog"

class FAddContentDialogModule : public IAddContentDialogModule
{

public:
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<FWidgetCarouselModule>("WidgetCarousel");
		FWidgetCarouselModuleStyle::Initialize();

		ContentSourceProviderManager = TSharedPtr<FContentSourceProviderManager>(new FContentSourceProviderManager());
		ContentSourceProviderManager->RegisterContentSourceProvider(MakeShareable(new FFeaturePackContentSourceProvider()));
		FAddContentDialogStyle::Initialize();
	}

	virtual void ShutdownModule() override
	{
		FAddContentDialogStyle::Shutdown();
		FWidgetCarouselModuleStyle::Shutdown();
	}

	virtual TSharedRef<FContentSourceProviderManager> GetContentSourceProviderManager() override
	{
		return ContentSourceProviderManager.ToSharedRef();
	}

	virtual void ShowDialog(TSharedRef<SWindow> ParentWindow) override
	{
		if (AddContentDialog.IsValid() == false)
		{
			TSharedRef<SWindow> Dialog = SNew(SAddContentDialog);
			FSlateApplication::Get().AddWindowAsNativeChild(Dialog, ParentWindow);
			AddContentDialog = TWeakPtr<SWindow>(Dialog);
		}		
	}

private:
	TSharedPtr<FContentSourceProviderManager> ContentSourceProviderManager;
	TWeakPtr<SWindow> AddContentDialog;
};

IMPLEMENT_MODULE(FAddContentDialogModule, AddContentDialog);

#undef LOCTEXT_NAMESPACE
