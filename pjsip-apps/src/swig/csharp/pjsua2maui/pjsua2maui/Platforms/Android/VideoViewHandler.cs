#if ANDROID
using Android.Content;
using Android.Graphics;
using Android.Views;
using Android.Runtime;
using Microsoft.Maui.Handlers;
using Microsoft.Maui.Platform;
using pjsua2maui.Controls;
using pjsua2maui.Models;
using libpjsua2.maui;

namespace pjsua2maui.Platforms.Android
{
   public partial class VideoViewHandler : ViewHandler<VideoView, SurfaceView>, IVideoViewHandlerBridge
   {
      [System.Runtime.InteropServices.DllImport("android")]
      private static extern IntPtr ANativeWindow_fromSurface(IntPtr jniEnv, IntPtr surface);
      public static IPropertyMapper<VideoView, VideoViewHandler> Mapper =
          new PropertyMapper<VideoView, VideoViewHandler>(ViewHandler.ViewMapper);

      public VideoViewHandler() : base(Mapper)
      {
      }
      protected override SurfaceView CreatePlatformView()
      {
         var surfaceView = new SurfaceView(Context);
         var surfaceCallback = new MySurfaceCallback(VirtualView);
         surfaceView.Holder.AddCallback(surfaceCallback);
         return surfaceView;
      }
      public void ShowVideoWindow(SoftCall inCall, bool show = true)
      {
         if (inCall != null &&
             inCall.vidWin != null &&
             inCall.vidPrev != null)
         {
            nint windHandle = 0;
            VideoWindowHandle vidWH = new VideoWindowHandle();
            if (show)
            {
               var surfaceView = PlatformView;

               IntPtr winPtr = ANativeWindow_fromSurface(JNIEnv.Handle,
                                               surfaceView.Holder.Surface.Handle);

               windHandle = (nint)winPtr;
            }
            vidWH.handle.window = windHandle;
            try
            {
               inCall.vidWin.setWindow(vidWH);
            }
            catch (Exception ex)
            {
               System.Diagnostics.Debug.WriteLine(@"ERROR: ", ex.Message);
            }
         }
      }
   }
}
#endif
