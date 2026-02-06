#if __IOS__

using Microsoft.Maui.Handlers;
using Microsoft.Maui.Platform;
using pjsua2maui.Controls;
using pjsua2maui.Models;
using libpjsua2.maui;

using UIKit;
using Microsoft.Maui.ApplicationModel;

namespace pjsua2maui.Platforms.iOS
{
   public partial class VideoViewHandler : ViewHandler<VideoView, UIView>, IVideoViewHandlerBridge
   {
      public static IPropertyMapper<VideoView, VideoViewHandler> Mapper =
         new PropertyMapper<VideoView, VideoViewHandler>(ViewHandler.ViewMapper);

      public VideoViewHandler() : base(Mapper)
      {
      }

      protected override UIView CreatePlatformView()
      {
         // You can customize this as needed for your VideoView
         return new UIView();
      }

      public void ShowVideoWindow(SoftCall inCall, bool show = true)
      {
         if (inCall != null &&
             inCall.vidWin != null &&
             inCall.vidPrev != null)
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
                    MainThread.BeginInvokeOnMainThread(() => {
                        var incomingVideoView = PlatformView;

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
}
#endif
