namespace pjsua2maui;

public partial class App : Application
{
	public App()
	{
		InitializeComponent();
	}

    protected override Window CreateWindow(IActivationState activationState)
    {
        Window window = new(new AppShell());
         
        // initial desired size
        window.Width = 400;
        window.Height = 750;

        // lock size to prevent resizing by the user 
        window.MinimumWidth = 400;
        window.MaximumWidth = 400;
        window.MinimumHeight = 750;
        window.MaximumHeight = 750;

        return window;
    }
}
