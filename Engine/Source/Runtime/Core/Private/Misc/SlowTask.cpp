// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/SlowTask.h"
#include "HAL/PlatformTime.h"
#include "Misc/FeedbackContext.h"

FSlowTask::FSlowTask(float InAmountOfWork, const FText& InDefaultMessage, bool bInEnabled, FFeedbackContext& InContext)
	: DefaultMessage(InDefaultMessage)
	, FrameMessage()
	, TotalAmountOfWork(InAmountOfWork)
	, CompletedWork(0)
	, CurrentFrameScope(0)
	, Visibility(ESlowTaskVisibility::Default)
	, StartTime(FPlatformTime::Seconds())
	, bEnabled(bInEnabled && IsInGameThread())
	, bCreatedDialog(false)		// only set to true if we create a dialog
	, Context(InContext)
{
	// If we have no work to do ourselves, create an arbitrary scope so that any actions performed underneath this still contribute to this one.
	if (TotalAmountOfWork == 0.f)
	{
		TotalAmountOfWork = CurrentFrameScope = 1.f;
	}
}

void FSlowTask::Initialize()
{
	if (bEnabled)
	{
		Context.ScopeStack->Push(this);
	}
}

void FSlowTask::Destroy()
{
	if (bEnabled)
	{
		if (bCreatedDialog)
		{
			checkSlow(GIsSlowTask);
			Context.FinalizeSlowTask();
		}

		FSlowTaskStack& Stack = *Context.ScopeStack;
		checkSlow(Stack.Num() != 0 && Stack.Last() == this);

		auto* Task = Stack.Last();
		if (ensureMsgf(Task == this, TEXT("Out-of-order scoped task construction/destruction")))
		{
			Stack.Pop(false);
		}
		else
		{
			Stack.RemoveSingleSwap(this, false);
		}

		if (Stack.Num() != 0)
		{
			// Stop anything else contributing to the parent frame
			auto* Parent = Stack.Last();
			Parent->EnterProgressFrame(0, Parent->FrameMessage);
		}

	}
}

void FSlowTask::MakeDialogDelayed(float Threshold, bool bShowCancelButton, bool bAllowInPIE)
{
	OpenDialogThreshold = Threshold;
	bDelayedDialogShowCancelButton = bShowCancelButton;
	bDelayedDialogAllowInPIE = bAllowInPIE;
}

void FSlowTask::EnterProgressFrame(float ExpectedWorkThisFrame, FText Text)
{
	FrameMessage = Text;
	CompletedWork += CurrentFrameScope;

#if PLATFORM_XBOXONE
	// Make sure OS events are getting through while the task is being processed
	FXboxOneMisc::PumpMessages(true);
#endif

	const float WorkRemaining = TotalAmountOfWork - CompletedWork;
	// Add a small threshold here because when there are a lot of tasks, numerical imprecision can add up and trigger this.
	ensureMsgf(ExpectedWorkThisFrame <= 1.01f * TotalAmountOfWork - CompletedWork, TEXT("Work overflow in slow task. Please revise call-site to account for entire progress range."));
	CurrentFrameScope = FMath::Min(WorkRemaining, ExpectedWorkThisFrame);

	if (!bCreatedDialog && OpenDialogThreshold.IsSet() && static_cast<float>(FPlatformTime::Seconds() - StartTime) > OpenDialogThreshold.GetValue())
	{
		MakeDialog(bDelayedDialogShowCancelButton, bDelayedDialogAllowInPIE);
	}

	if (bEnabled)
	{
		Context.RequestUpdateUI(bCreatedDialog || (*Context.ScopeStack)[0] == this);
	}
}

FText FSlowTask::GetCurrentMessage() const
{
	return FrameMessage.IsEmpty() ? DefaultMessage : FrameMessage;
}

void FSlowTask::MakeDialog(bool bShowCancelButton, bool bAllowInPIE)
{
	const bool bIsDisabledByPIE = Context.IsPlayingInEditor() && !bAllowInPIE;
	const bool bIsDialogAllowed = bEnabled && !GIsSilent && !bIsDisabledByPIE && !IsRunningCommandlet() && IsInGameThread();
	if (!GIsSlowTask && bIsDialogAllowed)
	{
		Context.StartSlowTask(GetCurrentMessage(), bShowCancelButton);
		if (GIsSlowTask)
		{
			bCreatedDialog = true;
		}
	}
}
