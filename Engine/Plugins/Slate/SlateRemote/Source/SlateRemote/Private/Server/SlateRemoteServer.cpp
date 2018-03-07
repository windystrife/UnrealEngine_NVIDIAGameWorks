// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Server/SlateRemoteServer.h"
#include "Common/UdpSocketBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Server/SlateRemoteServerMessage.h"
#include "SlateRemotePrivate.h"


/* FSlateRemoteServer structors
 *****************************************************************************/

FSlateRemoteServer::FSlateRemoteServer( ISocketSubsystem& InSocketSubsystem, const FIPv4Endpoint& InServerEndpoint )
	: HighestMessageReceived(0xFFFF)
	, ReplyAddr(InSocketSubsystem.CreateInternetAddr())
	, ServerSocket(nullptr)
	, SocketSubsystem(InSocketSubsystem)
	, TimeSinceLastPing(200.0f)
	, Timestamp(0.0)
{
	StartServer(InServerEndpoint);
}


FSlateRemoteServer::~FSlateRemoteServer()
{
	StopServer();
}


/* FSlateRemoteServer interface
 *****************************************************************************/

bool FSlateRemoteServer::StartServer( const FIPv4Endpoint& ServerEndpoint )
{
	ServerSocket = FUdpSocketBuilder(TEXT("SlateRemoteServerSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(ServerEndpoint);

	if (ServerSocket == nullptr)
	{
		GLog->Logf(TEXT("SlateRemoteServer: Failed to create server socket on %s"), *ServerEndpoint.ToText().ToString());

		return false;
	}

	TickDelegate = FTickerDelegate::CreateRaw(this, &FSlateRemoteServer::HandleTicker);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 0.0f);

	return true;
}


void FSlateRemoteServer::StopServer()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if (ServerSocket != nullptr)
	{
		SocketSubsystem.DestroySocket(ServerSocket);
		ServerSocket = nullptr;
	}
}


/* FSlateRemoteServer implementation
 *****************************************************************************/

void FSlateRemoteServer::ProcessGyroMessage( const FSlateRemoteServerMessage& Message )
{
	// @todo SlateRemote: implement gyro support
}


void FSlateRemoteServer::ProcessMotionMessage( const FSlateRemoteServerMessage& Message )
{
	// negate yaw angle
	FVector Attitude = FVector(Message.Data[0], -Message.Data[1], Message.Data[2]);
	FVector RotationRate = FVector(Message.Data[3], -Message.Data[4], Message.Data[5]);
	FVector Gravity = FVector(Message.Data[6], Message.Data[7], Message.Data[8]);
	FVector Accel = FVector(Message.Data[9], Message.Data[10], Message.Data[11]);

	FMotionEvent Event(0, Attitude, RotationRate, Gravity, Accel);
	FSlateApplication::Get().ProcessMotionDetectedEvent(Event);
}


void FSlateRemoteServer::ProcessPingMessage( const FSlateRemoteServerMessage& Message )
{
	TimeSinceLastPing = 0.0f;
	ReplyAddr->SetPort(41764);

	int32 BytesSent;
	const ANSICHAR* HELO = "HELO";
	ServerSocket->SendTo((uint8*)HELO, 5, BytesSent, *ReplyAddr);
}


void FSlateRemoteServer::ProcessTiltMessage( const FSlateRemoteServerMessage& Message )
{
	// get the raw and processed values from the remote device
	FVector CurrentAccelerometer(Message.Data[0], Message.Data[1], Message.Data[2]);
	float Pitch = Message.Data[3], Roll = Message.Data[4];

	// convert it into "Motion" data
	static float LastPitch, LastRoll;

	FVector Attitude = FVector(Pitch,0,Roll);
	FVector RotationRate = FVector(LastPitch - Pitch,0,LastRoll - Roll);
	FVector Gravity = FVector(0,0,0);
	FVector Accel = CurrentAccelerometer;

	// save the previous values to delta rotation
	LastPitch = Pitch;
	LastRoll = Roll;

	FMotionEvent Event(0, Attitude, RotationRate, Gravity, Accel);
	FSlateApplication::Get().ProcessMotionDetectedEvent(Event);
}


void FSlateRemoteServer::ProcessTouchMessage( const FSlateRemoteServerMessage& Message )
{
	// @todo This should be some global value or something in slate??
	static FVector2D LastTouchPositions[EKeys::NUM_TOUCH_KEYS];
					
	if (Message.Handle < EKeys::NUM_TOUCH_KEYS)
	{
		FSlateApplication& SlateApplication = FSlateApplication::Get();

		// convert from 0..1 to window coordinates to screen coordinates
		// @todo: Put this into a function in SlateApplication
		if (SlateApplication.GetActiveTopLevelWindow().IsValid())
		{
			FVector2D ScreenPosition;

			// The remote is interested in finding the game viewport for the user and mapping the input in to it
			TSharedPtr<SViewport> GameViewport = SlateApplication.GetGameViewport();
			if (GameViewport.IsValid())
			{
				FWidgetPath WidgetPath;

				WidgetPath = GameViewportWidgetPath.ToWidgetPath();
						
				if (WidgetPath.Widgets.Num() == 0 || WidgetPath.Widgets.Last().Widget != GameViewport)
				{
					SlateApplication.FindPathToWidget(GameViewport.ToSharedRef(), WidgetPath);
					GameViewportWidgetPath = WidgetPath;
				}

				const FGeometry& GameViewportGeometry = WidgetPath.Widgets.Last().Geometry;
				ScreenPosition = GameViewportGeometry.LocalToAbsolute(FVector2D(Message.Data[0], Message.Data[1]) * GameViewportGeometry.Size);
			}
			else
			{
				const FSlateRect WindowScreenRect = SlateApplication.GetActiveTopLevelWindow()->GetRectInScreen();
				const FVector2D WindowPosition = WindowScreenRect.GetSize() * FVector2D(Message.Data[0], Message.Data[1]);
				ScreenPosition = FVector2D(WindowScreenRect.Left, WindowScreenRect.Top) + WindowPosition;
			}

			// for up/down messages, we need to let the cursor go in the same location
			// (mouse up with a delta confuses things)
			if (Message.DataType == DT_TouchBegan || Message.DataType == DT_TouchEnded)
			{
				LastTouchPositions[Message.Handle] = ScreenPosition;
			}

			// create the event struct
			FPointerEvent Event(0, Message.Handle, ScreenPosition, LastTouchPositions[Message.Handle], Message.DataType != DT_TouchEnded);
			LastTouchPositions[Message.Handle] = ScreenPosition;

			// send input to handler
			if (Message.DataType == DT_TouchBegan)
			{
				SlateApplication.ProcessTouchStartedEvent(nullptr, Event);
			}
			else if (Message.DataType == DT_TouchEnded)
			{
				SlateApplication.ProcessTouchEndedEvent(Event);
			}
			else
			{
				SlateApplication.ProcessTouchMovedEvent(Event);
			}
		}
	}
	else
	{
		//checkf(Message.Handle < ARRAY_COUNT(LastTouchPositions), TEXT("Received handle %d, but it's too big for our array"), Message.Handle);
	}
}


/* FSlateRemoteServer callbacks
 *****************************************************************************/

bool FSlateRemoteServer::HandleTicker( float DeltaTime )
{
	FSlateRemoteServerMessage Message; // @todo SlateRemote: this is sketchy; byte ordering and packing ignored; use FArchive!
	int32 BytesRead;

	while (ServerSocket->RecvFrom((uint8*)&Message, sizeof(Message), BytesRead, *ReplyAddr))
	{
		if (BytesRead == 0)
		{
			return true;
		}

		if (BytesRead != sizeof(FSlateRemoteServerMessage))
		{
			UE_LOG(LogSlate, Log, TEXT("Received %d bytes, expected %d"), BytesRead, sizeof(FSlateRemoteServerMessage));
			continue;
		}

		// verify message data
		bool bIsValidMessageVersion =
			(Message.MagicTag == SLATE_REMOTE_SERVER_MESSAGE_MAGIC_ID) &&
			(Message.MessageVersion == SLATE_REMOTE_SERVER_PROTOCOL_VERSION);

		bool bIsValidID = 
			(Message.DataType == DT_Ping) ||
			(Message.MessageID > HighestMessageReceived) || 
			(Message.MessageID < 1000 && HighestMessageReceived > 65000);

		HighestMessageReceived = Message.MessageID;

		if (!bIsValidMessageVersion || !bIsValidID)
		{
//			debugf(TEXT("Dropped message %d, %d"), Message.MessageID, Message.DataType);
			continue;
		}

//		debugf(TEXT("Received message %d, %d"), Message.MessageID, Message.DataType);

		// handle tilt input
		switch(Message.DataType)
		{
			case DT_Motion:
			{
				ProcessMotionMessage(Message);
			} break;
			case DT_Tilt:
			{
				ProcessTiltMessage(Message);
			} break;
			case DT_Gyro:
			{
				ProcessGyroMessage(Message);
			} break;
			case DT_Ping:
			{
				ProcessPingMessage(Message);
			} break;
			default:
			{
				ProcessTouchMessage(Message);
			} break;

		}
	}

	TimeSinceLastPing += DeltaTime;

	return true;
}
