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

#include <cassert>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Specific.hh>
#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Protocols.h"
#include "Messages.hpp"
#include "../AvroUtils.hpp"
#include "../AvroBinary.h"


namespace {

using oes::common::PassThruEncoder;
using oes::common::MultiPartCounts;
using namespace oes::common::etp12::Protocol;

template <typename T>
std::string keyOf(const T&) {
    return {};
}

template <typename V>
std::string keyOf(const std::pair<const std::string, V>& entry) {
    return fmt::format("<key>{}</key>)", entry.first);
}

template <typename Iter>
bool encode_binary_multipart_range(
    PassThruEncoder& e, MultiPartCounts& counts, Iter& it, const Iter& end
) {
    assert(!counts.done());

    counts.encodeSize(e);

    for (; it != end; ++it) {
        auto&& v = *it;

        try {
            counts.encodeItem(e, v);
        } catch (const avro::Exception& ex) {
            OES_RELEASE_UNUSED(ex);
            assert(boost::starts_with(ex.what(), "EOF"));

            if (counts.actual_ == 0) {
                // a single collection entry/value is oversize,
                // so there's nothing we can do generically here...
                ///*re*/throw;
                throw avro::Exception(fmt::format(
                    "Oversize #{}{} of {}: {}",
                    counts.index_, keyOf(v), counts.total_, ex.what()
                ));
            }

            assert(!counts.done());
            return false;
        }
    }

    assert(counts.done());
    return true;
}

template <typename C>
bool encode_binary_multipart_col(PassThruEncoder& e, const C& c) {
    if (c.empty()) {
        return true; // no split
    }

    auto it = std::begin(c);
    const auto end = std::end(c);
    auto& counts = e.split_counts_.emplace(c.size());
    const bool done = encode_binary_multipart_range(e, counts, it, end);
    assert(done == counts.done());
    if (done) {
        /*
        ** Keeping the counts after all, even when not split,
        ** allowing to know genericaly how many parts where sent.
        e.split_counts_.reset();
        */
        assert(!e.wasSplit());
        return true; // no split
    }

    assert(e.split_counts_.has_value());
    e.split_encoder_ = [it, end, &counts](PassThruEncoder& enc) mutable -> bool {
        counts.nextMessage();
        const bool done = encode_binary_multipart_range(enc, counts, it, end);
        assert(done == counts.done());
        /*
        ** The message is not truncated yet, so adding the trailing 0 byte
        ** here is incorrect, it needs to immediately follow the last complete item,
        ** which we'll do manually in MultiPartCounts::updateBody() instead.
        ** Which also avoids hacking StringAsAvroOStreamView to allow that
        ** 1-byte "write" after refusing allocating more space.
        enc.encodeLong(0); // FIXME: HACK: should be enc.arrayEnd() or enc.mapEnd();
        */
        return done;
    };

    return false; // split happened, on oversize message
}

template <typename T>
void encode_binary_multipart(PassThruEncoder& e, const std::vector<T>& b) {
    e.arrayStart(); // writes 0 byte (i.e. nothing)
    if (encode_binary_multipart_col(e, b)) {
        e.arrayEnd(); // writes 1 byte, only when no split happened
    }
}

template <typename V>
void encode_binary_multipart(PassThruEncoder& e, const std::map<std::string, V>& b) {
    e.mapStart(); // writes 0 byte (i.e. nothing)
    if (encode_binary_multipart_col(e, b)) {
        e.mapEnd(); // writes 1 byte, only when no split happened
    }
}

template <typename TResponse>
auto unwrapHandler(EtpWrappedResponseHandler res_handler) {
    return [res_handler = std::move(res_handler)](EtpResponseStatus rc, TResponse res) {
        res_handler(std::move(rc), [res = std::move(res)](avro::Encoder& enc) mutable {
            // TODO: Change code-gen, to allow eliminating MultiPart BUT multi-field types,
            //       which thus lack the range() method, in a constexpr manner, to avoid tests below.
            // Note: These *ARE* MultiPart, so they must be tested *BEFORE* if constexpr (.isMultiPart())
            if constexpr (std::is_same_v<TResponse, ChannelDataFrame::GetFrameMetadataResponse>) {
                // multi-field, so no range() method, thus fallback
            } else if constexpr (std::is_same_v<TResponse, ChannelDataFrame::GetFrameResponseHeader>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, GrowingObject::GetPartsResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, GrowingObject::GetPartsByRangeResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, DiscoveryQuery::FindResourcesResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, StoreQuery::FindDataObjectsResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, GrowingObjectQuery::FindPartsResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, SupportedTypes::GetSupportedTypesResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, ChannelSubscribe::SubscriptionsStopped>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, ChannelDataLoad::ChannelsClosed>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, StoreOSDU::CopyDataObjectsByValueResponse>) {
                // multi-field, so no range() method
            } else if constexpr (std::is_same_v<TResponse, DataspaceOSDU::CopyDataspacesContentResponse>) {
                // multi-field, so no range() method
            } else if constexpr (res.info.isMultiPart()) {
                auto* my_enc = dynamic_cast<PassThruEncoder*>(&enc);
                if (my_enc && my_enc->binary_) {
                    // multi-part specific encoding
                    return encode_binary_multipart(*my_enc, res.range());
                }
            }
            avro::encode(enc, res); // fall-back on "regular" encoding
        });
    };
}

EtpCompleteHandler unwrapHandlerVoid(EtpWrappedResponseHandler res_handler) {
    return [res_handler = std::move(res_handler)](EtpResponseStatus rc) {
        res_handler(std::move(rc), nullptr);
    };
}

//============================================================================

struct ClientProtocolImpl {
    ClientProtocolImpl( // must be public, since inherited
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    ) : void_sender_(std::move(void_sender))
      , resp_sender_(std::move(resp_sender))
    {
    }

    static IProtocol_uptr make(
        oes::common::etp12::Datatypes::Protocol proto_id,
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    template <typename REQ>
    void send(IRequest& ireq, REQ req, EtpCompleteHandler done_cb) {
        assert(typeOf(ireq) == REQ::info.type_);

        void_sender_(
            ireq,
            [msg = std::move(req)](::avro::Encoder& enc) {
                msg.encode(enc);
            },
            std::move(done_cb)
        );
    }

    template <typename REQ, typename RES>
    void send(IRequest& ireq, REQ req, EtpResponseHandler<RES> process_cb) {
        assert(typeOf(ireq) == REQ::info.type_);

        resp_sender_(
            ireq,
            [msg = std::move(req)](::avro::Encoder& enc) {
                msg.encode(enc);
            },
            [pcb = std::move(process_cb)](EtpResponseStatus rc, ::avro::Decoder& dec) {
                if (rc.ok) {
                    RES res;
                    res.decode(dec);
                    pcb(std::move(rc), std::move(res));
                } else {
                    pcb(std::move(rc), RES{});
                }
            }
        );
    }

private:
    WrappedMessageSenderVoid void_sender_;
    WrappedMessageSenderResp resp_sender_;
};

//============================================================================

} // namespace

namespace oes::common::etp12::Protocol {

IProtocol::IProtocol(
    Datatypes::Protocol id, const char* name
) : id_(id)
  , name_(name)
  , name_id_(fmt::format("{}({})", name, int(id)))
{
}

IProtocol_uptr IProtocol::makeClientImpl(
    Datatypes::Protocol proto_id,
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    return ClientProtocolImpl::make(
        proto_id, std::move(void_sender), std::move(resp_sender)
    );
}

} // namespace oes::common::etp12::Protocol

//============================================================================

#include "codegen/Protocols.i.ipp"

namespace {

using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

IProtocol_uptr ClientProtocolImpl::make(
    oes::common::etp12::Datatypes::Protocol proto_id,
    WrappedMessageSenderVoid void_sender,
    WrappedMessageSenderResp resp_sender
) {
    using std::move;

    switch (proto_id) {
    case Protocol::eCore:
        return ICore::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eChannelStreaming:
        return IChannelStreaming::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eChannelDataFrame:
        return IChannelDataFrame::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eDiscovery:
        return IDiscovery::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eStore:
        return IStore::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eStoreNotification:
        return IStoreNotification::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eGrowingObject:
        return IGrowingObject::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eGrowingObjectNotification:
        return IGrowingObjectNotification::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eDEPRECATED_8:
        break;
    case Protocol::eDataArray:
        return IDataArray::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eRESERVED_10:
        break;
    case Protocol::eRESERVED_11:
        break;
    case Protocol::eRESERVED_12:
        break;
    case Protocol::eDiscoveryQuery:
        return IDiscoveryQuery::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eStoreQuery:
        return IStoreQuery::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eRESERVED_15:
        break;
    case Protocol::eGrowingObjectQuery:
        return IGrowingObjectQuery::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eRESERVED_17:
        break;
    case Protocol::eTransaction:
        return ITransaction::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eRESERVED_19:
        break;
    case Protocol::eRESERVED_20:
        break;
    case Protocol::eChannelSubscribe:
        return IChannelSubscribe::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eChannelDataLoad:
        return IChannelDataLoad::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eRESERVED_23:
        break;
    case Protocol::eDataspace:
        return IDataspace::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eSupportedTypes:
        return ISupportedTypes::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eStoreOSDU:
        return IStoreOSDU::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eDataspaceOSDU:
        return IDataspaceOSDU::makeClientImpl(move(void_sender), move(resp_sender));
    case Protocol::eCoreOSDU:
        return ICoreOSDU::makeClientImpl(move(void_sender), move(resp_sender));
    // no default on purpose
    }

    return nullptr;
}

} // namespace
