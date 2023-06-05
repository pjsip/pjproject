using System;
using System.IO;
using Xamarin.Forms;
using Xamarin.Forms.Platform.Android;
using pjsua2xamarin;
using pjsua2xamarin.Droid;
using Android.App;
using Android.Content;
using Android.Hardware;
using Android.Views;
using Android.Graphics;
using Android.Widget;
using Android.Renderscripts;
using static Xamarin.Essentials.Permissions;
using pjsua2xamarin.pjsua2;
using AndroidX.Lifecycle;
using System.Runtime.InteropServices;
using Android.Runtime;

[assembly: ExportRenderer(typeof(CallPage), typeof(CallPageRenderer))]
namespace pjsua2xamarin.Droid
{
    public class CallPageRenderer : PageRenderer, ISurfaceHolderCallback
    {
        global::Android.Widget.Button acceptCallButton;
        global::Android.Widget.Button hangupCallButton;
        global::Android.Widget.TextView peerTxt;
        global::Android.Widget.TextView statusTxt;
        global::Android.Views.View view;
        private static CallInfo lastCallInfo;
        private CallPage callPage;
        SurfaceView incomingView;

        [DllImport("android")]
        private static extern IntPtr ANativeWindow_fromSurface(IntPtr jni, IntPtr surface);

        public CallPageRenderer(Context context) : base(context)
        {
            MessagingCenter.Subscribe<BuddyPage, CallInfo>
            (this, "UpdateCallState", (obj, info) => {
                if (callPage == null)
                    return;

                lastCallInfo = info as CallInfo;
                if (lastCallInfo.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
                    Device.BeginInvokeOnMainThread(() => {
                        callPage.Navigation.PopAsync();
                    });
                } else {
                    Device.BeginInvokeOnMainThread(() => {
                        updateCallState(lastCallInfo);
                    });
                }
            });
            MessagingCenter.Subscribe<BuddyPage, CallInfo>
            (this, "UpdateMediaCallState", (obj, info) => {
                lastCallInfo = info as CallInfo;

                if (MyApp.currentCall.vidWin != null) {
                    incomingView.Visibility = ViewStates.Visible;
                }
            });
        }

        ~CallPageRenderer() {
            MessagingCenter.Unsubscribe<BuddyPage, CallInfo>(this, "UpdateCallState");
            MessagingCenter.Unsubscribe<BuddyPage, CallInfo>(this, "UpdateMediaCallState");
        }

        protected override void OnLayout(bool changed, int l, int t, int r, int b)
        {
            base.OnLayout(changed, l, t, r, b);

            var msw = MeasureSpec.MakeMeasureSpec(r - l, MeasureSpecMode.Exactly);
            var msh = MeasureSpec.MakeMeasureSpec(b - t, MeasureSpecMode.Exactly);

            view.Measure(msw, msh);
            view.Layout(0, 0, r - l, b - t);
        }

        protected override void OnElementChanged(ElementChangedEventArgs<Page> e)
        {
            base.OnElementChanged(e);

            if (e.OldElement != null || Element == null) {
                return;
            }

            try {
                SetupUserInterface();
                SetupEventHandlers();
                AddView(view);
                callPage = (CallPage)Element;
                System.Diagnostics.Debug.WriteLine(@"Call page done initialize");
                if (MyApp.currentCall != null) {
                    try {
                        lastCallInfo = MyApp.currentCall.getInfo();
                    } catch (Exception ex) {
                        System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
                    }
                    Device.BeginInvokeOnMainThread(() => {
                        updateCallState(lastCallInfo);
                    });
                }
            } catch (Exception ex) {
                System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
            }
        }

        void SetupUserInterface()
        {
            var activity = this.Context as Activity;
            view = activity.LayoutInflater.Inflate(Resource.Layout.activity_call, this, false);

            incomingView = view.FindViewById<SurfaceView>(Resource.Id.incomingVideoView);
            incomingView.Holder.AddCallback(this);

            peerTxt = view.FindViewById<TextView>(Resource.Id.peerTxt);
            statusTxt = view.FindViewById<TextView>(Resource.Id.statusTxt);

            if (MyApp.currentCall == null || MyApp.currentCall.vidWin == null) {
                incomingView.Visibility = ViewStates.Gone;
            }
        }

        void SetupEventHandlers()
        {
            acceptCallButton = view.FindViewById<global::Android.Widget.Button>(Resource.Id.acceptCallButton);
            acceptCallButton.Click += AcceptCallButtonTapped;

            hangupCallButton = view.FindViewById<global::Android.Widget.Button>(Resource.Id.hangupCallButton);
            hangupCallButton.Click += HangupCallButtonTapped;
        }

        void AcceptCallButtonTapped(object sender, EventArgs e)
        {
            CallOpParam prm = new CallOpParam();
            prm.statusCode = pjsip_status_code.PJSIP_SC_OK;
            try {
                MyApp.currentCall.answer(prm);
            } catch (Exception ex) {
                System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
            }

            acceptCallButton.Visibility = ViewStates.Gone;
        }

        void HangupCallButtonTapped(object sender, EventArgs e)
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

        private void updateVideoWindow(bool show)
        {
            if (MyApp.currentCall != null &&
                MyApp.currentCall.vidWin != null &&
                MyApp.currentCall.vidPrev != null)
            {
                long windHandle = 0;
                VideoWindowHandle vidWH = new VideoWindowHandle();
                if (show) {
                    IntPtr winPtr = ANativeWindow_fromSurface(JNIEnv.Handle,
                                           incomingView.Holder.Surface.Handle);
                    windHandle = winPtr.ToInt64();
                }
                vidWH.handle.setWindow(windHandle);
                try {
                    MyApp.currentCall.vidWin.setWindow(vidWH);
                } catch (Exception ex) {
                    System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
                }
            }
        }

        private void updateCallState(CallInfo ci)
        {
            String call_state = "";

            if (ci == null) {
                acceptCallButton.Visibility = ViewStates.Gone;
                hangupCallButton.Text = "OK";
                statusTxt.Text = "Call disconnected";
                return;
            }

            if (ci.role == pjsip_role_e.PJSIP_ROLE_UAC) {
                acceptCallButton.Visibility = ViewStates.Gone;
            }

            if (ci.state <
                pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
            {
                if (ci.role == pjsip_role_e.PJSIP_ROLE_UAS) {
                    call_state = "Incoming call..";
                    /* Default button texts are already 'Accept' & 'Reject' */
                } else {
                    hangupCallButton.Text = "Cancel";
                    call_state = ci.stateText;
                }
            } else if (ci.state >=
                       pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
            {
                acceptCallButton.Visibility = ViewStates.Gone;
                call_state = ci.stateText;
                if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
                    hangupCallButton.Text = "Hangup";
                } else if (ci.state ==
                           pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
                {
                    hangupCallButton.Text = "OK";
                    call_state = "Call disconnected: " + ci.lastReason;
                }
                if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
                    updateVideoWindow(true);
                }
            }

            peerTxt.Text = ci.remoteUri;
            statusTxt.Text = call_state;
        }

        void ISurfaceHolderCallback.SurfaceChanged(ISurfaceHolder holder, Format format, int width, int height)
        {
            updateVideoWindow(true);
        }

        void ISurfaceHolderCallback.SurfaceCreated(ISurfaceHolder holder)
        {
        }

        void ISurfaceHolderCallback.SurfaceDestroyed(ISurfaceHolder holder)
        {
            updateVideoWindow(false);
        }
    }
}

