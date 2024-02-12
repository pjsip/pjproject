using System;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{
    public class MyAccountConfigModel
    {
        public string idUri { get; set; }
        public string registrarUri { get; set; }
        public string proxy { get; set; }
        public string username { get; set; }
        public string password { get; set; }
        private MyAccountConfig accConfig;

        public MyAccountConfigModel(MyAccountConfig inAccConfig)
        {
            accConfig = inAccConfig;
            AccountConfig accCfg = accConfig.accCfg;

            idUri = accCfg.idUri;
            registrarUri = accCfg.regConfig.registrarUri;
            if (accCfg.sipConfig.proxies.Count > 0)
                proxy = accCfg.sipConfig.proxies[0];
            else
                proxy = "";

            if (accCfg.sipConfig.authCreds.Count > 0) {
                username = accCfg.sipConfig.authCreds[0].username;
                password = accCfg.sipConfig.authCreds[0].data;
            } else {
                username = "";
                password = "";
            }
        }
    }
}

