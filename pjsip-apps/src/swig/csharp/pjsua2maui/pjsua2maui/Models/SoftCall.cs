using libpjsua2.maui;

namespace pjsua2maui.Models
{
    public class SoftCall : Call
    {
        public VideoWindow vidWin;
        public VideoPreview vidPrev;

        public SoftCall(SoftAccount acc, int call_id) : base(acc, call_id)
        {
            vidWin = null;
            vidPrev = null;
        }

        override public void onCallState(OnCallStateParam prm)
        {
            try
            {
                CallInfo ci = getInfo();
                if (ci.state ==
                    pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
                {
                    SoftApp.ep.utilLogWrite(3, "SoftCall", this.dump(true, ""));
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error : " + ex.Message);
            }

            // Should not delete this call instance (self) in this context,
            // so the observer should manage this call instance deletion
            // out of this callback context.
            SoftApp.observer.notifyCallState(this);
        }

        override public void onCallMediaState(OnCallMediaStateParam prm)
        {
            CallInfo ci;
            try
            {
                ci = getInfo();
            }
            catch (Exception)
            {
                return;
            }

            CallMediaInfoVector cmiv = ci.media;

            for (int i = 0; i < cmiv.Count; i++)
            {
                CallMediaInfo cmi = cmiv[i];
                if (cmi.type == pjmedia_type.PJMEDIA_TYPE_AUDIO &&
                    (cmi.status ==
                            pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE ||
                     cmi.status ==
                            pjsua_call_media_status.PJSUA_CALL_MEDIA_REMOTE_HOLD))
                {
                    // connect ports
                    try
                    {
                        AudDevManager audMgr = SoftApp.ep.audDevManager();
                        AudioMedia am = getAudioMedia(i);
                        audMgr.getCaptureDevMedia().startTransmit(am);
                        am.startTransmit(audMgr.getPlaybackDevMedia());
                    }
                    catch (Exception e)
                    {
                        Console.WriteLine("Failed connecting media ports" +
                                          e.Message);
                        continue;
                    }
                }
                else if (cmi.type == pjmedia_type.PJMEDIA_TYPE_VIDEO &&
                           cmi.status == pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE &&
                           cmi.videoIncomingWindowId != pjsua2.INVALID_ID)
                {
                    vidWin = new VideoWindow(cmi.videoIncomingWindowId);
                    vidPrev = new VideoPreview(cmi.videoCapDev);
                }
            }

            SoftApp.observer.notifyCallMediaState(this);
        }
    }
}