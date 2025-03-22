using CommunityToolkit.Mvvm.Messaging;
using pjsua2maui.Messages;
using pjsua2maui.ViewModels;

namespace pjsua2maui.Views
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
            if (isAdd)
            {
                WeakReferenceMessenger.Default.Send(new AddBuddyMessage(budCfgViewModel.buddyConfig));
            }
            else
            {
                WeakReferenceMessenger.Default.Send(new EditBuddyMessage(budCfgViewModel.buddyConfig));
            }
            await Navigation.PopAsync();
        }

        async public void Cancel_Clicked(object sender, EventArgs e)
        {
            await Navigation.PopAsync();
        }
    }
}

