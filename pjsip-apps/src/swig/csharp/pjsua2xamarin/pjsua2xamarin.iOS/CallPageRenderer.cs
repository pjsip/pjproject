using AVFoundation;
using CoreGraphics;
using pjsua2xamarin;
using pjsua2xamarin.pjsua2;
using pjsua2xamarin.iOS;
using Foundation;
using System;
using UIKit;
using Xamarin.Forms;
using Xamarin.Forms.Platform.iOS;

[assembly: ExportRenderer(typeof(CallPage), typeof(CallPageRenderer))]
namespace pjsua2xamarin.iOS
{
    public class CallPageRenderer : PageRenderer
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
            MessagingCenter.Subscribe<BuddyPage, CallInfo>
                        (this, "UpdateCallState", (obj, info) => {
                            lastCallInfo = info as CallInfo;
                            Device.BeginInvokeOnMainThread(() => {
                                updateCallState(lastCallInfo);
                            });

                            if (lastCallInfo.state ==
                                pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
                            {
                                Device.BeginInvokeOnMainThread(() => {
                                    callPage.Navigation.PopAsync();
                                });
                            }
                        });
            MessagingCenter.Subscribe<BuddyPage, CallInfo>
            (this, "UpdateMediaCallState", (obj, info) => {
                lastCallInfo = info as CallInfo;

                if (MyApp.currentCall.vidWin != null) {
					incomingVideoView.Hidden = false;
				}
            });
        }

        protected override void OnElementChanged(VisualElementChangedEventArgs e)
        {
            base.OnElementChanged(e);

            if (e.OldElement != null || Element == null) {
                return;
            }

            callPage = (CallPage)Element;
            try {
                SetupUserInterface();
                SetupEventHandlers();

                if (MyApp.currentCall != null) {
                    try {
                        lastCallInfo = MyApp.currentCall.getInfo();
                    } catch (Exception ex) {
                        System.Diagnostics.Debug.WriteLine(@"ERROR: ",
                                                           ex.Message);
                    }
                    Device.BeginInvokeOnMainThread(() => {
                        updateCallState(lastCallInfo);
                    });
                } else {
                    incomingVideoView.Hidden = true;
				}
            } catch (Exception ex) {
                System.Diagnostics.Debug.WriteLine($"ERROR: {ex.Message}");
            }
        }

        protected override void Dispose(bool disposing)
        {
            updateVideoWindow(false);
            base.Dispose(disposing);
        }

        void SetupUserInterface()
        {
            var controlWidth = 150;
            var controlHeight = 50;
            var centerButtonX = View.Bounds.GetMidX() - 35f;
            var topLeftX = View.Bounds.X + 25;
            var topRightX = View.Bounds.Right - controlWidth - 25;
            var bottomButtonY = View.Bounds.Bottom - 150;
            var bottomLabelY = View.Bounds.Top + 15;

            incomingVideoView = new UIView() {
                Frame = new CGRect(0f, 0f, View.Bounds.Width,
                                   View.Bounds.Height)
            };

            acceptCallButton = new UIButton() {
                Frame = new CGRect(topLeftX, bottomButtonY, controlWidth,
                                   controlHeight)
            };
            acceptCallButton.SetTitle("Accept", UIControlState.Normal);
            acceptCallButton.SetTitleColor(color:UIColor.Black,
                                           UIControlState.Normal);
            acceptCallButton.BackgroundColor = UIColor.White;

            hangupCallButton = new UIButton() {
                Frame = new CGRect(topRightX, bottomButtonY, controlWidth,
                                   controlHeight)
            };
            hangupCallButton.SetTitle("Hangup", UIControlState.Normal);
            hangupCallButton.SetTitleColor(color: UIColor.Black,
                                           UIControlState.Normal);
            hangupCallButton.BackgroundColor = UIColor.White;

            peerLabel = new UILabel { 
                                    TextAlignment = UITextAlignment.Center,
                                    Frame = new CGRect(View.Bounds.X,
                                                       bottomLabelY,
                                                       View.Bounds.Right,
                                                       controlHeight)
                                    };
            callStatusLabel = new UILabel {
                                      TextAlignment = UITextAlignment.Center,
                                      Frame = new CGRect(View.Bounds.X,
                                                  bottomLabelY + controlHeight,
                                                  View.Bounds.Right,
                                                  controlHeight)
                                          };

            View.Add(incomingVideoView);
            View.Add(acceptCallButton);
            View.Add(hangupCallButton);
            View.Add(peerLabel);
            View.Add(callStatusLabel);
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
            try {
                MyApp.currentCall.answer(prm);
            } catch (Exception ex) {
                System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
            }

            acceptCallButton.Hidden = true;
        }

        void HangupCall()
        {
            if (MyApp.currentCall != null) {
                CallOpParam prm = new CallOpParam();
                prm.statusCode = pjsip_status_code.PJSIP_SC_DECLINE;
                try {
                    MyApp.currentCall.hangup(prm);
                } catch (Exception ex) {
                    System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
                }
            }
        }

        private void updateCallState(CallInfo ci)
        {
            String call_state = "";

            if (ci == null) {
                acceptCallButton.Hidden = true;
                hangupCallButton.SetTitle("OK", UIControlState.Normal);
                callStatusLabel.Text = "Call disconnected";
                return;
            }

            if (ci.role == pjsip_role_e.PJSIP_ROLE_UAC) {
                acceptCallButton.Hidden = true;
            }

            if (ci.state <
                pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
                if (ci.role == pjsip_role_e.PJSIP_ROLE_UAS) {
                    call_state = "Incoming call..";
                    /* Default button texts are already 'Accept' & 'Reject' */
                } else {
                    hangupCallButton.SetTitle("Cancel", UIControlState.Normal);
                    call_state = ci.stateText;
                }
            } else if (ci.state >=
                       pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
            {
                acceptCallButton.Hidden = true;
                call_state = ci.stateText;
                if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
                    hangupCallButton.SetTitle("Hangup", UIControlState.Normal);
                } else if (ci.state ==
                           pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
                {
                    hangupCallButton.SetTitle("OK", UIControlState.Normal);
                    call_state = "Call disconnected: " + ci.lastReason;
                }
                if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
                    updateVideoWindow(true);
                }
            }

            peerLabel.Text = ci.remoteUri;
            callStatusLabel.Text = call_state;
        }

        private void updateVideoWindow(bool show)
        {
            if (MyApp.currentCall != null &&
                MyApp.currentCall.vidWin != null &&
                MyApp.currentCall.vidPrev != null)
            {
                if (show) {
                    VideoWindowInfo winInfo =
                                        MyApp.currentCall.vidWin.getInfo();
                    IntPtr winPtr = winInfo.winHandle.handle.window;
                    UIView inView =
                               (UIView)ObjCRuntime.Runtime.GetNSObject(winPtr);
                    try {
                        Device.BeginInvokeOnMainThread(() => {
                            incomingVideoView.AddSubview(inView);
							inView.ContentMode = UIViewContentMode.ScaleAspectFit;
							inView.Center = incomingVideoView.Center;
							inView.Frame = incomingVideoView.Bounds;
						});
                    } catch (Exception ex) {
                        System.Diagnostics.Debug.WriteLine(@"ERROR: ",
                                                           ex.Message);
                    }
                }
            }
        }
    }
}

