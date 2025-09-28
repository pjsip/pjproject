using System;

using Android.App;
using Android.Content.PM;
using Android.Runtime;
using Android.OS;
using Java.Lang;
using Android.Hardware.Camera2;
using Android.Content;

namespace pjsua2xamarin.Droid
{
    [Activity(Label = "pjsua2xamarin", Icon = "@mipmap/icon", Theme = "@style/MainTheme", MainLauncher = true, ConfigurationChanges = ConfigChanges.ScreenSize | ConfigChanges.Orientation | ConfigChanges.UiMode | ConfigChanges.ScreenLayout | ConfigChanges.SmallestScreenSize )]
    public class MainActivity : global::Xamarin.Forms.Platform.Android.FormsAppCompatActivity
    {
        protected override void OnCreate(Bundle savedInstanceState)
        {
            base.OnCreate(savedInstanceState);

            Xamarin.Essentials.Platform.Init(this, savedInstanceState);
            global::Xamarin.Forms.Forms.Init(this, savedInstanceState);

            IntPtr class_ref = JNIEnv.FindClass("org/pjsip/PjCameraInfo2");
            if (class_ref != null) {
                IntPtr method_id = JNIEnv.GetStaticMethodID(class_ref,
                                   "SetCameraManager", "(Landroid/hardware/camera2/CameraManager;)V");

                if (method_id != null) {
                    CameraManager manager = GetSystemService(Context.CameraService) as CameraManager;

                    JNIEnv.CallStaticVoidMethod(class_ref, method_id, new JValue(manager));

                    Console.WriteLine("SUCCESS setting cameraManager");
                }
            }
            JavaSystem.LoadLibrary("c++_shared");
            JavaSystem.LoadLibrary("pjsua2");

            LoadApplication(new App());
        }
        public override void OnRequestPermissionsResult(int requestCode, string[] permissions, [GeneratedEnum] Android.Content.PM.Permission[] grantResults)
        {
            Xamarin.Essentials.Platform.OnRequestPermissionsResult(requestCode, permissions, grantResults);

            base.OnRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }
}
