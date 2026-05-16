using libpjsua2.maui;
using pjsua2maui.Models;
using Microsoft.Maui.Controls;

namespace pjsua2maui.Controls
{
   // This is a cross-platform placeholder for the native video view
   public class VideoView : View
   {
      public SoftCall? myCall { get; set; }
      public void ShowVideoWindow(SoftCall inCall, bool show = true)
      {
         myCall = inCall;
         (Handler as IVideoViewHandlerBridge)?.ShowVideoWindow(myCall, show);
      }
   }
}
