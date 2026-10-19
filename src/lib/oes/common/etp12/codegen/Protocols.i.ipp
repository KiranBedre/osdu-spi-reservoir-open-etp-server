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

//============================================================================

ICore::ICore(
) : IProtocol(Datatypes::Protocol::eCore, "Core")
{
}

const char* ICore::protocolName() {
    return "Core(0)";
}

const char* ICore::messageName(int messageType) {
    switch (messageType) {
    case /*1*/Core::RequestSession::info.type_.type_: return "RequestSession(1)";
    case /*2*/Core::OpenSession::info.type_.type_: return "OpenSession(2)";
    case /*5*/Core::CloseSession::info.type_.type_: return "CloseSession(5)";
    case /*6*/Core::Authorize::info.type_.type_: return "Authorize(6)";
    case /*7*/Core::AuthorizeResponse::info.type_.type_: return "AuthorizeResponse(7)";
    case /*8*/Core::Ping::info.type_.type_: return "Ping(8)";
    case /*9*/Core::Pong::info.type_.type_: return "Pong(9)";
    }
    return nullptr;
}

const char* ICore::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int ICore::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case RequestSession_req_t::info.type_.type_: // 1
        return RequestSession_res_t::info.type_.type_; // 2
    case CloseSession_req_t::info.type_.type_: // 5
        return 0; // no response message
    case Authorize_req_t::info.type_.type_: // 6
        return Authorize_res_t::info.type_.type_; // 7
    case Ping_req_t::info.type_.type_: // 8
        return Ping_res_t::info.type_.type_; // 9
    }
    return -1; // unknown request
}

void ICore::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case RequestSession_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<RequestSession_req_t*>(p_body.get()));
        auto* body = static_cast<RequestSession_req_t*>(p_body.get());
        RequestSession(
            ireq, std::move(*body),
            unwrapHandler<RequestSession_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CloseSession_req_t::info.type_.type_: { // 5
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CloseSession_req_t*>(p_body.get()));
        auto* body = static_cast<CloseSession_req_t*>(p_body.get());
        CloseSession(
            ireq, std::move(*body),
            unwrapHandlerVoid(std::move(wres_handler))
        );
        return;
    }

    case Authorize_req_t::info.type_.type_: { // 6
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<Authorize_req_t*>(p_body.get()));
        auto* body = static_cast<Authorize_req_t*>(p_body.get());
        Authorize(
            ireq, std::move(*body),
            unwrapHandler<Authorize_res_t>(std::move(wres_handler))
        );
        return;
    }

    case Ping_req_t::info.type_.type_: { // 8
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<Ping_req_t*>(p_body.get()));
        auto* body = static_cast<Ping_req_t*>(p_body.get());
        Ping(
            ireq, std::move(*body),
            unwrapHandler<Ping_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<ICore> ICore::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : ICore, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void RequestSession(
            IRequest& ireq, RequestSession_req_t req, // 0.1
            EtpResponseHandler<RequestSession_res_t> handler_cb // 0.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CloseSession(
            IRequest& ireq, CloseSession_req_t req, // 0.5
            EtpCompleteHandler handler_cb
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void Authorize(
            IRequest& ireq, Authorize_req_t req, // 0.6
            EtpResponseHandler<Authorize_res_t> handler_cb // 0.7
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void Ping(
            IRequest& ireq, Ping_req_t req, // 0.8
            EtpResponseHandler<Ping_res_t> handler_cb // 0.9
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IChannelStreaming::IChannelStreaming(
) : IProtocol(Datatypes::Protocol::eChannelStreaming, "ChannelStreaming")
{
}

const char* IChannelStreaming::protocolName() {
    return "ChannelStreaming(1)";
}

const char* IChannelStreaming::messageName(int messageType) {
    switch (messageType) {
    case /*1*/ChannelStreaming::ChannelMetadata::info.type_.type_: return "ChannelMetadata(1)";
    case /*2*/ChannelStreaming::ChannelData::info.type_.type_: return "ChannelData(2)";
    case /*3*/ChannelStreaming::StartStreaming::info.type_.type_: return "StartStreaming(3)";
    case /*4*/ChannelStreaming::StopStreaming::info.type_.type_: return "StopStreaming(4)";
    case /*5*/ChannelStreaming::TruncateChannels::info.type_.type_: return "TruncateChannels(5)";
    }
    return nullptr;
}

const char* IChannelStreaming::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IChannelStreaming::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case StartStreaming_req_t::info.type_.type_: // 3
        return StartStreaming_res_t::info.type_.type_; // 1
    case StopStreaming_req_t::info.type_.type_: // 4
        return 0; // no response message
    }
    return -1; // unknown request
}

void IChannelStreaming::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case StartStreaming_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<StartStreaming_req_t*>(p_body.get()));
        auto* body = static_cast<StartStreaming_req_t*>(p_body.get());
        StartStreaming(
            ireq, std::move(*body),
            unwrapHandler<StartStreaming_res_t>(std::move(wres_handler))
        );
        return;
    }

    case StopStreaming_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<StopStreaming_req_t*>(p_body.get()));
        auto* body = static_cast<StopStreaming_req_t*>(p_body.get());
        StopStreaming(
            ireq, std::move(*body),
            unwrapHandlerVoid(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IChannelStreaming> IChannelStreaming::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IChannelStreaming, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void StartStreaming(
            IRequest& ireq, StartStreaming_req_t req, // 1.3
            EtpResponseHandler<StartStreaming_res_t> handler_cb // 1.1
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void StopStreaming(
            IRequest& ireq, StopStreaming_req_t req, // 1.4
            EtpCompleteHandler handler_cb
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IChannelDataFrame::IChannelDataFrame(
) : IProtocol(Datatypes::Protocol::eChannelDataFrame, "ChannelDataFrame")
{
}

const char* IChannelDataFrame::protocolName() {
    return "ChannelDataFrame(2)";
}

const char* IChannelDataFrame::messageName(int messageType) {
    switch (messageType) {
    case /*1*/ChannelDataFrame::GetFrameMetadata::info.type_.type_: return "GetFrameMetadata(1)";
    case /*2*/ChannelDataFrame::GetFrameMetadataResponse::info.type_.type_: return "GetFrameMetadataResponse(2)";
    case /*3*/ChannelDataFrame::GetFrame::info.type_.type_: return "GetFrame(3)";
    case /*4*/ChannelDataFrame::GetFrameResponseHeader::info.type_.type_: return "GetFrameResponseHeader(4)";
    case /*5*/ChannelDataFrame::CancelGetFrame::info.type_.type_: return "CancelGetFrame(5)";
    case /*6*/ChannelDataFrame::GetFrameResponseRows::info.type_.type_: return "GetFrameResponseRows(6)";
    }
    return nullptr;
}

const char* IChannelDataFrame::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IChannelDataFrame::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetFrameMetadata_req_t::info.type_.type_: // 1
        return GetFrameMetadata_res_t::info.type_.type_; // 2
    case GetFrame_req_t::info.type_.type_: // 3
        return GetFrame_res_t::info.type_.type_; // 4
    case CancelGetFrame_req_t::info.type_.type_: // 5
        return 0; // no response message
    }
    return -1; // unknown request
}

void IChannelDataFrame::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetFrameMetadata_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetFrameMetadata_req_t*>(p_body.get()));
        auto* body = static_cast<GetFrameMetadata_req_t*>(p_body.get());
        GetFrameMetadata(
            ireq, std::move(*body),
            unwrapHandler<GetFrameMetadata_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetFrame_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetFrame_req_t*>(p_body.get()));
        auto* body = static_cast<GetFrame_req_t*>(p_body.get());
        GetFrame(
            ireq, std::move(*body),
            unwrapHandler<GetFrame_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CancelGetFrame_req_t::info.type_.type_: { // 5
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CancelGetFrame_req_t*>(p_body.get()));
        auto* body = static_cast<CancelGetFrame_req_t*>(p_body.get());
        CancelGetFrame(
            ireq, std::move(*body),
            unwrapHandlerVoid(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IChannelDataFrame> IChannelDataFrame::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IChannelDataFrame, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetFrameMetadata(
            IRequest& ireq, GetFrameMetadata_req_t req, // 2.1
            EtpResponseHandler<GetFrameMetadata_res_t> handler_cb // 2.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetFrame(
            IRequest& ireq, GetFrame_req_t req, // 2.3
            EtpResponseHandler<GetFrame_res_t> handler_cb // 2.4
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CancelGetFrame(
            IRequest& ireq, CancelGetFrame_req_t req, // 2.5
            EtpCompleteHandler handler_cb
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IDiscovery::IDiscovery(
) : IProtocol(Datatypes::Protocol::eDiscovery, "Discovery")
{
}

const char* IDiscovery::protocolName() {
    return "Discovery(3)";
}

const char* IDiscovery::messageName(int messageType) {
    switch (messageType) {
    case /*1*/Discovery::GetResources::info.type_.type_: return "GetResources(1)";
    case /*4*/Discovery::GetResourcesResponse::info.type_.type_: return "GetResourcesResponse(4)";
    case /*5*/Discovery::GetDeletedResources::info.type_.type_: return "GetDeletedResources(5)";
    case /*6*/Discovery::GetDeletedResourcesResponse::info.type_.type_: return "GetDeletedResourcesResponse(6)";
    case /*7*/Discovery::GetResourcesEdgesResponse::info.type_.type_: return "GetResourcesEdgesResponse(7)";
    }
    return nullptr;
}

const char* IDiscovery::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IDiscovery::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetResources_req_t::info.type_.type_: // 1
        return GetResources_res_t::info.type_.type_; // 4
    case GetDeletedResources_req_t::info.type_.type_: // 5
        return GetDeletedResources_res_t::info.type_.type_; // 6
    }
    return -1; // unknown request
}

void IDiscovery::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetResources_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetResources_req_t*>(p_body.get()));
        auto* body = static_cast<GetResources_req_t*>(p_body.get());
        GetResources(
            ireq, std::move(*body),
            unwrapHandler<GetResources_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetDeletedResources_req_t::info.type_.type_: { // 5
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDeletedResources_req_t*>(p_body.get()));
        auto* body = static_cast<GetDeletedResources_req_t*>(p_body.get());
        GetDeletedResources(
            ireq, std::move(*body),
            unwrapHandler<GetDeletedResources_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IDiscovery> IDiscovery::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IDiscovery, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetResources(
            IRequest& ireq, GetResources_req_t req, // 3.1
            EtpResponseHandler<GetResources_res_t> handler_cb // 3.4
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetDeletedResources(
            IRequest& ireq, GetDeletedResources_req_t req, // 3.5
            EtpResponseHandler<GetDeletedResources_res_t> handler_cb // 3.6
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IStore::IStore(
) : IProtocol(Datatypes::Protocol::eStore, "Store")
{
}

const char* IStore::protocolName() {
    return "Store(4)";
}

const char* IStore::messageName(int messageType) {
    switch (messageType) {
    case /*1*/Store::GetDataObjects::info.type_.type_: return "GetDataObjects(1)";
    case /*2*/Store::PutDataObjects::info.type_.type_: return "PutDataObjects(2)";
    case /*3*/Store::DeleteDataObjects::info.type_.type_: return "DeleteDataObjects(3)";
    case /*4*/Store::GetDataObjectsResponse::info.type_.type_: return "GetDataObjectsResponse(4)";
    case /*8*/Store::Chunk::info.type_.type_: return "Chunk(8)";
    case /*9*/Store::PutDataObjectsResponse::info.type_.type_: return "PutDataObjectsResponse(9)";
    case /*10*/Store::DeleteDataObjectsResponse::info.type_.type_: return "DeleteDataObjectsResponse(10)";
    }
    return nullptr;
}

const char* IStore::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IStore::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetDataObjects_req_t::info.type_.type_: // 1
        return GetDataObjects_res_t::info.type_.type_; // 4
    case PutDataObjects_req_t::info.type_.type_: // 2
        return PutDataObjects_res_t::info.type_.type_; // 9
    case DeleteDataObjects_req_t::info.type_.type_: // 3
        return DeleteDataObjects_res_t::info.type_.type_; // 10
    // Store::Chunk: MANUALLY ADDED BEGIN
    case Chunk_req_t::info.type_.type_: // 8
        return PutDataObjects_res_t::info.type_.type_; // 9
    }
    // Store::Chunk: MANUALLY ADDED END
    return -1; // unknown request
}

void IStore::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetDataObjects_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDataObjects_req_t*>(p_body.get()));
        auto* body = static_cast<GetDataObjects_req_t*>(p_body.get());
        GetDataObjects(
            ireq, std::move(*body),
            unwrapHandler<GetDataObjects_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutDataObjects_req_t::info.type_.type_: { // 2
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutDataObjects_req_t*>(p_body.get()));
        auto* body = static_cast<PutDataObjects_req_t*>(p_body.get());
        PutDataObjects(
            ireq, std::move(*body),
            unwrapHandler<PutDataObjects_res_t>(std::move(wres_handler))
        );
        return;
    }

    case DeleteDataObjects_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<DeleteDataObjects_req_t*>(p_body.get()));
        auto* body = static_cast<DeleteDataObjects_req_t*>(p_body.get());
        DeleteDataObjects(
            ireq, std::move(*body),
            unwrapHandler<DeleteDataObjects_res_t>(std::move(wres_handler))
        );
        return;
    }

    // Store::Chunk: MANUALLY ADDED BEGIN
    case Chunk_req_t::info.type_.type_: { // 8
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<Chunk_req_t*>(p_body.get()));
        auto* body = static_cast<Chunk_req_t*>(p_body.get());
        Chunk(
            ireq, std::move(*body),
            unwrapHandler<PutDataObjects_res_t>(std::move(wres_handler))
        );
        return;
    }
    // Store::Chunk: MANUALLY ADDED END

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IStore> IStore::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IStore, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetDataObjects(
            IRequest& ireq, GetDataObjects_req_t req, // 4.1
            EtpResponseHandler<GetDataObjects_res_t> handler_cb // 4.4
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutDataObjects(
            IRequest& ireq, PutDataObjects_req_t req, // 4.2
            EtpResponseHandler<PutDataObjects_res_t> handler_cb // 4.9
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void DeleteDataObjects(
            IRequest& ireq, DeleteDataObjects_req_t req, // 4.3
            EtpResponseHandler<DeleteDataObjects_res_t> handler_cb // 4.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        // Store::Chunk: MANUALLY ADDED BEGIN
        void Chunk(
            IRequest& ireq, Chunk_req_t req, // 4.8
            EtpResponseHandler<PutDataObjects_res_t> handler_cb // 4.9
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
        // Store::Chunk: MANUALLY ADDED END
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IStoreNotification::IStoreNotification(
) : IProtocol(Datatypes::Protocol::eStoreNotification, "StoreNotification")
{
}

const char* IStoreNotification::protocolName() {
    return "StoreNotification(5)";
}

const char* IStoreNotification::messageName(int messageType) {
    switch (messageType) {
    case /*2*/StoreNotification::ObjectChanged::info.type_.type_: return "ObjectChanged(2)";
    case /*3*/StoreNotification::ObjectDeleted::info.type_.type_: return "ObjectDeleted(3)";
    case /*4*/StoreNotification::UnsubscribeNotifications::info.type_.type_: return "UnsubscribeNotifications(4)";
    case /*5*/StoreNotification::ObjectAccessRevoked::info.type_.type_: return "ObjectAccessRevoked(5)";
    case /*6*/StoreNotification::SubscribeNotifications::info.type_.type_: return "SubscribeNotifications(6)";
    case /*7*/StoreNotification::SubscriptionEnded::info.type_.type_: return "SubscriptionEnded(7)";
    case /*8*/StoreNotification::UnsolicitedStoreNotifications::info.type_.type_: return "UnsolicitedStoreNotifications(8)";
    case /*9*/StoreNotification::Chunk::info.type_.type_: return "Chunk(9)";
    case /*10*/StoreNotification::SubscribeNotificationsResponse::info.type_.type_: return "SubscribeNotificationsResponse(10)";
    case /*11*/StoreNotification::ObjectActiveStatusChanged::info.type_.type_: return "ObjectActiveStatusChanged(11)";
    }
    return nullptr;
}

const char* IStoreNotification::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IStoreNotification::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case UnsubscribeNotifications_req_t::info.type_.type_: // 4
        return 0; // no response message
    case SubscribeNotifications_req_t::info.type_.type_: // 6
        return SubscribeNotifications_res_t::info.type_.type_; // 10
    }
    return -1; // unknown request
}

void IStoreNotification::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case UnsubscribeNotifications_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<UnsubscribeNotifications_req_t*>(p_body.get()));
        auto* body = static_cast<UnsubscribeNotifications_req_t*>(p_body.get());
        UnsubscribeNotifications(
            ireq, std::move(*body),
            unwrapHandlerVoid(std::move(wres_handler))
        );
        return;
    }

    case SubscribeNotifications_req_t::info.type_.type_: { // 6
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<SubscribeNotifications_req_t*>(p_body.get()));
        auto* body = static_cast<SubscribeNotifications_req_t*>(p_body.get());
        SubscribeNotifications(
            ireq, std::move(*body),
            unwrapHandler<SubscribeNotifications_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IStoreNotification> IStoreNotification::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IStoreNotification, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void UnsubscribeNotifications(
            IRequest& ireq, UnsubscribeNotifications_req_t req, // 5.4
            EtpCompleteHandler handler_cb
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void SubscribeNotifications(
            IRequest& ireq, SubscribeNotifications_req_t req, // 5.6
            EtpResponseHandler<SubscribeNotifications_res_t> handler_cb // 5.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IGrowingObject::IGrowingObject(
) : IProtocol(Datatypes::Protocol::eGrowingObject, "GrowingObject")
{
}

const char* IGrowingObject::protocolName() {
    return "GrowingObject(6)";
}

const char* IGrowingObject::messageName(int messageType) {
    switch (messageType) {
    case /*1*/GrowingObject::DeleteParts::info.type_.type_: return "DeleteParts(1)";
    case /*3*/GrowingObject::GetParts::info.type_.type_: return "GetParts(3)";
    case /*4*/GrowingObject::GetPartsByRange::info.type_.type_: return "GetPartsByRange(4)";
    case /*5*/GrowingObject::PutParts::info.type_.type_: return "PutParts(5)";
    case /*6*/GrowingObject::GetPartsResponse::info.type_.type_: return "GetPartsResponse(6)";
    case /*7*/GrowingObject::ReplacePartsByRange::info.type_.type_: return "ReplacePartsByRange(7)";
    case /*8*/GrowingObject::GetPartsMetadata::info.type_.type_: return "GetPartsMetadata(8)";
    case /*9*/GrowingObject::GetPartsMetadataResponse::info.type_.type_: return "GetPartsMetadataResponse(9)";
    case /*10*/GrowingObject::GetPartsByRangeResponse::info.type_.type_: return "GetPartsByRangeResponse(10)";
    case /*11*/GrowingObject::DeletePartsResponse::info.type_.type_: return "DeletePartsResponse(11)";
    case /*13*/GrowingObject::PutPartsResponse::info.type_.type_: return "PutPartsResponse(13)";
    case /*14*/GrowingObject::GetGrowingDataObjectsHeader::info.type_.type_: return "GetGrowingDataObjectsHeader(14)";
    case /*15*/GrowingObject::GetGrowingDataObjectsHeaderResponse::info.type_.type_: return "GetGrowingDataObjectsHeaderResponse(15)";
    case /*16*/GrowingObject::PutGrowingDataObjectsHeader::info.type_.type_: return "PutGrowingDataObjectsHeader(16)";
    case /*17*/GrowingObject::PutGrowingDataObjectsHeaderResponse::info.type_.type_: return "PutGrowingDataObjectsHeaderResponse(17)";
    case /*18*/GrowingObject::ReplacePartsByRangeResponse::info.type_.type_: return "ReplacePartsByRangeResponse(18)";
    case /*19*/GrowingObject::GetChangeAnnotations::info.type_.type_: return "GetChangeAnnotations(19)";
    case /*20*/GrowingObject::GetChangeAnnotationsResponse::info.type_.type_: return "GetChangeAnnotationsResponse(20)";
    }
    return nullptr;
}

const char* IGrowingObject::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IGrowingObject::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case DeleteParts_req_t::info.type_.type_: // 1
        return DeleteParts_res_t::info.type_.type_; // 11
    case GetParts_req_t::info.type_.type_: // 3
        return GetParts_res_t::info.type_.type_; // 6
    case GetPartsByRange_req_t::info.type_.type_: // 4
        return GetPartsByRange_res_t::info.type_.type_; // 10
    case PutParts_req_t::info.type_.type_: // 5
        return PutParts_res_t::info.type_.type_; // 13
    case ReplacePartsByRange_req_t::info.type_.type_: // 7
        return ReplacePartsByRange_res_t::info.type_.type_; // 18
    case GetPartsMetadata_req_t::info.type_.type_: // 8
        return GetPartsMetadata_res_t::info.type_.type_; // 9
    case GetGrowingDataObjectsHeader_req_t::info.type_.type_: // 14
        return GetGrowingDataObjectsHeader_res_t::info.type_.type_; // 15
    case PutGrowingDataObjectsHeader_req_t::info.type_.type_: // 16
        return PutGrowingDataObjectsHeader_res_t::info.type_.type_; // 17
    case GetChangeAnnotations_req_t::info.type_.type_: // 19
        return GetChangeAnnotations_res_t::info.type_.type_; // 20
    }
    return -1; // unknown request
}

void IGrowingObject::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case DeleteParts_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<DeleteParts_req_t*>(p_body.get()));
        auto* body = static_cast<DeleteParts_req_t*>(p_body.get());
        DeleteParts(
            ireq, std::move(*body),
            unwrapHandler<DeleteParts_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetParts_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetParts_req_t*>(p_body.get()));
        auto* body = static_cast<GetParts_req_t*>(p_body.get());
        GetParts(
            ireq, std::move(*body),
            unwrapHandler<GetParts_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetPartsByRange_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetPartsByRange_req_t*>(p_body.get()));
        auto* body = static_cast<GetPartsByRange_req_t*>(p_body.get());
        GetPartsByRange(
            ireq, std::move(*body),
            unwrapHandler<GetPartsByRange_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutParts_req_t::info.type_.type_: { // 5
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutParts_req_t*>(p_body.get()));
        auto* body = static_cast<PutParts_req_t*>(p_body.get());
        PutParts(
            ireq, std::move(*body),
            unwrapHandler<PutParts_res_t>(std::move(wres_handler))
        );
        return;
    }

    case ReplacePartsByRange_req_t::info.type_.type_: { // 7
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<ReplacePartsByRange_req_t*>(p_body.get()));
        auto* body = static_cast<ReplacePartsByRange_req_t*>(p_body.get());
        ReplacePartsByRange(
            ireq, std::move(*body),
            unwrapHandler<ReplacePartsByRange_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetPartsMetadata_req_t::info.type_.type_: { // 8
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetPartsMetadata_req_t*>(p_body.get()));
        auto* body = static_cast<GetPartsMetadata_req_t*>(p_body.get());
        GetPartsMetadata(
            ireq, std::move(*body),
            unwrapHandler<GetPartsMetadata_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetGrowingDataObjectsHeader_req_t::info.type_.type_: { // 14
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetGrowingDataObjectsHeader_req_t*>(p_body.get()));
        auto* body = static_cast<GetGrowingDataObjectsHeader_req_t*>(p_body.get());
        GetGrowingDataObjectsHeader(
            ireq, std::move(*body),
            unwrapHandler<GetGrowingDataObjectsHeader_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutGrowingDataObjectsHeader_req_t::info.type_.type_: { // 16
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutGrowingDataObjectsHeader_req_t*>(p_body.get()));
        auto* body = static_cast<PutGrowingDataObjectsHeader_req_t*>(p_body.get());
        PutGrowingDataObjectsHeader(
            ireq, std::move(*body),
            unwrapHandler<PutGrowingDataObjectsHeader_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetChangeAnnotations_req_t::info.type_.type_: { // 19
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetChangeAnnotations_req_t*>(p_body.get()));
        auto* body = static_cast<GetChangeAnnotations_req_t*>(p_body.get());
        GetChangeAnnotations(
            ireq, std::move(*body),
            unwrapHandler<GetChangeAnnotations_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IGrowingObject> IGrowingObject::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IGrowingObject, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void DeleteParts(
            IRequest& ireq, DeleteParts_req_t req, // 6.1
            EtpResponseHandler<DeleteParts_res_t> handler_cb // 6.11
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetParts(
            IRequest& ireq, GetParts_req_t req, // 6.3
            EtpResponseHandler<GetParts_res_t> handler_cb // 6.6
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetPartsByRange(
            IRequest& ireq, GetPartsByRange_req_t req, // 6.4
            EtpResponseHandler<GetPartsByRange_res_t> handler_cb // 6.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutParts(
            IRequest& ireq, PutParts_req_t req, // 6.5
            EtpResponseHandler<PutParts_res_t> handler_cb // 6.13
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void ReplacePartsByRange(
            IRequest& ireq, ReplacePartsByRange_req_t req, // 6.7
            EtpResponseHandler<ReplacePartsByRange_res_t> handler_cb // 6.18
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetPartsMetadata(
            IRequest& ireq, GetPartsMetadata_req_t req, // 6.8
            EtpResponseHandler<GetPartsMetadata_res_t> handler_cb // 6.9
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetGrowingDataObjectsHeader(
            IRequest& ireq, GetGrowingDataObjectsHeader_req_t req, // 6.14
            EtpResponseHandler<GetGrowingDataObjectsHeader_res_t> handler_cb // 6.15
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutGrowingDataObjectsHeader(
            IRequest& ireq, PutGrowingDataObjectsHeader_req_t req, // 6.16
            EtpResponseHandler<PutGrowingDataObjectsHeader_res_t> handler_cb // 6.17
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetChangeAnnotations(
            IRequest& ireq, GetChangeAnnotations_req_t req, // 6.19
            EtpResponseHandler<GetChangeAnnotations_res_t> handler_cb // 6.20
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IGrowingObjectNotification::IGrowingObjectNotification(
) : IProtocol(Datatypes::Protocol::eGrowingObjectNotification, "GrowingObjectNotification")
{
}

const char* IGrowingObjectNotification::protocolName() {
    return "GrowingObjectNotification(7)";
}

const char* IGrowingObjectNotification::messageName(int messageType) {
    switch (messageType) {
    case /*2*/GrowingObjectNotification::PartsChanged::info.type_.type_: return "PartsChanged(2)";
    case /*3*/GrowingObjectNotification::PartsDeleted::info.type_.type_: return "PartsDeleted(3)";
    case /*4*/GrowingObjectNotification::UnsubscribePartNotification::info.type_.type_: return "UnsubscribePartNotification(4)";
    case /*6*/GrowingObjectNotification::PartsReplacedByRange::info.type_.type_: return "PartsReplacedByRange(6)";
    case /*7*/GrowingObjectNotification::SubscribePartNotifications::info.type_.type_: return "SubscribePartNotifications(7)";
    case /*8*/GrowingObjectNotification::PartSubscriptionEnded::info.type_.type_: return "PartSubscriptionEnded(8)";
    case /*9*/GrowingObjectNotification::UnsolicitedPartNotifications::info.type_.type_: return "UnsolicitedPartNotifications(9)";
    case /*10*/GrowingObjectNotification::SubscribePartNotificationsResponse::info.type_.type_: return "SubscribePartNotificationsResponse(10)";
    }
    return nullptr;
}

const char* IGrowingObjectNotification::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IGrowingObjectNotification::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case UnsubscribePartNotification_req_t::info.type_.type_: // 4
        return UnsubscribePartNotification_res_t::info.type_.type_; // 8
    case SubscribePartNotifications_req_t::info.type_.type_: // 7
        return SubscribePartNotifications_res_t::info.type_.type_; // 10
    }
    return -1; // unknown request
}

void IGrowingObjectNotification::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case UnsubscribePartNotification_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<UnsubscribePartNotification_req_t*>(p_body.get()));
        auto* body = static_cast<UnsubscribePartNotification_req_t*>(p_body.get());
        UnsubscribePartNotification(
            ireq, std::move(*body),
            unwrapHandler<UnsubscribePartNotification_res_t>(std::move(wres_handler))
        );
        return;
    }

    case SubscribePartNotifications_req_t::info.type_.type_: { // 7
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<SubscribePartNotifications_req_t*>(p_body.get()));
        auto* body = static_cast<SubscribePartNotifications_req_t*>(p_body.get());
        SubscribePartNotifications(
            ireq, std::move(*body),
            unwrapHandler<SubscribePartNotifications_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IGrowingObjectNotification> IGrowingObjectNotification::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IGrowingObjectNotification, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void UnsubscribePartNotification(
            IRequest& ireq, UnsubscribePartNotification_req_t req, // 7.4
            EtpResponseHandler<UnsubscribePartNotification_res_t> handler_cb // 7.8
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void SubscribePartNotifications(
            IRequest& ireq, SubscribePartNotifications_req_t req, // 7.7
            EtpResponseHandler<SubscribePartNotifications_res_t> handler_cb // 7.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IDataArray::IDataArray(
) : IProtocol(Datatypes::Protocol::eDataArray, "DataArray")
{
}

const char* IDataArray::protocolName() {
    return "DataArray(9)";
}

const char* IDataArray::messageName(int messageType) {
    switch (messageType) {
    case /*1*/DataArray::GetDataArraysResponse::info.type_.type_: return "GetDataArraysResponse(1)";
    case /*2*/DataArray::GetDataArrays::info.type_.type_: return "GetDataArrays(2)";
    case /*3*/DataArray::GetDataSubarrays::info.type_.type_: return "GetDataSubarrays(3)";
    case /*4*/DataArray::PutDataArrays::info.type_.type_: return "PutDataArrays(4)";
    case /*5*/DataArray::PutDataSubarrays::info.type_.type_: return "PutDataSubarrays(5)";
    case /*6*/DataArray::GetDataArrayMetadata::info.type_.type_: return "GetDataArrayMetadata(6)";
    case /*7*/DataArray::GetDataArrayMetadataResponse::info.type_.type_: return "GetDataArrayMetadataResponse(7)";
    case /*8*/DataArray::GetDataSubarraysResponse::info.type_.type_: return "GetDataSubarraysResponse(8)";
    case /*9*/DataArray::PutUninitializedDataArrays::info.type_.type_: return "PutUninitializedDataArrays(9)";
    case /*10*/DataArray::PutDataArraysResponse::info.type_.type_: return "PutDataArraysResponse(10)";
    case /*11*/DataArray::PutDataSubarraysResponse::info.type_.type_: return "PutDataSubarraysResponse(11)";
    case /*12*/DataArray::PutUninitializedDataArraysResponse::info.type_.type_: return "PutUninitializedDataArraysResponse(12)";
    }
    return nullptr;
}

const char* IDataArray::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IDataArray::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetDataArrays_req_t::info.type_.type_: // 2
        return GetDataArrays_res_t::info.type_.type_; // 1
    case GetDataSubarrays_req_t::info.type_.type_: // 3
        return GetDataSubarrays_res_t::info.type_.type_; // 8
    case PutDataArrays_req_t::info.type_.type_: // 4
        return PutDataArrays_res_t::info.type_.type_; // 10
    case PutDataSubarrays_req_t::info.type_.type_: // 5
        return PutDataSubarrays_res_t::info.type_.type_; // 11
    case GetDataArrayMetadata_req_t::info.type_.type_: // 6
        return GetDataArrayMetadata_res_t::info.type_.type_; // 7
    case PutUninitializedDataArrays_req_t::info.type_.type_: // 9
        return PutUninitializedDataArrays_res_t::info.type_.type_; // 12
    }
    return -1; // unknown request
}

void IDataArray::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetDataArrays_req_t::info.type_.type_: { // 2
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDataArrays_req_t*>(p_body.get()));
        auto* body = static_cast<GetDataArrays_req_t*>(p_body.get());
        GetDataArrays(
            ireq, std::move(*body),
            unwrapHandler<GetDataArrays_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetDataSubarrays_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDataSubarrays_req_t*>(p_body.get()));
        auto* body = static_cast<GetDataSubarrays_req_t*>(p_body.get());
        GetDataSubarrays(
            ireq, std::move(*body),
            unwrapHandler<GetDataSubarrays_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutDataArrays_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutDataArrays_req_t*>(p_body.get()));
        auto* body = static_cast<PutDataArrays_req_t*>(p_body.get());
        PutDataArrays(
            ireq, std::move(*body),
            unwrapHandler<PutDataArrays_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutDataSubarrays_req_t::info.type_.type_: { // 5
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutDataSubarrays_req_t*>(p_body.get()));
        auto* body = static_cast<PutDataSubarrays_req_t*>(p_body.get());
        PutDataSubarrays(
            ireq, std::move(*body),
            unwrapHandler<PutDataSubarrays_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetDataArrayMetadata_req_t::info.type_.type_: { // 6
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDataArrayMetadata_req_t*>(p_body.get()));
        auto* body = static_cast<GetDataArrayMetadata_req_t*>(p_body.get());
        GetDataArrayMetadata(
            ireq, std::move(*body),
            unwrapHandler<GetDataArrayMetadata_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutUninitializedDataArrays_req_t::info.type_.type_: { // 9
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutUninitializedDataArrays_req_t*>(p_body.get()));
        auto* body = static_cast<PutUninitializedDataArrays_req_t*>(p_body.get());
        PutUninitializedDataArrays(
            ireq, std::move(*body),
            unwrapHandler<PutUninitializedDataArrays_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IDataArray> IDataArray::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IDataArray, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetDataArrays(
            IRequest& ireq, GetDataArrays_req_t req, // 9.2
            EtpResponseHandler<GetDataArrays_res_t> handler_cb // 9.1
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetDataSubarrays(
            IRequest& ireq, GetDataSubarrays_req_t req, // 9.3
            EtpResponseHandler<GetDataSubarrays_res_t> handler_cb // 9.8
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutDataArrays(
            IRequest& ireq, PutDataArrays_req_t req, // 9.4
            EtpResponseHandler<PutDataArrays_res_t> handler_cb // 9.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutDataSubarrays(
            IRequest& ireq, PutDataSubarrays_req_t req, // 9.5
            EtpResponseHandler<PutDataSubarrays_res_t> handler_cb // 9.11
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetDataArrayMetadata(
            IRequest& ireq, GetDataArrayMetadata_req_t req, // 9.6
            EtpResponseHandler<GetDataArrayMetadata_res_t> handler_cb // 9.7
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutUninitializedDataArrays(
            IRequest& ireq, PutUninitializedDataArrays_req_t req, // 9.9
            EtpResponseHandler<PutUninitializedDataArrays_res_t> handler_cb // 9.12
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IDiscoveryQuery::IDiscoveryQuery(
) : IProtocol(Datatypes::Protocol::eDiscoveryQuery, "DiscoveryQuery")
{
}

const char* IDiscoveryQuery::protocolName() {
    return "DiscoveryQuery(13)";
}

const char* IDiscoveryQuery::messageName(int messageType) {
    switch (messageType) {
    case /*1*/DiscoveryQuery::FindResources::info.type_.type_: return "FindResources(1)";
    case /*2*/DiscoveryQuery::FindResourcesResponse::info.type_.type_: return "FindResourcesResponse(2)";
    }
    return nullptr;
}

const char* IDiscoveryQuery::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IDiscoveryQuery::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case FindResources_req_t::info.type_.type_: // 1
        return FindResources_res_t::info.type_.type_; // 2
    }
    return -1; // unknown request
}

void IDiscoveryQuery::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case FindResources_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<FindResources_req_t*>(p_body.get()));
        auto* body = static_cast<FindResources_req_t*>(p_body.get());
        FindResources(
            ireq, std::move(*body),
            unwrapHandler<FindResources_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IDiscoveryQuery> IDiscoveryQuery::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IDiscoveryQuery, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void FindResources(
            IRequest& ireq, FindResources_req_t req, // 13.1
            EtpResponseHandler<FindResources_res_t> handler_cb // 13.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IStoreQuery::IStoreQuery(
) : IProtocol(Datatypes::Protocol::eStoreQuery, "StoreQuery")
{
}

const char* IStoreQuery::protocolName() {
    return "StoreQuery(14)";
}

const char* IStoreQuery::messageName(int messageType) {
    switch (messageType) {
    case /*1*/StoreQuery::FindDataObjects::info.type_.type_: return "FindDataObjects(1)";
    case /*2*/StoreQuery::FindDataObjectsResponse::info.type_.type_: return "FindDataObjectsResponse(2)";
    case /*3*/StoreQuery::Chunk::info.type_.type_: return "Chunk(3)";
    }
    return nullptr;
}

const char* IStoreQuery::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IStoreQuery::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case FindDataObjects_req_t::info.type_.type_: // 1
        return FindDataObjects_res_t::info.type_.type_; // 2
    }
    return -1; // unknown request
}

void IStoreQuery::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case FindDataObjects_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<FindDataObjects_req_t*>(p_body.get()));
        auto* body = static_cast<FindDataObjects_req_t*>(p_body.get());
        FindDataObjects(
            ireq, std::move(*body),
            unwrapHandler<FindDataObjects_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IStoreQuery> IStoreQuery::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IStoreQuery, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void FindDataObjects(
            IRequest& ireq, FindDataObjects_req_t req, // 14.1
            EtpResponseHandler<FindDataObjects_res_t> handler_cb // 14.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IGrowingObjectQuery::IGrowingObjectQuery(
) : IProtocol(Datatypes::Protocol::eGrowingObjectQuery, "GrowingObjectQuery")
{
}

const char* IGrowingObjectQuery::protocolName() {
    return "GrowingObjectQuery(16)";
}

const char* IGrowingObjectQuery::messageName(int messageType) {
    switch (messageType) {
    case /*1*/GrowingObjectQuery::FindParts::info.type_.type_: return "FindParts(1)";
    case /*2*/GrowingObjectQuery::FindPartsResponse::info.type_.type_: return "FindPartsResponse(2)";
    }
    return nullptr;
}

const char* IGrowingObjectQuery::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IGrowingObjectQuery::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case FindParts_req_t::info.type_.type_: // 1
        return FindParts_res_t::info.type_.type_; // 2
    }
    return -1; // unknown request
}

void IGrowingObjectQuery::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case FindParts_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<FindParts_req_t*>(p_body.get()));
        auto* body = static_cast<FindParts_req_t*>(p_body.get());
        FindParts(
            ireq, std::move(*body),
            unwrapHandler<FindParts_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IGrowingObjectQuery> IGrowingObjectQuery::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IGrowingObjectQuery, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void FindParts(
            IRequest& ireq, FindParts_req_t req, // 16.1
            EtpResponseHandler<FindParts_res_t> handler_cb // 16.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

ITransaction::ITransaction(
) : IProtocol(Datatypes::Protocol::eTransaction, "Transaction")
{
}

const char* ITransaction::protocolName() {
    return "Transaction(18)";
}

const char* ITransaction::messageName(int messageType) {
    switch (messageType) {
    case /*1*/Transaction::StartTransaction::info.type_.type_: return "StartTransaction(1)";
    case /*2*/Transaction::StartTransactionResponse::info.type_.type_: return "StartTransactionResponse(2)";
    case /*3*/Transaction::CommitTransaction::info.type_.type_: return "CommitTransaction(3)";
    case /*4*/Transaction::RollbackTransaction::info.type_.type_: return "RollbackTransaction(4)";
    case /*5*/Transaction::CommitTransactionResponse::info.type_.type_: return "CommitTransactionResponse(5)";
    case /*6*/Transaction::RollbackTransactionResponse::info.type_.type_: return "RollbackTransactionResponse(6)";
    }
    return nullptr;
}

const char* ITransaction::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int ITransaction::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case StartTransaction_req_t::info.type_.type_: // 1
        return StartTransaction_res_t::info.type_.type_; // 2
    case CommitTransaction_req_t::info.type_.type_: // 3
        return CommitTransaction_res_t::info.type_.type_; // 5
    case RollbackTransaction_req_t::info.type_.type_: // 4
        return RollbackTransaction_res_t::info.type_.type_; // 6
    }
    return -1; // unknown request
}

void ITransaction::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case StartTransaction_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<StartTransaction_req_t*>(p_body.get()));
        auto* body = static_cast<StartTransaction_req_t*>(p_body.get());
        StartTransaction(
            ireq, std::move(*body),
            unwrapHandler<StartTransaction_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CommitTransaction_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CommitTransaction_req_t*>(p_body.get()));
        auto* body = static_cast<CommitTransaction_req_t*>(p_body.get());
        CommitTransaction(
            ireq, std::move(*body),
            unwrapHandler<CommitTransaction_res_t>(std::move(wres_handler))
        );
        return;
    }

    case RollbackTransaction_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<RollbackTransaction_req_t*>(p_body.get()));
        auto* body = static_cast<RollbackTransaction_req_t*>(p_body.get());
        RollbackTransaction(
            ireq, std::move(*body),
            unwrapHandler<RollbackTransaction_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<ITransaction> ITransaction::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : ITransaction, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void StartTransaction(
            IRequest& ireq, StartTransaction_req_t req, // 18.1
            EtpResponseHandler<StartTransaction_res_t> handler_cb // 18.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CommitTransaction(
            IRequest& ireq, CommitTransaction_req_t req, // 18.3
            EtpResponseHandler<CommitTransaction_res_t> handler_cb // 18.5
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void RollbackTransaction(
            IRequest& ireq, RollbackTransaction_req_t req, // 18.4
            EtpResponseHandler<RollbackTransaction_res_t> handler_cb // 18.6
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IChannelSubscribe::IChannelSubscribe(
) : IProtocol(Datatypes::Protocol::eChannelSubscribe, "ChannelSubscribe")
{
}

const char* IChannelSubscribe::protocolName() {
    return "ChannelSubscribe(21)";
}

const char* IChannelSubscribe::messageName(int messageType) {
    switch (messageType) {
    case /*1*/ChannelSubscribe::GetChannelMetadata::info.type_.type_: return "GetChannelMetadata(1)";
    case /*2*/ChannelSubscribe::GetChannelMetadataResponse::info.type_.type_: return "GetChannelMetadataResponse(2)";
    case /*3*/ChannelSubscribe::SubscribeChannels::info.type_.type_: return "SubscribeChannels(3)";
    case /*4*/ChannelSubscribe::ChannelData::info.type_.type_: return "ChannelData(4)";
    case /*6*/ChannelSubscribe::RangeReplaced::info.type_.type_: return "RangeReplaced(6)";
    case /*7*/ChannelSubscribe::UnsubscribeChannels::info.type_.type_: return "UnsubscribeChannels(7)";
    case /*8*/ChannelSubscribe::SubscriptionsStopped::info.type_.type_: return "SubscriptionsStopped(8)";
    case /*9*/ChannelSubscribe::GetRanges::info.type_.type_: return "GetRanges(9)";
    case /*10*/ChannelSubscribe::GetRangesResponse::info.type_.type_: return "GetRangesResponse(10)";
    case /*11*/ChannelSubscribe::CancelGetRanges::info.type_.type_: return "CancelGetRanges(11)";
    case /*12*/ChannelSubscribe::SubscribeChannelsResponse::info.type_.type_: return "SubscribeChannelsResponse(12)";
    case /*13*/ChannelSubscribe::ChannelsTruncated::info.type_.type_: return "ChannelsTruncated(13)";
    case /*14*/ChannelSubscribe::GetChangeAnnotations::info.type_.type_: return "GetChangeAnnotations(14)";
    case /*15*/ChannelSubscribe::GetChangeAnnotationsResponse::info.type_.type_: return "GetChangeAnnotationsResponse(15)";
    }
    return nullptr;
}

const char* IChannelSubscribe::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IChannelSubscribe::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetChannelMetadata_req_t::info.type_.type_: // 1
        return GetChannelMetadata_res_t::info.type_.type_; // 2
    case SubscribeChannels_req_t::info.type_.type_: // 3
        return SubscribeChannels_res_t::info.type_.type_; // 12
    case UnsubscribeChannels_req_t::info.type_.type_: // 7
        return UnsubscribeChannels_res_t::info.type_.type_; // 8
    case GetRanges_req_t::info.type_.type_: // 9
        return GetRanges_res_t::info.type_.type_; // 10
    case CancelGetRanges_req_t::info.type_.type_: // 11
        return CancelGetRanges_res_t::info.type_.type_; // 10
    case GetChangeAnnotations_req_t::info.type_.type_: // 14
        return GetChangeAnnotations_res_t::info.type_.type_; // 15
    }
    return -1; // unknown request
}

void IChannelSubscribe::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetChannelMetadata_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetChannelMetadata_req_t*>(p_body.get()));
        auto* body = static_cast<GetChannelMetadata_req_t*>(p_body.get());
        GetChannelMetadata(
            ireq, std::move(*body),
            unwrapHandler<GetChannelMetadata_res_t>(std::move(wres_handler))
        );
        return;
    }

    case SubscribeChannels_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<SubscribeChannels_req_t*>(p_body.get()));
        auto* body = static_cast<SubscribeChannels_req_t*>(p_body.get());
        SubscribeChannels(
            ireq, std::move(*body),
            unwrapHandler<SubscribeChannels_res_t>(std::move(wres_handler))
        );
        return;
    }

    case UnsubscribeChannels_req_t::info.type_.type_: { // 7
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<UnsubscribeChannels_req_t*>(p_body.get()));
        auto* body = static_cast<UnsubscribeChannels_req_t*>(p_body.get());
        UnsubscribeChannels(
            ireq, std::move(*body),
            unwrapHandler<UnsubscribeChannels_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetRanges_req_t::info.type_.type_: { // 9
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetRanges_req_t*>(p_body.get()));
        auto* body = static_cast<GetRanges_req_t*>(p_body.get());
        GetRanges(
            ireq, std::move(*body),
            unwrapHandler<GetRanges_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CancelGetRanges_req_t::info.type_.type_: { // 11
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CancelGetRanges_req_t*>(p_body.get()));
        auto* body = static_cast<CancelGetRanges_req_t*>(p_body.get());
        CancelGetRanges(
            ireq, std::move(*body),
            unwrapHandler<CancelGetRanges_res_t>(std::move(wres_handler))
        );
        return;
    }

    case GetChangeAnnotations_req_t::info.type_.type_: { // 14
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetChangeAnnotations_req_t*>(p_body.get()));
        auto* body = static_cast<GetChangeAnnotations_req_t*>(p_body.get());
        GetChangeAnnotations(
            ireq, std::move(*body),
            unwrapHandler<GetChangeAnnotations_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IChannelSubscribe> IChannelSubscribe::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IChannelSubscribe, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetChannelMetadata(
            IRequest& ireq, GetChannelMetadata_req_t req, // 21.1
            EtpResponseHandler<GetChannelMetadata_res_t> handler_cb // 21.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void SubscribeChannels(
            IRequest& ireq, SubscribeChannels_req_t req, // 21.3
            EtpResponseHandler<SubscribeChannels_res_t> handler_cb // 21.12
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void UnsubscribeChannels(
            IRequest& ireq, UnsubscribeChannels_req_t req, // 21.7
            EtpResponseHandler<UnsubscribeChannels_res_t> handler_cb // 21.8
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetRanges(
            IRequest& ireq, GetRanges_req_t req, // 21.9
            EtpResponseHandler<GetRanges_res_t> handler_cb // 21.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CancelGetRanges(
            IRequest& ireq, CancelGetRanges_req_t req, // 21.11
            EtpResponseHandler<CancelGetRanges_res_t> handler_cb // 21.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void GetChangeAnnotations(
            IRequest& ireq, GetChangeAnnotations_req_t req, // 21.14
            EtpResponseHandler<GetChangeAnnotations_res_t> handler_cb // 21.15
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IChannelDataLoad::IChannelDataLoad(
) : IProtocol(Datatypes::Protocol::eChannelDataLoad, "ChannelDataLoad")
{
}

const char* IChannelDataLoad::protocolName() {
    return "ChannelDataLoad(22)";
}

const char* IChannelDataLoad::messageName(int messageType) {
    switch (messageType) {
    case /*1*/ChannelDataLoad::OpenChannels::info.type_.type_: return "OpenChannels(1)";
    case /*2*/ChannelDataLoad::OpenChannelsResponse::info.type_.type_: return "OpenChannelsResponse(2)";
    case /*3*/ChannelDataLoad::CloseChannels::info.type_.type_: return "CloseChannels(3)";
    case /*4*/ChannelDataLoad::ChannelData::info.type_.type_: return "ChannelData(4)";
    case /*6*/ChannelDataLoad::ReplaceRange::info.type_.type_: return "ReplaceRange(6)";
    case /*7*/ChannelDataLoad::ChannelsClosed::info.type_.type_: return "ChannelsClosed(7)";
    case /*8*/ChannelDataLoad::ReplaceRangeResponse::info.type_.type_: return "ReplaceRangeResponse(8)";
    case /*9*/ChannelDataLoad::TruncateChannels::info.type_.type_: return "TruncateChannels(9)";
    case /*10*/ChannelDataLoad::TruncateChannelsResponse::info.type_.type_: return "TruncateChannelsResponse(10)";
    }
    return nullptr;
}

const char* IChannelDataLoad::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IChannelDataLoad::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case OpenChannels_req_t::info.type_.type_: // 1
        return OpenChannels_res_t::info.type_.type_; // 2
    case CloseChannels_req_t::info.type_.type_: // 3
        return CloseChannels_res_t::info.type_.type_; // 7
    case ChannelData_req_t::info.type_.type_: // 4
        return 0; // no response message
    case ReplaceRange_req_t::info.type_.type_: // 6
        return ReplaceRange_res_t::info.type_.type_; // 8
    case TruncateChannels_req_t::info.type_.type_: // 9
        return TruncateChannels_res_t::info.type_.type_; // 10
    }
    return -1; // unknown request
}

void IChannelDataLoad::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case OpenChannels_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<OpenChannels_req_t*>(p_body.get()));
        auto* body = static_cast<OpenChannels_req_t*>(p_body.get());
        OpenChannels(
            ireq, std::move(*body),
            unwrapHandler<OpenChannels_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CloseChannels_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CloseChannels_req_t*>(p_body.get()));
        auto* body = static_cast<CloseChannels_req_t*>(p_body.get());
        CloseChannels(
            ireq, std::move(*body),
            unwrapHandler<CloseChannels_res_t>(std::move(wres_handler))
        );
        return;
    }

    case ChannelData_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<ChannelData_req_t*>(p_body.get()));
        auto* body = static_cast<ChannelData_req_t*>(p_body.get());
        ChannelData(
            ireq, std::move(*body),
            unwrapHandlerVoid(std::move(wres_handler))
        );
        return;
    }

    case ReplaceRange_req_t::info.type_.type_: { // 6
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<ReplaceRange_req_t*>(p_body.get()));
        auto* body = static_cast<ReplaceRange_req_t*>(p_body.get());
        ReplaceRange(
            ireq, std::move(*body),
            unwrapHandler<ReplaceRange_res_t>(std::move(wres_handler))
        );
        return;
    }

    case TruncateChannels_req_t::info.type_.type_: { // 9
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<TruncateChannels_req_t*>(p_body.get()));
        auto* body = static_cast<TruncateChannels_req_t*>(p_body.get());
        TruncateChannels(
            ireq, std::move(*body),
            unwrapHandler<TruncateChannels_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IChannelDataLoad> IChannelDataLoad::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IChannelDataLoad, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void OpenChannels(
            IRequest& ireq, OpenChannels_req_t req, // 22.1
            EtpResponseHandler<OpenChannels_res_t> handler_cb // 22.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CloseChannels(
            IRequest& ireq, CloseChannels_req_t req, // 22.3
            EtpResponseHandler<CloseChannels_res_t> handler_cb // 22.7
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void ChannelData(
            IRequest& ireq, ChannelData_req_t req, // 22.4
            EtpCompleteHandler handler_cb
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void ReplaceRange(
            IRequest& ireq, ReplaceRange_req_t req, // 22.6
            EtpResponseHandler<ReplaceRange_res_t> handler_cb // 22.8
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void TruncateChannels(
            IRequest& ireq, TruncateChannels_req_t req, // 22.9
            EtpResponseHandler<TruncateChannels_res_t> handler_cb // 22.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IDataspace::IDataspace(
) : IProtocol(Datatypes::Protocol::eDataspace, "Dataspace")
{
}

const char* IDataspace::protocolName() {
    return "Dataspace(24)";
}

const char* IDataspace::messageName(int messageType) {
    switch (messageType) {
    case /*1*/Dataspace::GetDataspaces::info.type_.type_: return "GetDataspaces(1)";
    case /*2*/Dataspace::GetDataspacesResponse::info.type_.type_: return "GetDataspacesResponse(2)";
    case /*3*/Dataspace::PutDataspaces::info.type_.type_: return "PutDataspaces(3)";
    case /*4*/Dataspace::DeleteDataspaces::info.type_.type_: return "DeleteDataspaces(4)";
    case /*5*/Dataspace::DeleteDataspacesResponse::info.type_.type_: return "DeleteDataspacesResponse(5)";
    case /*6*/Dataspace::PutDataspacesResponse::info.type_.type_: return "PutDataspacesResponse(6)";
    }
    return nullptr;
}

const char* IDataspace::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IDataspace::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetDataspaces_req_t::info.type_.type_: // 1
        return GetDataspaces_res_t::info.type_.type_; // 2
    case PutDataspaces_req_t::info.type_.type_: // 3
        return PutDataspaces_res_t::info.type_.type_; // 6
    case DeleteDataspaces_req_t::info.type_.type_: // 4
        return DeleteDataspaces_res_t::info.type_.type_; // 5
    }
    return -1; // unknown request
}

void IDataspace::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetDataspaces_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDataspaces_req_t*>(p_body.get()));
        auto* body = static_cast<GetDataspaces_req_t*>(p_body.get());
        GetDataspaces(
            ireq, std::move(*body),
            unwrapHandler<GetDataspaces_res_t>(std::move(wres_handler))
        );
        return;
    }

    case PutDataspaces_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<PutDataspaces_req_t*>(p_body.get()));
        auto* body = static_cast<PutDataspaces_req_t*>(p_body.get());
        PutDataspaces(
            ireq, std::move(*body),
            unwrapHandler<PutDataspaces_res_t>(std::move(wres_handler))
        );
        return;
    }

    case DeleteDataspaces_req_t::info.type_.type_: { // 4
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<DeleteDataspaces_req_t*>(p_body.get()));
        auto* body = static_cast<DeleteDataspaces_req_t*>(p_body.get());
        DeleteDataspaces(
            ireq, std::move(*body),
            unwrapHandler<DeleteDataspaces_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IDataspace> IDataspace::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IDataspace, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetDataspaces(
            IRequest& ireq, GetDataspaces_req_t req, // 24.1
            EtpResponseHandler<GetDataspaces_res_t> handler_cb // 24.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void PutDataspaces(
            IRequest& ireq, PutDataspaces_req_t req, // 24.3
            EtpResponseHandler<PutDataspaces_res_t> handler_cb // 24.6
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void DeleteDataspaces(
            IRequest& ireq, DeleteDataspaces_req_t req, // 24.4
            EtpResponseHandler<DeleteDataspaces_res_t> handler_cb // 24.5
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

ISupportedTypes::ISupportedTypes(
) : IProtocol(Datatypes::Protocol::eSupportedTypes, "SupportedTypes")
{
}

const char* ISupportedTypes::protocolName() {
    return "SupportedTypes(25)";
}

const char* ISupportedTypes::messageName(int messageType) {
    switch (messageType) {
    case /*1*/SupportedTypes::GetSupportedTypes::info.type_.type_: return "GetSupportedTypes(1)";
    case /*2*/SupportedTypes::GetSupportedTypesResponse::info.type_.type_: return "GetSupportedTypesResponse(2)";
    }
    return nullptr;
}

const char* ISupportedTypes::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int ISupportedTypes::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetSupportedTypes_req_t::info.type_.type_: // 1
        return GetSupportedTypes_res_t::info.type_.type_; // 2
    }
    return -1; // unknown request
}

void ISupportedTypes::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetSupportedTypes_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetSupportedTypes_req_t*>(p_body.get()));
        auto* body = static_cast<GetSupportedTypes_req_t*>(p_body.get());
        GetSupportedTypes(
            ireq, std::move(*body),
            unwrapHandler<GetSupportedTypes_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<ISupportedTypes> ISupportedTypes::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : ISupportedTypes, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetSupportedTypes(
            IRequest& ireq, GetSupportedTypes_req_t req, // 25.1
            EtpResponseHandler<GetSupportedTypes_res_t> handler_cb // 25.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

ICoreOSDU::ICoreOSDU(
) : IProtocol(Datatypes::Protocol::eCoreOSDU, "CoreOSDU")
{
}

const char* ICoreOSDU::protocolName() {
    return "CoreOSDU(2400)";
}

const char* ICoreOSDU::messageName(int messageType) {
    switch (messageType) {
    case /*1*/CoreOSDU::ResumeSession::info.type_.type_: return "ResumeSession(1)";
    case /*2*/CoreOSDU::ResumeSessionResponse::info.type_.type_: return "ResumeSessionResponse(2)";
    }
    return nullptr;
}

const char* ICoreOSDU::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int ICoreOSDU::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case ResumeSession_req_t::info.type_.type_: // 1
        return ResumeSession_res_t::info.type_.type_; // 2
    }
    return -1; // unknown request
}

void ICoreOSDU::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case ResumeSession_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<ResumeSession_req_t*>(p_body.get()));
        auto* body = static_cast<ResumeSession_req_t*>(p_body.get());
        ResumeSession(
            ireq, std::move(*body),
            unwrapHandler<ResumeSession_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<ICoreOSDU> ICoreOSDU::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : ICoreOSDU, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void ResumeSession(
            IRequest& ireq, ResumeSession_req_t req, // 2400.1
            EtpResponseHandler<ResumeSession_res_t> handler_cb // 2400.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IStoreOSDU::IStoreOSDU(
) : IProtocol(Datatypes::Protocol::eStoreOSDU, "StoreOSDU")
{
}

const char* IStoreOSDU::protocolName() {
    return "StoreOSDU(2404)";
}

const char* IStoreOSDU::messageName(int messageType) {
    switch (messageType) {
    case /*1*/StoreOSDU::CopyDataObjectsByValue::info.type_.type_: return "CopyDataObjectsByValue(1)";
    case /*2*/StoreOSDU::CopyDataObjectsByValueResponse::info.type_.type_: return "CopyDataObjectsByValueResponse(2)";
    }
    return nullptr;
}

const char* IStoreOSDU::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IStoreOSDU::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case CopyDataObjectsByValue_req_t::info.type_.type_: // 1
        return CopyDataObjectsByValue_res_t::info.type_.type_; // 2
    }
    return -1; // unknown request
}

void IStoreOSDU::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case CopyDataObjectsByValue_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CopyDataObjectsByValue_req_t*>(p_body.get()));
        auto* body = static_cast<CopyDataObjectsByValue_req_t*>(p_body.get());
        CopyDataObjectsByValue(
            ireq, std::move(*body),
            unwrapHandler<CopyDataObjectsByValue_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IStoreOSDU> IStoreOSDU::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IStoreOSDU, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void CopyDataObjectsByValue(
            IRequest& ireq, CopyDataObjectsByValue_req_t req, // 2404.1
            EtpResponseHandler<CopyDataObjectsByValue_res_t> handler_cb // 2404.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

IDataspaceOSDU::IDataspaceOSDU(
) : IProtocol(Datatypes::Protocol::eDataspaceOSDU, "DataspaceOSDU")
{
}

const char* IDataspaceOSDU::protocolName() {
    return "DataspaceOSDU(2424)";
}

const char* IDataspaceOSDU::messageName(int messageType) {
    switch (messageType) {
    case /*1*/DataspaceOSDU::GetDataspaceInfo::info.type_.type_: return "GetDataspaceInfo(1)";
    case /*2*/DataspaceOSDU::GetDataspaceInfoResponse::info.type_.type_: return "GetDataspaceInfoResponse(2)";
    case /*3*/DataspaceOSDU::CopyDataspacesContent::info.type_.type_: return "CopyDataspacesContent(3)";
    case /*4*/DataspaceOSDU::CopyDataspacesContentResponse::info.type_.type_: return "CopyDataspacesContentResponse(4)";
    case /*5*/DataspaceOSDU::LockDataspaces::info.type_.type_: return "LockDataspaces(5)";
    case /*6*/DataspaceOSDU::LockDataspacesResponse::info.type_.type_: return "LockDataspacesResponse(6)";
    case /*7*/DataspaceOSDU::CopyToDataspace::info.type_.type_: return "CopyToDataspace(7)";
    case /*8*/DataspaceOSDU::CopyToDataspaceResponse::info.type_.type_: return "CopyToDataspaceResponse(8)";
    case /*9*/DataspaceOSDU::CopyDataspaceFromRemote::info.type_.type_: return "CopyDataspaceFromRemote(9)";
    case /*10*/DataspaceOSDU::CopyDataspaceFromRemoteResponse::info.type_.type_: return "CopyDataspaceFromRemoteResponse(10)";
    }
    return nullptr;
}

const char* IDataspaceOSDU::messageTypeName(int messageType) const {
    return messageName(messageType);
}

int IDataspaceOSDU::responseMessageType(int requestMessageType) const {
    switch (requestMessageType) {
    case GetDataspaceInfo_req_t::info.type_.type_: // 1
        return GetDataspaceInfo_res_t::info.type_.type_; // 2
    case CopyDataspacesContent_req_t::info.type_.type_: // 3
        return CopyDataspacesContent_res_t::info.type_.type_; // 4
    case LockDataspaces_req_t::info.type_.type_: // 5
        return LockDataspaces_res_t::info.type_.type_; // 6
    case CopyToDataspace_req_t::info.type_.type_: // 7
        return CopyToDataspace_res_t::info.type_.type_; // 8
    case CopyDataspaceFromRemote_req_t::info.type_.type_: // 9
        return CopyDataspaceFromRemote_res_t::info.type_.type_; // 10
    }
    return -1; // unknown request
}

void IDataspaceOSDU::dispatchRequest(
    IRequest& ireq, EtpWrappedResponseHandler wres_handler
) {
    const auto& req_hdr = ireq.header();
    switch (req_hdr.m_messageType) {

    case GetDataspaceInfo_req_t::info.type_.type_: { // 1
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<GetDataspaceInfo_req_t*>(p_body.get()));
        auto* body = static_cast<GetDataspaceInfo_req_t*>(p_body.get());
        GetDataspaceInfo(
            ireq, std::move(*body),
            unwrapHandler<GetDataspaceInfo_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CopyDataspacesContent_req_t::info.type_.type_: { // 3
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CopyDataspacesContent_req_t*>(p_body.get()));
        auto* body = static_cast<CopyDataspacesContent_req_t*>(p_body.get());
        CopyDataspacesContent(
            ireq, std::move(*body),
            unwrapHandler<CopyDataspacesContent_res_t>(std::move(wres_handler))
        );
        return;
    }

    case LockDataspaces_req_t::info.type_.type_: { // 5
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<LockDataspaces_req_t*>(p_body.get()));
        auto* body = static_cast<LockDataspaces_req_t*>(p_body.get());
        LockDataspaces(
            ireq, std::move(*body),
            unwrapHandler<LockDataspaces_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CopyToDataspace_req_t::info.type_.type_: { // 7
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CopyToDataspace_req_t*>(p_body.get()));
        auto* body = static_cast<CopyToDataspace_req_t*>(p_body.get());
        CopyToDataspace(
            ireq, std::move(*body),
            unwrapHandler<CopyToDataspace_res_t>(std::move(wres_handler))
        );
        return;
    }

    case CopyDataspaceFromRemote_req_t::info.type_.type_: { // 9
        auto p_body = ireq.takeBody();
        assert(dynamic_cast<CopyDataspaceFromRemote_req_t*>(p_body.get()));
        auto* body = static_cast<CopyDataspaceFromRemote_req_t*>(p_body.get());
        CopyDataspaceFromRemote(
            ireq, std::move(*body),
            unwrapHandler<CopyDataspaceFromRemote_res_t>(std::move(wres_handler))
        );
        return;
    }

    } // switch

    throw std::runtime_error("Unknown message");
}

std::unique_ptr<IDataspaceOSDU> IDataspaceOSDU::makeClientImpl(
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    struct ClientImpl : IDataspaceOSDU, ClientProtocolImpl {
        using ClientProtocolImpl::ClientProtocolImpl;

        void GetDataspaceInfo(
            IRequest& ireq, GetDataspaceInfo_req_t req, // 2424.1
            EtpResponseHandler<GetDataspaceInfo_res_t> handler_cb // 2424.2
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CopyDataspacesContent(
            IRequest& ireq, CopyDataspacesContent_req_t req, // 2424.3
            EtpResponseHandler<CopyDataspacesContent_res_t> handler_cb // 2424.4
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void LockDataspaces(
            IRequest& ireq, LockDataspaces_req_t req, // 2424.5
            EtpResponseHandler<LockDataspaces_res_t> handler_cb // 2424.6
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CopyToDataspace(
            IRequest& ireq, CopyToDataspace_req_t req, // 2424.7
            EtpResponseHandler<CopyToDataspace_res_t> handler_cb // 2424.8
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }

        void CopyDataspaceFromRemote(
            IRequest& ireq, CopyDataspaceFromRemote_req_t req, // 2424.9
            EtpResponseHandler<CopyDataspaceFromRemote_res_t> handler_cb // 2424.10
        ) override final {
            send(ireq, std::move(req), std::move(handler_cb));
        }
    };
    return std::make_unique<ClientImpl>(std::move(void_sender), std::move(resp_sender));
}

//============================================================================

} //      oes::common::etp12::Protocol
