using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Xamarin.Forms;
using pjsua2xamarin.pjsua2;
using System.Threading;
using Xamarin.Forms.PlatformConfiguration;

namespace pjsua2xamarin
{
    public partial class BuddyPage : ContentPage, MyAppObserver
    {
        public BuddyViewModel buddyViewModel { get; set; }
        public static MyApp myApp = null;

        public void notifyRegState(int code, String reason, long expiration)
        {

        }
        public void notifyIncomingCall(MyCall call)
        {
            CallOpParam prm = new CallOpParam();

            if (MyApp.currentCall != null) {
                call.Dispose();
                return;
            }

            prm.statusCode = (pjsip_status_code.PJSIP_SC_RINGING);
            try {
                call.answer(prm);
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }
            MyApp.currentCall = call;

            Device.BeginInvokeOnMainThread(() => {
                Navigation.PushAsync(new CallPage());
            });   
        }

        public void notifyCallState(MyCall call)
        {
            if (MyApp.currentCall == null || call.getId() != MyApp.currentCall.getId())
                return;

            CallInfo ci = null;
            try {
                ci = call.getInfo();
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }

            if (ci == null)
                return;

            MessagingCenter.Send(this, "UpdateCallState", ci);

            if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {

                ThreadPool.QueueUserWorkItem(deleteCall);
            }
        }

        static void deleteCall(Object stateInfo)
        {
            MyApp.currentCall.Dispose();
            MyApp.currentCall = null;
        }

        public void notifyCallMediaState(MyCall call)
        {
            if (MyApp.currentCall == null || call.getId() != MyApp.currentCall.getId())
                return;

            CallInfo ci = null;

            try {
                ci = call.getInfo();
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }

            if (ci == null)
                return;

            MessagingCenter.Send(this, "UpdateMediaCallState", ci);
        }
        public void notifyBuddyState(MyBuddy buddy)
        {

        }

        public void notifyChangeNetwork()
        {

        }

        public void notifyWriteLog(int level, String sender, String msg)
        {

        }

        public BuddyPage()
        {
            InitializeComponent();
            try
            {
                myApp = new MyApp();
                String config_path = Environment.GetFolderPath(
                                          Environment.SpecialFolder.LocalApplicationData);
                myApp.init(this, config_path);
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            }

            BindingContext = buddyViewModel = new BuddyViewModel();

            MessagingCenter.Subscribe<AccountConfigPage, MyAccountConfigModel>
                                  (this, "SaveAccountConfig",  (obj, config) =>
            {
                var myCfg = config as MyAccountConfigModel;
                AccountConfig accCfg = MyApp.myAccCfg.accCfg;
                accCfg.idUri = myCfg.idUri;
                accCfg.regConfig.registrarUri = myCfg.registrarUri;
                accCfg.sipConfig.proxies.Clear();
                if (myCfg.proxy != "") {
                    accCfg.sipConfig.proxies.Add(myCfg.proxy);
                }
                accCfg.sipConfig.authCreds.Clear();
                if (myCfg.username != "" || myCfg.password != "") {
                    AuthCredInfo credInfo = new AuthCredInfo();
                    credInfo.username = myCfg.username;
                    credInfo.data = myCfg.password;
                    accCfg.sipConfig.authCreds.Add(credInfo);
                }

                try {
                    MyApp.account.modify(accCfg);
                } catch (Exception e) {
                    Console.WriteLine(e.Message);
                }
            });

            MessagingCenter.Subscribe<BuddyConfigPage, BuddyConfig>
                                  (this, "AddBuddy", (obj, config) =>
            {
                var budCfg = config as BuddyConfig;

                try {
                    MyApp.account.addBuddy(budCfg);
                } catch (Exception e) {
                    Console.WriteLine(e.Message);
                }
                buddyViewModel.LoadBuddiesCommand.Execute(null);
            });

            MessagingCenter.Subscribe<BuddyConfigPage, BuddyConfig>
                                    (this, "EditBuddy", (obj, config) =>
            {
                var budCfg = config as BuddyConfig;

                if (buddyViewModel.SelectedBuddy != null) {
                    MyApp.account.delBuddy(buddyViewModel.SelectedBuddy);
                    try {
                        MyApp.account.addBuddy(budCfg);
                    } catch (Exception e) {
                        Console.WriteLine(e.Message);
                    }
                    buddyViewModel.LoadBuddiesCommand.Execute(null);
                }
            });

            MessagingCenter.Subscribe<BuddyConfigPage, BuddyConfig>
                                     (this, "EditBuddy", (obj, config) =>
            {
                var budCfg = config as BuddyConfig;

                if (buddyViewModel.SelectedBuddy != null) {
                    MyApp.account.delBuddy(buddyViewModel.SelectedBuddy);
                    try {
                        MyApp.account.addBuddy(budCfg);
                    } catch (Exception e) {
                        Console.WriteLine(e.Message);
                    }
                    buddyViewModel.LoadBuddiesCommand.Execute(null);
                }
            });
        }

        async void Settings_Clicked(object sender, EventArgs e)
        {
            await Navigation.PushAsync(new AccountConfigPage(MyApp.myAccCfg));
        }

        void Quit_Clicked(object sender, EventArgs e)
        {
            myApp.deinit();
            System.Diagnostics.Process.GetCurrentProcess().Kill();
        }

        void Call_Clicked(object sender, EventArgs e)
        {
            if (buddyViewModel.SelectedBuddy != null) {
                MyCall call = new MyCall(MyApp.account, -1);
                CallOpParam prm = new CallOpParam(true);

                try {
                    call.makeCall(buddyViewModel.SelectedBuddy.cfg.uri, prm);
                } catch (Exception ex) {
                    Console.WriteLine(ex.Message);
                    call.Dispose();
                    return;
                }
                MyApp.currentCall = call;
                Navigation.PushAsync(new CallPage());
            }
        }

        async void Add_Clicked(object sender, EventArgs e)
        {
            await Navigation.PushAsync(new BuddyConfigPage(true, null));
        }

        async void Edit_Clicked(object sender, EventArgs e)
        {
            if (buddyViewModel.SelectedBuddy != null)
                await Navigation.PushAsync(new BuddyConfigPage(false, buddyViewModel));
        }

        void Delete_Clicked(object sender, EventArgs e)
        {
            if (buddyViewModel.SelectedBuddy != null) {
                MyApp.account.delBuddy(buddyViewModel.SelectedBuddy);
                BuddiesListView.SelectedItem = null;
                buddyViewModel.LoadBuddiesCommand.Execute(null);
            }
        }

        protected override void OnAppearing()
        {
            base.OnAppearing();

            if (buddyViewModel.Buddies.Count == 0)
                buddyViewModel.LoadBuddiesCommand.Execute(null);
        }
    }
}

