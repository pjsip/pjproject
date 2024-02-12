using System;
using System.Collections.Generic;
using Xamarin.Forms;
using pjsua2xamarin.pjsua2;

namespace pjsua2xamarin
{    
    public partial class BuddyConfigPage : ContentPage
    {
        private bool isAdd;
        private BuddyConfigViewModel budCfgViewModel;

        public BuddyConfigPage(bool inIsAdd, BuddyViewModel buddyViewModel = null)
        {
            InitializeComponent();

            isAdd = inIsAdd;
            if (isAdd)
                this.Title = "Add Buddy";
            else
                this.Title = "Edit Buddy";

            budCfgViewModel = new BuddyConfigViewModel();
            if (buddyViewModel != null)
                budCfgViewModel.init(buddyViewModel.SelectedBuddy.cfg);

            BindingContext = budCfgViewModel;
        }

        async public void Ok_Clicked(object sender, EventArgs e)
        {
            MessagingCenter.Send(this, isAdd?"AddBuddy":"EditBuddy",
                                 budCfgViewModel.buddyConfig);

            await Navigation.PopAsync();
        }

        async public void Cancel_Clicked(object sender, EventArgs e)
        {
            await Navigation.PopAsync();
        }
    }
}

