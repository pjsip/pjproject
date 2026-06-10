#if WINDOWS
using System;
using Microsoft.Maui.Handlers;
using Grid = Microsoft.UI.Xaml.Controls.Grid;
using WinRT;
using pjsua2maui.Controls;
using pjsua2maui.Models;
using libpjsua2.maui;

namespace pjsua2maui.Platforms.Windows;

public partial class VideoViewHandler : ViewHandler<VideoView, Grid>, IVideoViewHandlerBridge
{
    public static IPropertyMapper<VideoView, VideoViewHandler> Mapper =
        new PropertyMapper<VideoView, VideoViewHandler>(ViewHandler.ViewMapper);

    public VideoViewHandler() : base(Mapper)
    {
    }

    protected override Grid CreatePlatformView()
    {
        // Criamos um Grid nativo do WinUI 3 para servir de canvas/container para o vídeo
        var videoGrid = new Grid();
        return videoGrid;
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
                var gridView = PlatformView;

                // Captura o ponteiro nativo (HWND/IUnknown) do elemento visual do WinUI 3
                // O PJSIP no Windows espera receber o ponteiro correspondente ao controle ou à janela de desenho.
                IntPtr winPtr = gridView.As<IInspectable>().ThisPtr;

                windHandle = (nint)winPtr;
            }

            vidWH.handle.window = windHandle;

            try
            {
                InCall.vidWin.setWindow(vidWH);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"ERROR Windows Video: {ex.Message}");
            }
        }
    }
}
#endif