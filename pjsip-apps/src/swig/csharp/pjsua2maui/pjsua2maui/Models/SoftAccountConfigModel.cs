using libpjsua2.maui;

namespace pjsua2maui.Models
{
    public class SoftAccountConfigModel
    {
        public string idUri { get; set; }
        public string registrarUri { get; set; }
        public string proxy { get; set; }
        public string username { get; set; }
        public string password { get; set; }
        private SoftAccountConfig accConfig;

        public SoftAccountConfigModel(SoftAccountConfig inAccConfig)
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

