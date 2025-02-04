using System;
using CommunityToolkit.Mvvm.Messaging.Messages;
using libpjsua2.maui;

namespace pjsua2maui.Messages;

public class EditBuddyMessage : ValueChangedMessage<BuddyConfig>
{
	public EditBuddyMessage(BuddyConfig buddyConfig) : base(buddyConfig)
	{
	}
}

