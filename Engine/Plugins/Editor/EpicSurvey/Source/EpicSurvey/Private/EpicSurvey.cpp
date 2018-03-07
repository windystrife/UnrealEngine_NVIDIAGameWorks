// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EpicSurvey.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "EpicSurveyCommands.h"
#include "Survey.h"
#include "SSurvey.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "EngineAnalytics.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "SurveyTitleCdnStorage.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "SSurveyNotification.h"
#include "SurveyTitleLocalStorage.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor.h"

#include "Settings/EditorSettings.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

DEFINE_LOG_CATEGORY(LogEpicSurvey);

const FString FEpicSurvey::SurveyIndexFilename( TEXT("survey_index.json") );

TSharedRef< FEpicSurvey > FEpicSurvey::Create()
{
	TSharedRef< FUICommandList > ActionList = MakeShareable( new FUICommandList() );

	TSharedRef< FEpicSurvey > EpicSurvey = MakeShareable( new FEpicSurvey( ActionList ) );
	EpicSurvey->Initialize();
	return EpicSurvey;
}

FEpicSurvey::FEpicSurvey( const TSharedRef< FUICommandList >& InActionList ) 
	: InitializationState( EContentInitializationState::NotStarted )
	, ActionList( InActionList )
	, SurveyNotificationDelayTime(0)
	, SurveyNotificationDuration(5.0f)
	, SurveyPulseTimeInterval(5)
	, SurveyPulseDuration(5)
	, bSurveyIconPulsing(false)
	, bIsShowingToolbarNotification( false )
	, CurrentCulture(ECultureSpecification::Full)
{
	
}

FEpicSurvey::~FEpicSurvey() 
{
	if ( TitleCloud.IsValid() )
	{
		FEpicSurveyCommands::Unregister();

		if ( FModuleManager::Get().IsModuleLoaded("LevelEditor") )
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetNotificationBarExtensibilityManager()->RemoveExtender( NotificationBarExtender );
		}

		if ( FModuleManager::Get().IsModuleLoaded("MainFrame") )
		{
			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			MainFrameModule.OnMainFrameCreationFinished().RemoveAll( this );
		}
	}
}

TSharedPtr< FSlateDynamicImageBrush > FEpicSurvey::LoadRawDataAsBrush( FName ResourceName, const TArray< uint8 >& RawData ) const
{
	TSharedPtr< FSlateDynamicImageBrush > Brush;

	uint32 BytesPerPixel = 4;
	int32 Width = 0;
	int32 Height = 0;

	bool bSucceeded = false;
	TArray<uint8> DecodedImage;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
	if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawData.GetData(), RawData.Num() ) )
	{
		Width = ImageWrapper->GetWidth();
		Height = ImageWrapper->GetHeight();

		const TArray<uint8>* RawImageData = NULL;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawImageData))
		{
			DecodedImage.AddUninitialized( Width * Height * BytesPerPixel );
			DecodedImage = *RawImageData;
			bSucceeded = true;
		}
	}

	if ( bSucceeded )
	{
		Brush = FSlateDynamicImageBrush::CreateWithImageData( ResourceName, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight()), DecodedImage );
	}

	return Brush;
}

void FEpicSurvey::Initialize()
{
	//IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(MCP_SUBSYSTEM);
	//if (OnlineSub)
	//{
	//	TitleCloud = OnlineSub->GetTitleFileInterface();
	//}

	if ( !IsRunningCommandlet() )
	{
		if ( GConfig->GetInt(TEXT("EpicSurvey"),TEXT("NotificationDelayTime"), SurveyNotificationDelayTime, GEditorIni) )
		{
			// if the Notification is 0, then it will be displayed during SetActiveSurvey
			if( SurveyNotificationDelayTime > 0 )
			{
				NotificationDelegate.BindRaw( this, &FEpicSurvey::DisplayNotification );

				GEditor->GetTimerManager()->SetTimer( DisplayNotificationTimerHandle, NotificationDelegate, SurveyNotificationDelayTime, false );
			}
		}

		GConfig->GetInt(TEXT("EpicSurvey"),TEXT("PulseDuration"), SurveyPulseDuration, GEditorIni);

		if ( GConfig->GetInt(TEXT("EpicSurvey"),TEXT("PulseTimeInterval"), SurveyPulseTimeInterval, GEditorIni) )
		{
			StartPulseSurveyIconDelegate.BindRaw( this, &FEpicSurvey::StartPulseSurveyIcon );
			EndPulseSurveyIconDelegate.BindRaw( this, &FEpicSurvey::EndPulseSurveyIcon );
		}

		GConfig->GetFloat(TEXT("EpicSurvey"),TEXT("NotificationDuration"), SurveyNotificationDuration, GEditorIni);

		InitializeTitleCloud();
	}

	if ( TitleCloud.IsValid() )
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));

		if ( MainFrameModule.IsWindowInitialized() )
		{
			RootWindow = MainFrameModule.GetParentWindow();
		}
		else
		{
			MainFrameModule.OnMainFrameCreationFinished().AddSP( this, &FEpicSurvey::HandledMainFrameLoad );
		}

		FEpicSurveyCommands::Register();
		const FEpicSurveyCommands& Commands = FEpicSurveyCommands::Get();
		ActionList->MapAction( Commands.OpenEpicSurvey, FExecuteAction::CreateSP( this, &FEpicSurvey::OpenEpicSurveyWindow ) );
	}
}

void FEpicSurvey::InitializeTitleCloud()
{
	FCultureRef Culture = FInternationalization::Get().GetCurrentCulture();

	FString CultureString;
	switch (CurrentCulture)
	{
		case ECultureSpecification::Full:			CultureString = Culture->GetName();						break;
		case ECultureSpecification::LanguageOnly:	CultureString = Culture->GetTwoLetterISOLanguageName();	break;

		default: break;
	};

	FString SourceType;
	if ( GConfig->GetString(TEXT("EpicSurvey"),TEXT("Source"), SourceType, GEditorIni) && !SourceType.IsEmpty() )
	{
		if ( SourceType == TEXT("CDN") )
		{
			FString CdnUrl;
			if ( GConfig->GetString(TEXT("EpicSurvey"),TEXT("CdnUrl"), CdnUrl, GEditorIni) && !CdnUrl.IsEmpty() )
			{
				if ( !CultureString.IsEmpty() )
				{
					CdnUrl = CdnUrl.Replace( TEXT("{Culture}"), *CultureString );
				}
				else
				{
					// Remove the culture part of the path completely
					CdnUrl = CdnUrl.Replace( TEXT("{Culture}/"), TEXT("") );
				}
				TitleCloud = FSurveyTitleCdnStorage::Create( CdnUrl );
			}
		}
		else if ( SourceType == TEXT("LOCAL") )
		{
			FString EngineContentRelativeDirectory;
			if ( GConfig->GetString(TEXT("EpicSurvey"),TEXT("EngineContentRelativeDirectory"), EngineContentRelativeDirectory, GEditorIni) && !EngineContentRelativeDirectory.IsEmpty() )
			{
				FString RootDirectory = FPaths::Combine( FPlatformProcess::BaseDir(), *FPaths::EngineContentDir(), *EngineContentRelativeDirectory );
				if ( !CultureString.IsEmpty() )
				{
					RootDirectory = RootDirectory.Replace( TEXT("{Culture}"), *CultureString );
				}
				else
				{
					// Remove the culture part of the path completely
					RootDirectory = RootDirectory.Replace( TEXT("{Culture}/"), TEXT("") );
				}

				TitleCloud = FSurveyTitleLocalStorage::Create( RootDirectory );
			}
		}
	}

	if (TitleCloud.IsValid())
	{
		TitleCloud->AddOnEnumerateFilesCompleteDelegate_Handle(FOnEnumerateFilesCompleteDelegate::CreateSP(this, &FEpicSurvey::OnEnumerateFilesComplete));
		TitleCloud->AddOnReadFileCompleteDelegate_Handle(FOnReadFileCompleteDelegate::CreateSP(this, &FEpicSurvey::OnReadFileComplete));
		LoadSurveys();
	}
}

void FEpicSurvey::LoadSurveys()
{
	Surveys.Empty();
	SurveyIndexCloudFile = FCloudFileHeader();
	FilenameToLoadCallbacks.Empty();

	InitializationState = EContentInitializationState::Working;

	if ( TitleCloud.IsValid() )
	{
		TitleCloud->ClearFiles();
		TitleCloud->EnumerateFiles();
	}
	else
	{
		InitializationState = EContentInitializationState::Failure;
	}
}

void FEpicSurvey::OnEnumerateFilesComplete( bool bSuccess, const FString& ErrorString )
{
	if ( !bSuccess )
	{
		if (CurrentCulture != ECultureSpecification::None)
		{
			// Move on to the next culture specification and try again
			CurrentCulture = static_cast<ECultureSpecification>(int(CurrentCulture) + 1);
			InitializeTitleCloud();
		}
	}
	else
	{
		TArray< FCloudFileHeader > FileHeaders;
		TitleCloud->GetFileList( FileHeaders );

		for (int Index = 0; Index < FileHeaders.Num(); Index++)
		{
			if ( FileHeaders[ Index ].FileName == SurveyIndexFilename )
			{
				SurveyIndexCloudFile = FileHeaders[ Index ];
				TitleCloud->ReadFile( SurveyIndexCloudFile.DLName );
				break;
			}
		}
	}
}

void FEpicSurvey::OnReadFileComplete( bool bSuccess, const FString& DLName )
{
	if ( bSuccess )
	{
		if ( DLName == SurveyIndexCloudFile.DLName )
		{
			LoadSurveyIndexFile();
		}
		else
		{
			int32 FileHeaderIndex = INDEX_NONE;
			TArray< FCloudFileHeader > FileHeaders;
			TitleCloud->GetFileList( FileHeaders );

			for (int Index = 0; Index < FileHeaders.Num(); Index++)
			{
				if ( FileHeaders[ Index ].DLName == DLName )
				{
					FileHeaderIndex = Index;
					break;
				}
			}

			if ( FileHeaderIndex > INDEX_NONE )
			{
				const FCloudFileHeader FileHeader = FileHeaders[ FileHeaderIndex ];
				const FString FileExtension = FPaths::GetExtension( FileHeader.FileName );
				
				if ( FileExtension == TEXT("json") )
				{
					TArray< uint8 > FileContents;
					TitleCloud->GetFileContents( DLName, FileContents );

					FString SurveyJson;
					FFileHelper::BufferToString( SurveyJson, FileContents.GetData(), FileContents.Num() );

					TSharedPtr< FJsonObject > SurveyObject = NULL;
					TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<TCHAR>::Create( SurveyJson );
					if ( FJsonSerializer::Deserialize( Reader, SurveyObject ) )
					{
						TSharedPtr< FSurvey > NewSurvey = FSurvey::Create( SharedThis( this ), SurveyObject.ToSharedRef() );
						if ( NewSurvey.IsValid() )
						{
							switch( NewSurvey->GetSurveyType() )
							{
							case ESurveyType::Normal:
								{
									auto* Settings = GetDefault<UEditorSettings>();

									Surveys.Add( NewSurvey.ToSharedRef() );
									const bool IsActiveSurveyInProgress = ActiveSurvey.IsValid() ? Settings->InProgressSurveys.Contains( ActiveSurvey->GetIdentifier() ) : false;

									if ( !IsActiveSurveyInProgress )
									{
										const bool HasBeenCompleted = Settings->CompletedSurveys.Contains( NewSurvey->GetIdentifier() );

										if ( !HasBeenCompleted )
										{
											const bool IsInProgress = Settings->InProgressSurveys.Contains( NewSurvey->GetIdentifier() );

											if ( NewSurvey->CanAutoPrompt() )
											{
												SetActiveSurvey( NewSurvey );
											}
											else if ( IsInProgress )
											{
												SetActiveSurvey( NewSurvey );
											}
										}
									}
								}
								break;
							case ESurveyType::Branch:
								BranchSurveys.Add( FileHeader.FileName, NewSurvey );
								break;
							}
						}
					}
					else
					{
						UE_LOG(LogEpicSurvey, Verbose, TEXT("Parsing JSON survey failed. Filename: %s Message: %s"), *FileHeader.FileName, *Reader->GetErrorMessage());
					}
				}
				else if ( FileExtension == TEXT("png") )
				{
					TArray< FOnBrushLoaded > MapResults;
					FilenameToLoadCallbacks.MultiFind( FileHeaders[ FileHeaderIndex ].FileName, MapResults );

					if ( MapResults.Num() > 0 )
					{
						TArray< uint8 > FileContents;
						TitleCloud->GetFileContents( DLName, FileContents );

						for (int Index = 0; Index < MapResults.Num(); Index++)
						{
							MapResults[ Index ].Execute( LoadRawDataAsBrush( *(FString(TEXT("EpicSurvey.")) + FileHeaders[ FileHeaderIndex ].FileName), FileContents ) );
						}

						FilenameToLoadCallbacks.Remove( FileHeaders[ FileHeaderIndex ].FileName );
					}
				}
			}
		}
	}
	else
	{
		InitializationState = EContentInitializationState::Failure;
	}
}

void FEpicSurvey::DisplayToolbarNotification()
{
	if( ActiveSurvey.IsValid() && !bIsShowingToolbarNotification )
	{
		NotificationBarExtender = MakeShareable( new FExtender() );
		NotificationBarExtender->AddToolBarExtension(
			"Start", 
			EExtensionHook::After, 
			ActionList, 
			FToolBarExtensionDelegate::CreateSP(this, &FEpicSurvey::AddEpicSurveyNotifier));

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetNotificationBarExtensibilityManager()->AddExtender( NotificationBarExtender );
		bIsShowingToolbarNotification = true;

		LevelEditorModule.BroadcastNotificationBarChanged();

		if( StartPulseSurveyIconDelegate.IsBound() && EndPulseSurveyIconDelegate.IsBound() )
		{
			StartPulseSurveyIcon();
		}
	}
}

void FEpicSurvey::DisplayNotification()
{
	if( ActiveSurvey.IsValid() )
	{
		FNotificationInfo Info( NSLOCTEXT("EpicSurvey",  "DisplayNotification", "There is a new survey avaliable!") );
		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = false;
		Info.FadeOutDuration = 0.3f;
		if( SurveyNotificationDuration > 0.0f )
		{
			Info.ExpireDuration = SurveyNotificationDuration;
		}
		Info.ButtonDetails.Add( FNotificationButtonInfo( NSLOCTEXT("EpicSurvey", "DisplayNotificationButtonAccept", "Take Survey"), FText::GetEmpty(), FSimpleDelegate::CreateSP( this, &FEpicSurvey::AcceptSurveyNotification ) ) );
		Info.ButtonDetails.Add( FNotificationButtonInfo( NSLOCTEXT("EpicSurvey", "DisplayNotificationButtonCancel", "Not Now"), FText::GetEmpty(), FSimpleDelegate::CreateSP( this, &FEpicSurvey::CancelSurveyNotification ) ) );

		if ( DisplaySurveyNotification.IsValid() )
		{
			DisplaySurveyNotification.Pin()->ExpireAndFadeout();
			DisplaySurveyNotification.Reset();
		}

		DisplaySurveyNotification = FSlateNotificationManager::Get().AddNotification(Info);
		DisplaySurveyNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

		DisplayToolbarNotification();
	}
}

void FEpicSurvey::AcceptSurveyNotification()
{
	if ( DisplaySurveyNotification.IsValid() )
	{
		OpenEpicSurveyWindow();

		DisplaySurveyNotification.Pin()->SetCompletionState(SNotificationItem::CS_Success);
		DisplaySurveyNotification.Pin()->Fadeout();
		DisplaySurveyNotification.Reset();
	}
}

void FEpicSurvey::CancelSurveyNotification()
{
	DisplayToolbarNotification();

	if ( DisplaySurveyNotification.IsValid() )
	{
		DisplaySurveyNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
		DisplaySurveyNotification.Pin()->Fadeout();
		DisplaySurveyNotification.Reset();
	}
}

void FEpicSurvey::StartPulseSurveyIcon()
{
	FTimerHandle DummyHandle;
	GEditor->GetTimerManager()->SetTimer( DummyHandle, EndPulseSurveyIconDelegate, SurveyPulseDuration, false );

	bSurveyIconPulsing = true;
}

void FEpicSurvey::EndPulseSurveyIcon()
{
	bSurveyIconPulsing = false;

	FTimerHandle DummyHandle;
	GEditor->GetTimerManager()->SetTimer( DummyHandle, StartPulseSurveyIconDelegate, SurveyPulseTimeInterval, false );
}

bool FEpicSurvey::PromptSurvey( const FGuid& SurveyIdentifier )
{
	for (int Index = 0; Index < Surveys.Num(); Index++)
	{
		const TSharedRef< FSurvey > Survey = Surveys[ Index ];

		if ( Survey->GetIdentifier() == SurveyIdentifier )
		{
			SetActiveSurvey( Survey, false );
			return true;
		}
	}

	return false;
}

void FEpicSurvey::SetActiveSurvey( const TSharedPtr< FSurvey >& Survey, bool AutoPrompted )
{
	ActiveSurvey = NULL;
	bool HasBeenCompleted = true;

	if ( Survey.IsValid() )
	{
		HasBeenCompleted = GetDefault<UEditorSettings>()->CompletedSurveys.Contains( Survey->GetIdentifier() );

		if ( !HasBeenCompleted )
		{
			ActiveSurvey = Survey;

			if ( ActiveSurvey.IsValid() && ( !NotificationDelegate.IsBound() || !AutoPrompted ) )
			{
				DisplayNotification();
			}
		}
	}

	if ( HasBeenCompleted && bIsShowingToolbarNotification )
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetNotificationBarExtensibilityManager()->RemoveExtender( NotificationBarExtender );
		bIsShowingToolbarNotification = false;

		LevelEditorModule.BroadcastNotificationBarChanged();
	}
}

void FEpicSurvey::SubmitSurvey( const TSharedRef< FSurvey >& Survey )
{
	auto* Settings = GetMutableDefault<UEditorSettings>();
	if ( Settings->CompletedSurveys.Contains( Survey->GetIdentifier() ) )
	{
		return;
	}

	Survey->Submit();

	Settings->CompletedSurveys.Add( Survey->GetIdentifier() );
	Settings->InProgressSurveys.Remove( Survey->GetIdentifier() );
	Settings->PostEditChange();

	SetActiveSurvey( NULL );

	SurveyWindow.Pin()->RequestDestroyWindow();
}

void FEpicSurvey::AddBranch( const FString& BranchName )
{
	if( !BranchPoints.Contains(BranchName) )
	{
		BranchPoints.Add( BranchName, 0 );
	}
}

void FEpicSurvey::AddToBranchPoints( const FString& BranchName, const int32 InPoints )
{
	int32* Points = BranchPoints.Find( BranchName );
	if( Points )
	{
		(*Points) += InPoints;
	}
	ActiveSurvey->EvaluateBranches();
}

const int32 FEpicSurvey::GetBranchPoints( const FString& BranchName ) const
{
	const int32* Points = BranchPoints.Find( BranchName );
	if( Points )
	{
		return *Points;
	}
	return 0;
}

TSharedPtr< FSurvey > FEpicSurvey::GetBranchSurvey( const FString& Filename )
{
	TSharedPtr< FSurvey >* Survey = BranchSurveys.Find(Filename);
	if( Survey == NULL )
	{
		TArray<FCloudFileHeader> Files;
		TitleCloud->GetFileList(Files);
		for( auto FileIt=Files.CreateIterator(); FileIt; ++FileIt )
		{
			if( FileIt->FileName == Filename )
			{
				if( TitleCloud->ReadFile(FileIt->DLName) )
				{
					Survey = BranchSurveys.Find(Filename);
					if( Survey )
					{
						return *Survey;
					}
				}
				return TSharedPtr< FSurvey >();
			}
		}
	}
	return Survey ? *Survey : nullptr;
}

void FEpicSurvey::LoadSurveyIndexFile()
{
	TArray< FCloudFileHeader > FileHeaders;
	TitleCloud->GetFileList( FileHeaders );

	TArray<uint8> FileContents;
	TitleCloud->GetFileContents( SurveyIndexCloudFile.DLName, FileContents );

	FString SurveyIndexJson;
	FFileHelper::BufferToString( SurveyIndexJson, FileContents.GetData(), FileContents.Num() );

	TSharedPtr< FJsonObject > SurveyIndexObject = NULL;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<TCHAR>::Create( SurveyIndexJson );
	if ( FJsonSerializer::Deserialize( Reader, SurveyIndexObject ) )
	{
		TArray< TSharedPtr< FJsonValue > > PathsToSurveys = SurveyIndexObject->GetArrayField( TEXT("surveys") );
		for (int Index = 0; Index < PathsToSurveys.Num(); Index++)
		{
			const FString SurveyFileName = PathsToSurveys[Index]->AsString();

			for (int HeaderIndex = 0; HeaderIndex < FileHeaders.Num(); HeaderIndex++)
			{
				if ( FileHeaders[ HeaderIndex ].FileName == SurveyFileName )
				{
					TitleCloud->ReadFile( FileHeaders[ HeaderIndex ].DLName );
				}
			}
		}
	}
	else
	{
		InitializationState = EContentInitializationState::Failure;
	}
}

void FEpicSurvey::LoadCloudFileAsBrush( const FString& Filename, FOnBrushLoaded Callback )
{
	TArray< FCloudFileHeader > FileHeaders;
	TitleCloud->GetFileList( FileHeaders );

	for (int Index = 0; Index < FileHeaders.Num(); Index++)
	{
		if ( FileHeaders[ Index ].FileName == Filename )
		{
			if ( !FilenameToLoadCallbacks.Contains( Filename ) )
			{
				FilenameToLoadCallbacks.Add( Filename, Callback );
				if ( !TitleCloud->ReadFile( FileHeaders[ Index ].DLName ) )
				{
					FilenameToLoadCallbacks.Remove( Filename );
					Callback.Execute( NULL );
				}
			}
			else
			{
				FilenameToLoadCallbacks.Add( Filename, Callback );
			}
			break;
		}
	}
}

void FEpicSurvey::AddEpicContentMenus( FMenuBuilder& MenuBuilder )
{
	const FEpicSurveyCommands& Commands = FEpicSurveyCommands::Get();
	MenuBuilder.AddMenuEntry( Commands.OpenEpicSurvey, "Open Epic Survey" );
}

void FEpicSurvey::AddEpicSurveyNotifier( FToolBarBuilder& ToolBarBuilder )
{
	if ( ActiveSurvey.IsValid() )
	{
		ToolBarBuilder.AddWidget( SNew( SSurveyNotification, SharedThis(this), ActiveSurvey.ToSharedRef() ) );
	}
}

void FEpicSurvey::HandledMainFrameLoad( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow )
{
	RootWindow = InRootWindow;
}

void FEpicSurvey::OpenEpicSurveyWindow()
{
	if ( SurveyWindow.IsValid() )
	{
		SurveyWindow.Pin()->BringToFront();
	}
	else
	{
		auto Window = SNew(SWindow)
			.Title(LOCTEXT( "WindowTitle", "Epic Survey" ))
			.ClientSize(FVector2D(1000.0f, 600.0f))
			.SupportsMaximize(true)
			.SupportsMinimize(false);

		Window->SetOnWindowClosed( FOnWindowClosed::CreateRaw(this, &FEpicSurvey::OnEpicSurveyWindowClosed ) );

		if ( ActiveSurvey->GetInitializationState() == EContentInitializationState::NotStarted )
		{
			ActiveSurvey->Initialize();

			auto* Settings = GetMutableDefault<UEditorSettings>();
			if ( !Settings->InProgressSurveys.Contains( ActiveSurvey->GetIdentifier() ) )
			{
				Settings->InProgressSurveys.Add( ActiveSurvey->GetIdentifier() );
				Settings->PostEditChange();

				if ( FEngineAnalytics::IsAvailable() )
				{
					TArray< FAnalyticsEventAttribute > EventAttributes;

					EventAttributes.Add( FAnalyticsEventAttribute( TEXT("SurveyID"), ActiveSurvey->GetIdentifier().ToString() ) );

					FEngineAnalytics::GetProvider().RecordEvent( TEXT("OpenedSurvey"), EventAttributes);
				}
			}
		}

		Window->SetContent( SNew( SSurvey, SharedThis( this ), ActiveSurvey.ToSharedRef() ) );

		SurveyWindow = Window;

		FSlateApplication::Get().AddWindowAsNativeChild(Window, RootWindow.Pin().ToSharedRef());
	}
}

void FEpicSurvey::OnEpicSurveyWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	if( ActiveSurvey.IsValid() && !ActiveSurvey->IsReadyToSubmit() )
	{
		DisplayToolbarNotification();
	}
}

bool FEpicSurvey::ShouldPulseSurveyIcon( const TSharedRef< FSurvey >& Survey ) const
{
	return( bSurveyIconPulsing );
}

#undef LOCTEXT_NAMESPACE
