using System;
using System.Collections.Generic;

using Xamarin.Forms;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{
    public partial class AccountConfigPage : ContentPage
    {
        AccountConfigViewModel viewModel;

        public AccountConfigPage(MyAccountConfig accCfg)
        {
            InitializeComponent();

            viewModel = new AccountConfigViewModel();
            viewModel.init(accCfg);
            BindingContext = viewModel;
        }

        async void Ok_Clicked(object sender, EventArgs e)
        {
            MessagingCenter.Send(this, "SaveAccountConfig", viewModel.accCfg);
            await Navigation.PopAsync();
        }

        async void Cancel_Clicked(object sender, EventArgs e)
        {
            await Navigation.PopAsync();
        }
    }
}

