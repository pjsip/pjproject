using System;
using pjsua2xamarin.pjsua2;
using Xamarin.Forms;

namespace pjsua2xamarin
{
    public class AccountConfigViewModel : BaseViewModel
    {
        public MyAccountConfigModel accCfg { get; set; }

        public AccountConfigViewModel()
        {
            
        }

        public void init(MyAccountConfig inAccCfg = null)
        {
            accCfg = new MyAccountConfigModel(inAccCfg);
        }
    }
}

