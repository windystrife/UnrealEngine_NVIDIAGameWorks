// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once 

#include "Sockets.h"
#include "IPAddress.h"

class FSocketRaw; 
class FSocketX : public FSocket 
{
public:
	FSocketX(FSocketRaw* InSocket, ESocketType InSocketType, const FString& InSocketDescription, ISocketSubsystem * InSubsystem) 
		: FSocket(InSocketType, InSocketDescription)
		, Pimpl(InSocket)
		, SocketSubsystem(InSubsystem)
	{}

	virtual bool Close();

	virtual bool Bind( const FInternetAddr& Addr );

	virtual bool Connect( const FInternetAddr& Addr );

	virtual bool Listen( int32 MaxBacklog );

	virtual bool WaitForPendingConnection( bool& bHasPendingConnection, const FTimespan& WaitTime );

	virtual bool HasPendingData( uint32& PendingDataSize );

	virtual class FSocket* Accept( const FString& SocketDescription );

	virtual class FSocket* Accept( FInternetAddr& OutAddr, const FString& SocketDescription );

	virtual bool SendTo( const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination );

	virtual bool Send( const uint8* Data, int32 Count, int32& BytesSent );

	virtual bool RecvFrom( uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None );

	virtual bool Recv( uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None );

	virtual bool Wait( ESocketWaitConditions::Type Condition, FTimespan WaitTime );

	virtual ESocketConnectionState GetConnectionState();

	virtual void GetAddress( FInternetAddr& OutAddr );

	virtual bool GetPeerAddress( FInternetAddr& OutAddr ) override;

	virtual bool SetNonBlocking( bool bIsNonBlocking = true );

	virtual bool SetBroadcast( bool bAllowBroadcast = true );

	virtual bool JoinMulticastGroup( const FInternetAddr& GroupAddress );

	virtual bool LeaveMulticastGroup( const FInternetAddr& GroupAddress );

	virtual bool SetMulticastLoopback( bool bLoopback );

	virtual bool SetMulticastTtl( uint8 TimeToLive );

	virtual bool SetReuseAddr( bool bAllowReuse = true );

	virtual bool SetLinger( bool bShouldLinger = true, int32 Timeout = 0 );

	virtual bool SetRecvErr( bool bUseErrorQueue = true );

	virtual bool SetSendBufferSize( int32 Size, int32& NewSize );

	virtual bool SetReceiveBufferSize( int32 Size, int32& NewSize );

	virtual int32 GetPortNo();

	static  bool Init(); 
	static  bool GetHostByName( char const *NAME, FInternetAddr& Address ); 
	static  bool GetHostName( char *NAME ); 

	bool IsValid(); 
private: 

	FSocketRaw* Pimpl; 

	ISocketSubsystem *SocketSubsystem;
};