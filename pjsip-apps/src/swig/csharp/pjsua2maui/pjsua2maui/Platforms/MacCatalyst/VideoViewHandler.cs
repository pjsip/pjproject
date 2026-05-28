#if MACCATALYST
using libpjsua2.maui;
using Microsoft.Maui.Handlers;
using pjsua2maui.Controls;
using pjsua2maui.Models;
using UIKit;

namespace pjsua2maui.Platforms.MacCatalyst;

public partial class VideoViewHandler : ViewHandler<Controls.VideoView, UIView>, IVideoViewHandlerBridge
{
    public static IPropertyMapper<Controls.VideoView, VideoViewHandler> Mapper =
        new PropertyMapper<Controls.VideoView, VideoViewHandler>(ViewHandler.ViewMapper);

    public VideoViewHandler() : base(Mapper)
    {
    }

    protected override UIView CreatePlatformView()
    {
        // Cria uma UIView nativa que servirá de tela para o PJSIP desenhar o vídeo
        var uiView = new UIView();
        
        // Garante que o fundo seja preto para o contraste do vídeo
        uiView.BackgroundColor = UIColor.Black; 
        
        return uiView;
    }

    public void ShowVideoWindow(SoftCall InCall, bool Show = true)
    {
        if (InCall != null &&
            InCall.vidWin != null &&
            InCall.vidPrev != null)
        {
            nint windHandle = 0;
            VideoWindowHandle vidWH = new VideoWindowHandle();

            if (Show)
            {
                var nativeView = PlatformView;
                // Captures the native pointer of Apple UIView .
                // Depending of the way the Pjsip Wrapper was built, 
                // he can asks for the View or Layer pointer (nativeView.Layer.Handle).
                // testing with view handle:
                IntPtr winPtr = nativeView.Handle;
                // Changes if the pure View results in a black screen:
                // IntPtr winPtr = nativeView.Layer.Handle;
                windHandle = (nint)winPtr;
            }

            vidWH.handle.window = windHandle;

            try
            {
                InCall.vidWin.setWindow(vidWH);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"ERROR MacCatalyst Video: {ex.Message}");
            }
        }
    }
}
#endif