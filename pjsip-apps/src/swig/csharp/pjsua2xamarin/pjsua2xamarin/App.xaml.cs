using System;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace pjsua2xamarin
{
    public partial class App : Application
    {
        public App ()
        {
            InitializeComponent();

            MainPage = new NavigationPage(new BuddyPage());

        }

        protected override void OnStart ()
        {
        }

        protected override void OnSleep ()
        {
        }

        protected override void OnResume ()
        {
        }
    }
}

