//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
using System;
using System.Threading.Tasks;
using VoipTasks.BackgroundOperations;
using Windows.Foundation.Collections;
using Windows.Foundation.Metadata;

namespace VoipUI.Helpers
{
    static class VoipCallHelper
    {
        public static async Task<OperationResult> NewOutgoingCallAsync(String dstURI)
        {
            if (!ApiInformation.IsApiContractPresent("Windows.ApplicationModel.Calls.CallsVoipContract", 1))
            {
                return OperationResult.Failed;
            }

            AppServiceHelper appServiceHelper = new AppServiceHelper();

            ValueSet message = new ValueSet();
            message[NewCallArguments.DstURI.ToString()] = dstURI;
            message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.NewOutgoingCall;            

            ValueSet response = await appServiceHelper.SendMessageAsync(message);

            if (response != null)
            {
                return ((OperationResult)(response[BackgroundOperation.Result]));
            }

            return OperationResult.Failed;
        }

        public static OperationResult EndCallAsync()
        {
            AppServiceHelper appServiceHelper = new AppServiceHelper();

            ValueSet message = new ValueSet();
            message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.EndCall;

            appServiceHelper.SendMessage(message);
            
            return OperationResult.Succeeded;
        }
    }
}
