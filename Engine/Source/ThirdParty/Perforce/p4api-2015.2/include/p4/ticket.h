/*
 * Copyright 2004, Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * ticket.h - get/set tickets in local ticketfile.
 *
 * Public methods:
 *
 *      Ticket::GetTicket() - get a stored ticket from the ticketfile
 *      Ticket::ReplaceTicket() - change stored ticket value
 *      Ticket::DeleteTicket() - remove stored ticket value
 *      Ticket::List() - list all current tickets
 *      Ticket::List() - list current tickets of a specific user
 */

class FileSys;
class TicketTable;

class Ticket {

    public:
			Ticket( const StrPtr *path );
			~Ticket();

	char		*GetTicket( StrPtr &port, StrPtr &user );

	void		ReplaceTicket( const StrPtr &port, StrPtr &user,
			               StrPtr &ticket, Error *e )
			{ UpdateTicket( port, user, ticket, 0, e ); }

	void		DeleteTicket( const StrPtr &port, StrPtr &user,
			              Error *e )
			{ UpdateTicket( port, user, user, 1, e ); }

	void		List( StrBuf & );

	void		ListUser( const StrPtr &, StrBuf & );

    private:

	int		Init( );
	void		ReadTicketFile( Error *e );
	void		WriteTicketFile( Error *e );
	void		UpdateTicket( const StrPtr &port, StrPtr &user,
			              StrPtr &ticket, int remove, Error *e );

	TicketTable	*ticketTab;
	FileSys		*ticketFile;
	const StrPtr	*path;
};
