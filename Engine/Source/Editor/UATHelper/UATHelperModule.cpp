// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UATHelperModule.h"
#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Stats/Stats.h"
#include "Misc/MonitoredProcess.h"
#include "Modules/ModuleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "Framework/Docking/TabManager.h"
#include "Editor.h"
#include "EditorAnalytics.h"
#include "IUATHelperModule.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"

#include "GameProjectGenerationModule.h"
#include "AnalyticsEventAttribute.h"

#define LOCTEXT_NAMESPACE "UATHelper"

DEFINE_LOG_CATEGORY_STATIC(UATHelper, Log, All);

/* Event Data
*****************************************************************************/

struct EventData
{
	FString EventName;
	bool bProjectHasCode;
	double StartTime;
	IUATHelperModule::UatTaskResultCallack ResultCallback;
};

/* FMainFrameActionCallbacks callbacks
*****************************************************************************/

class FMainFrameActionsNotificationTask
{
public:

	FMainFrameActionsNotificationTask(TWeakPtr<SNotificationItem> InNotificationItemPtr, SNotificationItem::ECompletionState InCompletionState, const FText& InText)
		: CompletionState(InCompletionState)
		, NotificationItemPtr(InNotificationItemPtr)
		, Text(InText)
	{ }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if ( NotificationItemPtr.IsValid() )
		{
			if ( CompletionState == SNotificationItem::CS_Fail )
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
			}
			else
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
			}

			TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
			NotificationItem->SetText(Text);
			NotificationItem->SetCompletionState(CompletionState);
			NotificationItem->ExpireAndFadeout();
		}
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMainFrameActionsNotificationTask, STATGROUP_TaskGraphTasks);
	}

private:

	SNotificationItem::ECompletionState CompletionState;
	TWeakPtr<SNotificationItem> NotificationItemPtr;
	FText Text;
};


/**
* Helper class to deal with packaging issues encountered in UAT.
**/
class FPackagingErrorHandler
{
private:

	/**
	* Create a message to send to the Message Log.
	*
	* @Param MessageString - The error we wish to send to the Message Log.
	* @Param MessageType - The severity of the message, i.e. error, warning etc.
	**/
	static void AddMessageToMessageLog(FString MessageString, EMessageSeverity::Type MessageType)
	{
		FText MsgText = FText::FromString(MessageString);

		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(MessageType);
		Message->AddToken(FTextToken::Create(MsgText));

		FMessageLog MessageLog("PackagingResults");
		MessageLog.AddMessage(Message);
	}

	/**
	* Send Error to the Message Log.
	*
	* @Param MessageString - The error we wish to send to the Message Log.
	* @Param MessageType - The severity of the message, i.e. error, warning etc.
	**/
	static void SyncMessageWithMessageLog(FString MessageString, EMessageSeverity::Type MessageType)
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SendPackageErrorToMessageLog"),
		STAT_FSimpleDelegateGraphTask_SendPackageErrorToMessageLog,
			STATGROUP_TaskGraphTasks);

		// Remove any new line terminators
		MessageString.ReplaceInline(TEXT("\r"), TEXT(""));
		MessageString.ReplaceInline(TEXT("\n"), TEXT(""));

		/**
		* Dispatch the error from packaging to the message log.
		**/
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&FPackagingErrorHandler::AddMessageToMessageLog, MessageString, MessageType),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SendPackageErrorToMessageLog),
			nullptr, ENamedThreads::GameThread
			);
	}

public:
	/**
	* Determine if the output is an error we wish to send to the Message Log.
	*
	* @Param UATOutput - The current line of output from the UAT package process.
	**/
	static void ProcessAndHandleCookErrorOutput(FString UATOutput)
	{
		FString LhsUATOutputMsg, ParsedCookIssue;

		// note: CookResults:Warning: actually outputs some unhandled errors.
		if ( UATOutput.Split(TEXT("CookResults:Warning: "), &LhsUATOutputMsg, &ParsedCookIssue) )
		{
			SyncMessageWithMessageLog(ParsedCookIssue, EMessageSeverity::Warning);
		}

		if ( UATOutput.Split(TEXT("CookResults:Error: "), &LhsUATOutputMsg, &ParsedCookIssue) )
		{
			SyncMessageWithMessageLog(ParsedCookIssue, EMessageSeverity::Error);
		}
	}

	/**
	* Send the UAT Packaging error message to the Message Log.
	*
	* @Param ErrorCode - The UAT return code we received and wish to display the error message for.
	**/
	static void SendPackagingErrorToMessageLog(int32 ErrorCode)
	{
		SyncMessageWithMessageLog(FEditorAnalytics::TranslateErrorCode(ErrorCode), EMessageSeverity::Error);
	}
};


DECLARE_CYCLE_STAT(TEXT("Requesting FUATHelperModule::HandleUatProcessCompleted message dialog to present the error message"), STAT_FUATHelperModule_HandleUatProcessCompleted_DialogMessage, STATGROUP_TaskGraphTasks);


class FUATHelperModule : public IUATHelperModule
{
public:
	FUATHelperModule()
	{
	}

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual void CreateUatTask( const FString& CommandLine, const FText& PlatformDisplayName, const FText& TaskName, const FText &TaskShortName, const FSlateBrush* TaskIcon, UatTaskResultCallack ResultCallback )
	{
		// make sure that the UAT batch file is in place
	#if PLATFORM_WINDOWS
		FString RunUATScriptName = TEXT("RunUAT.bat");
		FString CmdExe = TEXT("cmd.exe");
	#elif PLATFORM_LINUX
		FString RunUATScriptName = TEXT("RunUAT.sh");
		FString CmdExe = TEXT("/bin/bash");
	#else
		FString RunUATScriptName = TEXT("RunUAT.command");
		FString CmdExe = TEXT("/bin/sh");
	#endif

		FString UatPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles") / RunUATScriptName);
		FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
		bool bHasCode = GameProjectModule.Get().ProjectHasCodeFiles();

		if (!FPaths::FileExists(UatPath))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("File"), FText::FromString(UatPath));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));

			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
			FString EventName = (CommandLine.Contains(TEXT("-package")) ? TEXT("Editor.Package") : TEXT("Editor.Cook"));
			FEditorAnalytics::ReportEvent(EventName + TEXT(".Failed"), PlatformDisplayName.ToString(), bHasCode, EAnalyticsErrorCodes::UATNotFound, ParamArray);
			
			return;
		}

	#if PLATFORM_WINDOWS
		FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *UatPath, *CommandLine);
	#else
		FString FullCommandLine = FString::Printf(TEXT("\"%s\" %s"), *UatPath, *CommandLine);
	#endif

		TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));

		// create notification item
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Platform"), PlatformDisplayName);
		Arguments.Add(TEXT("TaskName"), TaskName);
		FNotificationInfo Info( FText::Format( LOCTEXT("UatTaskInProgressNotification", "{TaskName} for {Platform}..."), Arguments) );
		
		Info.Image = TaskIcon;
		Info.bFireAndForget = false;
		Info.ExpireDuration = 3.0f;
		Info.Hyperlink = FSimpleDelegate::CreateStatic(&FUATHelperModule::HandleUatHyperlinkNavigate);
		Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
		Info.ButtonDetails.Add(
			FNotificationButtonInfo(
				LOCTEXT("UatTaskCancel", "Cancel"),
				LOCTEXT("UatTaskCancelToolTip", "Cancels execution of this task."),
				FSimpleDelegate::CreateStatic(&FUATHelperModule::HandleUatCancelButtonClicked, UatProcess)
			)
		);

		TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

		if (!NotificationItem.IsValid())
		{
			return;
		}

		FString EventName = (CommandLine.Contains(TEXT("-package")) ? TEXT("Editor.Package") : TEXT("Editor.Cook"));
		FEditorAnalytics::ReportEvent(EventName + TEXT(".Start"), PlatformDisplayName.ToString(), bHasCode);

		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);

		// launch the packager
		TWeakPtr<SNotificationItem> NotificationItemPtr(NotificationItem);

		EventData Data;
		Data.StartTime = FPlatformTime::Seconds();
		Data.EventName = EventName;
		Data.bProjectHasCode = bHasCode;
		Data.ResultCallback = ResultCallback;
		UatProcess->OnCanceled().BindStatic(&FUATHelperModule::HandleUatProcessCanceled, NotificationItemPtr, PlatformDisplayName, TaskShortName, Data);
		UatProcess->OnCompleted().BindStatic(&FUATHelperModule::HandleUatProcessCompleted, NotificationItemPtr, PlatformDisplayName, TaskShortName, Data);
		UatProcess->OnOutput().BindStatic(&FUATHelperModule::HandleUatProcessOutput, NotificationItemPtr, PlatformDisplayName, TaskShortName);

		TWeakPtr<FMonitoredProcess> UatProcessPtr(UatProcess);
		FEditorDelegates::OnShutdownPostPackagesSaved.Add(FSimpleDelegate::CreateStatic(&FUATHelperModule::HandleUatCancelButtonClicked, UatProcessPtr));

		if (UatProcess->Launch())
		{
			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileStart_Cue.CompileStart_Cue"));
		}
		else
		{
			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));

			NotificationItem->SetText(LOCTEXT("UatLaunchFailedNotification", "Failed to launch Unreal Automation Tool (UAT)!"));
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->ExpireAndFadeout();

			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
			FEditorAnalytics::ReportEvent(EventName + TEXT(".Failed"), PlatformDisplayName.ToString(), bHasCode, EAnalyticsErrorCodes::UATLaunchFailure, ParamArray);
			if (ResultCallback)
			{
				ResultCallback(TEXT("FailedToStart"), 0.0f);
			}
		}
	}

	static void HandleUatHyperlinkNavigate()
	{
		FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog"));
	}


	static void HandleUatCancelButtonClicked(TSharedPtr<FMonitoredProcess> PackagerProcess)
	{
		if ( PackagerProcess.IsValid() )
		{
			PackagerProcess->Cancel(true);
		}
	}

	static void HandleUatCancelButtonClicked(TWeakPtr<FMonitoredProcess> PackagerProcessPtr)
	{
		TSharedPtr<FMonitoredProcess> PackagerProcess = PackagerProcessPtr.Pin();
		if ( PackagerProcess.IsValid() )
		{
			PackagerProcess->Cancel(true);
		}
	}

	static void HandleUatProcessCanceled(TWeakPtr<SNotificationItem> NotificationItemPtr, FText PlatformDisplayName, FText TaskName, EventData Event)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Platform"), PlatformDisplayName);
		Arguments.Add(TEXT("TaskName"), TaskName);

		TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Fail,
			FText::Format(LOCTEXT("UatProcessFailedNotification", "{TaskName} canceled!"), Arguments)
			);

		TArray<FAnalyticsEventAttribute> ParamArray;
		const double TimeSec = FPlatformTime::Seconds() - Event.StartTime;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), TimeSec));
		FEditorAnalytics::ReportEvent(Event.EventName + TEXT(".Canceled"), PlatformDisplayName.ToString(), Event.bProjectHasCode, ParamArray);
		if (Event.ResultCallback)
		{
			Event.ResultCallback(TEXT("Canceled"), TimeSec);
		}
		//	FMessageLog("PackagingResults").Warning(FText::Format(LOCTEXT("UatProcessCanceledMessageLog", "{TaskName} for {Platform} canceled by user"), Arguments));
	}

	static void HandleUatProcessCompleted(int32 ReturnCode, TWeakPtr<SNotificationItem> NotificationItemPtr, FText PlatformDisplayName, FText TaskName, EventData Event)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Platform"), PlatformDisplayName);
		Arguments.Add(TEXT("TaskName"), TaskName);
		const double TimeSec = FPlatformTime::Seconds() - Event.StartTime;

		if ( ReturnCode == 0 )
		{
			TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
				NotificationItemPtr,
				SNotificationItem::CS_Success,
				FText::Format(LOCTEXT("UatProcessSucceededNotification", "{TaskName} complete!"), Arguments)
				);

			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), TimeSec));
			FEditorAnalytics::ReportEvent(Event.EventName + TEXT(".Completed"), PlatformDisplayName.ToString(), Event.bProjectHasCode, ParamArray);
			if (Event.ResultCallback)
			{
				Event.ResultCallback(TEXT("Completed"), TimeSec);
			}

			//		FMessageLog("PackagingResults").Info(FText::Format(LOCTEXT("UatProcessSuccessMessageLog", "{TaskName} for {Platform} completed successfully"), Arguments));
		}
		else
		{
			TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
				NotificationItemPtr,
				SNotificationItem::CS_Fail,
				FText::Format(LOCTEXT("PackagerFailedNotification", "{TaskName} failed!"), Arguments)
				);

			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), TimeSec));
			FEditorAnalytics::ReportEvent(Event.EventName + TEXT(".Failed"), PlatformDisplayName.ToString(), Event.bProjectHasCode, ReturnCode, ParamArray);
			if (Event.ResultCallback)
			{
				Event.ResultCallback(TEXT("Failed"), TimeSec);
			}

			// Send the error to the Message Log.
			if ( TaskName.EqualTo(LOCTEXT("PackagingTaskName", "Packaging")) )
			{
				FPackagingErrorHandler::SendPackagingErrorToMessageLog(ReturnCode);
			}

			// Present a message dialog if we want the error message to be prominent.
			if ( FEditorAnalytics::ShouldElevateMessageThroughDialog(ReturnCode) )
			{
				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateLambda([=] (){
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FEditorAnalytics::TranslateErrorCode(ReturnCode)));
				}),
					GET_STATID(STAT_FUATHelperModule_HandleUatProcessCompleted_DialogMessage),
					nullptr,
					ENamedThreads::GameThread
					);
			}
			//		FMessageLog("PackagingResults").Info(FText::Format(LOCTEXT("UatProcessFailedMessageLog", "{TaskName} for {Platform} failed"), Arguments));
		}
	}


	static void HandleUatProcessOutput(FString Output, TWeakPtr<SNotificationItem> NotificationItemPtr, FText PlatformDisplayName, FText TaskName)
	{
		if ( !Output.IsEmpty() && !Output.Equals("\r") )
		{
			UE_LOG(UATHelper, Log, TEXT("%s (%s): %s"), *TaskName.ToString(), *PlatformDisplayName.ToString(), *Output);

			if ( TaskName.EqualTo(LOCTEXT("PackagingTaskName", "Packaging")) )
			{
				// Deal with any cook errors that may have been encountered.
				FPackagingErrorHandler::ProcessAndHandleCookErrorOutput(Output);
			}
		}
	}
};


IMPLEMENT_MODULE(FUATHelperModule, UATHelper)


#undef LOCTEXT_NAMESPACE
