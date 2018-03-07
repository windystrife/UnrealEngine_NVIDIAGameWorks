// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtils.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Audio.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameEngine.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetDriver.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemBPCallHelper.h"



#include "VoiceModule.h"
#include "AudioDevice.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundWaveProcedural.h"

// Testing classes
#include "Tests/TestFriendsInterface.h"
#include "Tests/TestSessionInterface.h"
#include "Tests/TestCloudInterface.h"
#include "Tests/TestLeaderboardInterface.h"
#include "Tests/TestTimeInterface.h"
#include "Tests/TestIdentityInterface.h"
#include "Tests/TestTitleFileInterface.h"
#include "Tests/TestEntitlementsInterface.h"
#include "Tests/TestAchievementsInterface.h"
#include "Tests/TestSharingInterface.h"
#include "Tests/TestUserInterface.h"
#include "Tests/TestMessageInterface.h"
#include "Tests/TestVoice.h"
#include "Tests/TestExternalUIInterface.h"

UAudioComponent* CreateVoiceAudioComponent(uint32 SampleRate, int32 NumChannels)
{
	UAudioComponent* AudioComponent = nullptr;
	if (GEngine != nullptr)
	{
		if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice())
		{
			USoundWaveProcedural* SoundStreaming = NewObject<USoundWaveProcedural>();
			SoundStreaming->SampleRate = SampleRate;
			SoundStreaming->NumChannels = NumChannels;
			SoundStreaming->Duration = INDEFINITELY_LOOPING_DURATION;
			SoundStreaming->SoundGroup = SOUNDGROUP_Voice;
			SoundStreaming->bLooping = false;

			AudioComponent = AudioDevice->CreateComponent(SoundStreaming);
			if (AudioComponent)
			{
				AudioComponent->bIsUISound = true;
				AudioComponent->bAllowSpatialization = false;
				AudioComponent->SetVolumeMultiplier(1.5f);

				const FSoftObjectPath VoiPSoundClassName = GetDefault<UAudioSettings>()->VoiPSoundClass;
				if (VoiPSoundClassName.IsValid())
				{
					AudioComponent->SoundClassOverride = LoadObject<USoundClass>(nullptr, *VoiPSoundClassName.ToString());
				}
			}
			else
			{
				UE_LOG(LogVoiceDecode, Warning, TEXT("Unable to create voice audio component!"));
			}
		}
	}

	return AudioComponent;
}

UWorld* GetWorldForOnline(FName InstanceName)
{
	UWorld* World = NULL;
#ifdef WITH_EDITOR
	if (InstanceName != FOnlineSubsystemImpl::DefaultInstanceName && InstanceName != NAME_None)
	{
		FWorldContext& WorldContext = GEngine->GetWorldContextFromHandleChecked(InstanceName);
		check(WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE);
		World = WorldContext.World();
	}
	else
#endif
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		World = GameEngine ? GameEngine->GetGameWorld() : NULL;
	}

	return World;
}

int32 GetPortFromNetDriver(FName InstanceName)
{
	int32 Port = 0;
#if WITH_ENGINE
	if (GEngine)
	{
		UWorld* World = GetWorldForOnline(InstanceName);
		UNetDriver* NetDriver = World ? GEngine->FindNamedNetDriver(World, NAME_GameNetDriver) : NULL;
		if (NetDriver && NetDriver->GetNetMode() < NM_Client)
		{
			FString AddressStr = NetDriver->LowLevelGetNetworkNumber();
			int32 Colon = AddressStr.Find(":", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Colon != INDEX_NONE)
			{
				FString PortStr = AddressStr.Mid(Colon + 1);
				if (!PortStr.IsEmpty())
				{
					Port = FCString::Atoi(*PortStr);
				}
			}
		}
	}
#endif
	return Port;
}

bool HandleSessionCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = true;

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(InWorld);
	if (SessionInt.IsValid())
	{
		if (FParse::Command(&Cmd, TEXT("DUMP")))
		{
			SessionInt->DumpSessionState();
		}
	}

	return bWasHandled;
}

bool HandleVoiceCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = true;

	if (FParse::Command(&Cmd, TEXT("DUMP")))
	{
		LOG_SCOPE_VERBOSITY_OVERRIDE(LogVoice, ELogVerbosity::Display);
		bool bVoiceModule = FVoiceModule::IsAvailable();
		bool bVoiceModuleEnabled = false;
		if (bVoiceModule)
		{
			bVoiceModuleEnabled = FVoiceModule::Get().IsVoiceEnabled();
		}

		bool bRequiresPushToTalk = false;
		if (!GConfig->GetBool(TEXT("/Script/Engine.GameSession"), TEXT("bRequiresPushToTalk"), bRequiresPushToTalk, GGameIni))
		{
			UE_LOG(LogVoice, Warning, TEXT("Missing bRequiresPushToTalk key in [/Script/Engine.GameSession] of DefaultGame.ini"));
		}

		int32 MaxLocalTalkers = 0;
		if (!GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("MaxLocalTalkers"), MaxLocalTalkers, GEngineIni))
		{
			UE_LOG(LogVoice, Warning, TEXT("Missing MaxLocalTalkers key in OnlineSubsystem of DefaultEngine.ini"));
		}

		int32 MaxRemoteTalkers = 0;
		if (!GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("MaxRemoteTalkers"), MaxRemoteTalkers, GEngineIni))
		{
			UE_LOG(LogVoice, Warning, TEXT("Missing MaxRemoteTalkers key in OnlineSubsystem of DefaultEngine.ini"));
		}
		
		float VoiceNotificationDelta = 0.0f;
		if (!GConfig->GetFloat(TEXT("OnlineSubsystem"), TEXT("VoiceNotificationDelta"), VoiceNotificationDelta, GEngineIni))
		{
			UE_LOG(LogVoice, Warning, TEXT("Missing VoiceNotificationDelta key in OnlineSubsystem of DefaultEngine.ini"));
		}

		bool bHasVoiceInterfaceEnabled = false;
		if (!GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), bHasVoiceInterfaceEnabled, GEngineIni))
		{
			UE_LOG(LogVoice, Log, TEXT("Voice interface disabled by config [OnlineSubsystem].bHasVoiceEnabled"));
		}

		bool bDuckingOptOut = false;
		if (!GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bDuckingOptOut"), bDuckingOptOut, GEngineIni))
		{
			UE_LOG(LogVoice, Log, TEXT("Voice ducking not set by config [OnlineSubsystem].bDuckingOptOut"));
		}

		FString VoiceDump;

		bool bVoiceInterface = false;
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(InWorld);
		if (VoiceInt.IsValid())
		{
			bVoiceInterface = true;
			VoiceDump = VoiceInt->GetVoiceDebugState();
		}

		UE_LOG(LogVoice, Display, TEXT("Voice Module Available: %s"), bVoiceModule ? TEXT("true") : TEXT("false"));
		UE_LOG(LogVoice, Display, TEXT("Voice Module Enabled: %s"), bVoiceModuleEnabled ? TEXT("true") : TEXT("false"));
		UE_LOG(LogVoice, Display, TEXT("Voice Interface Available: %s"), bVoiceInterface ? TEXT("true") : TEXT("false"));
		UE_LOG(LogVoice, Display, TEXT("Voice Interface Enabled: %s"), bHasVoiceInterfaceEnabled ? TEXT("true") : TEXT("false"));
		UE_LOG(LogVoice, Display, TEXT("Ducking Opt Out Enabled: %s"), bDuckingOptOut ? TEXT("true") : TEXT("false"));
		UE_LOG(LogVoice, Display, TEXT("Max Local Talkers: %d"), MaxLocalTalkers);
		UE_LOG(LogVoice, Display, TEXT("Max Remote Talkers: %d"), MaxRemoteTalkers);
		UE_LOG(LogVoice, Display, TEXT("Notification Delta: %0.2f"), VoiceNotificationDelta);
		UE_LOG(LogVoice, Display, TEXT("Voice Requires Push To Talk: %s"), bRequiresPushToTalk ? TEXT("true") : TEXT("false"));

		TArray<FString> OutArray;
		VoiceDump.ParseIntoArray(OutArray, TEXT("\n"), false);
		for (const FString& Str : OutArray)
		{
			UE_LOG(LogVoice, Display, TEXT("%s"), *Str);
		}
	}
	else
	{
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(InWorld);
		if (VoiceInt.IsValid())
		{
		}
	}

	return bWasHandled;
}

/**
 * Exec handler that routes online specific execs to the proper subsystem
 *
 * @param InWorld World context
 * @param Cmd 	the exec command being executed
 * @param Ar 	the archive to log results to
 *
 * @return true if the handler consumed the input, false to continue searching handlers
 */
static bool OnlineExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	bool bWasHandled = false;

	// Ignore any execs that don't start with ONLINE
	if (FParse::Command(&Cmd, TEXT("ONLINE")))
	{
		FString SubName;
		FParse::Value(Cmd, TEXT("Sub="), SubName);
		// Allow for either Sub=<platform> or Subsystem=<platform>
		if (SubName.Len() > 0)
		{
			Cmd += FCString::Strlen(TEXT("Sub=")) + SubName.Len();
		}
		else
		{ 
			FParse::Value(Cmd, TEXT("Subsystem="), SubName);
			if (SubName.Len() > 0)
			{
				Cmd += FCString::Strlen(TEXT("Subsystem=")) + SubName.Len();
			}
		}

		IOnlineSubsystem* OnlineSub = NULL;
		// If the exec requested a specific subsystem, the grab that one for routing
		if (SubName.Len())
		{
			OnlineSub = Online::GetSubsystem(InWorld, *SubName);
		}
		// Otherwise use the default subsystem and route to that
		else
		{
			OnlineSub = Online::GetSubsystem(InWorld);
		}

		if (OnlineSub != NULL)
		{
			bWasHandled = OnlineSub->Exec(InWorld, Cmd, Ar);
			// If this wasn't handled, see if this is a testing request
			if (!bWasHandled)
			{
				if (FParse::Command(&Cmd, TEXT("TEST")))
				{
#if WITH_DEV_AUTOMATION_TESTS
					if (FParse::Command(&Cmd, TEXT("FRIENDS")))
					{
						TArray<FString> Invites;
						for (FString FriendId=FParse::Token(Cmd, false); !FriendId.IsEmpty(); FriendId=FParse::Token(Cmd, false))
						{
							Invites.Add(FriendId);
						}
						// This class deletes itself once done
						(new FTestFriendsInterface(SubName))->Test(InWorld, Invites);
						bWasHandled = true;
					}
					// Spawn the object that will exercise all of the session methods as host
					else if (FParse::Command(&Cmd, TEXT("SESSIONHOST")))
					{
						bool bTestLAN = FParse::Command(&Cmd, TEXT("LAN")) ? true : false;
						bool bTestPresence = FParse::Command(&Cmd, TEXT("PRESENCE")) ? true : false;

						FOnlineSessionSettings SettingsOverride;

						FString ParamOverride;
						while (FParse::Token(Cmd, ParamOverride, false))
						{
							FString Value;
							FParse::Token(Cmd, Value, false);

							if (Value.IsNumeric())
							{
								SettingsOverride.Set(FName(*ParamOverride), FCString::Atoi(*Value));
							}
							else
							{
								SettingsOverride.Set(FName(*ParamOverride), Value);
							}
						}

						// This class deletes itself once done
						(new FTestSessionInterface(SubName, true))->Test(InWorld, bTestLAN, bTestPresence, false, SettingsOverride);
						bWasHandled = true;
					}
					// Spawn the object that will exercise all of the session methods as client
					else if (FParse::Command(&Cmd, TEXT("SESSIONCLIENT")))
					{
						bool bTestLAN = FParse::Command(&Cmd, TEXT("LAN")) ? true : false;
						bool bTestPresence = FParse::Command(&Cmd, TEXT("PRESENCE")) ? true : false;

						FOnlineSessionSettings SettingsOverride;

						// This class deletes itself once done
						(new FTestSessionInterface(SubName, false))->Test(InWorld, bTestLAN, bTestPresence, false, SettingsOverride);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("STARTMATCHMAKING")))
					{
						FOnlineSessionSettings SettingsOverride;

						FString ParamOverride;
						while (FParse::Token(Cmd, ParamOverride, false))
						{
							FString Value;
							FParse::Token(Cmd, Value, false);

							if (Value.IsNumeric())
							{
								SettingsOverride.Set(FName(*ParamOverride), FCString::Atoi(*Value));
							}
							else
							{
								SettingsOverride.Set(FName(*ParamOverride), Value);
							}
						}

						// This class deletes itself once done
						(new FTestSessionInterface(SubName, false))->Test(InWorld, false, false, true, SettingsOverride);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("CLOUD")))
					{
						// This class deletes itself once done
						(new FTestCloudInterface(SubName))->Test(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("LEADERBOARDS")))
					{
						// This class deletes itself once done
						(new FTestLeaderboardInterface(SubName))->Test(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("VOICE")))
					{
						// This class deletes itself once done
						(new FTestVoice())->Test();
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("TIME")))
					{
						// This class deletes itself once done
						(new FTestTimeInterface(SubName))->Test(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("IDENTITY")))
					{
						FString Id = FParse::Token(Cmd, false);
						FString Auth = FParse::Token(Cmd, false);
						FString Type = FParse::Token(Cmd, false);

						bool bLogout = Id.Equals(TEXT("logout"), ESearchCase::IgnoreCase);

						// This class deletes itself once done
						(new FTestIdentityInterface(SubName))->Test(InWorld, FOnlineAccountCredentials(Type, Id, Auth), bLogout);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("UNIQUEIDREPL")))
					{
						extern void TestUniqueIdRepl(class UWorld* InWorld);
						TestUniqueIdRepl(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("KEYVALUEPAIR")))
					{
						extern void TestKeyValuePairs();
						TestKeyValuePairs();
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("TITLEFILE")))
					{
						// This class deletes itself once done
						(new FTestTitleFileInterface(SubName))->Test(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("ENTITLEMENTS")))
					{
						// This class also deletes itself once done
						(new FTestEntitlementsInterface(SubName))->Test(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("ACHIEVEMENTS")))
					{
						// This class also deletes itself once done
						(new FTestAchievementsInterface(SubName))->Test(InWorld);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("SHARING")))
					{
						bool bTestWithImage = FParse::Command(&Cmd, TEXT("IMG"));

						// This class also deletes itself once done
						(new FTestSharingInterface(SubName))->Test(InWorld, bTestWithImage);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("USER")))
					{
						TArray<FString> UserIds;
						for (FString Id=FParse::Token(Cmd, false); !Id.IsEmpty(); Id=FParse::Token(Cmd, false))
						{
							UserIds.Add(Id);
						}
						// This class also deletes itself once done
						(new FTestUserInterface(SubName))->Test(InWorld, UserIds);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("MESSAGE")))
					{
						TArray<FString> RecipientIds;
						for (FString UserId=FParse::Token(Cmd, false); !UserId.IsEmpty(); UserId=FParse::Token(Cmd, false))
						{
							RecipientIds.Add(UserId);
						}
						// This class also deletes itself once done
						(new FTestMessageInterface(SubName))->Test(InWorld, RecipientIds);
						bWasHandled = true;
					}
					else if (FParse::Command(&Cmd, TEXT("EXTERNALUI")))
					{
						// Full command usage:    EXTERNALUI ACHIEVEMENTS FRIENDS INVITE LOGIN PROFILE WEBURL
						// Example for one test:  EXTERNALUI WEBURL
						// Note that tests are enabled in alphabetical order
						bool bTestAchievementsUI = FParse::Command(&Cmd, TEXT("ACHIEVEMENTS")) ? true : false;
						bool bTestFriendsUI = FParse::Command(&Cmd, TEXT("FRIENDS")) ? true : false;
						bool bTestInviteUI = FParse::Command(&Cmd, TEXT("INVITE")) ? true : false;
						bool bTestLoginUI = FParse::Command(&Cmd, TEXT("LOGIN")) ? true : false;
						bool bTestProfileUI = FParse::Command(&Cmd, TEXT("PROFILE")) ? true : false;
						bool bTestWebURL = FParse::Command(&Cmd, TEXT("WEBURL")) ? true : false;

						// This class also deletes itself once done
						(new FTestExternalUIInterface(SubName, bTestLoginUI, bTestFriendsUI, bTestInviteUI, bTestAchievementsUI, bTestWebURL, bTestProfileUI))->Test();
						bWasHandled = true;
					}
#endif //WITH_DEV_AUTOMATION_TESTS
				}
				else if (FParse::Command(&Cmd, TEXT("SESSION")))
				{
					bWasHandled = HandleSessionCommands(InWorld, Cmd, Ar);
				}
				else if (FParse::Command(&Cmd, TEXT("VOICE")))
				{
					bWasHandled = HandleVoiceCommands(InWorld, Cmd, Ar);
				}
			}
		}
	}
	return bWasHandled;
}

/** Our entry point for all online exec routing */
FStaticSelfRegisteringExec OnlineExecRegistration(OnlineExec);

//////////////////////////////////////////////////////////////////////////
// FOnlineSubsystemBPCallHelper

FOnlineSubsystemBPCallHelper::FOnlineSubsystemBPCallHelper(const TCHAR* CallFunctionContext, UObject* WorldContextObject, FName SystemName)
	: OnlineSub(Online::GetSubsystem(GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull), SystemName))
	, FunctionContext(CallFunctionContext)
{
	if (OnlineSub == nullptr)
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s - Invalid or uninitialized OnlineSubsystem"), FunctionContext), ELogVerbosity::Warning);
	}
}

void FOnlineSubsystemBPCallHelper::QueryIDFromPlayerController(APlayerController* PlayerController)
{
	UserID.Reset();

	if (APlayerState* PlayerState = (PlayerController != NULL) ? PlayerController->PlayerState : NULL)
	{
		UserID = PlayerState->UniqueId.GetUniqueNetId();
		if (!UserID.IsValid())
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s - Cannot map local player to unique net ID"), FunctionContext), ELogVerbosity::Warning);
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s - Invalid player state"), FunctionContext), ELogVerbosity::Warning);
	}
}
