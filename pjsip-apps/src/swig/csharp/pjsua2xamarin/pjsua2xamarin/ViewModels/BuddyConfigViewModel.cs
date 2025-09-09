using System;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{
    public class BuddyConfigViewModel : BaseViewModel
    {
        private BuddyConfig _buddyConfig = new BuddyConfig();
        public BuddyConfig buddyConfig {
            get { return _buddyConfig; }
            set { SetProperty(ref _buddyConfig, value); }
        }

        public BuddyConfigViewModel()
        {
        }

        public void init(BuddyConfig budCfg) {
            _buddyConfig = budCfg;
        }
    }
}

