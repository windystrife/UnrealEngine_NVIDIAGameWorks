// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once 

class FInternetAddrRaw; 
struct FSocketRawData; 

class  FSocketRaw 
{
	FSocketRawData* SocketRawData; 

public: 

	FSocketRaw( bool IsTCP ); 

	FSocketRaw( FSocketRawData* ); 

	bool Close();

	int Bind(const FInternetAddrRaw& Addr);

	unsigned int Connect(const FInternetAddrRaw& Addr); 

	bool Listen(unsigned int MaxBacklog);

	bool WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime)

	bool HasPendingData(unsigned int& PendingDataSize);

	FSocketRaw* Accept();
	FSocketRaw* Accept(FInternetAddrRaw& OutAddr);

	bool SendTo(const unsigned char* Data, unsigned int Count, unsigned int & BytesSent, const FInternetAddrRaw& Destination);

	bool Send(const unsigned char* Data, unsigned int Count, unsigned int& BytesSent);

	bool RecvFrom(unsigned char* Data, unsigned int BufferSize, unsigned int& BytesRead, FInternetAddrRaw& Source, unsigned int Flags);

	bool Recv(unsigned char* Data, unsigned int BufferSize, unsigned int& BytesRead, unsigned Flags);

	bool WaitForRead ( int WaitTime ); 
	bool WaitForWrite ( int WaitTime ); 
	bool WaitForReadWrite ( int WaitTime );

	bool GetAddress(FInternetAddrRaw& OutAddr);

	bool SetNonBlocking(bool bIsNonBlocking = true);

	bool SetBroadcast(bool bAllowBroadcast = true);

	bool JoinMulticastGroup (const FInternetAddrRaw& GroupAddress);

	bool LeaveMulticastGroup (const FInternetAddrRaw& GroupAddress);

	bool SetMulticastLoopback (bool bLoopback);

	bool SetMulticastTtl (unsigned int TimeToLive);

	bool SetReuseAddr(bool bAllowReuse = true);

	bool SetLinger(bool bShouldLinger = true, int  Timeout = 0);
	
	bool SetSendBufferSize(unsigned int Size, unsigned int & NewSize);

	bool SetReceiveBufferSize(unsigned int Size, unsigned int & NewSize);
	
	unsigned int GetPortNo(bool& Succeeded);

	bool IsValid(); 

	static  bool GetHostByName( const char *NAME, FInternetAddrRaw& Address ); 
	static  bool GetHostName(const char *NAME ); 
	static  bool Init(); 
 	
};
