using CommunityToolkit.Mvvm.Messaging.Messages;
using pjsua2maui.Models;

namespace pjsua2maui.Messages;

public class SaveAccountConfigMessage : ValueChangedMessage<SoftAccountConfigModel>
{
	public SaveAccountConfigMessage(SoftAccountConfigModel softAccountConfigModel) : base(softAccountConfigModel)
	{
	}
}
