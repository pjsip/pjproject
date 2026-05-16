using libpjsua2.maui;
using pjsua2maui.Controls;
using pjsua2maui.Messages;
using pjsua2maui.Models;
using pjsua2maui.Views;
using CommunityToolkit.Mvvm.Messaging;
namespace pjsua2maui.Views;

public partial class CallPage : ContentPage
{
   private static CallInfo lastCallInfo;
   public CallPage()
   {
      InitializeComponent();

      acceptCallButton.Clicked += OnAcceptClicked;
      hangupCallButton.Clicked += OnHangupClicked;

      if (!WeakReferenceMessenger.Default.IsRegistered<UpdateCallStateMessage>(this))
      {
         WeakReferenceMessenger.Default.Register<UpdateCallStateMessage>(this, (r, m) =>
         {
            lastCallInfo = m.Value as CallInfo;
            MainThread.BeginInvokeOnMainThread(() => updateCallState(lastCallInfo));

            if (lastCallInfo.state ==
                  pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
            {
               MainThread.BeginInvokeOnMainThread(() => { this.Navigation.PopAsync(); });
            }
         });
      }
      if (!WeakReferenceMessenger.Default.IsRegistered<UpdateMediaCallStateMessage>(this))
      {
         WeakReferenceMessenger.Default.Register<UpdateMediaCallStateMessage>(this, (r, m) =>
         {
            if (SoftApp.currentCall.vidWin != null)
            {
               videoView.IsVisible = true;
            }
         });
      }

      // Example: set initial label values
      peerLabel.Text = "Peer: <not connected>";
      statusLabel.Text = "Status: Idle";
   }

   private void OnAcceptClicked(object sender, EventArgs e)
   {
      CallOpParam prm = new CallOpParam();
      prm.statusCode = pjsip_status_code.PJSIP_SC_OK;
      try
      {
         SoftApp.currentCall.answer(prm);
      }
      catch (Exception ex)
      {
         System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
      }

      acceptCallButton.IsVisible = false;
   }

   private void OnHangupClicked(object sender, EventArgs e)
   {
      if (SoftApp.currentCall != null)
      {
         CallOpParam prm = new CallOpParam();
         prm.statusCode = pjsip_status_code.PJSIP_SC_DECLINE;
         try
         {
            SoftApp.currentCall.hangup(prm);
         }
         catch (Exception ex)
         {
            System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
         }
      }
   }
   private void updateCallState(CallInfo ci)
   {
      String call_state = "";

      if (ci == null)
      {
         acceptCallButton.IsVisible = false;
         hangupCallButton.Text = "OK";
         statusLabel.Text = "Call disconnected";
         return;
      }

      if (ci.role == pjsip_role_e.PJSIP_ROLE_UAC)
      {
         acceptCallButton.IsVisible = false;
      }

      if (ci.state <
          pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
      {
         if (ci.role == pjsip_role_e.PJSIP_ROLE_UAS)
         {
            call_state = "Incoming call..";
         }
         else
         {
            hangupCallButton.Text = "Cancel";
            call_state = ci.stateText;
         }
      }
      else if (ci.state >=
                 pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
      {
         acceptCallButton.IsVisible = false;
         call_state = ci.stateText;
         if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
         {
            hangupCallButton.Text = "Hangup";
         }
         else if (ci.state ==
                    pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
         {
            hangupCallButton.Text = "OK";
            call_state = "Call disconnected: " + ci.lastReason;
         }
         if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
         {
            videoView.ShowVideoWindow(SoftApp.currentCall, true);
         }
      }

      peerLabel.Text = ci.remoteUri;
      statusLabel.Text = call_state;
   }
}

