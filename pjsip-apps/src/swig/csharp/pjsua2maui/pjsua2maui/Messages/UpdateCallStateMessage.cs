using System;
using CommunityToolkit.Mvvm.Messaging.Messages;
using libpjsua2.maui;

namespace pjsua2maui.Messages;

public class UpdateCallStateMessage : ValueChangedMessage<CallInfo>
{
	public UpdateCallStateMessage(CallInfo callInfo) : base(callInfo)
	{
	}
}


