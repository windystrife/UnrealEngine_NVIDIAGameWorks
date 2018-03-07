// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/FeedbackContext.h"
#include "HAL/PlatformTime.h"

FFeedbackContext::FFeedbackContext()
	: TreatWarningsAsErrors(0)
	, ScopeStack(MakeShareable(new FSlowTaskStack))
{
}

FFeedbackContext::~FFeedbackContext()
{
	ensureMsgf(LegacyAPIScopes.Num() == 0, TEXT("EndSlowTask has not been called for %d outstanding tasks"), LegacyAPIScopes.Num());
}

void FFeedbackContext::RequestUpdateUI(bool bForceUpdate)
{
	// Only update a maximum of 5 times a second
	static double MinUpdateTimeS = 0.2;

	static double LastUIUpdateTime = FPlatformTime::Seconds();
	const double CurrentTime = FPlatformTime::Seconds();

	if (bForceUpdate || CurrentTime - LastUIUpdateTime > MinUpdateTimeS)
	{
		LastUIUpdateTime = CurrentTime;
		UpdateUI();
	}
}

void FFeedbackContext::UpdateUI()
{
	ensure(IsInGameThread());

	if (ScopeStack->Num() != 0)
	{
		ProgressReported(ScopeStack->GetProgressFraction(0), (*ScopeStack)[0]->GetCurrentMessage());
	}
}

/**** Begin legacy API ****/
void FFeedbackContext::BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton )
{
	ensure(IsInGameThread());

	TUniquePtr<FSlowTask> NewScope(new FSlowTask(0, Task, true, *this));
	if (ShowProgressDialog)
	{
		NewScope->MakeDialog(bShowCancelButton);
	}

	NewScope->Initialize();
	LegacyAPIScopes.Add(MoveTemp(NewScope));
}

void FFeedbackContext::UpdateProgress( int32 Numerator, int32 Denominator )
{
	ensure(IsInGameThread());

	if (LegacyAPIScopes.Num() != 0)
	{
		LegacyAPIScopes.Last()->TotalAmountOfWork = Denominator;
		LegacyAPIScopes.Last()->CompletedWork = Numerator;
		LegacyAPIScopes.Last()->CurrentFrameScope = Denominator - Numerator;
		RequestUpdateUI();
	}
}

void FFeedbackContext::StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
{
	ensure(IsInGameThread());

	if (LegacyAPIScopes.Num() != 0)
	{
		if (Numerator > 0 && Denominator > 0)
		{
			UpdateProgress(Numerator, Denominator);
		}
		LegacyAPIScopes.Last()->FrameMessage = StatusText;
		RequestUpdateUI();
	}
}

void FFeedbackContext::StatusForceUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
{
	ensure(IsInGameThread());

	if (LegacyAPIScopes.Num() != 0)
	{
		UpdateProgress(Numerator, Denominator);
		LegacyAPIScopes.Last()->FrameMessage = StatusText;
		UpdateUI();
	}
}

void FFeedbackContext::EndSlowTask()
{
	ensure(IsInGameThread());

	check(LegacyAPIScopes.Num() != 0);
	LegacyAPIScopes.Last()->Destroy();
	LegacyAPIScopes.Pop();
}
/**** End legacy API ****/

bool FFeedbackContext::IsPlayingInEditor() const
{
	return GIsPlayInEditorWorld;
}
