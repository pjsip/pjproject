using pjsua2maui.Models;
using pjsua2maui.ViewModels;
using libpjsua2.maui;
using CommunityToolkit.Mvvm.Messaging;
using pjsua2maui.Messages;

namespace pjsua2maui.Views;

public partial class BuddyPage : ContentPage, ISoftObserver
{
    public BuddyViewModel buddyViewModel { get; set; }
    public static SoftApp myApp = null;

    public void notifyRegState(int code, String reason, long expiration)
    {

    }
    public void notifyIncomingCall(SoftCall call)
    {
        CallOpParam prm = new CallOpParam();

        if (SoftApp.currentCall != null)
        {
            call.Dispose();
            return;
        }

        prm.statusCode = (pjsip_status_code.PJSIP_SC_RINGING);
        try
        {
            call.answer(prm);
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }
        SoftApp.currentCall = call;
        this.Dispatcher.DispatchAsync(async () => { await Navigation.PushAsync(new CallPage()); });
    }

    public void notifyCallState(SoftCall call)
    {
        if (SoftApp.currentCall == null || call.getId() != SoftApp.currentCall.getId())
            return;

        CallInfo ci = null;
        try
        {
            ci = call.getInfo();
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }

        if (ci == null)
            return;
        WeakReferenceMessenger.Default.Send(new UpdateCallStateMessage(ci));
        
        if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
        {
            ThreadPool.QueueUserWorkItem(deleteCall);
        }
    }

    static void deleteCall(Object stateInfo)
    {
        SoftApp.currentCall.Dispose();
        SoftApp.currentCall = null;
    }

    public void notifyCallMediaState(SoftCall call)
    {
        if (SoftApp.currentCall == null || call.getId() != SoftApp.currentCall.getId())
            return;

        CallInfo ci = null;

        try
        {
            ci = call.getInfo();
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }

        if (ci == null)
            return;
        WeakReferenceMessenger.Default.Send(new UpdateMediaCallStateMessage(ci));
    }
    public void notifyBuddyState(SoftBuddy buddy)
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
            Dispatcher.Dispatch(() => { myApp = new SoftApp(); });
            String config_path = Environment.GetFolderPath(
                                      Environment.SpecialFolder.LocalApplicationData);
            Dispatcher.Dispatch(() => myApp.init(this, config_path));
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }

        BindingContext = buddyViewModel = new BuddyViewModel();

        WeakReferenceMessenger.Default.Register<SaveAccountConfigMessage>(this, (r, m) =>
        {
            var myCfg = m.Value as SoftAccountConfigModel;
            AccountConfig accCfg = SoftApp.myAccCfg.accCfg;
            accCfg.idUri = myCfg.idUri;
            accCfg.regConfig.registrarUri = myCfg.registrarUri;
            accCfg.sipConfig.proxies.Clear();
            if (myCfg.proxy != "")
            {
                accCfg.sipConfig.proxies.Add(myCfg.proxy);
            }
            accCfg.sipConfig.authCreds.Clear();
            if (myCfg.username != "" || myCfg.password != "")
            {
                AuthCredInfo credInfo = new AuthCredInfo();
                credInfo.username = myCfg.username;
                credInfo.data = myCfg.password;
                accCfg.sipConfig.authCreds.Add(credInfo);
            }

            try
            {
                SoftApp.account.modify(accCfg);
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
            }
        });

        WeakReferenceMessenger.Default.Register<AddBuddyMessage>(this, (r, m) =>
        {
            var budCfg = m.Value as BuddyConfig;

            try
            {
                SoftApp.account.addBuddy(budCfg);
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
            }
            buddyViewModel.LoadBuddiesCommand.Execute(null);
        });

        WeakReferenceMessenger.Default.Register<EditBuddyMessage>(this, (r, m) =>
        {
            var budCfg = m.Value as BuddyConfig;

            if (buddyViewModel.SelectedBuddy != null)
            {
                SoftApp.account.delBuddy(buddyViewModel.SelectedBuddy);
                try
                {
                    SoftApp.account.addBuddy(budCfg);
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                }
                buddyViewModel.LoadBuddiesCommand.Execute(null);
            }
        });
    }

    async void Settings_Clicked(object sender, EventArgs e)
    {
        await Navigation.PushAsync(new AccountConfigPage(SoftApp.myAccCfg));
    }

    void Quit_Clicked(object sender, EventArgs e)
    {
        myApp.deinit();
    }

    void Call_Clicked(object sender, EventArgs e)
    {
        if (buddyViewModel.SelectedBuddy != null)
        {
            SoftCall call = new SoftCall(SoftApp.account, -1);
            CallOpParam prm = new CallOpParam(true);

            try
            {
                call.makeCall(buddyViewModel.SelectedBuddy.cfg.uri, prm);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                call.Dispose();
                return;
            }
            SoftApp.currentCall = call;
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
        if (buddyViewModel.SelectedBuddy != null)
        {
            SoftApp.account.delBuddy(buddyViewModel.SelectedBuddy);
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

