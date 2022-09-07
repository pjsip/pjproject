/*
 * Copyright (C) 2022 Teluu Inc. (http://www.teluu.com)
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
using System.Diagnostics;
using System.Threading;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{
    public class MyAccount : Account
    {
    	~MyAccount()
    	{
            Debug.WriteLine("*** MyAccount is being deleted");
    	}

        override public void onRegState(OnRegStateParam prm)
        {
	        AccountInfo ai = getInfo();
	        Debug.WriteLine("***" + (ai.regIsActive? "": "Un") +
	    		          "Register: code=" + prm.code);
        }
    }

    public class MyLogWriter : LogWriter
    {
        override public void write(LogEntry entry)
        {
            Debug.WriteLine(entry.msg);
        }

        ~MyLogWriter()
        {
            Debug.WriteLine("*** MyLogWriter is being deleted");

            // A bit hack here.
            // Detach the native instance as the library deletes it automatically upon libDestroy().
            swigCMemOwn = false;
        }
    }

    public class PjSample
    {
        public static Endpoint ep = new Endpoint();
        public static MyLogWriter writer = new MyLogWriter();
        public static MyAccount acc = new MyAccount();

        /* Preview of Colorbar */
        private static VideoPreview vp = new VideoPreview(2);

        public PjSample()
        {
        }

        private void checkThread(string name)
        {
            if (!ep.libIsThreadRegistered())
                ep.libRegisterThread(name);
        }

        public void start()
        {
            try
            {
                ep.libCreate();

                // Init library
                EpConfig epConfig = new EpConfig();
                epConfig.logConfig.writer = writer;
                epConfig.logConfig.decor &= ~(uint)pj_log_decoration.PJ_LOG_HAS_NEWLINE;
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
                Debug.WriteLine("*** PJSUA2 STARTED ***");

                // Add account
                AccountConfig accCfg = new AccountConfig();
                accCfg.idUri = "sip:test1@pjsip.org";
                accCfg.regConfig.registrarUri = "sip:sip.pjsip.org";
                accCfg.sipConfig.authCreds.Add(
                    new AuthCredInfo("digest", "*", "test1", 0, "test1"));
                acc.create(accCfg);
                Debug.WriteLine("*** ACC CREATED ***");

            }
            catch (Exception err)
            {
                Debug.WriteLine("Exception: " + err.Message);
            }
        }

        public void stop()
        {
            try
            {
                checkThread("pjsua2.stop");

                acc.shutdown();
                // Explicitly delete account when unused
                acc.Dispose();

            }
            catch (Exception err)
            {
                Debug.WriteLine("Exception: " + err.Message);
            }

            new Thread(() =>
                {
                    try
                    {
                        checkThread("pjsua2.stop.2");

                        Debug.WriteLine("*** DESTROYING PJSUA2 ***");
                        ep.libDestroy();
                        ep.Dispose();
                        Debug.WriteLine("*** PJSUA2 DESTROYED ***");
                    }
                    catch (Exception err)
                    {
                        Debug.WriteLine("Exception: " + err.Message);
                    }
                }).Start();
        }

        public void startPreview(IntPtr hwnd)
        {
            try
            {
                VideoPreviewOpParam param = new VideoPreviewOpParam();
                param.window.handle.setWindow(hwnd.ToInt64());

                // Video render operation needs to be invoked from non-main-thread.
                new Thread(() =>
                {
                    try
                    {
                        checkThread("pjsua2.startPreview");

                        vp.start(param);
                        Debug.WriteLine("Preview started");
                    }
                    catch (Exception err)
                    {
                        Debug.WriteLine("Exception: " + err.Message);
                    }
                }).Start(); 

            }
            catch (Exception err)
            {
                Debug.WriteLine("Exception: " + err.Message);
            }
        }

        public void updatePreviewWindow(IntPtr hwnd)
        {
            try
            {
                // Video render operation needs to be invoked from non-main-thread.
                new Thread(() =>
                {
                    try
                    {
                        checkThread("pjsua2.updatePreviewWindow");

                        VideoWindowHandle handle = new VideoWindowHandle();
                        handle.handle.setWindow(hwnd.ToInt64());

                        VideoWindow window = vp.getVideoWindow();
                        window.setWindow(handle);

                        Debug.WriteLine("Preview window updated");
                    }
                    catch (Exception err)
                    {
                        Debug.WriteLine("Exception: " + err.Message);
                    }
                }).Start();

            }
            catch (Exception err)
            {
                Debug.WriteLine("Exception: " + err.Message);
            }
        }

        public void stopPreview()
        {
            try
            {
                checkThread("pjsua2.stopPreview");

                vp.stop();
                Debug.WriteLine("Preview stopped");
            }
            catch (Exception err)
            {
                Debug.WriteLine("Exception: " + err.Message);
            }
        }
    }

}
