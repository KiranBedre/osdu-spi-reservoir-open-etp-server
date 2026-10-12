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

namespace oes::common::etp12::Protocol {

struct OES_COMMON_EXPORT ICore : IProtocol {
    enum { protocolId = 0 };

    using RequestSession_req_t = Core::RequestSession;
    using RequestSession_res_t = Core::OpenSession;
    virtual void RequestSession( // messageTypeId 1
        IRequest& ireq, RequestSession_req_t req,
        EtpResponseHandler<RequestSession_res_t> process_cb // messageTypeId 2
    ) = 0;

    using CloseSession_req_t = Core::CloseSession;
    virtual void CloseSession( // messageTypeId 5
        IRequest& ireq, CloseSession_req_t req,
        EtpCompleteHandler done_cb
    ) = 0;

    using Authorize_req_t = Core::Authorize;
    using Authorize_res_t = Core::AuthorizeResponse;
    virtual void Authorize( // messageTypeId 6
        IRequest& ireq, Authorize_req_t req,
        EtpResponseHandler<Authorize_res_t> process_cb // messageTypeId 7
    ) = 0;

    using Ping_req_t = Core::Ping;
    using Ping_res_t = Core::Pong;
    virtual void Ping( // messageTypeId 8
        IRequest& ireq, Ping_req_t req,
        EtpResponseHandler<Ping_res_t> process_cb // messageTypeId 9
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<ICore> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    ICore();
};

struct OES_COMMON_EXPORT IChannelStreaming : IProtocol {
    enum { protocolId = 1 };

    using StartStreaming_req_t = ChannelStreaming::StartStreaming;
    using StartStreaming_res_t = ChannelStreaming::ChannelMetadata;
    virtual void StartStreaming( // messageTypeId 3
        IRequest& ireq, StartStreaming_req_t req,
        EtpResponseHandler<StartStreaming_res_t> process_cb // messageTypeId 1
    ) = 0;

    using StopStreaming_req_t = ChannelStreaming::StopStreaming;
    virtual void StopStreaming( // messageTypeId 4
        IRequest& ireq, StopStreaming_req_t req,
        EtpCompleteHandler done_cb
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IChannelStreaming> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IChannelStreaming();
};

struct OES_COMMON_EXPORT IChannelDataFrame : IProtocol {
    enum { protocolId = 2 };

    using GetFrameMetadata_req_t = ChannelDataFrame::GetFrameMetadata;
    using GetFrameMetadata_res_t = ChannelDataFrame::GetFrameMetadataResponse;
    virtual void GetFrameMetadata( // messageTypeId 1
        IRequest& ireq, GetFrameMetadata_req_t req,
        EtpResponseHandler<GetFrameMetadata_res_t> process_cb // messageTypeId 2
    ) = 0;

    using GetFrame_req_t = ChannelDataFrame::GetFrame;
    using GetFrame_res_t = ChannelDataFrame::GetFrameResponseHeader;
    virtual void GetFrame( // messageTypeId 3
        IRequest& ireq, GetFrame_req_t req,
        EtpResponseHandler<GetFrame_res_t> process_cb // messageTypeId 4
    ) = 0;

    using CancelGetFrame_req_t = ChannelDataFrame::CancelGetFrame;
    virtual void CancelGetFrame( // messageTypeId 5
        IRequest& ireq, CancelGetFrame_req_t req,
        EtpCompleteHandler done_cb
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IChannelDataFrame> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IChannelDataFrame();
};

struct OES_COMMON_EXPORT IDiscovery : IProtocol {
    enum { protocolId = 3 };

    using GetResources_req_t = Discovery::GetResources;
    using GetResources_res_t = Discovery::GetResourcesResponse;
    virtual void GetResources( // messageTypeId 1
        IRequest& ireq, GetResources_req_t req,
        EtpResponseHandler<GetResources_res_t> process_cb // messageTypeId 4
    ) = 0;

    using GetDeletedResources_req_t = Discovery::GetDeletedResources;
    using GetDeletedResources_res_t = Discovery::GetDeletedResourcesResponse;
    virtual void GetDeletedResources( // messageTypeId 5
        IRequest& ireq, GetDeletedResources_req_t req,
        EtpResponseHandler<GetDeletedResources_res_t> process_cb // messageTypeId 6
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IDiscovery> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IDiscovery();
};

struct OES_COMMON_EXPORT IStore : IProtocol {
    enum { protocolId = 4 };

    using GetDataObjects_req_t = Store::GetDataObjects;
    using GetDataObjects_res_t = Store::GetDataObjectsResponse;
    virtual void GetDataObjects( // messageTypeId 1
        IRequest& ireq, GetDataObjects_req_t req,
        EtpResponseHandler<GetDataObjects_res_t> process_cb // messageTypeId 4
    ) = 0;

    using PutDataObjects_req_t = Store::PutDataObjects;
    using PutDataObjects_res_t = Store::PutDataObjectsResponse;
    virtual void PutDataObjects( // messageTypeId 2
        IRequest& ireq, PutDataObjects_req_t req,
        EtpResponseHandler<PutDataObjects_res_t> process_cb // messageTypeId 9
    ) = 0;

    using DeleteDataObjects_req_t = Store::DeleteDataObjects;
    using DeleteDataObjects_res_t = Store::DeleteDataObjectsResponse;
    virtual void DeleteDataObjects( // messageTypeId 3
        IRequest& ireq, DeleteDataObjects_req_t req,
        EtpResponseHandler<DeleteDataObjects_res_t> process_cb // messageTypeId 10
    ) = 0;

    // Store::Chunk: MANUALLY ADDED BEGIN
    using Chunk_req_t = Store::Chunk;
    virtual void Chunk( // messageTypeId 8
        IRequest& ireq, Chunk_req_t req,
        EtpResponseHandler<PutDataObjects_res_t> process_cb // messageTypeId 9
    ) = 0;
    // Store::Chunk: MANUALLY ADDED END

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IStore> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IStore();
};

struct OES_COMMON_EXPORT IStoreNotification : IProtocol {
    enum { protocolId = 5 };

    using UnsubscribeNotifications_req_t = StoreNotification::UnsubscribeNotifications;
    virtual void UnsubscribeNotifications( // messageTypeId 4
        IRequest& ireq, UnsubscribeNotifications_req_t req,
        EtpCompleteHandler done_cb
    ) = 0;

    using SubscribeNotifications_req_t = StoreNotification::SubscribeNotifications;
    using SubscribeNotifications_res_t = StoreNotification::SubscribeNotificationsResponse;
    virtual void SubscribeNotifications( // messageTypeId 6
        IRequest& ireq, SubscribeNotifications_req_t req,
        EtpResponseHandler<SubscribeNotifications_res_t> process_cb // messageTypeId 10
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IStoreNotification> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IStoreNotification();
};

struct OES_COMMON_EXPORT IGrowingObject : IProtocol {
    enum { protocolId = 6 };

    using DeleteParts_req_t = GrowingObject::DeleteParts;
    using DeleteParts_res_t = GrowingObject::DeletePartsResponse;
    virtual void DeleteParts( // messageTypeId 1
        IRequest& ireq, DeleteParts_req_t req,
        EtpResponseHandler<DeleteParts_res_t> process_cb // messageTypeId 11
    ) = 0;

    using GetParts_req_t = GrowingObject::GetParts;
    using GetParts_res_t = GrowingObject::GetPartsResponse;
    virtual void GetParts( // messageTypeId 3
        IRequest& ireq, GetParts_req_t req,
        EtpResponseHandler<GetParts_res_t> process_cb // messageTypeId 6
    ) = 0;

    using GetPartsByRange_req_t = GrowingObject::GetPartsByRange;
    using GetPartsByRange_res_t = GrowingObject::GetPartsByRangeResponse;
    virtual void GetPartsByRange( // messageTypeId 4
        IRequest& ireq, GetPartsByRange_req_t req,
        EtpResponseHandler<GetPartsByRange_res_t> process_cb // messageTypeId 10
    ) = 0;

    using PutParts_req_t = GrowingObject::PutParts;
    using PutParts_res_t = GrowingObject::PutPartsResponse;
    virtual void PutParts( // messageTypeId 5
        IRequest& ireq, PutParts_req_t req,
        EtpResponseHandler<PutParts_res_t> process_cb // messageTypeId 13
    ) = 0;

    using ReplacePartsByRange_req_t = GrowingObject::ReplacePartsByRange;
    using ReplacePartsByRange_res_t = GrowingObject::ReplacePartsByRangeResponse;
    virtual void ReplacePartsByRange( // messageTypeId 7
        IRequest& ireq, ReplacePartsByRange_req_t req,
        EtpResponseHandler<ReplacePartsByRange_res_t> process_cb // messageTypeId 18
    ) = 0;

    using GetPartsMetadata_req_t = GrowingObject::GetPartsMetadata;
    using GetPartsMetadata_res_t = GrowingObject::GetPartsMetadataResponse;
    virtual void GetPartsMetadata( // messageTypeId 8
        IRequest& ireq, GetPartsMetadata_req_t req,
        EtpResponseHandler<GetPartsMetadata_res_t> process_cb // messageTypeId 9
    ) = 0;

    using GetGrowingDataObjectsHeader_req_t = GrowingObject::GetGrowingDataObjectsHeader;
    using GetGrowingDataObjectsHeader_res_t = GrowingObject::GetGrowingDataObjectsHeaderResponse;
    virtual void GetGrowingDataObjectsHeader( // messageTypeId 14
        IRequest& ireq, GetGrowingDataObjectsHeader_req_t req,
        EtpResponseHandler<GetGrowingDataObjectsHeader_res_t> process_cb // messageTypeId 15
    ) = 0;

    using PutGrowingDataObjectsHeader_req_t = GrowingObject::PutGrowingDataObjectsHeader;
    using PutGrowingDataObjectsHeader_res_t = GrowingObject::PutGrowingDataObjectsHeaderResponse;
    virtual void PutGrowingDataObjectsHeader( // messageTypeId 16
        IRequest& ireq, PutGrowingDataObjectsHeader_req_t req,
        EtpResponseHandler<PutGrowingDataObjectsHeader_res_t> process_cb // messageTypeId 17
    ) = 0;

    using GetChangeAnnotations_req_t = GrowingObject::GetChangeAnnotations;
    using GetChangeAnnotations_res_t = GrowingObject::GetChangeAnnotationsResponse;
    virtual void GetChangeAnnotations( // messageTypeId 19
        IRequest& ireq, GetChangeAnnotations_req_t req,
        EtpResponseHandler<GetChangeAnnotations_res_t> process_cb // messageTypeId 20
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IGrowingObject> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IGrowingObject();
};

struct OES_COMMON_EXPORT IGrowingObjectNotification : IProtocol {
    enum { protocolId = 7 };

    using UnsubscribePartNotification_req_t = GrowingObjectNotification::UnsubscribePartNotification;
    using UnsubscribePartNotification_res_t = GrowingObjectNotification::PartSubscriptionEnded;
    virtual void UnsubscribePartNotification( // messageTypeId 4
        IRequest& ireq, UnsubscribePartNotification_req_t req,
        EtpResponseHandler<UnsubscribePartNotification_res_t> process_cb // messageTypeId 8
    ) = 0;

    using SubscribePartNotifications_req_t = GrowingObjectNotification::SubscribePartNotifications;
    using SubscribePartNotifications_res_t = GrowingObjectNotification::SubscribePartNotificationsResponse;
    virtual void SubscribePartNotifications( // messageTypeId 7
        IRequest& ireq, SubscribePartNotifications_req_t req,
        EtpResponseHandler<SubscribePartNotifications_res_t> process_cb // messageTypeId 10
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IGrowingObjectNotification> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IGrowingObjectNotification();
};

struct OES_COMMON_EXPORT IDataArray : IProtocol {
    enum { protocolId = 9 };

    using GetDataArrays_req_t = DataArray::GetDataArrays;
    using GetDataArrays_res_t = DataArray::GetDataArraysResponse;
    virtual void GetDataArrays( // messageTypeId 2
        IRequest& ireq, GetDataArrays_req_t req,
        EtpResponseHandler<GetDataArrays_res_t> process_cb // messageTypeId 1
    ) = 0;

    using GetDataSubarrays_req_t = DataArray::GetDataSubarrays;
    using GetDataSubarrays_res_t = DataArray::GetDataSubarraysResponse;
    virtual void GetDataSubarrays( // messageTypeId 3
        IRequest& ireq, GetDataSubarrays_req_t req,
        EtpResponseHandler<GetDataSubarrays_res_t> process_cb // messageTypeId 8
    ) = 0;

    using PutDataArrays_req_t = DataArray::PutDataArrays;
    using PutDataArrays_res_t = DataArray::PutDataArraysResponse;
    virtual void PutDataArrays( // messageTypeId 4
        IRequest& ireq, PutDataArrays_req_t req,
        EtpResponseHandler<PutDataArrays_res_t> process_cb // messageTypeId 10
    ) = 0;

    using PutDataSubarrays_req_t = DataArray::PutDataSubarrays;
    using PutDataSubarrays_res_t = DataArray::PutDataSubarraysResponse;
    virtual void PutDataSubarrays( // messageTypeId 5
        IRequest& ireq, PutDataSubarrays_req_t req,
        EtpResponseHandler<PutDataSubarrays_res_t> process_cb // messageTypeId 11
    ) = 0;

    using GetDataArrayMetadata_req_t = DataArray::GetDataArrayMetadata;
    using GetDataArrayMetadata_res_t = DataArray::GetDataArrayMetadataResponse;
    virtual void GetDataArrayMetadata( // messageTypeId 6
        IRequest& ireq, GetDataArrayMetadata_req_t req,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    ) = 0;

    using PutUninitializedDataArrays_req_t = DataArray::PutUninitializedDataArrays;
    using PutUninitializedDataArrays_res_t = DataArray::PutUninitializedDataArraysResponse;
    virtual void PutUninitializedDataArrays( // messageTypeId 9
        IRequest& ireq, PutUninitializedDataArrays_req_t req,
        EtpResponseHandler<PutUninitializedDataArrays_res_t> process_cb // messageTypeId 12
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IDataArray> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IDataArray();
};

struct OES_COMMON_EXPORT IDiscoveryQuery : IProtocol {
    enum { protocolId = 13 };

    using FindResources_req_t = DiscoveryQuery::FindResources;
    using FindResources_res_t = DiscoveryQuery::FindResourcesResponse;
    virtual void FindResources( // messageTypeId 1
        IRequest& ireq, FindResources_req_t req,
        EtpResponseHandler<FindResources_res_t> process_cb // messageTypeId 2
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IDiscoveryQuery> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IDiscoveryQuery();
};

struct OES_COMMON_EXPORT IStoreQuery : IProtocol {
    enum { protocolId = 14 };

    using FindDataObjects_req_t = StoreQuery::FindDataObjects;
    using FindDataObjects_res_t = StoreQuery::FindDataObjectsResponse;
    virtual void FindDataObjects( // messageTypeId 1
        IRequest& ireq, FindDataObjects_req_t req,
        EtpResponseHandler<FindDataObjects_res_t> process_cb // messageTypeId 2
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IStoreQuery> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IStoreQuery();
};

struct OES_COMMON_EXPORT IGrowingObjectQuery : IProtocol {
    enum { protocolId = 16 };

    using FindParts_req_t = GrowingObjectQuery::FindParts;
    using FindParts_res_t = GrowingObjectQuery::FindPartsResponse;
    virtual void FindParts( // messageTypeId 1
        IRequest& ireq, FindParts_req_t req,
        EtpResponseHandler<FindParts_res_t> process_cb // messageTypeId 2
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IGrowingObjectQuery> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IGrowingObjectQuery();
};

struct OES_COMMON_EXPORT ITransaction : IProtocol {
    enum { protocolId = 18 };

    using StartTransaction_req_t = Transaction::StartTransaction;
    using StartTransaction_res_t = Transaction::StartTransactionResponse;
    virtual void StartTransaction( // messageTypeId 1
        IRequest& ireq, StartTransaction_req_t req,
        EtpResponseHandler<StartTransaction_res_t> process_cb // messageTypeId 2
    ) = 0;

    using CommitTransaction_req_t = Transaction::CommitTransaction;
    using CommitTransaction_res_t = Transaction::CommitTransactionResponse;
    virtual void CommitTransaction( // messageTypeId 3
        IRequest& ireq, CommitTransaction_req_t req,
        EtpResponseHandler<CommitTransaction_res_t> process_cb // messageTypeId 5
    ) = 0;

    using RollbackTransaction_req_t = Transaction::RollbackTransaction;
    using RollbackTransaction_res_t = Transaction::RollbackTransactionResponse;
    virtual void RollbackTransaction( // messageTypeId 4
        IRequest& ireq, RollbackTransaction_req_t req,
        EtpResponseHandler<RollbackTransaction_res_t> process_cb // messageTypeId 6
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<ITransaction> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    ITransaction();
};

struct OES_COMMON_EXPORT IChannelSubscribe : IProtocol {
    enum { protocolId = 21 };

    using GetChannelMetadata_req_t = ChannelSubscribe::GetChannelMetadata;
    using GetChannelMetadata_res_t = ChannelSubscribe::GetChannelMetadataResponse;
    virtual void GetChannelMetadata( // messageTypeId 1
        IRequest& ireq, GetChannelMetadata_req_t req,
        EtpResponseHandler<GetChannelMetadata_res_t> process_cb // messageTypeId 2
    ) = 0;

    using SubscribeChannels_req_t = ChannelSubscribe::SubscribeChannels;
    using SubscribeChannels_res_t = ChannelSubscribe::SubscribeChannelsResponse;
    virtual void SubscribeChannels( // messageTypeId 3
        IRequest& ireq, SubscribeChannels_req_t req,
        EtpResponseHandler<SubscribeChannels_res_t> process_cb // messageTypeId 12
    ) = 0;

    using UnsubscribeChannels_req_t = ChannelSubscribe::UnsubscribeChannels;
    using UnsubscribeChannels_res_t = ChannelSubscribe::SubscriptionsStopped;
    virtual void UnsubscribeChannels( // messageTypeId 7
        IRequest& ireq, UnsubscribeChannels_req_t req,
        EtpResponseHandler<UnsubscribeChannels_res_t> process_cb // messageTypeId 8
    ) = 0;

    using GetRanges_req_t = ChannelSubscribe::GetRanges;
    using GetRanges_res_t = ChannelSubscribe::GetRangesResponse;
    virtual void GetRanges( // messageTypeId 9
        IRequest& ireq, GetRanges_req_t req,
        EtpResponseHandler<GetRanges_res_t> process_cb // messageTypeId 10
    ) = 0;

    using CancelGetRanges_req_t = ChannelSubscribe::CancelGetRanges;
    using CancelGetRanges_res_t = ChannelSubscribe::GetRangesResponse;
    virtual void CancelGetRanges( // messageTypeId 11
        IRequest& ireq, CancelGetRanges_req_t req,
        EtpResponseHandler<CancelGetRanges_res_t> process_cb // messageTypeId 10
    ) = 0;

    using GetChangeAnnotations_req_t = ChannelSubscribe::GetChangeAnnotations;
    using GetChangeAnnotations_res_t = ChannelSubscribe::GetChangeAnnotationsResponse;
    virtual void GetChangeAnnotations( // messageTypeId 14
        IRequest& ireq, GetChangeAnnotations_req_t req,
        EtpResponseHandler<GetChangeAnnotations_res_t> process_cb // messageTypeId 15
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IChannelSubscribe> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IChannelSubscribe();
};

struct OES_COMMON_EXPORT IChannelDataLoad : IProtocol {
    enum { protocolId = 22 };

    using OpenChannels_req_t = ChannelDataLoad::OpenChannels;
    using OpenChannels_res_t = ChannelDataLoad::OpenChannelsResponse;
    virtual void OpenChannels( // messageTypeId 1
        IRequest& ireq, OpenChannels_req_t req,
        EtpResponseHandler<OpenChannels_res_t> process_cb // messageTypeId 2
    ) = 0;

    using CloseChannels_req_t = ChannelDataLoad::CloseChannels;
    using CloseChannels_res_t = ChannelDataLoad::ChannelsClosed;
    virtual void CloseChannels( // messageTypeId 3
        IRequest& ireq, CloseChannels_req_t req,
        EtpResponseHandler<CloseChannels_res_t> process_cb // messageTypeId 7
    ) = 0;

    using ChannelData_req_t = ChannelDataLoad::ChannelData;
    virtual void ChannelData( // messageTypeId 4
        IRequest& ireq, ChannelData_req_t req,
        EtpCompleteHandler done_cb
    ) = 0;

    using ReplaceRange_req_t = ChannelDataLoad::ReplaceRange;
    using ReplaceRange_res_t = ChannelDataLoad::ReplaceRangeResponse;
    virtual void ReplaceRange( // messageTypeId 6
        IRequest& ireq, ReplaceRange_req_t req,
        EtpResponseHandler<ReplaceRange_res_t> process_cb // messageTypeId 8
    ) = 0;

    using TruncateChannels_req_t = ChannelDataLoad::TruncateChannels;
    using TruncateChannels_res_t = ChannelDataLoad::TruncateChannelsResponse;
    virtual void TruncateChannels( // messageTypeId 9
        IRequest& ireq, TruncateChannels_req_t req,
        EtpResponseHandler<TruncateChannels_res_t> process_cb // messageTypeId 10
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IChannelDataLoad> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IChannelDataLoad();
};

struct OES_COMMON_EXPORT IDataspace : IProtocol {
    enum { protocolId = 24 };

    using GetDataspaces_req_t = Dataspace::GetDataspaces;
    using GetDataspaces_res_t = Dataspace::GetDataspacesResponse;
    virtual void GetDataspaces( // messageTypeId 1
        IRequest& ireq, GetDataspaces_req_t req,
        EtpResponseHandler<GetDataspaces_res_t> process_cb // messageTypeId 2
    ) = 0;

    using PutDataspaces_req_t = Dataspace::PutDataspaces;
    using PutDataspaces_res_t = Dataspace::PutDataspacesResponse;
    virtual void PutDataspaces( // messageTypeId 3
        IRequest& ireq, PutDataspaces_req_t req,
        EtpResponseHandler<PutDataspaces_res_t> process_cb // messageTypeId 6
    ) = 0;

    using DeleteDataspaces_req_t = Dataspace::DeleteDataspaces;
    using DeleteDataspaces_res_t = Dataspace::DeleteDataspacesResponse;
    virtual void DeleteDataspaces( // messageTypeId 4
        IRequest& ireq, DeleteDataspaces_req_t req,
        EtpResponseHandler<DeleteDataspaces_res_t> process_cb // messageTypeId 5
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IDataspace> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IDataspace();
};

struct OES_COMMON_EXPORT ISupportedTypes : IProtocol {
    enum { protocolId = 25 };

    using GetSupportedTypes_req_t = SupportedTypes::GetSupportedTypes;
    using GetSupportedTypes_res_t = SupportedTypes::GetSupportedTypesResponse;
    virtual void GetSupportedTypes( // messageTypeId 1
        IRequest& ireq, GetSupportedTypes_req_t req,
        EtpResponseHandler<GetSupportedTypes_res_t> process_cb // messageTypeId 2
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<ISupportedTypes> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    ISupportedTypes();
};

struct OES_COMMON_EXPORT ICoreOSDU : IProtocol {
    enum { protocolId = 2400 };

    using ResumeSession_req_t = CoreOSDU::ResumeSession;
    using ResumeSession_res_t = CoreOSDU::ResumeSessionResponse;
    virtual void ResumeSession( // messageTypeId 1
        IRequest& ireq, ResumeSession_req_t req,
        EtpResponseHandler<ResumeSession_res_t> process_cb // messageTypeId 2
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<ICoreOSDU> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    ICoreOSDU();
};

struct OES_COMMON_EXPORT IStoreOSDU : IProtocol {
    enum { protocolId = 2404 };

    using CopyDataObjectsByValue_req_t = StoreOSDU::CopyDataObjectsByValue;
    using CopyDataObjectsByValue_res_t = StoreOSDU::CopyDataObjectsByValueResponse;
    virtual void CopyDataObjectsByValue( // messageTypeId 1
        IRequest& ireq, CopyDataObjectsByValue_req_t req,
        EtpResponseHandler<CopyDataObjectsByValue_res_t> process_cb // messageTypeId 2
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IStoreOSDU> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IStoreOSDU();
};

struct OES_COMMON_EXPORT IDataspaceOSDU : IProtocol {
    enum { protocolId = 2424 };

    using GetDataspaceInfo_req_t = DataspaceOSDU::GetDataspaceInfo;
    using GetDataspaceInfo_res_t = DataspaceOSDU::GetDataspaceInfoResponse;
    virtual void GetDataspaceInfo( // messageTypeId 1
        IRequest& ireq, GetDataspaceInfo_req_t req,
        EtpResponseHandler<GetDataspaceInfo_res_t> process_cb // messageTypeId 2
    ) = 0;

    using CopyDataspacesContent_req_t = DataspaceOSDU::CopyDataspacesContent;
    using CopyDataspacesContent_res_t = DataspaceOSDU::CopyDataspacesContentResponse;
    virtual void CopyDataspacesContent( // messageTypeId 3
        IRequest& ireq, CopyDataspacesContent_req_t req,
        EtpResponseHandler<CopyDataspacesContent_res_t> process_cb // messageTypeId 4
    ) = 0;

    using LockDataspaces_req_t = DataspaceOSDU::LockDataspaces;
    using LockDataspaces_res_t = DataspaceOSDU::LockDataspacesResponse;
    virtual void LockDataspaces( // messageTypeId 5
        IRequest& ireq, LockDataspaces_req_t req,
        EtpResponseHandler<LockDataspaces_res_t> process_cb // messageTypeId 6
    ) = 0;

    using CopyToDataspace_req_t = DataspaceOSDU::CopyToDataspace;
    using CopyToDataspace_res_t = DataspaceOSDU::CopyToDataspaceResponse;
    virtual void CopyToDataspace( // messageTypeId 7
        IRequest& ireq, CopyToDataspace_req_t req,
        EtpResponseHandler<CopyToDataspace_res_t> process_cb // messageTypeId 8
    ) = 0;

    using CopyDataspaceFromRemote_req_t = DataspaceOSDU::CopyDataspaceFromRemote;
    using CopyDataspaceFromRemote_res_t = DataspaceOSDU::CopyDataspaceFromRemoteResponse;
    virtual void CopyDataspaceFromRemote( // messageTypeId 9
        IRequest& ireq, CopyDataspaceFromRemote_req_t req,
        EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb // messageTypeId 10
    ) = 0;

public: // IProtocol interface
    const char* messageTypeName(int messageType) const final override;
    int responseMessageType(int requestMessageType) const final override;
    void dispatchRequest(
        IRequest& ireq, EtpWrappedResponseHandler res_handler
    ) final override;

public: // Utilities
    static const char* protocolName();
    static const char* messageName(int messageType);

    static std::unique_ptr<IDataspaceOSDU> makeClientImpl(
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IDataspaceOSDU();
};

} //      oes::common::etp12::Protocol
