using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Phone.Controls;
using Microsoft.Phone.Shell;
using PjsuaCLI.BackEnd;

namespace PjsuaCLI.UI
{
    public partial class MainPage : PhoneApplicationPage, IPjsuaCallback
    {
        // Constructor
        public MainPage()
        {
            InitializeComponent();

            // Sample code to localize the ApplicationBar
            //BuildLocalizedApplicationBar();
        }

        public virtual void OnPjsuaStarted(string outStr)
        {
            setStatus(outStr);
        }

        public virtual void OnPjsuaStopped(int restart)
        {
            setStatus("Telnet stopped, restart(" + (restart == 0 ? "No" : "Yes") + ")");
        }

        public void setStatus(string status)
        {
            txtStatus.Text = status;
        }

        // Sample code for building a localized ApplicationBar
        //private void BuildLocalizedApplicationBar()
        //{
        //    // Set the page's ApplicationBar to a new instance of ApplicationBar.
        //    ApplicationBar = new ApplicationBar();

        //    // Create a new button and set the text value to the localized string from AppResources.
        //    ApplicationBarIconButton appBarButton = new ApplicationBarIconButton(new Uri("/Assets/AppBar/appbar.add.rest.png", UriKind.Relative));
        //    appBarButton.Text = AppResources.AppBarButtonText;
        //    ApplicationBar.Buttons.Add(appBarButton);

        //    // Create a new menu item with the localized string from AppResources.
        //    ApplicationBarMenuItem appBarMenuItem = new ApplicationBarMenuItem(AppResources.AppBarMenuItemText);
        //    ApplicationBar.MenuItems.Add(appBarMenuItem);
        //}

        protected override void OnNavigatedTo(System.Windows.Navigation.NavigationEventArgs nee)
        {
            base.OnNavigatedTo(nee);

            Globals.Instance.PjsuaCallback.SetCallback(this);
            Globals.Instance.pjsuaStart();
        }

        protected override void OnNavigatedFrom(System.Windows.Navigation.NavigationEventArgs nee)
        {
            base.OnNavigatedFrom(nee);
            Globals.Instance.pjsuaDestroy();
        }
    }
}