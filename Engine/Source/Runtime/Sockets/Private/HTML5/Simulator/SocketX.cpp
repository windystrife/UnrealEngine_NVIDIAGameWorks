// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketX.h"
#include "Sockets/SocketRaw.h"
#include "Sockets/IPAddressRaw.h"

#define ADDTOPIMPL(BasePtr) *((FInternetAddrX*)&BasePtr)->GetPimpl()

bool FSocketX::Close()
{
	return Pimpl->Close();
}

bool FSocketX::Bind( const FInternetAddr& Addr )
{
	return Pimpl->Bind(ADDTOPIMPL(Addr)) == 0; 
}

bool FSocketX::Connect( const FInternetAddr& Addr )
{
	int32 Return =  Pimpl->Connect(ADDTOPIMPL(Addr));

	check(SocketSubsystem);
	ESocketErrors Error = SocketSubsystem->TranslateErrorCode(Return);

	// "would block" is not an error
	return ((Error == SE_NO_ERROR) || (Error == SE_EWOULDBLOCK));
}

bool FSocketX::Listen( int32 MaxBacklog )
{
	return Pimpl->Listen(MaxBacklog);
}

bool FSocketX::WaitForPendingConnection( bool& bHasPendingConnection, const FTimespan& WaitTime)
{
	return Pimpl->WaitForPendingConnection(bHasPendingConnection, WaitTime); 
}

bool FSocketX::HasPendingData( uint32& PendingDataSize )
{
	return Pimpl->HasPendingData(PendingDataSize); 
}

class FSocket* FSocketX::Accept( const FString& SocketDescription )
{
	FSocketRaw* RawSocket = Pimpl->Accept(); 
	return new FSocketX( RawSocket, SocketType, SocketDescription,SocketSubsystem);
}

class FSocket* FSocketX::Accept( FInternetAddr& OutAddr, const FString& SocketDescription )
{
	FSocketRaw* RawSocket = Pimpl->Accept(ADDTOPIMPL(OutAddr));  
	return new FSocketX( RawSocket, SocketType, SocketDescription,SocketSubsystem);
}

bool FSocketX::SendTo( const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination )
{
	return Pimpl->SendTo(Data,Count,(unsigned int&)BytesSent, ADDTOPIMPL(Destination));
}

bool FSocketX::Send( const uint8* Data, int32 Count, int32& BytesSent )
{
	return Pimpl->Send( Data, Count, (unsigned int&)BytesSent);
}

bool FSocketX::RecvFrom( uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags /*= ESocketReceiveFlags::None */ )
{
	return Pimpl->RecvFrom( Data, BufferSize, (unsigned int&) BytesRead,  ADDTOPIMPL(Source), (int)Flags );
}

bool FSocketX::Recv( uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags /*= ESocketReceiveFlags::None */ )
{
	return Pimpl->Recv(Data,BufferSize,(unsigned int&)BytesRead,(int)Flags);
}

bool FSocketX::Wait( ESocketWaitConditions::Type Condition, FTimespan WaitTime )
{
	if ((Condition == ESocketWaitConditions::WaitForRead) )
	{
		return Pimpl->WaitForRead((int)WaitTime.GetTotalMilliseconds());
	}
	if ( Condition == ESocketWaitConditions::WaitForWrite)
	{
		return Pimpl->WaitForWrite((int)WaitTime.GetTotalMilliseconds());
	}
		
	return Pimpl->WaitForReadWrite((int)WaitTime.GetTotalMilliseconds());
}

ESocketConnectionState FSocketX::GetConnectionState()
{
	// @fix-me - 
	if ( !Pimpl->WaitForReadWrite( 1 ) )
	{
		 return ESocketConnectionState::SCS_ConnectionError;
	}
	else 
	{
		 return ESocketConnectionState::SCS_Connected; 
	}

}

void FSocketX::GetAddress( FInternetAddr& OutAddr )
{
	Pimpl->GetAddress(ADDTOPIMPL(OutAddr) );
}

bool FSocketX::GetPeerAddress( FInternetAddr& OutAddr )
{
	Pimpl->GetPeerAddress(ADDTOPIMPL(OutAddr));
}

bool FSocketX::SetNonBlocking( bool bIsNonBlocking /*= true */ )
{
	return Pimpl->SetNonBlocking( bIsNonBlocking ); 
}

bool FSocketX::SetBroadcast( bool bAllowBroadcast /*= true */ )
{
	return Pimpl->SetBroadcast( bAllowBroadcast );
}

bool FSocketX::JoinMulticastGroup( const FInternetAddr& GroupAddress )
{
	return Pimpl->JoinMulticastGroup( ADDTOPIMPL(GroupAddress) );
}

bool FSocketX::LeaveMulticastGroup( const FInternetAddr& GroupAddress )
{
	return Pimpl->LeaveMulticastGroup( ADDTOPIMPL(GroupAddress) ); 
}

bool FSocketX::SetMulticastLoopback( bool bLoopback )
{
	return Pimpl->SetMulticastLoopback(bLoopback);
}

bool FSocketX::SetMulticastTtl( uint8 TimeToLive )
{
	return Pimpl->SetMulticastTtl( TimeToLive); 
}

bool FSocketX::SetReuseAddr( bool bAllowReuse /*= true */ )
{
	return Pimpl->SetReuseAddr(  bAllowReuse ); 
}

bool FSocketX::SetLinger( bool bShouldLinger /*= true*/, int32 Timeout /*= 0 */ )
{
	return Pimpl->SetLinger( bShouldLinger, Timeout); 
}

bool FSocketX::SetRecvErr( bool bUseErrorQueue /*= true */ )
{
	return	Pimpl->SetLinger(bUseErrorQueue); 
}

bool FSocketX::SetSendBufferSize( int32 Size, int32& NewSize )
{
	return Pimpl->SetSendBufferSize( Size, (unsigned int&)NewSize );
}

bool FSocketX::SetReceiveBufferSize( int32 Size, int32& NewSize )
{
	return Pimpl->SetReceiveBufferSize( Size, (unsigned int&)NewSize); 
}

int32 FSocketX::GetPortNo()
{
	bool bOK = false; 
	int32 PortNo =  Pimpl->GetPortNo(bOK);
	check ( bOK ); 
	return PortNo; 
}

bool FSocketX::IsValid()
{
	return Pimpl->IsValid(); 
}