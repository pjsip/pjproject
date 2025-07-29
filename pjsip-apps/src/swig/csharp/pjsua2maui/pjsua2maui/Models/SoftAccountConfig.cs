using libpjsua2.maui;

namespace pjsua2maui.Models;

public class SoftAccountConfig
{
    public AccountConfig accCfg;
    public List<BuddyConfig> buddyCfgs = new List<BuddyConfig>();

    public SoftAccountConfig()
    {
        accCfg = new AccountConfig();
    }

    public void readObject(ContainerNode accNode)
    {
        try
        {
            accCfg.readObject(accNode);
            ContainerNode buddiesNode = accNode.readArray("Buddies");
            buddyCfgs.Clear();
            while (buddiesNode.hasUnread())
            {
                BuddyConfig budCfg = new BuddyConfig();
                budCfg.readObject(buddiesNode);
                buddyCfgs.Add(budCfg);
            }
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }
    }

    public void writeObject(ContainerNode accNode)
    {
        try
        {
            accCfg.writeObject(accNode);
            ContainerNode buddiesNode = accNode.writeNewArray("Buddies");
            foreach (BuddyConfig budCfg in buddyCfgs)
            {
                budCfg.writeObject(buddiesNode);
            }
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }
    }
}