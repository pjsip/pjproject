using Microsoft.Extensions.Logging;
using pjsua2maui.Controls;
#if ANDROID
using pjsua2maui.Platforms.Android;
#endif
#if IOS
using pjsua2maui.Platforms.iOS;
#endif

namespace pjsua2maui;

public static class MauiProgram
{
    public static MauiApp CreateMauiApp()
    {
        var builder = MauiApp.CreateBuilder();
        builder
           .UseMauiApp<App>()
           .ConfigureFonts(fonts =>
           {
               fonts.AddFont("OpenSans-Regular.ttf", "OpenSansRegular");
               fonts.AddFont("OpenSans-Semibold.ttf", "OpenSansSemibold");
           });
        builder.ConfigureMauiHandlers(handlers =>
        {
#if ANDROID
            handlers.AddHandler(typeof(pjsua2maui.Controls.VideoView), typeof(pjsua2maui.Platforms.Android.VideoViewHandler));
#endif
#if IOS
            handlers.AddHandler(typeof(pjsua2maui.Controls.VideoView), typeof(pjsua2maui.Platforms.iOS.VideoViewHandler));
#endif
#if MACCATALYST
            handlers.AddHandler(typeof(pjsua2maui.Controls.VideoView), typeof(pjsua2maui.Platforms.MacCatalyst.VideoViewHandler));
#endif
#if WINDOWS
            handlers.AddHandler(typeof(pjsua2maui.Controls.VideoView), typeof(pjsua2maui.Platforms.Windows.VideoViewHandler));
#endif

        });

#if DEBUG
        builder.Logging.AddDebug();
#endif
        return builder.Build();
    }
}
