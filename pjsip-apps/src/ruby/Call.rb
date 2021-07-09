module SIP
    class Call < Pjsua2::Call
        def initialize(owner, account, callId = Pjsua2::PJSUA_INVALID_ID)
            super(account, callId)
            @owner = owner
        end

        def onCallState(callState)
            @owner.onCallState(self, callState)
        end
    end
end
