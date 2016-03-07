/* $Id*/
/*
* Copyright (C) 2016 Teluu Inc. (http://www.teluu.com)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

using System;
using System.Threading.Tasks;
using VoipTasks.BackgroundOperations;
using Windows.Foundation.Collections;
using Windows.Foundation.Metadata;
using VoipBackEnd;

namespace VoipUI.Helpers
{
    static class EndpointHelper
    {
        public static async Task<OperationResult> StartServiceAsync()
        {
            if (!ApiInformation.IsApiContractPresent("Windows.ApplicationModel.Calls.CallsVoipContract", 1))
            {                
                return OperationResult.Failed;
            }

            AppServiceHelper appServiceHelper = new AppServiceHelper();

            ValueSet message = new ValueSet();
            message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.StartService;

            ValueSet response = await appServiceHelper.SendMessageAsync(message);

            if (response != null)
            {
                return ((OperationResult)(response[BackgroundOperation.Result]));
            }
            return OperationResult.Failed;
        }

        public static async Task<OperationResult> GetAccountInfo()
        {
            if (!ApiInformation.IsApiContractPresent("Windows.ApplicationModel.Calls.CallsVoipContract", 1))
            {
                return OperationResult.Failed;
            }

            AppServiceHelper appServiceHelper = new AppServiceHelper();

            ValueSet message = new ValueSet();
            message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.GetAccountInfo;

            ValueSet response = await appServiceHelper.SendMessageAsync(message);

            if (response != null)
            {
                return ((OperationResult)(response[BackgroundOperation.Result]));
            }
            return OperationResult.Failed;
        }

        public static async Task<OperationResult> ModifyAccount(String id, String registrar, String proxy, String username, String password)
        {
            if (!ApiInformation.IsApiContractPresent("Windows.ApplicationModel.Calls.CallsVoipContract", 1))
            {
                return OperationResult.Failed;
            }

            AppServiceHelper appServiceHelper = new AppServiceHelper();

            ValueSet message = new ValueSet();
            message[ModifyAccountArguments.id.ToString()] = id;
            message[ModifyAccountArguments.registrar.ToString()] = registrar;
            message[ModifyAccountArguments.proxy.ToString()] = proxy;
            message[ModifyAccountArguments.username.ToString()] = username;
            message[ModifyAccountArguments.password.ToString()] = password;
            message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.ModifyAccount;

            ValueSet response = await appServiceHelper.SendMessageAsync(message);

            if (response != null)
            {
                return ((OperationResult)(response[BackgroundOperation.Result]));
            }
            return OperationResult.Failed;
        }
    }
}
