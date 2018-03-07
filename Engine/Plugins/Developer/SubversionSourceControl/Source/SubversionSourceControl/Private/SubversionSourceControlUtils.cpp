// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlUtils.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ISourceControlModule.h"
#include "SubversionSourceControlModule.h"
#include "SubversionSourceControlCommand.h"
#include "XmlFile.h"

namespace SubversionSourceControlConstants
{
	/** The maximum number of files we submit in a single svn command */
	const int32 MaxFilesPerBatch = 50;
}

FSVNScopedTempFile::FSVNScopedTempFile(const FText& InText)
{
	Filename = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), TEXT("SVN-Temp"), TEXT(".txt"));
	if (!FFileHelper::SaveStringToFile(InText.ToString(), *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to write to temp file: %s"), *Filename);
	}
}

FSVNScopedTempFile::FSVNScopedTempFile(const FString& InText)
{
	Filename = FPaths::CreateTempFilename( *FPaths::ProjectLogDir(), TEXT("SVN-Temp"), TEXT( ".txt" ) );
	if (!FFileHelper::SaveStringToFile(InText, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to write to temp file: %s"), *Filename);
	}
}

FSVNScopedTempFile::~FSVNScopedTempFile()
{
	if(FPaths::FileExists(Filename))
	{
		if(!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to delete temp file: %s"), *Filename);
		}
	}
}

const FString& FSVNScopedTempFile::GetFilename() const
{
	return Filename;
}

namespace SubversionSourceControlUtils
{

static FString DetectSubversionPath()
{
	auto& Settings = FModuleManager::GetModulePtr<FSubversionSourceControlModule>("SubversionSourceControl")->AccessSettings();

	FString SVNPath = Settings.GetExecutableOverride();
	if (!SVNPath.IsEmpty())
	{
		if (FPaths::FileExists(SVNPath))
		{
			UE_LOG(LogSourceControl, Log, TEXT("Using user-supplied path %s for svn operations"), *FPaths::ConvertRelativePathToFull(SVNPath));
			return SVNPath;
		}
		
		UE_LOG(LogSourceControl, Log, TEXT("Specified svn executable (%s) does not exist. Falling back to default behaviour."), *SVNPath);
	}

	const bool bLaunchDetached = false;
	const bool bLaunchHidden = true;

#if PLATFORM_WINDOWS
	const TCHAR* Command[] = { TEXT("where"), TEXT("svn.exe") };
	const FString DefaultPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/svn") / FPlatformProcess::GetBinariesSubdirectory() / TEXT("svn.exe");
#elif PLATFORM_MAC
	const TCHAR* Command[] = { TEXT("/usr/bin/which"), TEXT("svn") };
	const FString DefaultPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/svn") / FPlatformProcess::GetBinariesSubdirectory() / TEXT("bin/svn");
#else
	const TCHAR* Command[] = { TEXT("/usr/bin/which"), TEXT("svn") };
	const FString DefaultPath = TEXT("/usr/bin/svn");
#endif

	{
		// Attmpt to detect a system wide version of the svn command line tools
		void* ReadPipe = nullptr, *WritePipe = nullptr;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		FProcHandle ProcHandle = FPlatformProcess::CreateProc(Command[0], Command[1], bLaunchDetached, bLaunchHidden, bLaunchHidden, NULL, 0, NULL, WritePipe);
		if (ProcHandle.IsValid())
		{
			FPlatformProcess::WaitForProc(ProcHandle);
			SVNPath = FPlatformProcess::ReadPipe(ReadPipe);

			// Trim at the first \n
			int32 NewLine = INDEX_NONE;
			if (SVNPath.FindChar('\n', NewLine))
			{
				SVNPath = SVNPath.Left(NewLine);
			}
		}
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		FPlatformProcess::CloseProc(ProcHandle);
	}

	bool bPathIsValid = !SVNPath.IsEmpty() && FPaths::FileExists(SVNPath);

#if PLATFORM_MAC
	// On mac we need to check that the developer tools are installed if the svn path is /usr/bin/svn
	if (SVNPath == TEXT("/usr/bin/svn"))
	{
		void* ReadPipe = nullptr, *WritePipe = nullptr;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		FProcHandle DevToolsProc = FPlatformProcess::CreateProc(TEXT("/usr/bin/xcode-select"), TEXT("-p"), bLaunchDetached, bLaunchHidden, bLaunchHidden, NULL, 0, NULL, WritePipe);
		if (DevToolsProc.IsValid())
		{
			FPlatformProcess::WaitForProc(DevToolsProc);

			int32 ReturnCode = 0;
			if (!FPlatformProcess::GetProcReturnCode(DevToolsProc, &ReturnCode) || ReturnCode != 0)
			{
				bPathIsValid = false;
			}
		}
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		FPlatformProcess::CloseProc(DevToolsProc);
	}
#endif

	if (!bPathIsValid)
	{
		UE_LOG(LogSourceControl, Log, TEXT("Unable to detect system-level svn binary."));
		SVNPath = DefaultPath;
	}

	UE_LOG(LogSourceControl, Log, TEXT("Using path %s for svn operations"), *FPaths::ConvertRelativePathToFull(SVNPath));
	return SVNPath;
}

static bool RunCommandInternal(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, FString& OutResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password)
{
	int32 ReturnCode = 0;
	FString StdError;
	FString FullCommand = InCommand;

	for (int32 Index = 0; Index < InParameters.Num(); Index++)
	{
		FullCommand += TEXT(" ");
		FullCommand += InParameters[Index];
	}

	for (int32 Index = 0; Index < InFiles.Num(); Index++)
	{
		FullCommand += TEXT(" \"") + InFiles[Index] + TEXT("\"");
	}

	// always non-interactive
	FullCommand += TEXT(" --non-interactive");

	// always trust server cert
	FullCommand += TEXT(" --trust-server-cert");

	if (UserName.Len() > 0)
	{
		FullCommand += FString::Printf(TEXT(" --username %s"), *UserName);
	}

	// note: dont mirror passwords to the output log
	UE_LOG(LogSourceControl, Log, TEXT("Attempting \"svn %s --password ********\""), *FullCommand);

	if (Password.Len() > 0)
	{
		FullCommand += FString::Printf(TEXT(" --password \"%s\""), *Password);
	}
	
	static const FString SVNBinaryPath = DetectSubversionPath();
	FPlatformProcess::ExecProcess(*SVNBinaryPath, *FullCommand, &ReturnCode, &OutResults, &StdError);

	// parse output & errors
	TArray<FString> Errors;
	StdError.ParseIntoArray(Errors, LINE_TERMINATOR, true);
	OutErrorMessages.Append(MoveTemp(Errors));

	return ReturnCode == 0;
}

bool RunCommand(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, TArray<FXmlFile>& OutXmlResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password)
{
	bool bResult = true;

	TArray<FString> Parameters = InParameters;
	Parameters.Add(TEXT("--xml"));

	if(InFiles.Num() > 0)
	{
		// Batch files up so we dont exceed command-line limits
		int32 FileCount = 0;
		while(FileCount < InFiles.Num())
		{
			TArray<FString> FilesInBatch;
			for(int32 FileIndex = 0; FileCount < InFiles.Num() && FileIndex < SubversionSourceControlConstants::MaxFilesPerBatch; FileIndex++, FileCount++)
			{
				FilesInBatch.Add(InFiles[FileCount]);
			}

			FString Results;
			const bool bThisResult = RunCommandInternal(InCommand, FilesInBatch, Parameters, Results, OutErrorMessages, UserName, Password);
			bResult &= bThisResult;
			if(bThisResult)
			{
				FXmlFile* XmlFile = new (OutXmlResults) FXmlFile();
				bResult &= XmlFile->LoadFile(Results, EConstructMethod::ConstructFromBuffer);
			}
		}
	}
	else
	{
		FString Results;
		const bool bThisResult = RunCommandInternal(InCommand, InFiles, Parameters, Results, OutErrorMessages, UserName, Password);
		bResult &= bThisResult;
		if(bThisResult)
		{
			FXmlFile* XmlFile = new (OutXmlResults) FXmlFile();
			bResult &= XmlFile->LoadFile(Results, EConstructMethod::ConstructFromBuffer);
		}
	}

	return bResult;
}

bool RunAtomicCommand(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password)
{
	check(InFiles.Num() < SubversionSourceControlConstants::MaxFilesPerBatch);

	FString Results;
	if( RunCommandInternal(InCommand, InFiles, InParameters, Results, OutErrorMessages, UserName, Password) )
	{
		Results.ParseIntoArray(OutResults, LINE_TERMINATOR, true);
		return true;
	}
	return false;
}

bool RunCommand(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password)
{
	bool bResult = true;

	if(InFiles.Num() > 0)
	{
		// Batch files up so we dont exceed command-line limits
		int32 FileCount = 0;
		while(FileCount < InFiles.Num())
		{
			TArray<FString> FilesInBatch;
			for(int32 FileIndex = 0; FileIndex < InFiles.Num() && FileIndex < SubversionSourceControlConstants::MaxFilesPerBatch; FileIndex++, FileCount++)
			{
				FilesInBatch.Add(InFiles[FileIndex]);
			}

			FString Results;
			bResult &= RunCommandInternal(InCommand, FilesInBatch, InParameters, Results, OutErrorMessages, UserName, Password);
			Results.ParseIntoArray(OutResults, LINE_TERMINATOR, true);
		}
	}
	else
	{
		FString Results;
		bResult &= RunCommandInternal(InCommand, InFiles, InParameters, Results, OutErrorMessages, UserName, Password);
		Results.ParseIntoArray(OutResults, LINE_TERMINATOR, true);
	}

	return bResult;
}

/** Helper function for ParseStatusResults */
static EWorkingCopyState::Type GetWorkingCopyState(const FString& InValue)
{
	static TMap<FString, EWorkingCopyState::Type> ValueStateMap;
	if(ValueStateMap.Num() == 0)
	{
		ValueStateMap.Add(TEXT("none"), EWorkingCopyState::Unknown);
		ValueStateMap.Add(TEXT("normal"), EWorkingCopyState::Pristine);
		ValueStateMap.Add(TEXT("added"), EWorkingCopyState::Added);
		ValueStateMap.Add(TEXT("conflicted"), EWorkingCopyState::Conflicted);
		ValueStateMap.Add(TEXT("deleted"), EWorkingCopyState::Deleted);
		ValueStateMap.Add(TEXT("external"), EWorkingCopyState::External);
		ValueStateMap.Add(TEXT("ignored"), EWorkingCopyState::Ignored);
		ValueStateMap.Add(TEXT("incomplete"), EWorkingCopyState::Incomplete);
		ValueStateMap.Add(TEXT("merged"), EWorkingCopyState::Merged);
		ValueStateMap.Add(TEXT("missing"), EWorkingCopyState::Missing);
		ValueStateMap.Add(TEXT("modified"), EWorkingCopyState::Modified);
		ValueStateMap.Add(TEXT("obstructed"), EWorkingCopyState::Obstructed);
		ValueStateMap.Add(TEXT("unversioned"), EWorkingCopyState::NotControlled);
	}

	return ValueStateMap.FindRef(InValue);
}

/** Helper function for ParseStatusResults */
static ELockState::Type GetLockState(const FString& InOwner, const FString& InUserName)
{
	if(InOwner.Len() == 0)
	{
		return ELockState::NotLocked;
	}
	else if(InOwner == InUserName)
	{
		return ELockState::Locked;
	}
	else
	{
		return ELockState::LockedOther;
	}
}

FString TranslateAction(const FString& InAction)
{
	static TMap<FString, FString> ActionActionMap;
	if(ActionActionMap.Num() == 0)
	{
		ActionActionMap.Add(TEXT("A"), TEXT("add"));
		ActionActionMap.Add(TEXT("D"), TEXT("delete"));
		ActionActionMap.Add(TEXT("R"), TEXT("replace"));
		ActionActionMap.Add(TEXT("M"), TEXT("edit"));
	}

	return ActionActionMap.FindRef(InAction);
}

FDateTime GetDate(const FString& InDateString)
{
	// Date format output from SVN is e.g. YYYY-MM-DDTHH:MM:SS.msZ
	FString TrimmedDate = InDateString.Replace(TEXT("T"), TEXT(" "));
	TrimmedDate = TrimmedDate.Replace(TEXT("Z"), TEXT(" "));
	TrimmedDate = TrimmedDate.Replace(TEXT("-"), TEXT(" "));
	TrimmedDate = TrimmedDate.Replace(TEXT(":"), TEXT(" "));
	TrimmedDate = TrimmedDate.Replace(TEXT("."), TEXT(" "));

	TArray<FString> Tokens;
	TrimmedDate.ParseIntoArray(Tokens, TEXT(" "), true);

	int32 Year = FMath::Clamp(Tokens.Num() > 0 ? FCString::Atoi(*Tokens[0]) : 0, 0, 9999);
	int32 Month = FMath::Clamp(Tokens.Num() > 1 ? FCString::Atoi(*Tokens[1]) : 0, 1, 12);
	int32 Day = FMath::Clamp(Tokens.Num() > 2 ? FCString::Atoi(*Tokens[2]) : 0, 1, FDateTime::DaysInMonth(Year, Month));
	int32 Hour = FMath::Clamp(Tokens.Num() > 3 ? FCString::Atoi(*Tokens[3]) : 0, 0, 23);
	int32 Minute = FMath::Clamp(Tokens.Num() > 4 ? FCString::Atoi(*Tokens[4]) : 0, 0, 59);
	int32 Second = FMath::Clamp(Tokens.Num() > 5 ? FCString::Atoi(*Tokens[5]) : 0, 0, 59);
	int32 Millisecond = FMath::Clamp(Tokens.Num() > 6 ? (int32)((1.0f / (float)FCString::Atof(*Tokens[6])) * 1000.0f) : 0, 0, 1000);

	return FDateTime( Year, Month, Day, Hour, Minute, Second, Millisecond );
}

/** Helper function for ParseLogResults() - get the repsitory-relative filename of a file in our working copy */
static FString GetRepoName(const FString& InFilename, const FString& UserName)
{
	FString Result;
	TArray<FXmlFile> ResultsXml;
	TArray<FString> ErrorMessages;

	TArray<FString> Files;
	Files.Add(InFilename);

	if(RunCommand(TEXT("info"), Files, TArray<FString>(), ResultsXml, ErrorMessages, UserName))
	{
		static const FString Info(TEXT("info"));
		static const FString Entry(TEXT("entry"));
		static const FString Url(TEXT("url"));
		static const FString Repository(TEXT("repository"));
		static const FString Root(TEXT("root"));

		for(auto ResultIt(ResultsXml.CreateConstIterator()); ResultIt; ResultIt++)
		{
			const FXmlNode* InfoNode = ResultIt->GetRootNode();
			if(InfoNode == NULL || InfoNode->GetTag() != Info)
			{
				continue;
			}

			const FXmlNode* EntryNode = InfoNode->FindChildNode(Entry);
			if(EntryNode == NULL)
			{
				continue;
			}

			const FXmlNode* URLNode = EntryNode->FindChildNode(Url);
			if(URLNode == NULL)
			{
				continue;
			}

			FString URL = URLNode->GetContent();

			const FXmlNode* RepositoryNode = EntryNode->FindChildNode(Repository);
			if(RepositoryNode == NULL)
			{
				continue;
			}

			const FXmlNode* RootNode = RepositoryNode->FindChildNode(Root);
			if(RootNode == NULL)
			{
				continue;
			}

			FString RootStr = RootNode->GetContent();
			const int32 RootLength = RootStr.Len();
			if (URL.Left(RootLength) == RootStr)
			{
				Result = URL.Right(URL.Len() - RootLength);
				break;
			}
		}
	}

	return Result;
}

void ParseLogResults(const FString& InFilename, const TArray<FXmlFile>& ResultsXml, const FString& UserName, FHistoryOutput& OutHistory)
{
	static const FString Log(TEXT("log"));
	static const FString LogEntry(TEXT("logentry"));
	static const FString Revision(TEXT("revision"));
	static const FString Msg(TEXT("msg"));
	static const FString Author(TEXT("author"));
	static const FString Date(TEXT("date"));
	static const FString Paths(TEXT("paths"));
	static const FString Path(TEXT("path"));
	static const FString Kind(TEXT("kind"));
	static const FString File(TEXT("file"));
	static const FString Action(TEXT("action"));
	static const FString CopyFrom_Path(TEXT("copyfrom-path"));
	static const FString CopyFrom_Rev(TEXT("copyfrom-rev"));

	for(auto ResultIt(ResultsXml.CreateConstIterator()); ResultIt; ResultIt++)
	{
		const FXmlNode* LogNode = ResultIt->GetRootNode();
		if(LogNode != NULL && LogNode->GetTag() == Log)
		{
			TArray< TSharedRef<FSubversionSourceControlRevision, ESPMode::ThreadSafe> > Revisions;

			const TArray<FXmlNode*> LogChildren = LogNode->GetChildrenNodes();
			for(auto LogEntryIter(LogChildren.CreateConstIterator()); LogEntryIter; LogEntryIter++)
			{
				const FXmlNode* LogEntryNode = *LogEntryIter;
				if(LogEntryNode == NULL || LogEntryNode->GetTag() != LogEntry)
				{
					continue;
				}

				TSharedRef<FSubversionSourceControlRevision, ESPMode::ThreadSafe> SourceControlRevision = MakeShareable(new FSubversionSourceControlRevision);
				Revisions.Add(SourceControlRevision);

				SourceControlRevision->Filename = InFilename;
				SourceControlRevision->Revision = LogEntryNode->GetAttribute(Revision);
				SourceControlRevision->RevisionNumber = FCString::Atoi(*SourceControlRevision->Revision);
			
				const FXmlNode* MsgNode = LogEntryNode->FindChildNode(Msg);
				if(MsgNode != NULL)
				{
					SourceControlRevision->Description = MsgNode->GetContent();
				}

				const FXmlNode* AuthorNode = LogEntryNode->FindChildNode(Author);
				if(AuthorNode != NULL)
				{
					SourceControlRevision->UserName = AuthorNode->GetContent();
				}

				const FXmlNode* DateNode = LogEntryNode->FindChildNode(Date);
				if(DateNode != NULL)
				{
					SourceControlRevision->Date = GetDate(DateNode->GetContent());
				}

				// find the repository filename of this file
				FString RepoName = GetRepoName(InFilename, UserName);

				// to find the operation that was performed on this file, we need to look at the paths in this log entry
				const FXmlNode* PathsNode = LogEntryNode->FindChildNode(Paths);
				if(PathsNode != NULL)
				{
					const TArray<FXmlNode*> PathsChildren = PathsNode->GetChildrenNodes();
					for(auto PathIter(PathsChildren.CreateConstIterator()); PathIter; PathIter++)
					{
						const FXmlNode* PathNode = *PathIter;
						if(PathNode == NULL || PathNode->GetTag() != Path)
						{
							continue;
						}

						if(PathNode->GetAttribute(Kind) == File)
						{
							// check if this path matches our file
							SourceControlRevision->RepoFilename = PathNode->GetContent();
							if(SourceControlRevision->RepoFilename == RepoName)
							{
								SourceControlRevision->Action = TranslateAction(PathNode->GetAttribute(Action));

								const FString CopyFromPath = PathNode->GetAttribute(CopyFrom_Path);
								const FString CopyFromRev = PathNode->GetAttribute(CopyFrom_Rev);
								if(CopyFromPath.Len() > 0 && CopyFromRev.Len() > 0)
								{
									TSharedRef<FSubversionSourceControlRevision, ESPMode::ThreadSafe> CopyFromRevision = MakeShareable(new FSubversionSourceControlRevision);
									CopyFromRevision->Filename = CopyFromPath;
									CopyFromRevision->RevisionNumber = FCString::Atoi(*CopyFromRev);

									SourceControlRevision->BranchSource = CopyFromRevision;
								}

								break;
							}
						}
					}
				}
			}

			if(Revisions.Num() > 0)
			{
				OutHistory.Add(InFilename, Revisions);
			}
		}
	}
}

void ParseInfoResults(const TArray<FXmlFile>& ResultsXml, FString& OutWorkingCopyRoot, FString& OutRepoRoot)
{
	static const FString Info(TEXT("info"));
	static const FString Entry(TEXT("entry"));
	static const FString Wc_Info(TEXT("wc-info"));
	static const FString WcRoot_AbsPath(TEXT("wcroot-abspath"));
	static const FString Repository(TEXT("repository"));
	static const FString Root(TEXT("root"));

	for(auto ResultIt(ResultsXml.CreateConstIterator()); ResultIt; ResultIt++)
	{
		const FXmlNode* InfoNode = ResultIt->GetRootNode();
		if(InfoNode == NULL || InfoNode->GetTag() != Info)
		{
			continue;
		}

		const FXmlNode* EntryNode = InfoNode->FindChildNode(Entry);
		if(EntryNode == NULL)
		{
			continue;
		}

		const FXmlNode* RepositoryNode = EntryNode->FindChildNode(Repository);
		if(RepositoryNode == NULL)
		{
			continue;
		}

		const FXmlNode* RootNode = RepositoryNode->FindChildNode(Root);
		if(RootNode == NULL)
		{
			continue;
		}

		OutRepoRoot = RootNode->GetContent();

		const FXmlNode* WcInfoNode = EntryNode->FindChildNode(Wc_Info);
		if(WcInfoNode == NULL)
		{
			continue;
		}

		const FXmlNode* WcRootAbsPathNode = WcInfoNode->FindChildNode(WcRoot_AbsPath);
		if(WcRootAbsPathNode == NULL)
		{
			continue;
		}

		FString WcRootAbsPath = WcRootAbsPathNode->GetContent();
		FPaths::NormalizeDirectoryName(WcRootAbsPath);
		OutWorkingCopyRoot = WcRootAbsPath;
		break;
	}
}

void ParseStatusResults(const TArray<FXmlFile>& ResultsXml, const TArray<FString>& InErrorMessages, const FString& InUserName, const FString& InWorkingCopyRoot, TArray<FSubversionSourceControlState>& OutStates)
{
	static const FString Status(TEXT("status"));
	static const FString Target(TEXT("target"));
	static const FString Changelist(TEXT("changelist"));
	static const FString Entry(TEXT("entry"));
	static const FString Path(TEXT("path"));
	static const FString Wc_Status(TEXT("wc-status"));
	static const FString Item(TEXT("item"));
	static const FString Lock(TEXT("lock"));
	static const FString Owner(TEXT("owner"));
	static const FString Repos_Status(TEXT("repos-status"));
	static const FString None(TEXT("none"));
	static const FString Copied(TEXT("copied"));

	for(auto ResultIt(ResultsXml.CreateConstIterator()); ResultIt; ResultIt++)
	{
		const FXmlNode* StatusNode = ResultIt->GetRootNode();
		if(StatusNode != NULL && StatusNode->GetTag() == Status)
		{
			const TArray<FXmlNode*> StatusChildren = StatusNode->GetChildrenNodes();
			for(auto TargetIter(StatusChildren.CreateConstIterator()); TargetIter; TargetIter++)
			{
				FXmlNode* TargetNode = *TargetIter;
				if (TargetNode == NULL ||
					!(TargetNode->GetTag() == Target || TargetNode->GetTag() == Changelist))
				{
					continue;
				}

				const TArray<FXmlNode*> TargetChildren = TargetNode->GetChildrenNodes();
				for(auto EntryIter(TargetChildren.CreateConstIterator()); EntryIter; EntryIter++)
				{
					FXmlNode* EntryNode = *EntryIter;
					if(EntryNode == NULL || EntryNode->GetTag() != Entry)
					{
						continue;
					}

					FString PathAttrib = EntryNode->GetAttribute(Path);
					if(PathAttrib.Len() == 0)
					{
						continue;
					}

					// found a valid entry - fixup the filename & create a new state
					PathAttrib = FPaths::ConvertRelativePathToFull(PathAttrib);
					FPaths::NormalizeFilename(PathAttrib);
					FSubversionSourceControlState State(PathAttrib);

					// assume we are not locked for now
					State.LockState = ELockState::NotLocked;

					const FXmlNode* WcStatusNode = EntryNode->FindChildNode(Wc_Status);
					if(WcStatusNode != NULL)
					{
						if(PathAttrib.StartsWith(InWorkingCopyRoot))
						{
							State.WorkingCopyState = GetWorkingCopyState(WcStatusNode->GetAttribute(Item));
							if(State.WorkingCopyState == EWorkingCopyState::Added)
							{
								const FString CopiedAttrib = WcStatusNode->GetAttribute(Copied);
								if(CopiedAttrib == TEXT("true"))
								{
									State.bCopied = true;
								}
							}
							else if( State.WorkingCopyState == EWorkingCopyState::Conflicted )
							{
								// As far as I can tell this is the "correct" way of finding out what revisions are in conflict.
								// Bunny ears around correct because we're dirstatting and parsing filenames, which can obviously
								// result in undesirable behavior:
								TArray<FString> Filenames;
								// Looking for two files that end in .r####, # of digits is unbounded:
								FString WildCard = PathAttrib + ".r*";
								IFileManager::Get().FindFiles( Filenames, *WildCard, true /*=Files*/, false /*=Directories*/ );
								if( Filenames.Num() == 2 )
								{
									int MergeBaseFileRevNumber;
									{
										// This is just a guess, we'll swap filenames if it turns out that Filenames[0] is actually
										// the conflicting file:
										FString PendingMergeBaseFile = Filenames[0];
										FString PendingMergeConflictingFile = Filenames[1];

										// Helper function to find the filename with the lower .r###:
										const auto EndingToInt = [](const FString& FileName)
										{
											int32 Idx = -1;
											const bool found = FileName.FindLastChar('r', Idx);
											check(found); // regex failed, Filenames should only contain files that end in .r*
											int32 RetVal = -1;
											TTypeFromString<int>::FromString(RetVal, &FileName[Idx + 1]);
											return RetVal;
										};
										int FirstFile = EndingToInt(PendingMergeBaseFile);
										int SecondFile = EndingToInt(PendingMergeConflictingFile);
										check(FirstFile != SecondFile);
										if (FirstFile > SecondFile)
										{
											// Guessed wrong, swap our decision!
											Swap(PendingMergeBaseFile, PendingMergeConflictingFile);
											Swap(FirstFile, SecondFile);
										}

										MergeBaseFileRevNumber = FirstFile;
									}

									// Save the result, this information can be used to perform a merge operation:
									State.PendingMergeBaseFileRevNumber = MergeBaseFileRevNumber;

									// For the file into a 'locked' state, since it's in conflict, if we don't do
									// this we can't perform a merge because of logic in the asset tools module:
									State.LockState = ELockState::Locked;
								}
							}
						}
						else
						{
							State.WorkingCopyState = EWorkingCopyState::NotAWorkingCopy;
						}

						// find the lock state (if any)
						const FXmlNode* LockNode = WcStatusNode->FindChildNode(Lock);
						if(LockNode != NULL)
						{
							const FXmlNode* OwnerNode = LockNode->FindChildNode(Owner);
							if(OwnerNode != NULL)
							{
								State.LockUser = OwnerNode->GetContent();
								State.LockState = GetLockState(State.LockUser, InUserName);	
							}
						}
					}
					
					// check for lock state.
					const FXmlNode* RepoStatusNode = EntryNode->FindChildNode(Repos_Status);
					if(RepoStatusNode != NULL)
					{
						// find the lock state
						const FXmlNode* LockNode = RepoStatusNode->FindChildNode(Lock);
						if(LockNode != NULL)
						{
							const FXmlNode* OwnerNode = LockNode->FindChildNode(Owner);
							if(OwnerNode != NULL)
							{
								State.LockUser = OwnerNode->GetContent();
								State.LockState = GetLockState(State.LockUser, InUserName);	
							}
						}

						State.bNewerVersionOnServer = (RepoStatusNode->GetAttribute(Item) != None);
					}	

					OutStates.Add(State);
				}
			}
		}
	}

	// also see if we can glean anything from the error messages
	for (int32 Index = 0; Index < InErrorMessages.Num(); ++Index)
	{
		const FString& Error = InErrorMessages[Index];
		int32 TruncatePos = Error.Find(TEXT("' is not a working copy"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if(TruncatePos != INDEX_NONE)
		{
			// found an error about a file that is not in the working copy
			FString LeftString(Error.Left(TruncatePos));
			int32 QuotePos;
			if(LeftString.FindChar(TEXT('\''), QuotePos))
			{
				FString Filename(LeftString.Right(LeftString.Len() - (QuotePos + 1)));
				Filename = FPaths::ConvertRelativePathToFull(Filename);
				FPaths::NormalizeFilename(Filename);
				OutStates.Add(FSubversionSourceControlState(Filename));
				FSubversionSourceControlState& State = OutStates.Last();

				if(Filename.StartsWith(InWorkingCopyRoot))
				{
					State.WorkingCopyState = EWorkingCopyState::NotControlled;
				}
				else
				{
					State.WorkingCopyState = EWorkingCopyState::NotAWorkingCopy;
				}
			}
		}
	}
}

bool UpdateCachedStates(const TArray<FSubversionSourceControlState>& InStates)
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

	for(int StatusIndex = 0; StatusIndex < InStates.Num(); StatusIndex++)
	{
		const FSubversionSourceControlState& InState = InStates[StatusIndex];
		TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(InState.LocalFilename);
		auto History = MoveTemp(State->History);
		*State = InState;
		State->TimeStamp = FDateTime::Now();
		State->History = MoveTemp(History);
	}

	return InStates.Num() > 0;
}

bool CheckFilename(const FString& InString)
{
	if( InString.Contains(TEXT("...")) ||
		InString.Contains(TEXT("*")) ||
		InString.Contains(TEXT("?")))
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Filename '%s' with wildcards is not supported by Subversion"), *InString);
		return false;
	}

	return true;
}

bool CheckFilenames(const TArray<FString>& InStrings)
{
	bool bResult = true;

	for(auto Iter(InStrings.CreateConstIterator()); Iter; Iter++)
	{
		bResult &= CheckFilename(*Iter);
	}

	return bResult;
}


/**
 * Helper struct for RemoveRedundantErrors()
 */
struct FRemoveRedundantErrors
{
	FRemoveRedundantErrors(const FString& InFilter)
		: Filter(InFilter)
	{
	}

	bool operator()(const FString& String) const
	{
		if(String.Contains(Filter))
		{
			return true;
		}

		return false;
	}

	/** The filter string we try to identify in the reported error */
	FString Filter;
};

void RemoveRedundantErrors(FSubversionSourceControlCommand& InCommand, const FString& InFilter)
{
	bool bFoundRedundantError = false;
	for(auto Iter(InCommand.ErrorMessages.CreateConstIterator()); Iter; Iter++)
	{
		// Perforce reports files that are already synced as errors, so copy any errors
		// we get to the info list in this case
		if(Iter->Contains(InFilter))
		{
			InCommand.InfoMessages.Add(*Iter);
			bFoundRedundantError = true;
		}
	}

	InCommand.ErrorMessages.RemoveAll( FRemoveRedundantErrors(InFilter) );

	// if we have no error messages now, assume success!
	if(bFoundRedundantError && InCommand.ErrorMessages.Num() == 0 && !InCommand.bCommandSuccessful)
	{
		InCommand.bCommandSuccessful = true;
	}
}

void QuoteFilename(FString& InString)
{
	const FString Quote(TEXT("\""));
	InString = Quote + InString + Quote;
}

void QuoteFilenames(TArray<FString>& InStrings)
{
	for(auto Iter(InStrings.CreateIterator()); Iter; Iter++)
	{
		QuoteFilename(*Iter);
	}
}

}
