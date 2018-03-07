/*
 * Copyright 1995, 2009 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * NetConnect.h - RPC connection handler
 *
 * Classes Defined:
 *
 *	NetIoPtrs - input/output parameters for SendOrReceive()
 *	NetEndPoint - an endpoint for making connections 
 *	NetTransport - an RPC connection to/from a remote host 
 *
 * Description:
 *
 *	These classes provide abstract base classes for an endpoint for
 *	a connection, and a connection itself.
 *
 *	It should go without saying, but destructing a NetTransport must 
 *	imply a Close().
 *
 * Public Methods:
 *
 *	NetEndPoint::Listen() - set up for subsequent Accept()
 *	NetEndPoint::ListenCheck() - see if we can listen on the given address
 *	NetEndPoint::CheaterCheck() - check if supplied port is the licensed one
 *	NetEndPoint::Unlisten() - cancel Listen()
 *	NetEndPoint::Transport() - return an appropriate NetTransport
 *	NetEndPoint::GetListenAddress() - return address suitable for Listen()
 *
 *	NetTransport::Accept() - accept a single incoming connection
 *	NetTransport::Connect() - make a single outgoing connection
 *	NetTransport::Send() - send stream data
 *	NetTransport::Receive() - receive stream data
 *	NetTransport::SendOrReceive() - send or receive what's available
 *	NetTransport::Close() - close connection
 *
 *	NetTransport::GetAddress() - return connection's local address
 *	NetTransport::GetPeerAddress() - return address of the peer
 *	NetTransport::GetBuffering() - return transport level send buffering
 */

# ifndef __NETCONNECT_H__
# define __NETCONNECT_H__

# include <error.h>

class KeepAlive;
class NetTransport;
class RpcZksClient;    // NetEndPoint's friend

enum PeekResults
{
    PeekTimeout = 0,
    PeekSSL,
    PeekCleartext
};

struct NetIoPtrs {

	char		*sendPtr;
	char		*sendEnd;

	char		*recvPtr;
	char		*recvEnd;

} ;

class NetEndPoint {
    friend class RpcZksClient;
    public:
	static NetEndPoint *	Create( const char *addr, Error *e );
	StrPtr 			GetAddress() { return ppaddr.HostPort(); }
	virtual void            GetExpiration( StrBuf &buf );

	virtual			~NetEndPoint();

	virtual StrPtr		*GetListenAddress( int raf_flags ) = 0;
	virtual StrPtr		*GetHost() = 0;

	// like GetHost(), but NetTcpEndPoint transforms into our standard printable form
	virtual StrBuf		GetPrintableHost()
				{
				    return *GetHost();
				}

	virtual void             GetMyFingerprint(StrBuf &value)
				{
				    value.Clear();
				}
	virtual bool		IsAccepted()
				{
				    return isAccepted;
				}

	virtual void		Listen( Error *e ) = 0;
	virtual void		ListenCheck( Error *e ) = 0;
	virtual int		CheaterCheck( const char *port ) = 0;
	virtual void		Unlisten() = 0;

	virtual NetTransport *	Connect( Error *e ) = 0;
	virtual NetTransport *	Accept( Error *e ) = 0;

	virtual int 		IsSingle() = 0;

	NetPortParser &		GetPortParser() { return ppaddr; }

    protected:
	NetPortParser		ppaddr;		// parsed transport/host/service endpoint
	bool			isAccepted;

	virtual int		GetFd() { return -1; }; // method used by RpcZksClient
} ;

class NetTransport : public KeepAlive {

    public:
	virtual		~NetTransport();
	virtual void    ClientMismatch( Error *e );
	virtual void	DoHandshake( Error * /* e */) {} // default: do nothing

	virtual bool	HasAddress() = 0;
	virtual StrPtr *GetAddress( int raf_flags ) = 0;
	virtual StrPtr *GetPeerAddress( int raf_flags ) = 0;
	virtual int	GetPortNum()
	    		{
			    return -1;
			}
	virtual bool	IsSockIPv6()
	    		{
			    return false;
			}
	virtual bool	IsAccepted() = 0;

	virtual void	Send( const char *buffer, int length, Error *e ) = 0;
	virtual int	Receive( char *buffer, int length, Error *e ) = 0;
	virtual void	Close() = 0;
	virtual void	SetBreak( KeepAlive *breakCallback ) = 0;
	virtual int	GetSendBuffering() = 0;
	virtual int	GetRecvBuffering() = 0;
	virtual void    GetEncryptionType(StrBuf &value)
	                {
			    value.Clear();
			}

	virtual void    GetPeerFingerprint(StrBuf &value)
	                {
			    value.Clear();
			}
	// I&O

	virtual int	SendOrReceive( NetIoPtrs &io, Error *se, Error *re );

	// DO NOT USE -- experimental only!

	virtual int	GetFd() { return -1; }

protected:
	PeekResults	CheckForHandshake(int fd);
	virtual int	Peek( int fd, char *buffer, int length );
} ;

# endif // # ifndef __NETCONNECT_H__
