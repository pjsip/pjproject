using CommunityToolkit.Mvvm.Messaging;
using libpjsua2.maui;
using pjsua2maui.Messages;
using pjsua2maui.Models;
using pjsua2maui.ViewModels;

namespace pjsua2maui.Views
{
    public partial class AccountConfigPage : ContentPage
    {
        AccountConfigViewModel viewModel;

        public AccountConfigPage(SoftAccountConfig accCfg)
        {
            InitializeComponent();

            viewModel = new AccountConfigViewModel();
            viewModel.init(accCfg);
            BindingContext = viewModel;
        }

        async void Ok_Clicked(object sender, EventArgs e)
        {
            WeakReferenceMessenger.Default.Send(new SaveAccountConfigMessage(viewModel.accCfg));
            await Navigation.PopAsync();
        }

        async void Cancel_Clicked(object sender, EventArgs e)
        {
            await Navigation.PopAsync();
        }
    }
}

