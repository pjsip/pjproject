using pjsua2maui.Models;

namespace pjsua2maui.ViewModels;

public class AVConfigViewModel : BaseViewModel
{
    public SoftAccountConfigModel accCfg { get; set; }

    public AVConfigViewModel()
    {
        
    }

    public void init(SoftAccountConfig inAccCfg = null)
    {
        accCfg = new SoftAccountConfigModel(inAccCfg);
    }
}

