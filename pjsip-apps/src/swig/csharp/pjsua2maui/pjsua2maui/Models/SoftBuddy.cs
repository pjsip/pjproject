using libpjsua2.maui;

namespace pjsua2maui.Models;

public class SoftBuddy : Buddy
{
    public BuddyConfig cfg { get; set; }

    public SoftBuddy(BuddyConfig config)
    {
        cfg = config;
    }

    public String getStatusText()
    {
        BuddyInfo bi;

        try
        {
            bi = getInfo();
        }
        catch (Exception)
        {
            return "?";
        }

        String status = "";
        if (bi.subState == pjsip_evsub_state.PJSIP_EVSUB_STATE_ACTIVE)
        {
            if (bi.presStatus.status ==
                pjsua_buddy_status.PJSUA_BUDDY_STATUS_ONLINE)
            {
                status = bi.presStatus.statusText;
                if (status == null || status.Length == 0)
                {
                    status = "Online";
                }
            }
            else if (bi.presStatus.status ==
                       pjsua_buddy_status.PJSUA_BUDDY_STATUS_OFFLINE)
            {
                status = "Offline";
            }
            else
            {
                status = "Unknown";
            }
        }
        return status;
    }

    override public void onBuddyState()
    {
        SoftApp.observer.notifyBuddyState(this);
    }
}