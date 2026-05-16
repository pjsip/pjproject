using Android.App;
using Android.Hardware.Camera2;
using Android.Runtime;
using Java.Lang;

namespace pjsua2maui;

[Application]
public class MainApplication : MauiApplication
{
	public MainApplication(IntPtr handle, JniHandleOwnership ownership)
		: base(handle, ownership)
	{
		 IntPtr? class_ref = JNIEnv.FindClass("org/pjsip/PjCameraInfo2");
        if (class_ref != null)
        {
            IntPtr? method_id = JNIEnv.GetStaticMethodID(class_ref.Value,
                               "SetCameraManager", "(Landroid/hardware/camera2/CameraManager;)V");

            if (method_id != null)
            {
                
                CameraManager manager = GetSystemService(Android.Content.Context.CameraService) as CameraManager;

                JNIEnv.CallStaticVoidMethod(class_ref.Value, method_id.Value, new JValue(manager));

                Console.WriteLine("SUCCESS setting cameraManager");
            }
        }
        JavaSystem.LoadLibrary("c++_shared");
        JavaSystem.LoadLibrary("pjsua2");
	}

	protected override MauiApp CreateMauiApp() => MauiProgram.CreateMauiApp();
}
