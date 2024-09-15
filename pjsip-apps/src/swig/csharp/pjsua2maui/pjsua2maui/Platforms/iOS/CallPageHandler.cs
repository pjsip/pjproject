#if __IOS__
using CommunityToolkit.Mvvm.Messaging;
using CoreGraphics;
using Microsoft.Maui.Controls.Handlers.Compatibility;
using Microsoft.Maui.Controls.Platform;  
using UIKit;
using pjsua2maui.Controls;
using pjsua2maui.Views;
using pjsua2maui.Messages;
using pjsua2maui.Models;
using libpjsua2.maui;

namespace pjsua2maui.Platforms.iOS;


public class CallPageRenderer : VisualElementRenderer<CallView>
{
    UIView incomingVideoView;
    UIButton acceptCallButton;
    UIButton hangupCallButton;
    UILabel peerLabel;
    UILabel callStatusLabel;
    private static CallInfo lastCallInfo;
    private CallPage callPage;

    public CallPageRenderer()
    {
        WeakReferenceMessenger.Default.Register<UpdateCallStateMessage>(this, (r, m) =>
        {
            lastCallInfo = m.Value as CallInfo;
            Dispatcher.GetForCurrentThread().Dispatch(() => updateCallState(lastCallInfo));

            if (lastCallInfo.state ==
                pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
            {
                Dispatcher.GetForCurrentThread().Dispatch(() => { callPage.Navigation.PopAsync(); });
            }
        });

        WeakReferenceMessenger.Default.Register<UpdateCallStateMessage>(this, (r, m) =>
        {
            lastCallInfo = m.Value as CallInfo;

            if (SoftApp.currentCall.vidWin != null)
            {
                incomingVideoView.Hidden = false;
            }
        });
    }

    protected override void OnElementChanged(ElementChangedEventArgs<CallView> e)
    {
        base.OnElementChanged(e);

        if (e.OldElement != null || Element == null)
        {
            return;
        }

        callPage = (CallPage)Element.Parent;
        try
        {
            var controlWidth = 150;
            var controlHeight = 50;
            var centerButtonX = (Element.Bounds.X / 2) - 35f;
            var topLeftX = Element.Bounds.X + 25;
            var topRightX = Element.Bounds.Right - controlWidth - 25;
            var bottomButtonY = Element.Bounds.Bottom - 150;
            var bottomLabelY = Element.Bounds.Top + 15;

            incomingVideoView = new UIView()
            {
                Frame = new CGRect(0f, 0f, Element.Bounds.Width,
                                   Element.Bounds.Height)
            };

            acceptCallButton = new UIButton()
            {
                Frame = new CGRect(topLeftX, bottomButtonY, controlWidth,
                                   controlHeight)
            };
            acceptCallButton.SetTitle("Accept", UIControlState.Normal);
            acceptCallButton.SetTitleColor(color: UIColor.Black,
                                           UIControlState.Normal);
            acceptCallButton.BackgroundColor = UIColor.White;

            hangupCallButton = new UIButton()
            {
                Frame = new CGRect(topRightX, bottomButtonY, controlWidth,
                                   controlHeight)
            };
            hangupCallButton.SetTitle("Hangup", UIControlState.Normal);
            hangupCallButton.SetTitleColor(color: UIColor.Black,
                                           UIControlState.Normal);
            hangupCallButton.BackgroundColor = UIColor.White;

            peerLabel = new UILabel
            {
                TextAlignment = UITextAlignment.Center,
                Frame = new CGRect(Element.Bounds.X,
                                                       bottomLabelY,
                                                       Element.Bounds.Right,
                                                       controlHeight)
            };
            callStatusLabel = new UILabel
            {
                TextAlignment = UITextAlignment.Center,
                Frame = new CGRect(Element.Bounds.X,
                                                  bottomLabelY + controlHeight,
                                                  Element.Bounds.Right,
                                                  controlHeight)
            };

            this.Add(incomingVideoView);
            this.Add(acceptCallButton);
            this.Add(hangupCallButton);
            this.Add(peerLabel);
            this.Add(callStatusLabel);


            SetupEventHandlers();

            if (SoftApp.currentCall != null)
            {
                try
                {
                    lastCallInfo = SoftApp.currentCall.getInfo();
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine(@"ERROR: ",
                                                       ex.Message);
                }
                
                Application.Current.Dispatcher.Dispatch(() => updateCallState(lastCallInfo));
            }
            else
            {
                incomingVideoView.Hidden = true;
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"ERROR: {ex.Message}");
        }
    }
     

    protected override void Dispose(bool disposing)
    {
        updateVideoWindow(false);
        base.Dispose(disposing);
    }

    void SetupEventHandlers()
    {
        acceptCallButton.TouchUpInside += (object sender, EventArgs e) => {
            AcceptCall();
        };

        hangupCallButton.TouchUpInside += (object sender, EventArgs e) => {
            HangupCall();
        };
    }

    void AcceptCall()
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

        acceptCallButton.Hidden = true;
    }

    void HangupCall()
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
            acceptCallButton.Hidden = true;
            hangupCallButton.SetTitle("OK", UIControlState.Normal);
            callStatusLabel.Text = "Call disconnected";
            return;
        }

        if (ci.role == pjsip_role_e.PJSIP_ROLE_UAC)
        {
            acceptCallButton.Hidden = true;
        }

        if (ci.state <
            pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
        {
            if (ci.role == pjsip_role_e.PJSIP_ROLE_UAS)
            {
                call_state = "Incoming call..";
                /* Default button texts are already 'Accept' & 'Reject' */
            }
            else
            {
                hangupCallButton.SetTitle("Cancel", UIControlState.Normal);
                call_state = ci.stateText;
            }
        }
        else if (ci.state >=
                   pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
        {
            acceptCallButton.Hidden = true;
            call_state = ci.stateText;
            if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
            {
                hangupCallButton.SetTitle("Hangup", UIControlState.Normal);
            }
            else if (ci.state ==
                       pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
            {
                hangupCallButton.SetTitle("OK", UIControlState.Normal);
                call_state = "Call disconnected: " + ci.lastReason;
            }
            if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
            {
                updateVideoWindow(true);
            }
        }

        peerLabel.Text = ci.remoteUri;
        callStatusLabel.Text = call_state;
    }

    private void updateVideoWindow(bool show)
    {
        if (SoftApp.currentCall != null &&
            SoftApp.currentCall.vidWin != null &&
            SoftApp.currentCall.vidPrev != null)
        {
            if (show)
            {
                VideoWindowInfo winInfo =
                                    SoftApp.currentCall.vidWin.getInfo();
                IntPtr winPtr = winInfo.winHandle.handle.window;
                UIView inView =
                           (UIView)ObjCRuntime.Runtime.GetNSObject(winPtr);
                try
                {
                    this.BeginInvokeOnMainThread(() => {
                        incomingVideoView.AddSubview(inView);
                        inView.ContentMode = UIViewContentMode.ScaleAspectFit;
                        inView.Center = incomingVideoView.Center;
                        inView.Frame = incomingVideoView.Bounds;
                    });
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine(@"ERROR: ",
                                                       ex.Message);
                }
            }
        }
    }
}
#endif