// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DefaultSourceControlProvider.h"
#include "Logging/MessageLog.h"

#if SOURCE_CONTROL_WITH_SLATE
	#include "Widgets/SNullWidget.h"
#endif

#define LOCTEXT_NAMESPACE "DefaultSourceControlProvider"

void FDefaultSourceControlProvider::Init(bool bForceConnection)
{
	FMessageLog("SourceControl").Info(LOCTEXT("SourceControlDisabled", "Source control is disabled"));
}

void FDefaultSourceControlProvider::Close()
{

}

FText FDefaultSourceControlProvider::GetStatusText() const
{
	return LOCTEXT("SourceControlDisabled", "Source control is disabled");
}

bool FDefaultSourceControlProvider::IsAvailable() const
{
	return false;
}

bool FDefaultSourceControlProvider::IsEnabled() const
{
	return false;
}

const FName& FDefaultSourceControlProvider::GetName(void) const
{
	static FName ProviderName("None"); 
	return ProviderName; 
}

ECommandResult::Type FDefaultSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	return ECommandResult::Failed;
}

TArray<FSourceControlStateRef> FDefaultSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	return TArray<FSourceControlStateRef>();
}

FDelegateHandle FDefaultSourceControlProvider::RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	return FDelegateHandle();
}

void FDefaultSourceControlProvider::UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle )
{

}

ECommandResult::Type FDefaultSourceControlProvider::Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	return ECommandResult::Failed;
}

bool FDefaultSourceControlProvider::CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const
{
	return false;
}

void FDefaultSourceControlProvider::CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation )
{
}

bool FDefaultSourceControlProvider::UsesLocalReadOnlyState() const
{
	return false;
}

bool FDefaultSourceControlProvider::UsesChangelists() const
{
	return false;
}

bool FDefaultSourceControlProvider::UsesCheckout() const
{
	return false;
}

void FDefaultSourceControlProvider::Tick()
{

}

TArray< TSharedRef<ISourceControlLabel> > FDefaultSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	return TArray< TSharedRef<ISourceControlLabel> >();
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FDefaultSourceControlProvider::MakeSettingsWidget() const
{
	return SNullWidget::NullWidget;
}
#endif // SOURCE_CONTROL_WITH_SLATE

#undef LOCTEXT_NAMESPACE
