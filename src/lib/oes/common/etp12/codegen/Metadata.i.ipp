#pragma once
// WARNING: DO NOT EDIT: SharedComponents/src/bin/etpgencpp Generated File

// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

namespace oes::common::etp12 {

const std::map<int, const ProtocolInfo>& protocols() {
    static const std::map<int, const ProtocolInfo> protocols__{
        { 0, { 0, "Core" } },
        { 1, { 1, "ChannelStreaming" } },
        { 2, { 2, "ChannelDataFrame" } },
        { 3, { 3, "Discovery" } },
        { 4, { 4, "Store" } },
        { 5, { 5, "StoreNotification" } },
        { 6, { 6, "GrowingObject" } },
        { 7, { 7, "GrowingObjectNotification" } },
        { 9, { 9, "DataArray" } },
        { 13, { 13, "DiscoveryQuery" } },
        { 14, { 14, "StoreQuery" } },
        { 16, { 16, "GrowingObjectQuery" } },
        { 18, { 18, "Transaction" } },
        { 21, { 21, "ChannelSubscribe" } },
        { 22, { 22, "ChannelDataLoad" } },
        { 24, { 24, "Dataspace" } },
        { 25, { 25, "SupportedTypes" } },
        { 2400, { 2400, "CoreOSDU" } },
        { 2404, { 2404, "StoreOSDU" } },
        { 2424, { 2424, "DataspaceOSDU" } },
    };
    return protocols__;
}

const std::map<std::pair<int, int>, const MessageInfo>& messages() {
    static const std::map<std::pair<int, int>, const MessageInfo> messages__{
        { { 0, 1 }, { 0, 1, false, "RequestSession", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 2 }, { 0, 2, false, "OpenSession", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 5 }, { 0, 5, false, "CloseSession", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 6 }, { 0, 6, false, "Authorize", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 7 }, { 0, 7, false, "AuthorizeResponse", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 8 }, { 0, 8, false, "Ping", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 9 }, { 0, 9, false, "Pong", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 1000 }, { 0, 1000, true, "ProtocolException", "Energistics.Etp.v12.Protocol.Core" } },
        { { 0, 1001 }, { 0, 1001, false, "Acknowledge", "Energistics.Etp.v12.Protocol.Core" } },
        { { 1, 1 }, { 1, 1, false, "ChannelMetadata", "Energistics.Etp.v12.Protocol.ChannelStreaming" } },
        { { 1, 2 }, { 1, 2, false, "ChannelData", "Energistics.Etp.v12.Protocol.ChannelStreaming" } },
        { { 1, 3 }, { 1, 3, false, "StartStreaming", "Energistics.Etp.v12.Protocol.ChannelStreaming" } },
        { { 1, 4 }, { 1, 4, false, "StopStreaming", "Energistics.Etp.v12.Protocol.ChannelStreaming" } },
        { { 1, 5 }, { 1, 5, false, "TruncateChannels", "Energistics.Etp.v12.Protocol.ChannelStreaming" } },
        { { 2, 1 }, { 2, 1, false, "GetFrameMetadata", "Energistics.Etp.v12.Protocol.ChannelDataFrame" } },
        { { 2, 2 }, { 2, 2, true, "GetFrameMetadataResponse", "Energistics.Etp.v12.Protocol.ChannelDataFrame" } },
        { { 2, 3 }, { 2, 3, false, "GetFrame", "Energistics.Etp.v12.Protocol.ChannelDataFrame" } },
        { { 2, 4 }, { 2, 4, true, "GetFrameResponseHeader", "Energistics.Etp.v12.Protocol.ChannelDataFrame" } },
        { { 2, 5 }, { 2, 5, false, "CancelGetFrame", "Energistics.Etp.v12.Protocol.ChannelDataFrame" } },
        { { 2, 6 }, { 2, 6, true, "GetFrameResponseRows", "Energistics.Etp.v12.Protocol.ChannelDataFrame" } },
        { { 3, 1 }, { 3, 1, false, "GetResources", "Energistics.Etp.v12.Protocol.Discovery" } },
        { { 3, 4 }, { 3, 4, true, "GetResourcesResponse", "Energistics.Etp.v12.Protocol.Discovery" } },
        { { 3, 5 }, { 3, 5, false, "GetDeletedResources", "Energistics.Etp.v12.Protocol.Discovery" } },
        { { 3, 6 }, { 3, 6, true, "GetDeletedResourcesResponse", "Energistics.Etp.v12.Protocol.Discovery" } },
        { { 3, 7 }, { 3, 7, true, "GetResourcesEdgesResponse", "Energistics.Etp.v12.Protocol.Discovery" } },
        { { 4, 1 }, { 4, 1, false, "GetDataObjects", "Energistics.Etp.v12.Protocol.Store" } },
        { { 4, 2 }, { 4, 2, true, "PutDataObjects", "Energistics.Etp.v12.Protocol.Store" } },
        { { 4, 3 }, { 4, 3, false, "DeleteDataObjects", "Energistics.Etp.v12.Protocol.Store" } },
        { { 4, 4 }, { 4, 4, true, "GetDataObjectsResponse", "Energistics.Etp.v12.Protocol.Store" } },
        { { 4, 8 }, { 4, 8, true, "Chunk", "Energistics.Etp.v12.Protocol.Store" } },
        { { 4, 9 }, { 4, 9, true, "PutDataObjectsResponse", "Energistics.Etp.v12.Protocol.Store" } },
        { { 4, 10 }, { 4, 10, true, "DeleteDataObjectsResponse", "Energistics.Etp.v12.Protocol.Store" } },
        { { 5, 2 }, { 5, 2, true, "ObjectChanged", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 3 }, { 5, 3, false, "ObjectDeleted", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 4 }, { 5, 4, false, "UnsubscribeNotifications", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 5 }, { 5, 5, false, "ObjectAccessRevoked", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 6 }, { 5, 6, false, "SubscribeNotifications", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 7 }, { 5, 7, false, "SubscriptionEnded", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 8 }, { 5, 8, false, "UnsolicitedStoreNotifications", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 9 }, { 5, 9, true, "Chunk", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 10 }, { 5, 10, true, "SubscribeNotificationsResponse", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 5, 11 }, { 5, 11, false, "ObjectActiveStatusChanged", "Energistics.Etp.v12.Protocol.StoreNotification" } },
        { { 6, 1 }, { 6, 1, false, "DeleteParts", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 3 }, { 6, 3, false, "GetParts", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 4 }, { 6, 4, false, "GetPartsByRange", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 5 }, { 6, 5, false, "PutParts", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 6 }, { 6, 6, true, "GetPartsResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 7 }, { 6, 7, true, "ReplacePartsByRange", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 8 }, { 6, 8, false, "GetPartsMetadata", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 9 }, { 6, 9, true, "GetPartsMetadataResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 10 }, { 6, 10, true, "GetPartsByRangeResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 11 }, { 6, 11, true, "DeletePartsResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 13 }, { 6, 13, true, "PutPartsResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 14 }, { 6, 14, false, "GetGrowingDataObjectsHeader", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 15 }, { 6, 15, true, "GetGrowingDataObjectsHeaderResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 16 }, { 6, 16, false, "PutGrowingDataObjectsHeader", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 17 }, { 6, 17, true, "PutGrowingDataObjectsHeaderResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 18 }, { 6, 18, false, "ReplacePartsByRangeResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 19 }, { 6, 19, false, "GetChangeAnnotations", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 6, 20 }, { 6, 20, true, "GetChangeAnnotationsResponse", "Energistics.Etp.v12.Protocol.GrowingObject" } },
        { { 7, 2 }, { 7, 2, false, "PartsChanged", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 3 }, { 7, 3, false, "PartsDeleted", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 4 }, { 7, 4, false, "UnsubscribePartNotification", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 6 }, { 7, 6, true, "PartsReplacedByRange", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 7 }, { 7, 7, false, "SubscribePartNotifications", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 8 }, { 7, 8, false, "PartSubscriptionEnded", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 9 }, { 7, 9, false, "UnsolicitedPartNotifications", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 7, 10 }, { 7, 10, true, "SubscribePartNotificationsResponse", "Energistics.Etp.v12.Protocol.GrowingObjectNotification" } },
        { { 9, 1 }, { 9, 1, true, "GetDataArraysResponse", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 2 }, { 9, 2, false, "GetDataArrays", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 3 }, { 9, 3, false, "GetDataSubarrays", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 4 }, { 9, 4, false, "PutDataArrays", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 5 }, { 9, 5, false, "PutDataSubarrays", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 6 }, { 9, 6, false, "GetDataArrayMetadata", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 7 }, { 9, 7, true, "GetDataArrayMetadataResponse", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 8 }, { 9, 8, true, "GetDataSubarraysResponse", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 9 }, { 9, 9, false, "PutUninitializedDataArrays", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 10 }, { 9, 10, true, "PutDataArraysResponse", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 11 }, { 9, 11, true, "PutDataSubarraysResponse", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 9, 12 }, { 9, 12, true, "PutUninitializedDataArraysResponse", "Energistics.Etp.v12.Protocol.DataArray" } },
        { { 13, 1 }, { 13, 1, false, "FindResources", "Energistics.Etp.v12.Protocol.DiscoveryQuery" } },
        { { 13, 2 }, { 13, 2, true, "FindResourcesResponse", "Energistics.Etp.v12.Protocol.DiscoveryQuery" } },
        { { 14, 1 }, { 14, 1, false, "FindDataObjects", "Energistics.Etp.v12.Protocol.StoreQuery" } },
        { { 14, 2 }, { 14, 2, true, "FindDataObjectsResponse", "Energistics.Etp.v12.Protocol.StoreQuery" } },
        { { 14, 3 }, { 14, 3, true, "Chunk", "Energistics.Etp.v12.Protocol.StoreQuery" } },
        { { 16, 1 }, { 16, 1, false, "FindParts", "Energistics.Etp.v12.Protocol.GrowingObjectQuery" } },
        { { 16, 2 }, { 16, 2, true, "FindPartsResponse", "Energistics.Etp.v12.Protocol.GrowingObjectQuery" } },
        { { 18, 1 }, { 18, 1, false, "StartTransaction", "Energistics.Etp.v12.Protocol.Transaction" } },
        { { 18, 2 }, { 18, 2, false, "StartTransactionResponse", "Energistics.Etp.v12.Protocol.Transaction" } },
        { { 18, 3 }, { 18, 3, false, "CommitTransaction", "Energistics.Etp.v12.Protocol.Transaction" } },
        { { 18, 4 }, { 18, 4, false, "RollbackTransaction", "Energistics.Etp.v12.Protocol.Transaction" } },
        { { 18, 5 }, { 18, 5, false, "CommitTransactionResponse", "Energistics.Etp.v12.Protocol.Transaction" } },
        { { 18, 6 }, { 18, 6, false, "RollbackTransactionResponse", "Energistics.Etp.v12.Protocol.Transaction" } },
        { { 21, 1 }, { 21, 1, false, "GetChannelMetadata", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 2 }, { 21, 2, true, "GetChannelMetadataResponse", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 3 }, { 21, 3, false, "SubscribeChannels", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 4 }, { 21, 4, false, "ChannelData", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 6 }, { 21, 6, true, "RangeReplaced", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 7 }, { 21, 7, false, "UnsubscribeChannels", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 8 }, { 21, 8, true, "SubscriptionsStopped", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 9 }, { 21, 9, false, "GetRanges", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 10 }, { 21, 10, true, "GetRangesResponse", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 11 }, { 21, 11, false, "CancelGetRanges", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 12 }, { 21, 12, true, "SubscribeChannelsResponse", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 13 }, { 21, 13, false, "ChannelsTruncated", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 14 }, { 21, 14, false, "GetChangeAnnotations", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 21, 15 }, { 21, 15, true, "GetChangeAnnotationsResponse", "Energistics.Etp.v12.Protocol.ChannelSubscribe" } },
        { { 22, 1 }, { 22, 1, false, "OpenChannels", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 2 }, { 22, 2, true, "OpenChannelsResponse", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 3 }, { 22, 3, false, "CloseChannels", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 4 }, { 22, 4, false, "ChannelData", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 6 }, { 22, 6, true, "ReplaceRange", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 7 }, { 22, 7, true, "ChannelsClosed", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 8 }, { 22, 8, false, "ReplaceRangeResponse", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 9 }, { 22, 9, false, "TruncateChannels", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 22, 10 }, { 22, 10, true, "TruncateChannelsResponse", "Energistics.Etp.v12.Protocol.ChannelDataLoad" } },
        { { 24, 1 }, { 24, 1, false, "GetDataspaces", "Energistics.Etp.v12.Protocol.Dataspace" } },
        { { 24, 2 }, { 24, 2, true, "GetDataspacesResponse", "Energistics.Etp.v12.Protocol.Dataspace" } },
        { { 24, 3 }, { 24, 3, false, "PutDataspaces", "Energistics.Etp.v12.Protocol.Dataspace" } },
        { { 24, 4 }, { 24, 4, false, "DeleteDataspaces", "Energistics.Etp.v12.Protocol.Dataspace" } },
        { { 24, 5 }, { 24, 5, true, "DeleteDataspacesResponse", "Energistics.Etp.v12.Protocol.Dataspace" } },
        { { 24, 6 }, { 24, 6, true, "PutDataspacesResponse", "Energistics.Etp.v12.Protocol.Dataspace" } },
        { { 25, 1 }, { 25, 1, false, "GetSupportedTypes", "Energistics.Etp.v12.Protocol.SupportedTypes" } },
        { { 25, 2 }, { 25, 2, true, "GetSupportedTypesResponse", "Energistics.Etp.v12.Protocol.SupportedTypes" } },
        { { 2400, 1 }, { 2400, 1, false, "ResumeSession", "Energistics.Etp.v12.Protocol.CoreOSDU" } },
        { { 2400, 2 }, { 2400, 2, false, "ResumeSessionResponse", "Energistics.Etp.v12.Protocol.CoreOSDU" } },
        { { 2404, 1 }, { 2404, 1, false, "CopyDataObjectsByValue", "Energistics.Etp.v12.Protocol.StoreOSDU" } },
        { { 2404, 2 }, { 2404, 2, true, "CopyDataObjectsByValueResponse", "Energistics.Etp.v12.Protocol.StoreOSDU" } },
        { { 2424, 1 }, { 2424, 1, false, "GetDataspaceInfo", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 2 }, { 2424, 2, true, "GetDataspaceInfoResponse", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 3 }, { 2424, 3, false, "CopyDataspacesContent", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 4 }, { 2424, 4, true, "CopyDataspacesContentResponse", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 5 }, { 2424, 5, false, "LockDataspaces", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 6 }, { 2424, 6, false, "LockDataspacesResponse", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 7 }, { 2424, 7, false, "CopyToDataspace", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 8 }, { 2424, 8, true, "CopyToDataspaceResponse", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 9 }, { 2424, 9, false, "CopyDataspaceFromRemote", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
        { { 2424, 10 }, { 2424, 10, true, "CopyDataspaceFromRemoteResponse", "Energistics.Etp.v12.Protocol.DataspaceOSDU" } },
    };
    return messages__;
}

const std::map<std::pair<int, int>, std::pair<int, int>>& apis() {
    static const std::map<std::pair<int, int>, std::pair<int, int>>& apis__{
        { { 0, 1 }, { 0, 2 } },
        { { 0, 5 }, { -1, -1 } },
        { { 0, 6 }, { 0, 7 } },
        { { 0, 8 }, { 0, 9 } },
        { { 1, 3 }, { 1, 1 } },
        { { 1, 4 }, { -1, -1 } },
        { { 2, 1 }, { 2, 2 } },
        { { 2, 3 }, { 2, 4 } },
        { { 2, 5 }, { -1, -1 } },
        { { 3, 1 }, { 3, 4 } },
        { { 3, 5 }, { 3, 6 } },
        { { 4, 1 }, { 4, 4 } },
        { { 4, 2 }, { 4, 9 } },
        { { 4, 3 }, { 4, 10 } },
        { { 5, 4 }, { -1, -1 } },
        { { 5, 6 }, { 5, 10 } },
        { { 6, 1 }, { 6, 11 } },
        { { 6, 3 }, { 6, 6 } },
        { { 6, 4 }, { 6, 10 } },
        { { 6, 5 }, { 6, 13 } },
        { { 6, 7 }, { 6, 18 } },
        { { 6, 8 }, { 6, 9 } },
        { { 6, 14 }, { 6, 15 } },
        { { 6, 16 }, { 6, 17 } },
        { { 6, 19 }, { 6, 20 } },
        { { 7, 4 }, { 7, 8 } },
        { { 7, 7 }, { 7, 10 } },
        { { 9, 2 }, { 9, 1 } },
        { { 9, 3 }, { 9, 8 } },
        { { 9, 4 }, { 9, 10 } },
        { { 9, 5 }, { 9, 11 } },
        { { 9, 6 }, { 9, 7 } },
        { { 9, 9 }, { 9, 12 } },
        { { 13, 1 }, { 13, 2 } },
        { { 14, 1 }, { 14, 2 } },
        { { 16, 1 }, { 16, 2 } },
        { { 18, 1 }, { 18, 2 } },
        { { 18, 3 }, { 18, 5 } },
        { { 18, 4 }, { 18, 6 } },
        { { 21, 1 }, { 21, 2 } },
        { { 21, 3 }, { 21, 12 } },
        { { 21, 7 }, { 21, 8 } },
        { { 21, 9 }, { 21, 10 } },
        { { 21, 11 }, { 21, 10 } },
        { { 21, 14 }, { 21, 15 } },
        { { 22, 1 }, { 22, 2 } },
        { { 22, 3 }, { 22, 7 } },
        { { 22, 4 }, { -1, -1 } },
        { { 22, 6 }, { 22, 8 } },
        { { 22, 9 }, { 22, 10 } },
        { { 24, 1 }, { 24, 2 } },
        { { 24, 3 }, { 24, 6 } },
        { { 24, 4 }, { 24, 5 } },
        { { 25, 1 }, { 25, 2 } },
        { { 2400, 1 }, { 2400, 2 } },
        { { 2404, 1 }, { 2404, 2 } },
        { { 2424, 1 }, { 2424, 2 } },
        { { 2424, 3 }, { 2424, 4 } },
        { { 2424, 5 }, { 2424, 6 } },
        { { 2424, 7 }, { 2424, 8 } },
        { { 2424, 9 }, { 2424, 10 } },
    };
    return apis__;
}

avro::ValidSchema schemaOf(int protocolId, int messageTypeId) {
    switch (protocolId) {

    case int(Datatypes::Protocol::eCore): // 0
        switch (messageTypeId) {
        case Protocol::Core::RequestSession::info.type_.type_: // 1
            return Protocol::Core::RequestSession::schema();
        case Protocol::Core::OpenSession::info.type_.type_: // 2
            return Protocol::Core::OpenSession::schema();
        case Protocol::Core::CloseSession::info.type_.type_: // 5
            return Protocol::Core::CloseSession::schema();
        case Protocol::Core::Authorize::info.type_.type_: // 6
            return Protocol::Core::Authorize::schema();
        case Protocol::Core::AuthorizeResponse::info.type_.type_: // 7
            return Protocol::Core::AuthorizeResponse::schema();
        case Protocol::Core::Ping::info.type_.type_: // 8
            return Protocol::Core::Ping::schema();
        case Protocol::Core::Pong::info.type_.type_: // 9
            return Protocol::Core::Pong::schema();
        case Protocol::Core::ProtocolException::info.type_.type_: // 1000
            return Protocol::Core::ProtocolException::schema();
        case Protocol::Core::Acknowledge::info.type_.type_: // 1001
            return Protocol::Core::Acknowledge::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelStreaming): // 1
        switch (messageTypeId) {
        case Protocol::ChannelStreaming::ChannelMetadata::info.type_.type_: // 1
            return Protocol::ChannelStreaming::ChannelMetadata::schema();
        case Protocol::ChannelStreaming::ChannelData::info.type_.type_: // 2
            return Protocol::ChannelStreaming::ChannelData::schema();
        case Protocol::ChannelStreaming::StartStreaming::info.type_.type_: // 3
            return Protocol::ChannelStreaming::StartStreaming::schema();
        case Protocol::ChannelStreaming::StopStreaming::info.type_.type_: // 4
            return Protocol::ChannelStreaming::StopStreaming::schema();
        case Protocol::ChannelStreaming::TruncateChannels::info.type_.type_: // 5
            return Protocol::ChannelStreaming::TruncateChannels::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelDataFrame): // 2
        switch (messageTypeId) {
        case Protocol::ChannelDataFrame::GetFrameMetadata::info.type_.type_: // 1
            return Protocol::ChannelDataFrame::GetFrameMetadata::schema();
        case Protocol::ChannelDataFrame::GetFrameMetadataResponse::info.type_.type_: // 2
            return Protocol::ChannelDataFrame::GetFrameMetadataResponse::schema();
        case Protocol::ChannelDataFrame::GetFrame::info.type_.type_: // 3
            return Protocol::ChannelDataFrame::GetFrame::schema();
        case Protocol::ChannelDataFrame::GetFrameResponseHeader::info.type_.type_: // 4
            return Protocol::ChannelDataFrame::GetFrameResponseHeader::schema();
        case Protocol::ChannelDataFrame::CancelGetFrame::info.type_.type_: // 5
            return Protocol::ChannelDataFrame::CancelGetFrame::schema();
        case Protocol::ChannelDataFrame::GetFrameResponseRows::info.type_.type_: // 6
            return Protocol::ChannelDataFrame::GetFrameResponseRows::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDiscovery): // 3
        switch (messageTypeId) {
        case Protocol::Discovery::GetResources::info.type_.type_: // 1
            return Protocol::Discovery::GetResources::schema();
        case Protocol::Discovery::GetResourcesResponse::info.type_.type_: // 4
            return Protocol::Discovery::GetResourcesResponse::schema();
        case Protocol::Discovery::GetDeletedResources::info.type_.type_: // 5
            return Protocol::Discovery::GetDeletedResources::schema();
        case Protocol::Discovery::GetDeletedResourcesResponse::info.type_.type_: // 6
            return Protocol::Discovery::GetDeletedResourcesResponse::schema();
        case Protocol::Discovery::GetResourcesEdgesResponse::info.type_.type_: // 7
            return Protocol::Discovery::GetResourcesEdgesResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStore): // 4
        switch (messageTypeId) {
        case Protocol::Store::GetDataObjects::info.type_.type_: // 1
            return Protocol::Store::GetDataObjects::schema();
        case Protocol::Store::PutDataObjects::info.type_.type_: // 2
            return Protocol::Store::PutDataObjects::schema();
        case Protocol::Store::DeleteDataObjects::info.type_.type_: // 3
            return Protocol::Store::DeleteDataObjects::schema();
        case Protocol::Store::GetDataObjectsResponse::info.type_.type_: // 4
            return Protocol::Store::GetDataObjectsResponse::schema();
        case Protocol::Store::Chunk::info.type_.type_: // 8
            return Protocol::Store::Chunk::schema();
        case Protocol::Store::PutDataObjectsResponse::info.type_.type_: // 9
            return Protocol::Store::PutDataObjectsResponse::schema();
        case Protocol::Store::DeleteDataObjectsResponse::info.type_.type_: // 10
            return Protocol::Store::DeleteDataObjectsResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreNotification): // 5
        switch (messageTypeId) {
        case Protocol::StoreNotification::ObjectChanged::info.type_.type_: // 2
            return Protocol::StoreNotification::ObjectChanged::schema();
        case Protocol::StoreNotification::ObjectDeleted::info.type_.type_: // 3
            return Protocol::StoreNotification::ObjectDeleted::schema();
        case Protocol::StoreNotification::UnsubscribeNotifications::info.type_.type_: // 4
            return Protocol::StoreNotification::UnsubscribeNotifications::schema();
        case Protocol::StoreNotification::ObjectAccessRevoked::info.type_.type_: // 5
            return Protocol::StoreNotification::ObjectAccessRevoked::schema();
        case Protocol::StoreNotification::SubscribeNotifications::info.type_.type_: // 6
            return Protocol::StoreNotification::SubscribeNotifications::schema();
        case Protocol::StoreNotification::SubscriptionEnded::info.type_.type_: // 7
            return Protocol::StoreNotification::SubscriptionEnded::schema();
        case Protocol::StoreNotification::UnsolicitedStoreNotifications::info.type_.type_: // 8
            return Protocol::StoreNotification::UnsolicitedStoreNotifications::schema();
        case Protocol::StoreNotification::Chunk::info.type_.type_: // 9
            return Protocol::StoreNotification::Chunk::schema();
        case Protocol::StoreNotification::SubscribeNotificationsResponse::info.type_.type_: // 10
            return Protocol::StoreNotification::SubscribeNotificationsResponse::schema();
        case Protocol::StoreNotification::ObjectActiveStatusChanged::info.type_.type_: // 11
            return Protocol::StoreNotification::ObjectActiveStatusChanged::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObject): // 6
        switch (messageTypeId) {
        case Protocol::GrowingObject::DeleteParts::info.type_.type_: // 1
            return Protocol::GrowingObject::DeleteParts::schema();
        case Protocol::GrowingObject::GetParts::info.type_.type_: // 3
            return Protocol::GrowingObject::GetParts::schema();
        case Protocol::GrowingObject::GetPartsByRange::info.type_.type_: // 4
            return Protocol::GrowingObject::GetPartsByRange::schema();
        case Protocol::GrowingObject::PutParts::info.type_.type_: // 5
            return Protocol::GrowingObject::PutParts::schema();
        case Protocol::GrowingObject::GetPartsResponse::info.type_.type_: // 6
            return Protocol::GrowingObject::GetPartsResponse::schema();
        case Protocol::GrowingObject::ReplacePartsByRange::info.type_.type_: // 7
            return Protocol::GrowingObject::ReplacePartsByRange::schema();
        case Protocol::GrowingObject::GetPartsMetadata::info.type_.type_: // 8
            return Protocol::GrowingObject::GetPartsMetadata::schema();
        case Protocol::GrowingObject::GetPartsMetadataResponse::info.type_.type_: // 9
            return Protocol::GrowingObject::GetPartsMetadataResponse::schema();
        case Protocol::GrowingObject::GetPartsByRangeResponse::info.type_.type_: // 10
            return Protocol::GrowingObject::GetPartsByRangeResponse::schema();
        case Protocol::GrowingObject::DeletePartsResponse::info.type_.type_: // 11
            return Protocol::GrowingObject::DeletePartsResponse::schema();
        case Protocol::GrowingObject::PutPartsResponse::info.type_.type_: // 13
            return Protocol::GrowingObject::PutPartsResponse::schema();
        case Protocol::GrowingObject::GetGrowingDataObjectsHeader::info.type_.type_: // 14
            return Protocol::GrowingObject::GetGrowingDataObjectsHeader::schema();
        case Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse::info.type_.type_: // 15
            return Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse::schema();
        case Protocol::GrowingObject::PutGrowingDataObjectsHeader::info.type_.type_: // 16
            return Protocol::GrowingObject::PutGrowingDataObjectsHeader::schema();
        case Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse::info.type_.type_: // 17
            return Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse::schema();
        case Protocol::GrowingObject::ReplacePartsByRangeResponse::info.type_.type_: // 18
            return Protocol::GrowingObject::ReplacePartsByRangeResponse::schema();
        case Protocol::GrowingObject::GetChangeAnnotations::info.type_.type_: // 19
            return Protocol::GrowingObject::GetChangeAnnotations::schema();
        case Protocol::GrowingObject::GetChangeAnnotationsResponse::info.type_.type_: // 20
            return Protocol::GrowingObject::GetChangeAnnotationsResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObjectNotification): // 7
        switch (messageTypeId) {
        case Protocol::GrowingObjectNotification::PartsChanged::info.type_.type_: // 2
            return Protocol::GrowingObjectNotification::PartsChanged::schema();
        case Protocol::GrowingObjectNotification::PartsDeleted::info.type_.type_: // 3
            return Protocol::GrowingObjectNotification::PartsDeleted::schema();
        case Protocol::GrowingObjectNotification::UnsubscribePartNotification::info.type_.type_: // 4
            return Protocol::GrowingObjectNotification::UnsubscribePartNotification::schema();
        case Protocol::GrowingObjectNotification::PartsReplacedByRange::info.type_.type_: // 6
            return Protocol::GrowingObjectNotification::PartsReplacedByRange::schema();
        case Protocol::GrowingObjectNotification::SubscribePartNotifications::info.type_.type_: // 7
            return Protocol::GrowingObjectNotification::SubscribePartNotifications::schema();
        case Protocol::GrowingObjectNotification::PartSubscriptionEnded::info.type_.type_: // 8
            return Protocol::GrowingObjectNotification::PartSubscriptionEnded::schema();
        case Protocol::GrowingObjectNotification::UnsolicitedPartNotifications::info.type_.type_: // 9
            return Protocol::GrowingObjectNotification::UnsolicitedPartNotifications::schema();
        case Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse::info.type_.type_: // 10
            return Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataArray): // 9
        switch (messageTypeId) {
        case Protocol::DataArray::GetDataArraysResponse::info.type_.type_: // 1
            return Protocol::DataArray::GetDataArraysResponse::schema();
        case Protocol::DataArray::GetDataArrays::info.type_.type_: // 2
            return Protocol::DataArray::GetDataArrays::schema();
        case Protocol::DataArray::GetDataSubarrays::info.type_.type_: // 3
            return Protocol::DataArray::GetDataSubarrays::schema();
        case Protocol::DataArray::PutDataArrays::info.type_.type_: // 4
            return Protocol::DataArray::PutDataArrays::schema();
        case Protocol::DataArray::PutDataSubarrays::info.type_.type_: // 5
            return Protocol::DataArray::PutDataSubarrays::schema();
        case Protocol::DataArray::GetDataArrayMetadata::info.type_.type_: // 6
            return Protocol::DataArray::GetDataArrayMetadata::schema();
        case Protocol::DataArray::GetDataArrayMetadataResponse::info.type_.type_: // 7
            return Protocol::DataArray::GetDataArrayMetadataResponse::schema();
        case Protocol::DataArray::GetDataSubarraysResponse::info.type_.type_: // 8
            return Protocol::DataArray::GetDataSubarraysResponse::schema();
        case Protocol::DataArray::PutUninitializedDataArrays::info.type_.type_: // 9
            return Protocol::DataArray::PutUninitializedDataArrays::schema();
        case Protocol::DataArray::PutDataArraysResponse::info.type_.type_: // 10
            return Protocol::DataArray::PutDataArraysResponse::schema();
        case Protocol::DataArray::PutDataSubarraysResponse::info.type_.type_: // 11
            return Protocol::DataArray::PutDataSubarraysResponse::schema();
        case Protocol::DataArray::PutUninitializedDataArraysResponse::info.type_.type_: // 12
            return Protocol::DataArray::PutUninitializedDataArraysResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDiscoveryQuery): // 13
        switch (messageTypeId) {
        case Protocol::DiscoveryQuery::FindResources::info.type_.type_: // 1
            return Protocol::DiscoveryQuery::FindResources::schema();
        case Protocol::DiscoveryQuery::FindResourcesResponse::info.type_.type_: // 2
            return Protocol::DiscoveryQuery::FindResourcesResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreQuery): // 14
        switch (messageTypeId) {
        case Protocol::StoreQuery::FindDataObjects::info.type_.type_: // 1
            return Protocol::StoreQuery::FindDataObjects::schema();
        case Protocol::StoreQuery::FindDataObjectsResponse::info.type_.type_: // 2
            return Protocol::StoreQuery::FindDataObjectsResponse::schema();
        case Protocol::StoreQuery::Chunk::info.type_.type_: // 3
            return Protocol::StoreQuery::Chunk::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObjectQuery): // 16
        switch (messageTypeId) {
        case Protocol::GrowingObjectQuery::FindParts::info.type_.type_: // 1
            return Protocol::GrowingObjectQuery::FindParts::schema();
        case Protocol::GrowingObjectQuery::FindPartsResponse::info.type_.type_: // 2
            return Protocol::GrowingObjectQuery::FindPartsResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eTransaction): // 18
        switch (messageTypeId) {
        case Protocol::Transaction::StartTransaction::info.type_.type_: // 1
            return Protocol::Transaction::StartTransaction::schema();
        case Protocol::Transaction::StartTransactionResponse::info.type_.type_: // 2
            return Protocol::Transaction::StartTransactionResponse::schema();
        case Protocol::Transaction::CommitTransaction::info.type_.type_: // 3
            return Protocol::Transaction::CommitTransaction::schema();
        case Protocol::Transaction::RollbackTransaction::info.type_.type_: // 4
            return Protocol::Transaction::RollbackTransaction::schema();
        case Protocol::Transaction::CommitTransactionResponse::info.type_.type_: // 5
            return Protocol::Transaction::CommitTransactionResponse::schema();
        case Protocol::Transaction::RollbackTransactionResponse::info.type_.type_: // 6
            return Protocol::Transaction::RollbackTransactionResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelSubscribe): // 21
        switch (messageTypeId) {
        case Protocol::ChannelSubscribe::GetChannelMetadata::info.type_.type_: // 1
            return Protocol::ChannelSubscribe::GetChannelMetadata::schema();
        case Protocol::ChannelSubscribe::GetChannelMetadataResponse::info.type_.type_: // 2
            return Protocol::ChannelSubscribe::GetChannelMetadataResponse::schema();
        case Protocol::ChannelSubscribe::SubscribeChannels::info.type_.type_: // 3
            return Protocol::ChannelSubscribe::SubscribeChannels::schema();
        case Protocol::ChannelSubscribe::ChannelData::info.type_.type_: // 4
            return Protocol::ChannelSubscribe::ChannelData::schema();
        case Protocol::ChannelSubscribe::RangeReplaced::info.type_.type_: // 6
            return Protocol::ChannelSubscribe::RangeReplaced::schema();
        case Protocol::ChannelSubscribe::UnsubscribeChannels::info.type_.type_: // 7
            return Protocol::ChannelSubscribe::UnsubscribeChannels::schema();
        case Protocol::ChannelSubscribe::SubscriptionsStopped::info.type_.type_: // 8
            return Protocol::ChannelSubscribe::SubscriptionsStopped::schema();
        case Protocol::ChannelSubscribe::GetRanges::info.type_.type_: // 9
            return Protocol::ChannelSubscribe::GetRanges::schema();
        case Protocol::ChannelSubscribe::GetRangesResponse::info.type_.type_: // 10
            return Protocol::ChannelSubscribe::GetRangesResponse::schema();
        case Protocol::ChannelSubscribe::CancelGetRanges::info.type_.type_: // 11
            return Protocol::ChannelSubscribe::CancelGetRanges::schema();
        case Protocol::ChannelSubscribe::SubscribeChannelsResponse::info.type_.type_: // 12
            return Protocol::ChannelSubscribe::SubscribeChannelsResponse::schema();
        case Protocol::ChannelSubscribe::ChannelsTruncated::info.type_.type_: // 13
            return Protocol::ChannelSubscribe::ChannelsTruncated::schema();
        case Protocol::ChannelSubscribe::GetChangeAnnotations::info.type_.type_: // 14
            return Protocol::ChannelSubscribe::GetChangeAnnotations::schema();
        case Protocol::ChannelSubscribe::GetChangeAnnotationsResponse::info.type_.type_: // 15
            return Protocol::ChannelSubscribe::GetChangeAnnotationsResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelDataLoad): // 22
        switch (messageTypeId) {
        case Protocol::ChannelDataLoad::OpenChannels::info.type_.type_: // 1
            return Protocol::ChannelDataLoad::OpenChannels::schema();
        case Protocol::ChannelDataLoad::OpenChannelsResponse::info.type_.type_: // 2
            return Protocol::ChannelDataLoad::OpenChannelsResponse::schema();
        case Protocol::ChannelDataLoad::CloseChannels::info.type_.type_: // 3
            return Protocol::ChannelDataLoad::CloseChannels::schema();
        case Protocol::ChannelDataLoad::ChannelData::info.type_.type_: // 4
            return Protocol::ChannelDataLoad::ChannelData::schema();
        case Protocol::ChannelDataLoad::ReplaceRange::info.type_.type_: // 6
            return Protocol::ChannelDataLoad::ReplaceRange::schema();
        case Protocol::ChannelDataLoad::ChannelsClosed::info.type_.type_: // 7
            return Protocol::ChannelDataLoad::ChannelsClosed::schema();
        case Protocol::ChannelDataLoad::ReplaceRangeResponse::info.type_.type_: // 8
            return Protocol::ChannelDataLoad::ReplaceRangeResponse::schema();
        case Protocol::ChannelDataLoad::TruncateChannels::info.type_.type_: // 9
            return Protocol::ChannelDataLoad::TruncateChannels::schema();
        case Protocol::ChannelDataLoad::TruncateChannelsResponse::info.type_.type_: // 10
            return Protocol::ChannelDataLoad::TruncateChannelsResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataspace): // 24
        switch (messageTypeId) {
        case Protocol::Dataspace::GetDataspaces::info.type_.type_: // 1
            return Protocol::Dataspace::GetDataspaces::schema();
        case Protocol::Dataspace::GetDataspacesResponse::info.type_.type_: // 2
            return Protocol::Dataspace::GetDataspacesResponse::schema();
        case Protocol::Dataspace::PutDataspaces::info.type_.type_: // 3
            return Protocol::Dataspace::PutDataspaces::schema();
        case Protocol::Dataspace::DeleteDataspaces::info.type_.type_: // 4
            return Protocol::Dataspace::DeleteDataspaces::schema();
        case Protocol::Dataspace::DeleteDataspacesResponse::info.type_.type_: // 5
            return Protocol::Dataspace::DeleteDataspacesResponse::schema();
        case Protocol::Dataspace::PutDataspacesResponse::info.type_.type_: // 6
            return Protocol::Dataspace::PutDataspacesResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eSupportedTypes): // 25
        switch (messageTypeId) {
        case Protocol::SupportedTypes::GetSupportedTypes::info.type_.type_: // 1
            return Protocol::SupportedTypes::GetSupportedTypes::schema();
        case Protocol::SupportedTypes::GetSupportedTypesResponse::info.type_.type_: // 2
            return Protocol::SupportedTypes::GetSupportedTypesResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eCoreOSDU): // 2400
        switch (messageTypeId) {
        case Protocol::CoreOSDU::ResumeSession::info.type_.type_: // 1
            return Protocol::CoreOSDU::ResumeSession::schema();
        case Protocol::CoreOSDU::ResumeSessionResponse::info.type_.type_: // 2
            return Protocol::CoreOSDU::ResumeSessionResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreOSDU): // 2404
        switch (messageTypeId) {
        case Protocol::StoreOSDU::CopyDataObjectsByValue::info.type_.type_: // 1
            return Protocol::StoreOSDU::CopyDataObjectsByValue::schema();
        case Protocol::StoreOSDU::CopyDataObjectsByValueResponse::info.type_.type_: // 2
            return Protocol::StoreOSDU::CopyDataObjectsByValueResponse::schema();
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataspaceOSDU): // 2424
        switch (messageTypeId) {
        case Protocol::DataspaceOSDU::GetDataspaceInfo::info.type_.type_: // 1
            return Protocol::DataspaceOSDU::GetDataspaceInfo::schema();
        case Protocol::DataspaceOSDU::GetDataspaceInfoResponse::info.type_.type_: // 2
            return Protocol::DataspaceOSDU::GetDataspaceInfoResponse::schema();
        case Protocol::DataspaceOSDU::CopyDataspacesContent::info.type_.type_: // 3
            return Protocol::DataspaceOSDU::CopyDataspacesContent::schema();
        case Protocol::DataspaceOSDU::CopyDataspacesContentResponse::info.type_.type_: // 4
            return Protocol::DataspaceOSDU::CopyDataspacesContentResponse::schema();
        case Protocol::DataspaceOSDU::LockDataspaces::info.type_.type_: // 5
            return Protocol::DataspaceOSDU::LockDataspaces::schema();
        case Protocol::DataspaceOSDU::LockDataspacesResponse::info.type_.type_: // 6
            return Protocol::DataspaceOSDU::LockDataspacesResponse::schema();
        case Protocol::DataspaceOSDU::CopyToDataspace::info.type_.type_: // 7
            return Protocol::DataspaceOSDU::CopyToDataspace::schema();
        case Protocol::DataspaceOSDU::CopyToDataspaceResponse::info.type_.type_: // 8
            return Protocol::DataspaceOSDU::CopyToDataspaceResponse::schema();
        case Protocol::DataspaceOSDU::CopyDataspaceFromRemote::info.type_.type_: // 9
            return Protocol::DataspaceOSDU::CopyDataspaceFromRemote::schema();
        case Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse::info.type_.type_: // 10
            return Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse::schema();
        default:
            break;
        }
        break;

    default:
        break;
    }
    return avro::ValidSchema(); // invalid/empty schema
}

bool recodeMessage(int protocolId, int messageTypeId, avro::Decoder& dec, avro::Encoder& enc) {
    switch (protocolId) {

    case int(Datatypes::Protocol::eCore): // 0
        switch (messageTypeId) {
        case Protocol::Core::RequestSession::info.type_.type_: // 1
            return recode<Protocol::Core::RequestSession>(dec, enc);
        case Protocol::Core::OpenSession::info.type_.type_: // 2
            return recode<Protocol::Core::OpenSession>(dec, enc);
        case Protocol::Core::CloseSession::info.type_.type_: // 5
            return recode<Protocol::Core::CloseSession>(dec, enc);
        case Protocol::Core::Authorize::info.type_.type_: // 6
            return recode<Protocol::Core::Authorize>(dec, enc);
        case Protocol::Core::AuthorizeResponse::info.type_.type_: // 7
            return recode<Protocol::Core::AuthorizeResponse>(dec, enc);
        case Protocol::Core::Ping::info.type_.type_: // 8
            return recode<Protocol::Core::Ping>(dec, enc);
        case Protocol::Core::Pong::info.type_.type_: // 9
            return recode<Protocol::Core::Pong>(dec, enc);
        case Protocol::Core::ProtocolException::info.type_.type_: // 1000
            return recode<Protocol::Core::ProtocolException>(dec, enc);
        case Protocol::Core::Acknowledge::info.type_.type_: // 1001
            return recode<Protocol::Core::Acknowledge>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelStreaming): // 1
        switch (messageTypeId) {
        case Protocol::ChannelStreaming::ChannelMetadata::info.type_.type_: // 1
            return recode<Protocol::ChannelStreaming::ChannelMetadata>(dec, enc);
        case Protocol::ChannelStreaming::ChannelData::info.type_.type_: // 2
            return recode<Protocol::ChannelStreaming::ChannelData>(dec, enc);
        case Protocol::ChannelStreaming::StartStreaming::info.type_.type_: // 3
            return recode<Protocol::ChannelStreaming::StartStreaming>(dec, enc);
        case Protocol::ChannelStreaming::StopStreaming::info.type_.type_: // 4
            return recode<Protocol::ChannelStreaming::StopStreaming>(dec, enc);
        case Protocol::ChannelStreaming::TruncateChannels::info.type_.type_: // 5
            return recode<Protocol::ChannelStreaming::TruncateChannels>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelDataFrame): // 2
        switch (messageTypeId) {
        case Protocol::ChannelDataFrame::GetFrameMetadata::info.type_.type_: // 1
            return recode<Protocol::ChannelDataFrame::GetFrameMetadata>(dec, enc);
        case Protocol::ChannelDataFrame::GetFrameMetadataResponse::info.type_.type_: // 2
            return recode<Protocol::ChannelDataFrame::GetFrameMetadataResponse>(dec, enc);
        case Protocol::ChannelDataFrame::GetFrame::info.type_.type_: // 3
            return recode<Protocol::ChannelDataFrame::GetFrame>(dec, enc);
        case Protocol::ChannelDataFrame::GetFrameResponseHeader::info.type_.type_: // 4
            return recode<Protocol::ChannelDataFrame::GetFrameResponseHeader>(dec, enc);
        case Protocol::ChannelDataFrame::CancelGetFrame::info.type_.type_: // 5
            return recode<Protocol::ChannelDataFrame::CancelGetFrame>(dec, enc);
        case Protocol::ChannelDataFrame::GetFrameResponseRows::info.type_.type_: // 6
            return recode<Protocol::ChannelDataFrame::GetFrameResponseRows>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDiscovery): // 3
        switch (messageTypeId) {
        case Protocol::Discovery::GetResources::info.type_.type_: // 1
            return recode<Protocol::Discovery::GetResources>(dec, enc);
        case Protocol::Discovery::GetResourcesResponse::info.type_.type_: // 4
            return recode<Protocol::Discovery::GetResourcesResponse>(dec, enc);
        case Protocol::Discovery::GetDeletedResources::info.type_.type_: // 5
            return recode<Protocol::Discovery::GetDeletedResources>(dec, enc);
        case Protocol::Discovery::GetDeletedResourcesResponse::info.type_.type_: // 6
            return recode<Protocol::Discovery::GetDeletedResourcesResponse>(dec, enc);
        case Protocol::Discovery::GetResourcesEdgesResponse::info.type_.type_: // 7
            return recode<Protocol::Discovery::GetResourcesEdgesResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStore): // 4
        switch (messageTypeId) {
        case Protocol::Store::GetDataObjects::info.type_.type_: // 1
            return recode<Protocol::Store::GetDataObjects>(dec, enc);
        case Protocol::Store::PutDataObjects::info.type_.type_: // 2
            return recode<Protocol::Store::PutDataObjects>(dec, enc);
        case Protocol::Store::DeleteDataObjects::info.type_.type_: // 3
            return recode<Protocol::Store::DeleteDataObjects>(dec, enc);
        case Protocol::Store::GetDataObjectsResponse::info.type_.type_: // 4
            return recode<Protocol::Store::GetDataObjectsResponse>(dec, enc);
        case Protocol::Store::Chunk::info.type_.type_: // 8
            return recode<Protocol::Store::Chunk>(dec, enc);
        case Protocol::Store::PutDataObjectsResponse::info.type_.type_: // 9
            return recode<Protocol::Store::PutDataObjectsResponse>(dec, enc);
        case Protocol::Store::DeleteDataObjectsResponse::info.type_.type_: // 10
            return recode<Protocol::Store::DeleteDataObjectsResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreNotification): // 5
        switch (messageTypeId) {
        case Protocol::StoreNotification::ObjectChanged::info.type_.type_: // 2
            return recode<Protocol::StoreNotification::ObjectChanged>(dec, enc);
        case Protocol::StoreNotification::ObjectDeleted::info.type_.type_: // 3
            return recode<Protocol::StoreNotification::ObjectDeleted>(dec, enc);
        case Protocol::StoreNotification::UnsubscribeNotifications::info.type_.type_: // 4
            return recode<Protocol::StoreNotification::UnsubscribeNotifications>(dec, enc);
        case Protocol::StoreNotification::ObjectAccessRevoked::info.type_.type_: // 5
            return recode<Protocol::StoreNotification::ObjectAccessRevoked>(dec, enc);
        case Protocol::StoreNotification::SubscribeNotifications::info.type_.type_: // 6
            return recode<Protocol::StoreNotification::SubscribeNotifications>(dec, enc);
        case Protocol::StoreNotification::SubscriptionEnded::info.type_.type_: // 7
            return recode<Protocol::StoreNotification::SubscriptionEnded>(dec, enc);
        case Protocol::StoreNotification::UnsolicitedStoreNotifications::info.type_.type_: // 8
            return recode<Protocol::StoreNotification::UnsolicitedStoreNotifications>(dec, enc);
        case Protocol::StoreNotification::Chunk::info.type_.type_: // 9
            return recode<Protocol::StoreNotification::Chunk>(dec, enc);
        case Protocol::StoreNotification::SubscribeNotificationsResponse::info.type_.type_: // 10
            return recode<Protocol::StoreNotification::SubscribeNotificationsResponse>(dec, enc);
        case Protocol::StoreNotification::ObjectActiveStatusChanged::info.type_.type_: // 11
            return recode<Protocol::StoreNotification::ObjectActiveStatusChanged>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObject): // 6
        switch (messageTypeId) {
        case Protocol::GrowingObject::DeleteParts::info.type_.type_: // 1
            return recode<Protocol::GrowingObject::DeleteParts>(dec, enc);
        case Protocol::GrowingObject::GetParts::info.type_.type_: // 3
            return recode<Protocol::GrowingObject::GetParts>(dec, enc);
        case Protocol::GrowingObject::GetPartsByRange::info.type_.type_: // 4
            return recode<Protocol::GrowingObject::GetPartsByRange>(dec, enc);
        case Protocol::GrowingObject::PutParts::info.type_.type_: // 5
            return recode<Protocol::GrowingObject::PutParts>(dec, enc);
        case Protocol::GrowingObject::GetPartsResponse::info.type_.type_: // 6
            return recode<Protocol::GrowingObject::GetPartsResponse>(dec, enc);
        case Protocol::GrowingObject::ReplacePartsByRange::info.type_.type_: // 7
            return recode<Protocol::GrowingObject::ReplacePartsByRange>(dec, enc);
        case Protocol::GrowingObject::GetPartsMetadata::info.type_.type_: // 8
            return recode<Protocol::GrowingObject::GetPartsMetadata>(dec, enc);
        case Protocol::GrowingObject::GetPartsMetadataResponse::info.type_.type_: // 9
            return recode<Protocol::GrowingObject::GetPartsMetadataResponse>(dec, enc);
        case Protocol::GrowingObject::GetPartsByRangeResponse::info.type_.type_: // 10
            return recode<Protocol::GrowingObject::GetPartsByRangeResponse>(dec, enc);
        case Protocol::GrowingObject::DeletePartsResponse::info.type_.type_: // 11
            return recode<Protocol::GrowingObject::DeletePartsResponse>(dec, enc);
        case Protocol::GrowingObject::PutPartsResponse::info.type_.type_: // 13
            return recode<Protocol::GrowingObject::PutPartsResponse>(dec, enc);
        case Protocol::GrowingObject::GetGrowingDataObjectsHeader::info.type_.type_: // 14
            return recode<Protocol::GrowingObject::GetGrowingDataObjectsHeader>(dec, enc);
        case Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse::info.type_.type_: // 15
            return recode<Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse>(dec, enc);
        case Protocol::GrowingObject::PutGrowingDataObjectsHeader::info.type_.type_: // 16
            return recode<Protocol::GrowingObject::PutGrowingDataObjectsHeader>(dec, enc);
        case Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse::info.type_.type_: // 17
            return recode<Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse>(dec, enc);
        case Protocol::GrowingObject::ReplacePartsByRangeResponse::info.type_.type_: // 18
            return recode<Protocol::GrowingObject::ReplacePartsByRangeResponse>(dec, enc);
        case Protocol::GrowingObject::GetChangeAnnotations::info.type_.type_: // 19
            return recode<Protocol::GrowingObject::GetChangeAnnotations>(dec, enc);
        case Protocol::GrowingObject::GetChangeAnnotationsResponse::info.type_.type_: // 20
            return recode<Protocol::GrowingObject::GetChangeAnnotationsResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObjectNotification): // 7
        switch (messageTypeId) {
        case Protocol::GrowingObjectNotification::PartsChanged::info.type_.type_: // 2
            return recode<Protocol::GrowingObjectNotification::PartsChanged>(dec, enc);
        case Protocol::GrowingObjectNotification::PartsDeleted::info.type_.type_: // 3
            return recode<Protocol::GrowingObjectNotification::PartsDeleted>(dec, enc);
        case Protocol::GrowingObjectNotification::UnsubscribePartNotification::info.type_.type_: // 4
            return recode<Protocol::GrowingObjectNotification::UnsubscribePartNotification>(dec, enc);
        case Protocol::GrowingObjectNotification::PartsReplacedByRange::info.type_.type_: // 6
            return recode<Protocol::GrowingObjectNotification::PartsReplacedByRange>(dec, enc);
        case Protocol::GrowingObjectNotification::SubscribePartNotifications::info.type_.type_: // 7
            return recode<Protocol::GrowingObjectNotification::SubscribePartNotifications>(dec, enc);
        case Protocol::GrowingObjectNotification::PartSubscriptionEnded::info.type_.type_: // 8
            return recode<Protocol::GrowingObjectNotification::PartSubscriptionEnded>(dec, enc);
        case Protocol::GrowingObjectNotification::UnsolicitedPartNotifications::info.type_.type_: // 9
            return recode<Protocol::GrowingObjectNotification::UnsolicitedPartNotifications>(dec, enc);
        case Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse::info.type_.type_: // 10
            return recode<Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataArray): // 9
        switch (messageTypeId) {
        case Protocol::DataArray::GetDataArraysResponse::info.type_.type_: // 1
            return recode<Protocol::DataArray::GetDataArraysResponse>(dec, enc);
        case Protocol::DataArray::GetDataArrays::info.type_.type_: // 2
            return recode<Protocol::DataArray::GetDataArrays>(dec, enc);
        case Protocol::DataArray::GetDataSubarrays::info.type_.type_: // 3
            return recode<Protocol::DataArray::GetDataSubarrays>(dec, enc);
        case Protocol::DataArray::PutDataArrays::info.type_.type_: // 4
            return recode<Protocol::DataArray::PutDataArrays>(dec, enc);
        case Protocol::DataArray::PutDataSubarrays::info.type_.type_: // 5
            return recode<Protocol::DataArray::PutDataSubarrays>(dec, enc);
        case Protocol::DataArray::GetDataArrayMetadata::info.type_.type_: // 6
            return recode<Protocol::DataArray::GetDataArrayMetadata>(dec, enc);
        case Protocol::DataArray::GetDataArrayMetadataResponse::info.type_.type_: // 7
            return recode<Protocol::DataArray::GetDataArrayMetadataResponse>(dec, enc);
        case Protocol::DataArray::GetDataSubarraysResponse::info.type_.type_: // 8
            return recode<Protocol::DataArray::GetDataSubarraysResponse>(dec, enc);
        case Protocol::DataArray::PutUninitializedDataArrays::info.type_.type_: // 9
            return recode<Protocol::DataArray::PutUninitializedDataArrays>(dec, enc);
        case Protocol::DataArray::PutDataArraysResponse::info.type_.type_: // 10
            return recode<Protocol::DataArray::PutDataArraysResponse>(dec, enc);
        case Protocol::DataArray::PutDataSubarraysResponse::info.type_.type_: // 11
            return recode<Protocol::DataArray::PutDataSubarraysResponse>(dec, enc);
        case Protocol::DataArray::PutUninitializedDataArraysResponse::info.type_.type_: // 12
            return recode<Protocol::DataArray::PutUninitializedDataArraysResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDiscoveryQuery): // 13
        switch (messageTypeId) {
        case Protocol::DiscoveryQuery::FindResources::info.type_.type_: // 1
            return recode<Protocol::DiscoveryQuery::FindResources>(dec, enc);
        case Protocol::DiscoveryQuery::FindResourcesResponse::info.type_.type_: // 2
            return recode<Protocol::DiscoveryQuery::FindResourcesResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreQuery): // 14
        switch (messageTypeId) {
        case Protocol::StoreQuery::FindDataObjects::info.type_.type_: // 1
            return recode<Protocol::StoreQuery::FindDataObjects>(dec, enc);
        case Protocol::StoreQuery::FindDataObjectsResponse::info.type_.type_: // 2
            return recode<Protocol::StoreQuery::FindDataObjectsResponse>(dec, enc);
        case Protocol::StoreQuery::Chunk::info.type_.type_: // 3
            return recode<Protocol::StoreQuery::Chunk>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObjectQuery): // 16
        switch (messageTypeId) {
        case Protocol::GrowingObjectQuery::FindParts::info.type_.type_: // 1
            return recode<Protocol::GrowingObjectQuery::FindParts>(dec, enc);
        case Protocol::GrowingObjectQuery::FindPartsResponse::info.type_.type_: // 2
            return recode<Protocol::GrowingObjectQuery::FindPartsResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eTransaction): // 18
        switch (messageTypeId) {
        case Protocol::Transaction::StartTransaction::info.type_.type_: // 1
            return recode<Protocol::Transaction::StartTransaction>(dec, enc);
        case Protocol::Transaction::StartTransactionResponse::info.type_.type_: // 2
            return recode<Protocol::Transaction::StartTransactionResponse>(dec, enc);
        case Protocol::Transaction::CommitTransaction::info.type_.type_: // 3
            return recode<Protocol::Transaction::CommitTransaction>(dec, enc);
        case Protocol::Transaction::RollbackTransaction::info.type_.type_: // 4
            return recode<Protocol::Transaction::RollbackTransaction>(dec, enc);
        case Protocol::Transaction::CommitTransactionResponse::info.type_.type_: // 5
            return recode<Protocol::Transaction::CommitTransactionResponse>(dec, enc);
        case Protocol::Transaction::RollbackTransactionResponse::info.type_.type_: // 6
            return recode<Protocol::Transaction::RollbackTransactionResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelSubscribe): // 21
        switch (messageTypeId) {
        case Protocol::ChannelSubscribe::GetChannelMetadata::info.type_.type_: // 1
            return recode<Protocol::ChannelSubscribe::GetChannelMetadata>(dec, enc);
        case Protocol::ChannelSubscribe::GetChannelMetadataResponse::info.type_.type_: // 2
            return recode<Protocol::ChannelSubscribe::GetChannelMetadataResponse>(dec, enc);
        case Protocol::ChannelSubscribe::SubscribeChannels::info.type_.type_: // 3
            return recode<Protocol::ChannelSubscribe::SubscribeChannels>(dec, enc);
        case Protocol::ChannelSubscribe::ChannelData::info.type_.type_: // 4
            return recode<Protocol::ChannelSubscribe::ChannelData>(dec, enc);
        case Protocol::ChannelSubscribe::RangeReplaced::info.type_.type_: // 6
            return recode<Protocol::ChannelSubscribe::RangeReplaced>(dec, enc);
        case Protocol::ChannelSubscribe::UnsubscribeChannels::info.type_.type_: // 7
            return recode<Protocol::ChannelSubscribe::UnsubscribeChannels>(dec, enc);
        case Protocol::ChannelSubscribe::SubscriptionsStopped::info.type_.type_: // 8
            return recode<Protocol::ChannelSubscribe::SubscriptionsStopped>(dec, enc);
        case Protocol::ChannelSubscribe::GetRanges::info.type_.type_: // 9
            return recode<Protocol::ChannelSubscribe::GetRanges>(dec, enc);
        case Protocol::ChannelSubscribe::GetRangesResponse::info.type_.type_: // 10
            return recode<Protocol::ChannelSubscribe::GetRangesResponse>(dec, enc);
        case Protocol::ChannelSubscribe::CancelGetRanges::info.type_.type_: // 11
            return recode<Protocol::ChannelSubscribe::CancelGetRanges>(dec, enc);
        case Protocol::ChannelSubscribe::SubscribeChannelsResponse::info.type_.type_: // 12
            return recode<Protocol::ChannelSubscribe::SubscribeChannelsResponse>(dec, enc);
        case Protocol::ChannelSubscribe::ChannelsTruncated::info.type_.type_: // 13
            return recode<Protocol::ChannelSubscribe::ChannelsTruncated>(dec, enc);
        case Protocol::ChannelSubscribe::GetChangeAnnotations::info.type_.type_: // 14
            return recode<Protocol::ChannelSubscribe::GetChangeAnnotations>(dec, enc);
        case Protocol::ChannelSubscribe::GetChangeAnnotationsResponse::info.type_.type_: // 15
            return recode<Protocol::ChannelSubscribe::GetChangeAnnotationsResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelDataLoad): // 22
        switch (messageTypeId) {
        case Protocol::ChannelDataLoad::OpenChannels::info.type_.type_: // 1
            return recode<Protocol::ChannelDataLoad::OpenChannels>(dec, enc);
        case Protocol::ChannelDataLoad::OpenChannelsResponse::info.type_.type_: // 2
            return recode<Protocol::ChannelDataLoad::OpenChannelsResponse>(dec, enc);
        case Protocol::ChannelDataLoad::CloseChannels::info.type_.type_: // 3
            return recode<Protocol::ChannelDataLoad::CloseChannels>(dec, enc);
        case Protocol::ChannelDataLoad::ChannelData::info.type_.type_: // 4
            return recode<Protocol::ChannelDataLoad::ChannelData>(dec, enc);
        case Protocol::ChannelDataLoad::ReplaceRange::info.type_.type_: // 6
            return recode<Protocol::ChannelDataLoad::ReplaceRange>(dec, enc);
        case Protocol::ChannelDataLoad::ChannelsClosed::info.type_.type_: // 7
            return recode<Protocol::ChannelDataLoad::ChannelsClosed>(dec, enc);
        case Protocol::ChannelDataLoad::ReplaceRangeResponse::info.type_.type_: // 8
            return recode<Protocol::ChannelDataLoad::ReplaceRangeResponse>(dec, enc);
        case Protocol::ChannelDataLoad::TruncateChannels::info.type_.type_: // 9
            return recode<Protocol::ChannelDataLoad::TruncateChannels>(dec, enc);
        case Protocol::ChannelDataLoad::TruncateChannelsResponse::info.type_.type_: // 10
            return recode<Protocol::ChannelDataLoad::TruncateChannelsResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataspace): // 24
        switch (messageTypeId) {
        case Protocol::Dataspace::GetDataspaces::info.type_.type_: // 1
            return recode<Protocol::Dataspace::GetDataspaces>(dec, enc);
        case Protocol::Dataspace::GetDataspacesResponse::info.type_.type_: // 2
            return recode<Protocol::Dataspace::GetDataspacesResponse>(dec, enc);
        case Protocol::Dataspace::PutDataspaces::info.type_.type_: // 3
            return recode<Protocol::Dataspace::PutDataspaces>(dec, enc);
        case Protocol::Dataspace::DeleteDataspaces::info.type_.type_: // 4
            return recode<Protocol::Dataspace::DeleteDataspaces>(dec, enc);
        case Protocol::Dataspace::DeleteDataspacesResponse::info.type_.type_: // 5
            return recode<Protocol::Dataspace::DeleteDataspacesResponse>(dec, enc);
        case Protocol::Dataspace::PutDataspacesResponse::info.type_.type_: // 6
            return recode<Protocol::Dataspace::PutDataspacesResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eSupportedTypes): // 25
        switch (messageTypeId) {
        case Protocol::SupportedTypes::GetSupportedTypes::info.type_.type_: // 1
            return recode<Protocol::SupportedTypes::GetSupportedTypes>(dec, enc);
        case Protocol::SupportedTypes::GetSupportedTypesResponse::info.type_.type_: // 2
            return recode<Protocol::SupportedTypes::GetSupportedTypesResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eCoreOSDU): // 2400
        switch (messageTypeId) {
        case Protocol::CoreOSDU::ResumeSession::info.type_.type_: // 1
            return recode<Protocol::CoreOSDU::ResumeSession>(dec, enc);
        case Protocol::CoreOSDU::ResumeSessionResponse::info.type_.type_: // 2
            return recode<Protocol::CoreOSDU::ResumeSessionResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreOSDU): // 2404
        switch (messageTypeId) {
        case Protocol::StoreOSDU::CopyDataObjectsByValue::info.type_.type_: // 1
            return recode<Protocol::StoreOSDU::CopyDataObjectsByValue>(dec, enc);
        case Protocol::StoreOSDU::CopyDataObjectsByValueResponse::info.type_.type_: // 2
            return recode<Protocol::StoreOSDU::CopyDataObjectsByValueResponse>(dec, enc);
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataspaceOSDU): // 2424
        switch (messageTypeId) {
        case Protocol::DataspaceOSDU::GetDataspaceInfo::info.type_.type_: // 1
            return recode<Protocol::DataspaceOSDU::GetDataspaceInfo>(dec, enc);
        case Protocol::DataspaceOSDU::GetDataspaceInfoResponse::info.type_.type_: // 2
            return recode<Protocol::DataspaceOSDU::GetDataspaceInfoResponse>(dec, enc);
        case Protocol::DataspaceOSDU::CopyDataspacesContent::info.type_.type_: // 3
            return recode<Protocol::DataspaceOSDU::CopyDataspacesContent>(dec, enc);
        case Protocol::DataspaceOSDU::CopyDataspacesContentResponse::info.type_.type_: // 4
            return recode<Protocol::DataspaceOSDU::CopyDataspacesContentResponse>(dec, enc);
        case Protocol::DataspaceOSDU::LockDataspaces::info.type_.type_: // 5
            return recode<Protocol::DataspaceOSDU::LockDataspaces>(dec, enc);
        case Protocol::DataspaceOSDU::LockDataspacesResponse::info.type_.type_: // 6
            return recode<Protocol::DataspaceOSDU::LockDataspacesResponse>(dec, enc);
        case Protocol::DataspaceOSDU::CopyToDataspace::info.type_.type_: // 7
            return recode<Protocol::DataspaceOSDU::CopyToDataspace>(dec, enc);
        case Protocol::DataspaceOSDU::CopyToDataspaceResponse::info.type_.type_: // 8
            return recode<Protocol::DataspaceOSDU::CopyToDataspaceResponse>(dec, enc);
        case Protocol::DataspaceOSDU::CopyDataspaceFromRemote::info.type_.type_: // 9
            return recode<Protocol::DataspaceOSDU::CopyDataspaceFromRemote>(dec, enc);
        case Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse::info.type_.type_: // 10
            return recode<Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse>(dec, enc);
        default:
            break;
        }
        break;

    default:
        break;
    }
    return false;
}

} //      oes::common::etp12

