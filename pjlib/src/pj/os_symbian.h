/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __OS_SYMBIAN_H__
#define __OS_SYMBIAN_H__

#include <pj/sock.h>
#include <pj/os.h>
#include <pj/string.h>

#include <e32base.h>
#include <e32cmn.h>
#include <e32std.h>
#include <es_sock.h>
#include <in_sock.h>
#include <charconv.h>
#include <utf.h>
#include <e32cons.h>

// Forward declarations
class CPjSocketReader;

#ifndef PJ_SYMBIAN_TIMER_PRIORITY
#    define PJ_SYMBIAN_TIMER_PRIORITY	EPriorityNormal
#endif

//
// PJLIB Symbian's Socket
//
class CPjSocket
{
public:
    enum
    {
	MAX_LEN = 1500,
    };

    // Construct CPjSocket
    CPjSocket(RSocket &sock)
	: sock_(sock), connected_(false), sockReader_(NULL)
    { 
    }

    // Destroy CPjSocket
    ~CPjSocket();

    // Get the internal RSocket
    RSocket& Socket()
    {
	return sock_;
    }

    // Get socket connected flag.
    bool IsConnected() const
    {
	return connected_;
    }

    // Set socket connected flag.
    void SetConnected(bool connected)
    {
	connected_ = connected;
    }

    // Get socket reader, if any.
    // May return NULL.
    CPjSocketReader *Reader()
    {
	return sockReader_;
    }

    // Create socket reader.
    CPjSocketReader *CreateReader(unsigned max_len=CPjSocket::MAX_LEN);

    // Delete socket reader when it's not wanted.
    void DestroyReader();
    
private:
    RSocket	     sock_;	    // Must not be reference, or otherwise
				    // it may point to local variable!
    bool	     connected_;
    CPjSocketReader *sockReader_;
};


//
// Socket reader, used by select() and ioqueue abstraction
//
class CPjSocketReader : public CActive
{
public:
    // Construct.
    static CPjSocketReader *NewL(CPjSocket &sock, unsigned max_len=CPjSocket::MAX_LEN);

    // Destroy;
    ~CPjSocketReader();

    // Start asynchronous read from the socket.
    void StartRecv(void (*cb)(void *key)=NULL, 
		   void *key=NULL, 
		   TDes8 *aDesc = NULL,
		   TUint flags = 0);

    // Start asynchronous read from the socket.
    void StartRecvFrom(void (*cb)(void *key)=NULL, 
		       void *key=NULL, 
		       TDes8 *aDesc = NULL,
		       TUint flags = 0,
		       TSockAddr *fromAddr = NULL);

    // Cancel asynchronous read.
    void DoCancel();

    // Implementation: called when read has completed.
    void RunL();

    // Check if there's pending data.
    bool HasData() const
    {
	return buffer_.Length() != 0;
    }

    // Append data to aDesc, up to aDesc's maximum size.
    // If socket is datagram based, buffer_ will be clared.
    void ReadData(TDes8 &aDesc, TInetAddr *addr=NULL);

private:
    CPjSocket	   &sock_;
    bool	    isDatagram_;
    TPtr8	    buffer_;
    TInetAddr	    recvAddr_;

    void	   (*readCb_)(void *key);
    void	    *key_;

    //
    // Constructor
    //
    CPjSocketReader(CPjSocket &sock);
    void ConstructL(unsigned max_len);
};



//
// Time-out Timer Active Object
//
class CPjTimeoutTimer : public CActive
{
public:
    static CPjTimeoutTimer *NewL();
    ~CPjTimeoutTimer();

    void StartTimer(TUint miliSeconds);
    bool HasTimedOut() const;

protected:
    virtual void RunL();
    virtual void DoCancel();
    virtual TInt RunError(TInt aError);

private:
    RTimer	timer_;
    pj_bool_t	hasTimedOut_;

    CPjTimeoutTimer();
    void ConstructL();
};



//
// Symbian OS helper for PJLIB
//
class PjSymbianOS
{
public:
    //
    // Get the singleton instance of PjSymbianOS
    //
    static PjSymbianOS *Instance();

    //
    // Set parameters
    //
    void SetParameters(pj_symbianos_params *params);
    
    //
    // Initialize.
    //
    TInt Initialize();

    //
    // Shutdown.
    //
    void Shutdown();


    //
    // Socket helper.
    //

    // Get RSocketServ instance to be used by all sockets.
    RSocketServ &SocketServ()
    {
	return appSocketServ_ ? *appSocketServ_ : socketServ_;
    }

    // Get RConnection instance, if any.
    RConnection *Connection() 
    {
    	return appConnection_;
    }
    
    // Convert TInetAddr to pj_sockaddr_in
    static inline void Addr2pj(const TInetAddr & sym_addr,
			       pj_sockaddr_in &pj_addr)
    {
	pj_bzero(&pj_addr, sizeof(pj_sockaddr_in));
	pj_addr.sin_family = pj_AF_INET();
	pj_addr.sin_addr.s_addr = pj_htonl(sym_addr.Address());
	pj_addr.sin_port = pj_htons((pj_uint16_t) sym_addr.Port());
    }


    // Convert pj_sockaddr_in to TInetAddr
    static inline void pj2Addr(const pj_sockaddr_in &pj_addr,
			       TInetAddr & sym_addr)
    {
	sym_addr.Init(KAfInet);
	sym_addr.SetAddress((TUint32)pj_ntohl(pj_addr.sin_addr.s_addr));
	sym_addr.SetPort(pj_ntohs(pj_addr.sin_port));
    }


    //
    // Resolver helper
    //

    // Get RHostResolver instance
    RHostResolver & GetResolver()
    {
	return appHostResolver_ ? *appHostResolver_ : hostResolver_;
    }


    //
    // Unicode Converter
    //

    // Convert to Unicode
    TInt ConvertToUnicode(TDes16 &aUnicode, const TDesC8 &aForeign);

    // Convert from Unicode
    TInt ConvertFromUnicode(TDes8 &aForeign, const TDesC16 &aUnicode);

    //
    // Get console
    //
    
    // Get console
    CConsoleBase *Console()
    {
	return console_;
    }
    
    //
    // Get select() timeout timer.
    //
    CPjTimeoutTimer *SelectTimeoutTimer()
    {
	return selectTimeoutTimer_;
    }

    //
    // Wait for any active objects to run.
    //
    void WaitForActiveObjects(TInt aPriority = CActive::EPriorityStandard)
    {
	TInt aError;
	User::WaitForAnyRequest();
	CActiveScheduler::RunIfReady(aError, aPriority);
    }

private:
    bool isSocketServInitialized_;
    RSocketServ socketServ_;

    bool isResolverInitialized_;
    RHostResolver hostResolver_;

    CConsoleBase* console_;

    CPjTimeoutTimer *selectTimeoutTimer_;

    // App parameters
    RSocketServ *appSocketServ_;
    RConnection *appConnection_;
    RHostResolver *appHostResolver_;
    
private:
    PjSymbianOS();
};


#endif	/* __OS_SYMBIAN_H__ */

