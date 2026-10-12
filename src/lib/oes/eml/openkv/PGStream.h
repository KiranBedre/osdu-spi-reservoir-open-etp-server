#ifndef OES_EML_OPENKV_PGSTREAM_H
#define OES_EML_OPENKV_PGSTREAM_H

#include "../fwd.h"
#include "AdminDB.h"
#include <oes/common/etp12/Messages.h>

#include <string>
#include <memory>
#include <sstream>

namespace oes::eml::openkv {

using oes::common::etp12::Datatypes::Object::TableData;

/**
 * @brief SpaceStreamStatus support enum class to manage the whole stream process status
 * 
 */
enum class SpaceStreamStatus : uint8_t {
    eOngoing = 1,
    eDone,
    eFailed = 0
};

/**
 * @brief SpaceTableStreamStatus support enum class to manage the tables binary data stream individually 
 * 
 */
enum class SpaceTableStreamStatus : uint8_t {
    eStart = 1,       // 0000 0001
    eDone = 2,        // 0000 0010
    eOngoing = 4,     // 0000 0100
    eChunk = 8,       // 0000 1000
    eFinalChunk = 16, // 0001 0000
    eFailed = 0       // 0000 0000
};

/**
 * @brief Check if the given flags includes a given flag
 * 
 * @param raw_flags uint8_t flags
 * @param flag the flag you want to check
 * @return true raw_flags includes the given flag
 * @return false raw_flags does not include the given flag
 */
bool hasTableStreamStatus(const uint8_t raw_flags, const SpaceTableStreamStatus flag);

/**
 * @brief Insert SpaceTableStreamStatus::eChunk flag in flags
 * 
 * @param flags 
 */
void insertChunkFlag(uint8_t& flags);

/**
 * @brief Insert SpaceTableStreamStatus::eFinalChunk flag in flags
 * 
 * @param flags 
 */
void insertFinalChunkFlag(uint8_t& flags);


/**
 * @brief PGStream used as base class to manage tables data stream
 * 
 */
class PGStream {
protected:
    PGStream(const Space& space, Connection_ptr&& conn);
    virtual ~PGStream();

    PGStream() = delete;
    PGStream(PGStream&) = delete;
    PGStream& operator=(PGStream&) = delete;

    const Space& m_space_;
    Connection_ptr m_conn_;
};

/**
 * @brief PGStreamer producer class to manage the table data streaming
 * 
 */
class PGStreamer final: protected PGStream {
public:
    PGStreamer(const Space& space, Connection_ptr conn, const bool has_ref, const size_t max_msg_size);
    ~PGStreamer();

    PGStreamer() = delete;
    PGStreamer(PGStreamer&) = delete;
    PGStreamer& operator=(PGStreamer&) = delete;

    SpaceStreamStatus stream(TableData& td);

private:
    class Impl_;
    std::unique_ptr<Impl_> impl_;
};

/**
 * @brief PGConsumer consumer class to ingest binary table data 
 * 
 */
class PGConsumer final: protected PGStream {
public:
    PGConsumer(const Space& space, Connection_ptr conn);
    ~PGConsumer();

    PGConsumer() = delete;
    PGConsumer(PGConsumer&) = delete;
    PGConsumer& operator=(PGConsumer&) = delete;

    SpaceStreamStatus consume(TableData& td);

private:
    class Impl_;
    std::unique_ptr<Impl_> impl_;
};

} // namespace oes::eml::openkv {

#endif // OES_EML_OPENKV_PGSTREAM_H