using libpjsua2.maui;

namespace pjsua2maui.Models;

public class SoftApp
{
    public static Endpoint ep = new Endpoint();
    public static SoftCall currentCall = null;
    public static SoftAccount account = null;
    public static SoftAccountConfig myAccCfg;
    public static ISoftObserver observer;

    private static SoftLogWriter logWriter = new SoftLogWriter();
    private EpConfig epConfig = new EpConfig();
    private TransportConfig sipTpConfig = new TransportConfig();
    private String appDir;

    private const String configName = "Softhand.json";
    private const int SIP_PORT = 6000;
    private const int LOG_LEVEL = 5;

    public SoftApp()
    {

    }

    public void init(ISoftObserver obs, String app_dir)
    {
        observer = obs;
        appDir = app_dir;

        /* Create endpoint */
        try
        {
            Dispatcher.GetForCurrentThread().Dispatch(ep.libCreate);
        }
        catch (Exception e)
        {
            Console.WriteLine("Error init : " + e.Message);
            return;
        }

        myAccCfg = new SoftAccountConfig();
        /* Load config */
        String configPath = appDir + "/" + configName;
        if (File.Exists(configPath))
        {
            Dispatcher.GetForCurrentThread().Dispatch(() => loadConfig(configPath));
        }
        else
        {
            /* Set 'default' values */
            sipTpConfig.port = SIP_PORT;
        }

        /* Override log level setting */
        epConfig.logConfig.level = LOG_LEVEL;
        epConfig.logConfig.consoleLevel = LOG_LEVEL;

        /* Set log config. */
        LogConfig log_cfg = epConfig.logConfig;
        logWriter = new SoftLogWriter();
        log_cfg.writer = logWriter;
        log_cfg.decor += (uint)pj_log_decoration.PJ_LOG_HAS_NEWLINE;

        UaConfig ua_cfg = epConfig.uaConfig;
        ua_cfg.userAgent = "Softhand " + ep.libVersion().full;

        /* Init endpoint */
        try
        {
            Dispatcher.GetForCurrentThread().Dispatch(() => ep.libInit(epConfig));
        }
        catch (Exception)
        {
            return;
        }

        /* Create transports. */
        try
        {
            Dispatcher.GetForCurrentThread().Dispatch(() => ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP,
                               sipTpConfig));
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }

        try
        {
            Dispatcher.GetForCurrentThread().Dispatch(() => ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TCP,
                               sipTpConfig));
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }

        //try
        //{
        //    sipTpConfig.port = SIP_PORT + 1;
        //    Dispatcher.GetForCurrentThread().Dispatch(() => ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TLS,
        //                       sipTpConfig));
        //}
        //catch (Exception e)
        //{
        //    Console.WriteLine(e.Message);
        //}

        /* Set SIP port back to default for JSON saved config */
        sipTpConfig.port = SIP_PORT;
        AccountConfig accountConfig = myAccCfg.accCfg;
        if (accountConfig.idUri == "")
        {
            accountConfig.idUri = "sip:localhost";
        }
        accountConfig.natConfig.iceEnabled = true;
        accountConfig.videoConfig.autoTransmitOutgoing = true;
        accountConfig.videoConfig.autoShowIncoming = true;
        accountConfig.mediaConfig.srtpUse = pjmedia_srtp_use.PJMEDIA_SRTP_OPTIONAL;
        accountConfig.mediaConfig.srtpSecureSignaling = 0;

        account = new SoftAccount(accountConfig);
        try
        {
            Dispatcher.GetForCurrentThread().Dispatch(() => {
                account.create(accountConfig);

                /* Add Buddies */
                foreach (BuddyConfig budCfg in myAccCfg.buddyCfgs)
                {
                    account.addBuddy(budCfg);
                }
            });
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
            account = null;
        }

        /* Start. */
        try
        {
            Dispatcher.GetForCurrentThread().Dispatch(ep.libStart);
        }
        catch (Exception e)
        {
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
        try
        {
            ep.libDestroy();
        }
        catch (Exception) { }

        /* Force delete Endpoint here, to avoid deletion from a non-
         * registered thread (by GC?). 
         */
        ep.Dispose();
        ep = null;
    }

    private void loadConfig(String filename)
    {
        JsonDocument json = new JsonDocument();
        try
        {
            /* Load file */
            json.loadFile(filename);
            ContainerNode root = json.getRootContainer();

            /* Read endpoint config */
            epConfig.readObject(root);

            /* Read transport config */
            ContainerNode tpNode = root.readContainer("SipTransport");
            sipTpConfig.readObject(tpNode);

            /* Read account config */
            ContainerNode accNode = root.readContainer("SoftAccountConfig");
            myAccCfg.readObject(accNode);

            /* Force delete json now */
            json.Dispose();
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }
    }

    private void buildAccConfigs()
    {
        SoftAccountConfig tmpAccCfg = new SoftAccountConfig();
        tmpAccCfg.accCfg = account.cfg;

        tmpAccCfg.buddyCfgs.Clear();
        for (int j = 0; j < account.buddyList.Count; j++)
        {
            SoftBuddy bud = (SoftBuddy)account.buddyList[j];
            tmpAccCfg.buddyCfgs.Add(bud.cfg);
        }

        myAccCfg = tmpAccCfg;
    }

    private void saveConfig(String filename)
    {
        try
        {
            JsonDocument json = new JsonDocument();

            /* Write endpoint config */
            json.writeObject(epConfig);

            /* Write transport config */
            ContainerNode tpNode = json.writeNewContainer("SipTransport");
            sipTpConfig.writeObject(tpNode);

            /* Write account configs */
            buildAccConfigs();
            ContainerNode accNode = json.writeNewContainer("SoftAccountConfig");
            myAccCfg.writeObject(accNode);

            /* Save file */
            json.saveFile(filename);

            /* Force delete json now */
            json.Dispose();
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }
    }
}

