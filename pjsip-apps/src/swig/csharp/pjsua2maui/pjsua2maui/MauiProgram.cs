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
#if ANDROID
      builder.ConfigureMauiHandlers(handlers =>
      {
         handlers.AddHandler(typeof(pjsua2maui.Controls.VideoView), typeof(pjsua2maui.Platforms.Android.VideoViewHandler));
      });
#endif

#if DEBUG
      builder.Logging.AddDebug();
#endif
#if __IOS__
      builder.ConfigureMauiHandlers(handlers =>
      {
         handlers.AddHandler(typeof(pjsua2maui.Controls.VideoView), typeof(pjsua2maui.Platforms.iOS.VideoViewHandler));
      });
#endif
      return builder.Build();
   }
}
