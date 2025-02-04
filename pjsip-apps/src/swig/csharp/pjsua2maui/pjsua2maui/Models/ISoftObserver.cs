using System;
namespace pjsua2maui.Models;

/* Interface to separate UI & engine a bit better */
public interface ISoftObserver
{
    void notifyRegState(int code, String reason, long expiration);
    void notifyIncomingCall(SoftCall call);
    void notifyCallState(SoftCall call);
    void notifyCallMediaState(SoftCall call);
    void notifyBuddyState(SoftBuddy buddy);
    void notifyChangeNetwork();
}

