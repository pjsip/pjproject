using System;
using CommunityToolkit.Mvvm.Messaging.Messages;
using libpjsua2.maui;

namespace pjsua2maui.Messages;

public class AddBuddyMessage : ValueChangedMessage<BuddyConfig>
{
	public AddBuddyMessage(BuddyConfig buddyConfig) : base(buddyConfig)
	{
	}
}
