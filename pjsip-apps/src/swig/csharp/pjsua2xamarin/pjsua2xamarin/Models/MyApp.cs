using System;
using System.Collections.Generic;
using System.IO;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{
    /* Interface to separate UI & engine a bit better */
    public interface MyAppObserver
    {
        void notifyRegState(int code, String reason, long expiration);
        void notifyIncomingCall(MyCall call);
        void notifyCallState(MyCall call);
        void notifyCallMediaState(MyCall call);
        void notifyBuddyState(MyBuddy buddy);
        void notifyChangeNetwork();
    }

    public class MyCall : Call
    {
        public VideoWindow vidWin;
        public VideoPreview vidPrev;

        public MyCall(MyAccount acc, int call_id) : base(acc, call_id)
        {
            vidWin = null;
        }

        override public void onCallState(OnCallStateParam prm)
        {
            try {
                CallInfo ci = getInfo();
                if (ci.state ==
                    pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
                {
                     MyApp.ep.utilLogWrite(3, "MyCall", this.dump(true, ""));
                }
            } catch (Exception ex) {
                Console.WriteLine("Error : " + ex.Message);
            }

            // Should not delete this call instance (self) in this context,
            // so the observer should manage this call instance deletion
            // out of this callback context.
            MyApp.observer.notifyCallState(this);
        }

        override public void onCallMediaState(OnCallMediaStateParam prm)
        {
            CallInfo ci;
            try {
                ci = getInfo();
            } catch (Exception) {
                return;
            }

            CallMediaInfoVector cmiv = ci.media;

            for (int i = 0; i < cmiv.Count; i++) {
                CallMediaInfo cmi = cmiv[i];
                if (cmi.type == pjmedia_type.PJMEDIA_TYPE_AUDIO &&
                    (cmi.status ==
                            pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE ||
                     cmi.status ==
                            pjsua_call_media_status.PJSUA_CALL_MEDIA_REMOTE_HOLD)) {
                    // connect ports
                    try {
                        AudDevManager audMgr = MyApp.ep.audDevManager();
                        AudioMedia am = getAudioMedia(i);
                        audMgr.getCaptureDevMedia().startTransmit(am);
                        am.startTransmit(audMgr.getPlaybackDevMedia());
                    } catch (Exception e) {
                        Console.WriteLine("Failed connecting media ports" +
                                          e.Message);
                        continue;
                    }
                } else if (cmi.type == pjmedia_type.PJMEDIA_TYPE_VIDEO &&
                           cmi.status ==
                                pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE &&
                           cmi.videoIncomingWindowId !=
                                (int)pjsua2.pjsua_invalid_id_const_.PJSUA_INVALID_ID)
                {
                    vidWin = new VideoWindow(cmi.videoIncomingWindowId);
                    vidPrev = new VideoPreview(cmi.videoCapDev);
                }
            }

            MyApp.observer.notifyCallMediaState(this);
        }
    }

    public class MyBuddy : Buddy
    {
        public BuddyConfig cfg { get; set; }

        public MyBuddy(BuddyConfig config)
        {
            cfg = config;
        }

        public String getStatusText()
        {
            BuddyInfo bi;

            try {
                bi = getInfo();
            } catch (Exception) {
                return "?";
            }

            String status = "";
            if (bi.subState == pjsip_evsub_state.PJSIP_EVSUB_STATE_ACTIVE) {
                if (bi.presStatus.status ==
                    pjsua_buddy_status.PJSUA_BUDDY_STATUS_ONLINE) {
                    status = bi.presStatus.statusText;
                    if (status == null || status.Length == 0) {
                        status = "Online";
                    }
                } else if (bi.presStatus.status ==
                           pjsua_buddy_status.PJSUA_BUDDY_STATUS_OFFLINE) {
                    status = "Offline";
                } else {
                    status = "Unknown";
                }
            }
            return status;
        }

        override public void onBuddyState()
        {
            MyApp.observer.notifyBuddyState(this);
        }
    }

    public class MyAccountConfig
    {
        public AccountConfig accCfg;
        public List<BuddyConfig> buddyCfgs = new List<BuddyConfig>();

        public MyAccountConfig() {
            accCfg = new AccountConfig();
        }

        public void readObject(ContainerNode accNode)
        {
            try {
                accCfg.readObject(accNode);
                ContainerNode buddiesNode = accNode.readArray("Buddies");
                buddyCfgs.Clear();
                while (buddiesNode.hasUnread()) {
                    BuddyConfig budCfg = new BuddyConfig();
                    budCfg.readObject(buddiesNode);
                    buddyCfgs.Add(budCfg);
                }
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }
        }

        public void writeObject(ContainerNode accNode)
        {
            try {
                accCfg.writeObject(accNode);
                ContainerNode buddiesNode = accNode.writeNewArray("Buddies");
                foreach (BuddyConfig budCfg in buddyCfgs) {
                    budCfg.writeObject(buddiesNode);
                }
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }
        }
    }

    public class MyAccount : Account
    {
        public List<MyBuddy> buddyList = new List<MyBuddy>();
        public AccountConfig cfg;

        ~MyAccount()
        {
            Console.WriteLine("*** Account is being deleted");
        }

        public MyAccount(AccountConfig config)
        {
            cfg = config;
        }

        public MyBuddy addBuddy(BuddyConfig bud_cfg)
        {
            /* Create Buddy */
            MyBuddy bud = new MyBuddy(bud_cfg);
            try {
                bud.create(this, bud_cfg);
            } catch (Exception) {
                bud.Dispose();
            }

            if (bud != null) {
                buddyList.Add(bud);
                if (bud_cfg.subscribe)
                    try {
                        bud.subscribePresence(true);
                    } catch (Exception) { }
            }

            return bud;
        }

        public void delBuddy(MyBuddy buddy)
        {
            buddyList.Remove(buddy);
            buddy.Dispose();
        }

        override public void onRegState(OnRegStateParam prm)
        {
            AccountInfo ai = getInfo();
            Console.WriteLine("***" + (ai.regIsActive ? "" : "Un") +
                              "Register: code=" + prm.code);

            MyApp.observer.notifyRegState((int)prm.code, prm.reason, prm.expiration);
        }

        override public void onIncomingCall(OnIncomingCallParam prm)
        {
            Console.WriteLine("======== Incoming call ======== ");
            MyCall call = new MyCall(this, prm.callId);
            MyApp.observer.notifyIncomingCall(call);
        }

        override public void onInstantMessage(OnInstantMessageParam prm)
        {
            Console.WriteLine("======== Incoming pager ======== ");
            Console.WriteLine("From     : " + prm.fromUri);
            Console.WriteLine("To       : " + prm.toUri);
            Console.WriteLine("Contact  : " + prm.contactUri);
            Console.WriteLine("Mimetype : " + prm.contentType);
            Console.WriteLine("Body     : " + prm.msgBody);
        }
    }

    public class MyLogWriter : LogWriter
    {
        override public void write(LogEntry entry)
        {
            Console.WriteLine(entry.msg);
        }
    }

    public class MyApp
    {
        public static Endpoint ep = new Endpoint();
        public static MyCall currentCall = null;
        public static MyAccount account = null;
        public static MyAccountConfig myAccCfg;
        public static MyAppObserver observer;

        private static MyLogWriter logWriter = new MyLogWriter();
        private EpConfig epConfig = new EpConfig();
        private TransportConfig sipTpConfig = new TransportConfig();
        private String appDir;

        private const String configName = "pjsua2.json";
        private const int SIP_PORT = 6000;
        private const int LOG_LEVEL = 5;

        public MyApp()
        {

        }

        public void init(MyAppObserver obs, String app_dir)
        {
            observer = obs;
            appDir = app_dir;

            /* Create endpoint */
            try {
                ep.libCreate();
            } catch (Exception e) {
                Console.WriteLine("Error init : " + e.Message);
                return;
            }

            myAccCfg = new MyAccountConfig();
            /* Load config */
            String configPath = appDir + "/" + configName;
            if (File.Exists(configPath)) {
                loadConfig(configPath);
            } else {
                /* Set 'default' values */
                sipTpConfig.port = SIP_PORT;
            }

            /* Override log level setting */
            epConfig.logConfig.level = LOG_LEVEL;
            epConfig.logConfig.consoleLevel = LOG_LEVEL;

            /* Set log config. */
            LogConfig log_cfg = epConfig.logConfig;
            logWriter = new MyLogWriter();
            log_cfg.writer = logWriter;
            log_cfg.decor += (uint)pj_log_decoration.PJ_LOG_HAS_NEWLINE;

            UaConfig ua_cfg = epConfig.uaConfig;
            ua_cfg.userAgent = "Pjsua2 Xamarin " + ep.libVersion().full;

            /* Init endpoint */
            try {
                ep.libInit(epConfig);
            } catch (Exception) {
                return;
            }

            /* Create transports. */
            try {
                ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP,
                                   sipTpConfig);
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }

            try {
                ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TCP,
                                   sipTpConfig);
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }

            try {
                sipTpConfig.port = SIP_PORT + 1;
                ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TLS,
                                   sipTpConfig);
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }

            /* Set SIP port back to default for JSON saved config */
            sipTpConfig.port = SIP_PORT;
            AccountConfig accountConfig = myAccCfg.accCfg;
            if (accountConfig.idUri == "") {
                accountConfig.idUri = "sip:localhost";
            }
            accountConfig.natConfig.iceEnabled = true;
            accountConfig.videoConfig.autoTransmitOutgoing = true;
            accountConfig.videoConfig.autoShowIncoming = true;
            accountConfig.mediaConfig.srtpUse = pjmedia_srtp_use.PJMEDIA_SRTP_OPTIONAL;
            accountConfig.mediaConfig.srtpSecureSignaling = 0;

            account = new MyAccount(accountConfig);
            try {
                account.create(accountConfig);

                /* Add Buddies */
                foreach (BuddyConfig budCfg in myAccCfg.buddyCfgs) {
                    account.addBuddy(budCfg);
                }
            } catch (Exception e) {
                Console.WriteLine(e.Message);
                account = null;
            }

            /* Start. */
            try {
                ep.libStart();
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }
        }

        public void deinit()
        {
            String configPath = appDir + "/" + configName;
            saveConfig(configPath);

            /* Shutdown pjsua. Note that Endpoint destructor will also invoke
             * libDestroy(), so this will be a test of double libDestroy().
             */
            try {
                ep.libDestroy();
            } catch (Exception) { }

            /* Force delete Endpoint here, to avoid deletion from a non-
             * registered thread (by GC?). 
             */
            ep.Dispose();
            ep = null;
        }

        private void loadConfig(String filename)
        {
            try {
                JsonDocument json = new JsonDocument();
                /* Load file */
                json.loadFile(filename);
                ContainerNode root = json.getRootContainer();

                /* Read endpoint config */
                epConfig.readObject(root);

                /* Read transport config */
                ContainerNode tpNode = root.readContainer("SipTransport");
                sipTpConfig.readObject(tpNode);

                /* Read account config */
                ContainerNode accNode = root.readContainer("MyAccountConfig");
                myAccCfg.readObject(accNode);

                /* Force delete json now */
                json.Dispose();
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }
        }

        private void buildAccConfigs()
        {
            MyAccountConfig tmpAccCfg = new MyAccountConfig();
            tmpAccCfg.accCfg = account.cfg;

            tmpAccCfg.buddyCfgs.Clear();
            for (int j = 0; j < account.buddyList.Count; j++) {
                MyBuddy bud = (MyBuddy)account.buddyList[j];
                tmpAccCfg.buddyCfgs.Add(bud.cfg);
            }

            myAccCfg = tmpAccCfg;
        }

        private void saveConfig(String filename)
        {
            try {
                JsonDocument json = new JsonDocument();

                /* Write endpoint config */
                json.writeObject(epConfig);

                /* Write transport config */
                ContainerNode tpNode = json.writeNewContainer("SipTransport");
                sipTpConfig.writeObject(tpNode);

                /* Write account configs */
                buildAccConfigs();
                ContainerNode accNode = json.writeNewContainer("MyAccountConfig");
                myAccCfg.writeObject(accNode);

                /* Save file */
                json.saveFile(filename);

                /* Force delete json now */
                json.Dispose();
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }
        }
    }
}

