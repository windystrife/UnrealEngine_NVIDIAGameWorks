/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * netbuffer.h - buffer I/O to transport
 *
 * Classes Defined:
 *
 *	NetBuffer - send/receive buffers for transport
 *
 * Description:
 *
 *	NetBuffer provides a Send/Receive interface, and holds input
 *	and output buffers.  It attempts to avoid buffering if it can
 *	directly pass the caller's data to the transport.
 *
 *	NetBuffer also provides for compressing the link, one half at
 *	a time.
 *
 *	NB: once compression is turned on, it is on for the rest of the
 *	life of the NetBuffer.  Thus the caller should recreate the 
 *	NetBuffer for each connection.
 *
 * Public Methods:
 *
 *	NetBuffer::NetBuffer( NetTransport *t ) - take ownership of t
 *	NetBuffer::SetBufferSizes() - up read/write buffer sizes to himark
 *	NetBuffer::SendCompression() - zlib the send pipe
 *	NetBuffer::RecvCompression() - zlib the recv pipe
 *	NetBuffer::Send() - send block data
 *	NetBuffer::Receive() - receive block data
 *	NetBuffer::Flush() - flush buffered send data
 *	NetBuffer::Close() - close tranport; does not imply Flush()!
 *	NetBuffer::IsAlive() - check for disconnection, clear receive buffer
 *	NetBuffer::GetBuffering() - amount of transport send buffering
 *	NetBuffer::~NetBuffer() - destroy transport; implies Close()
 */

/* 
 * Buffer pointer mumbo jumbo.
 *
 *	Receive buffer:
 *
 *	 done: already passed up via NetBuffer::Receive()
 *	 ready: read from transport, ready for NetBuffer::Receive()
 *	 room: space for transport->Receive()
 *
 *	    recvBuf.Text()
 *	    |                     recvBuf.End()
 *	    |                     |
 *	    ^ done ^ ready ^ room ^
 *	           |       |      |
 *	           |       |      ioPtrs.recvEnd
 *	           |       ioPtrs.recvPtr
 *	           recvPtr
 *
 *	 Send buffer:
 *
 *	     done: already written by transport->Send()
 *	     ready: given to us by NetBuffer::Send, ready for transport
 *	     room: space for NetBuffer::Send
 *
 *	    sendBuf.Text()
 *	    |                     sendBuf.End()
 *	    |                     |
 *	    ^ done ^ ready ^ room ^
 *	           |       |
 *	           |       ioPtrs.sendEnd
 *	           ioPtrs.sendPtr
 */

typedef struct z_stream_s z_stream;

class NetBuffer : public NetTransport {

    public:
			NetBuffer( NetTransport *t );
			~NetBuffer();

	// NetTransport s

	StrPtr *	GetAddress( int raf_flags )
			{ return transport->GetAddress( raf_flags ); }
	StrPtr *	GetPeerAddress( int raf_flags )
			{ return transport->GetPeerAddress( raf_flags ); }
	int		GetPortNum()
	    		{ return transport->GetPortNum(); }
	bool		IsSockIPv6()
	    		{ return transport->IsSockIPv6(); }
	void    	ClientMismatch( Error *e )
			{ if( transport ) transport->ClientMismatch(e); };
	void		DoHandshake( Error *e )
			{ if( transport ) transport->DoHandshake(e); };

	void		Send( const char *buffer, int length, Error *e );
	int		Receive( char *buffer, int length, Error *e );
	virtual bool	IsAccepted()
			{
			    if( transport ) return transport->IsAccepted();
			    else return false;
			}


	void		Flush( Error *e ) { Flush( e, e ); }

	void		Close() 
			{ transport->Close(); }

	int		IsAlive();
	void		SetBreak( KeepAlive *breakCallback )
			{ transport->SetBreak( breakCallback ); }
	int		GetSendBuffering() 
			{ return transport->GetSendBuffering(); }
	int		GetRecvBuffering() 
			{ return transport->GetRecvBuffering(); }
	void            GetEncryptionType(StrBuf &value)
	                { transport->GetEncryptionType( value ); }
	void            GetPeerFingerprint(StrBuf &value)
	                { transport->GetPeerFingerprint( value ); }

	// NetBuffer specials
	// These babies take both send and receive Errors, so 
	// that we can track them separately.  Receive() might do
	// a Flush().  Send() might read data.

	int		Receive( char *buf, int len, Error *re, Error *se );
	void		Send( const char *buf, int len, Error *re, Error *se );
	void		Flush( Error *re, Error *se );

	void		SetBufferSizes( int recvSize, int sendSize );

	void		SendCompression( Error *e );
	void		RecvCompression( Error *e );

	int RecvReady()	{ return ioPtrs.recvPtr - recvPtr; }

    private:


	int RecvDone()	{ return recvPtr - recvBuf.Text(); }
	int RecvRoom() 	{ return ioPtrs.recvEnd - ioPtrs.recvPtr; }
	int SendDone()	{ return ioPtrs.sendPtr - sendBuf.Text(); }
	int SendReady()	{ return ioPtrs.sendEnd - ioPtrs.sendPtr; }
	int SendRoom() 	{ return sendBuf.End() - ioPtrs.sendEnd; }

	void ResetRecv() 
	{
	    recvPtr = recvBuf.Text();
	    ioPtrs.recvPtr = recvBuf.Text();
	    ioPtrs.recvEnd = recvBuf.End();
	}
	
	void ResetSend()
	{
	    ioPtrs.sendPtr = sendBuf.Text();
	    ioPtrs.sendEnd = sendBuf.Text();
	}

	void PackRecv()
	{
	    if( RecvDone() )
	    {
		int l = RecvReady();
		if( l == 0 )
		{
		    recvPtr = ioPtrs.recvPtr = recvBuf.Text();
		}
		else if( !RecvRoom() )
		{
		    memmove( recvBuf.Text(), recvPtr, l );
		    recvPtr = recvBuf.Text();
		    ioPtrs.recvPtr = recvBuf.Text() + l;
		}
	    }
	}

	void PackSend()
	{
	    if( !SendReady() )
	    {
		ResetSend();
	    }
	    else if( !SendRoom() && SendDone() )
	    {
		int l = SendReady();
		memmove( sendBuf.Text(), ioPtrs.sendPtr, l );
		ioPtrs.sendPtr = sendBuf.Text();
		ioPtrs.sendEnd = sendBuf.Text() + l;
	    }
	}

	// Buffer data.
	NetTransport	*transport;
	char		*recvPtr;
	NetIoPtrs	ioPtrs;

	StrBuf		sendBuf;
	StrBuf		recvBuf;

	// For compression

	int		compressing;
	z_stream	*zin;
	z_stream	*zout;

} ;
