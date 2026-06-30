using ObjCRuntime;
using UIKit;

namespace pjsua2maui;
/// <summary>
/// Main Class of application
/// </summary>
public class Program
{
    /// <summary>
    /// Protected costructor 
    /// Avoids S1118 Warning of Sonar Linter
    /// </summary>
    protected Program()
    {

    }
    /// <summary>
    /// This is the main entry point of the application.
    /// </summary>
    /// <param name="args">arguments to the application</param>
    static void Main(string[] args)
    {
        // if you want to use a different Application Delegate class from "AppDelegate"
        // you can specify it here.
        UIApplication.Main(args, null, typeof(AppDelegate));
    }
}
