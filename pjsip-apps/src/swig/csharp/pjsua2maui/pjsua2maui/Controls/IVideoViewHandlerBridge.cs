using libpjsua2.maui;
using pjsua2maui.Models;
namespace pjsua2maui.Controls
{
   // Interface for handler bridge to allow VideoView to communicate with its handler
   public interface IVideoViewHandlerBridge
   {
      void ShowVideoWindow(SoftCall inCall, bool show = true);
   }
}
