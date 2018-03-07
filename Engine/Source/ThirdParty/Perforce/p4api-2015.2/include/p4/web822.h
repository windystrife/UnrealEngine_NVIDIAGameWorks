/*
 * web822.h -- a transport protocol with RFC822 style headers
 *
 * Classes:
 *
 *	Web822 -- a transport protocol with RFC822 style headers
 *
 * Description:
 *
 *	Web822 is a wrapper around a buffered TCP/IP transport
 *	(Web822 -> NetBuffer -> NetTransport) that provides
 *	an interface to RFC822 style headers and body.
 *
 * Public methods:
 *
 *	Web822::LoadHeader() - read headers prior to first GetVar()
 *	Web822::SendHeader() - send headers after last SetVar()
 *
 *	Web822::Receive() - read body data
 *	Web822::Send() - send body data
 *	Web822::operator << - send strings and numbers (easily)
 *
 *	(from StrDict)
 *	Web822::GetVar() - get a the value of a header line
 *	Web822::SetVar() - set a the value of a header line
 *
 * NB this interface is in flux and under construction.
 */

class Web822 : public StrDict {

    public:
			Web822( NetTransport *t ) : transport( t ) {}
			
	virtual		~Web822() { transport.Flush( &e ); }

	// From StrDict

	StrPtr *	VGetVar( const StrPtr &var )
			{ return recvHeaders.GetVar( var ); }

	void		VSetVar( const StrPtr &var, const StrPtr &val )
			{ sendHeaders.SetVar( var, val ); }

	int		VGetVarX( int x, StrRef &var, StrRef &val )
			{ return recvHeaders.VGetVarX( x, var, val ); }
			
	// Send a response
	
	void		SendResponse( char *s, int length )
			{  Send( s, length ); }

	// For headers

	int		LoadHeader();
	void	SendHeader( const StrPtr *respond = 0 );
	void	GetRecvHeaders(StrBuf *headers);

	// For body data
	
	int		LoadBody();
	char *	GetBodyData();
	int		GetBodyLgth();
	
	// For debugging purposes
	
	void		SendRecvHeaders();
	void		SendSendHeaders();

	// Wrappers around NetBuffer::Send()

	void		Send( const char *s, int l ) 
			{ transport.Send( s, l, &e ); }

	int		Receive( char *s, int l ) 
			{ return transport.Receive( s, l, &e ); }

	Web822& operator <<( const char *s )
			{ *this << StrRef( (char*)s ); return *this; }

	Web822& operator <<( const StrPtr *s )
			{ *this << *s; return *this; }

	Web822& operator <<( const StrPtr &s )
			{ Send( s.Text(), s.Length() ); return *this; }

	Web822& operator <<( const int v )
			{
			    char buf[ 24 ];
			    sprintf( buf, "%d", v );
			    *this << buf;
			    return *this;
			}
	
	// So we can tell if endpoints are both running on local machine
	StrPtr * GetAddress( int raf_flags );
	StrPtr * GetPeerAddress( int raf_flags );
	
    private:
	// Our transport

	NetBuffer	transport;

	// stored headers

	StrBufDict	recvHeaders;
	StrBufDict	sendHeaders;

	// stored body
	
	StrBuf		recvBody;

	int		haveReadBody;	// have we read the body of this message?
	// I/O Errors.

	Error		e;

} ;
