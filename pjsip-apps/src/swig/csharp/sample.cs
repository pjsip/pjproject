/* $Id $ */
/*
 * Copyright (C) 2018-2018 Teluu Inc. (http://www.teluu.com)
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

using System;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{
    public class MyAccount : Account
    {
    	~MyAccount()
    	{
            Console.WriteLine("*** Account is being deleted");
    	}

        override public void onRegState(OnRegStateParam prm)
        {
	    AccountInfo ai = getInfo();
	    Console.WriteLine("***" + (ai.regIsActive? "": "Un") +
	    		      "Register: code=" + prm.code);
        }
    }

    public class MyLogWriter : LogWriter
    {
        override public void write(LogEntry entry)
        {
            Console.WriteLine(entry.msg);
        }
    }

    public class sample
    {
        public static Endpoint ep = new Endpoint();
        public static MyLogWriter writer = new MyLogWriter();

        public sample()
        {
        }

        public void test1()
        {
            try {
                ep.libCreate();

                // Init library
                EpConfig epConfig = new EpConfig();
                epConfig.logConfig.writer = writer;
                ep.libInit(epConfig);

                // Create transport
                TransportConfig tcfg = new TransportConfig();
                tcfg.port = 5080;
                ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP,
                		   tcfg);
                ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TCP,
                		   tcfg);

                // Start library
                ep.libStart();
                Console.WriteLine("*** PJSUA2 STARTED ***");

                // Add account
                AccountConfig accCfg = new AccountConfig();
    		accCfg.idUri = "sip:test1@pjsip.org";
    		accCfg.regConfig.registrarUri = "sip:sip.pjsip.org";
    		accCfg.sipConfig.authCreds.Add(
    		    new AuthCredInfo("digest", "*", "test1", 0, "test1") );
                MyAccount acc = new MyAccount();
                acc.create(accCfg);

                Console.WriteLine("*** DESTROYING PJSUA2 ***");
                // Explicitly delete account when unused
                acc.Dispose();
                ep.libDestroy();
            } catch (Exception err) {
                Console.WriteLine("Exception: " + err.Message);
            }
        }
    }
}
