using System;
using CommunityToolkit.Mvvm.Messaging.Messages;
using libpjsua2.maui;

namespace pjsua2maui.Messages;

public class UpdateMediaCallStateMessage : ValueChangedMessage<CallInfo>
{
	public UpdateMediaCallStateMessage(CallInfo callInfo) : base(callInfo)
	{
	}
}


