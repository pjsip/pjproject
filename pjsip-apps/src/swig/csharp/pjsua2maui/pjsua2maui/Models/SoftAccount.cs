using libpjsua2.maui;

namespace pjsua2maui.Models;

public class SoftAccount : Account
{
    public List<SoftBuddy> buddyList = new List<SoftBuddy>();
    public AccountConfig cfg;

    ~SoftAccount()
    {
        Console.WriteLine("*** Account is being deleted");
    }

    public SoftAccount(AccountConfig config)
    {
        cfg = config;
    }

    public SoftBuddy addBuddy(BuddyConfig bud_cfg)
    {
        /* Create Buddy */
        SoftBuddy bud = new SoftBuddy(bud_cfg);
        try
        {
            bud.create(this, bud_cfg);
        }
        catch (Exception)
        {
            bud.Dispose();
        }

        if (bud != null)
        {
            buddyList.Add(bud);
            if (bud_cfg.subscribe)
                try
                {
                    bud.subscribePresence(true);
                }
                catch (Exception) { }
        }

        return bud;
    }

    public void delBuddy(SoftBuddy buddy)
    {
        buddyList.Remove(buddy);
        buddy.Dispose();
    }

    override public void onRegState(OnRegStateParam prm)
    {
        AccountInfo ai = getInfo();
        Console.WriteLine("***" + (ai.regIsActive ? "" : "Un") +
                          "Register: code=" + prm.code);

        SoftApp.observer.notifyRegState((int)prm.code, prm.reason, prm.expiration);
    }

    override public void onIncomingCall(OnIncomingCallParam prm)
    {
        Console.WriteLine("======== Incoming call ======== ");
        SoftCall call = new SoftCall(this, prm.callId);
        SoftApp.observer.notifyIncomingCall(call);
    }

    override public void onInstantMessage(OnInstantMessageParam prm)
    {
        Console.WriteLine("======== Incoming pager ======== ");
        Console.WriteLine("From     : " + prm.fromUri);
        Console.WriteLine("To       : " + prm.toUri);
        Console.WriteLine("Contact  : " + prm.contactUri);
        Console.WriteLine("Mimetype : " + prm.contentType);
        Console.WriteLine("Body     : " + prm.msgBody);
    }
}