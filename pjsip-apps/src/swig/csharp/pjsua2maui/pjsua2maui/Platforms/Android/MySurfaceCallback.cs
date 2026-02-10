using Android.Graphics;
using Android.Views;
using pjsua2maui.Controls;
using pjsua2maui.Models;
using System;

namespace pjsua2maui.Platforms.Android
{
    public class MySurfaceCallback : Java.Lang.Object, ISurfaceHolderCallback
    {
        public VideoView videoView;
        public event Action<ISurfaceHolder> SurfaceCreatedEvent;
        public event Action<ISurfaceHolder> SurfaceDestroyedEvent;
        public event Action<ISurfaceHolder, Format, int, int> SurfaceChangedEvent;

        public void SurfaceCreated(ISurfaceHolder holder)
        {
            SurfaceCreatedEvent?.Invoke(holder);
            videoView?.ShowVideoWindow(SoftApp.currentCall, true);
        }

        public void SurfaceDestroyed(ISurfaceHolder holder)
        {
            SurfaceDestroyedEvent?.Invoke(holder);
            videoView?.ShowVideoWindow(SoftApp.currentCall, false);
        }

        public void SurfaceChanged(ISurfaceHolder holder, Format format, int width, int height)
        {
            SurfaceChangedEvent?.Invoke(holder, format, width, height);
            videoView?.ShowVideoWindow(SoftApp.currentCall, true);

        }

        public MySurfaceCallback(VideoView videoView)
       {
            this.videoView = videoView;
       }
    }
}
