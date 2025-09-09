using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Threading.Tasks;
using pjsua2xamarin.pjsua2;
using Xamarin.Essentials;
using Xamarin.Forms;

namespace pjsua2xamarin
{
    public class BuddyViewModel : BaseViewModel
    {
        private MyBuddy _selectedBuddy;
        public MyBuddy SelectedBuddy {
            get { return _selectedBuddy; }
            set { SetProperty(ref _selectedBuddy, value); }
        }
        public ObservableCollection<MyBuddy> Buddies { get; set; }
        public Command LoadBuddiesCommand { get; set; }

        public BuddyViewModel()
        {
            Buddies = new ObservableCollection<MyBuddy>();
            LoadBuddiesCommand = new Command(() => ExecuteLoadBuddiesCommand());
        }

        void ExecuteLoadBuddiesCommand()
        {
            if (IsBusy)
                return;

            IsBusy = true;

            try {
                Buddies.Clear();
                foreach (var buddy in MyApp.account.buddyList) {
                    Buddies.Add(buddy);
                }
            } catch (Exception e) {
                Console.WriteLine(e.Message);
            } finally {
                IsBusy = false;
            }
        }
    }
}

