using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using PjsuaCLI.BackEnd;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace PjsuaCLI.UI
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public partial class MainPage : Page, IPjsuaCallback
    {
        public MainPage()
        {
            this.InitializeComponent();
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

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);

            Globals.Instance.PjsuaCallback.SetCallback(this);
            Globals.Instance.pjsuaStart();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            base.OnNavigatedFrom(e);
            Globals.Instance.pjsuaDestroy();
        }
    }
}
