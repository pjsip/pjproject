using pjsua2maui.Models;

namespace pjsua2maui.ViewModels;

public class AccountConfigViewModel : BaseViewModel
{
    public SoftAccountConfigModel accCfg { get; set; }

    public AccountConfigViewModel()
    {
        
    }

    public void init(SoftAccountConfig inAccCfg = null)
    {
        accCfg = new SoftAccountConfigModel(inAccCfg);
    }
}

