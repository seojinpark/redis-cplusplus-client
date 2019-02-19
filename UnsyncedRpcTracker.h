/* Copyright (c) 2017 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef RAMCLOUD_UNSYNCEDRPCTRACKER_H
#define RAMCLOUD_UNSYNCEDRPCTRACKER_H

#include <cinttypes>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <functional>

namespace RAMCloud {

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;             \
    TypeName& operator=(const TypeName&) = delete;

/**
 * A temporary storage for RPC requests that have been responded by master but
 * have not been made durable in backups.
 *
 * Each client should keep an instance of this class to keep the information
 * on which RPCs were processed by a master, so that should be retried in case
 * of crash of the master.
 *
 * TODO: more detailed explanation why retry such RPCs?
 */
class UnsyncedRpcTracker {
  public:
    explicit UnsyncedRpcTracker();
    ~UnsyncedRpcTracker();
    void registerUnsynced(int socket, int dbindex, const char* msg, int msgSize,
                          uint64_t opNumInServer, uint64_t syncedInServer);
    void updateSyncState(int socket, uint64_t syncedInServer);
    void flushSession(int socket, std::string hostIp, uint16_t replayPort);
    void pingMasterByTimeout();
    void sync();
    void sync(std::function<void()> callback);

  private:

    /**
     * Holds info about an RPC whose effect is not made durable yet, which is
     * necessary to retry the RPC when a master crashes and loose the effects.
     */
    struct UnsyncedRpc {

        /// Default constructor
        UnsyncedRpc(int dbindex, char* data, uint32_t size,
                    uint64_t opNum, std::function<void()> callback)
            : dbindex(dbindex), data(data), size(size), opNum(opNum),
              callback(callback) {}

        ///////////////////////////////////////
        /////// Info for retries
        ///////////////////////////////////////
        int dbindex;    // Redis dbindex for this command.

        /**
         * The pointer to the RPC request that was originally constructed by
         * this client. In case of master crash, a retry RPC with this request
         * will be sent to recovery master.
         * This request must be constructed by linearizable object RPC.
         */
        char* data;     // Pointer to RPC request.
        uint32_t size;  // RPC request size.

        ///////////////////////////////////////
        /////// Info for garbage collection
        ///////////////////////////////////////
        /**
         * Location of updated value of the object in master's log.
         * This information will be matched later with master's sync point,
         * so that we can safely discard RPC records as they become durable.
         */
        uint64_t opNum;

        /**
         * The callback to be invoked as the effects of this RPC becomes
         * permanently durable.
         */
        std::function<void()> callback;

      private:
        DISALLOW_COPY_AND_ASSIGN(UnsyncedRpc)
    };

    /**
     * Each instance of this class stores information about unsynced RPCs
     * sent to a master, which is identified by Transport::Session.
     */
    struct Master {
      public:
        /**
         * Constructor for Master
         *
         * \param session
         *      The boost_intrusive pointer to transport session
         */
        explicit Master()
            : lastestSyncNum(0)
            , rpcs()
        {}

        void updateSyncState(uint64_t syncNum);

        /**
         * Caches the most up-to-date information on the state of master's log.
         */
        uint64_t lastestSyncNum;

        /**
         * Queue keeping #UnsyncedRpc sent to this master.
         */
        std::queue<UnsyncedRpc> rpcs;

      private:
        DISALLOW_COPY_AND_ASSIGN(Master)
    };

    /// Helper methods
    Master* getOrInitMasterRecord(int socket);

    /**
     * Maps from #socket to target #Master.
     * Masters are dynamically allocated and must be freed explicitly.
     */
    typedef std::unordered_map<int, Master*> MasterMap;
    MasterMap masters;

    /**
     * Monitor-style lock. Any operation on internal data structure should
     * hold this lock.
     */
    std::mutex mutex;
    typedef std::lock_guard<std::mutex> Lock;

    uint64_t lastOpNum;

    DISALLOW_COPY_AND_ASSIGN(UnsyncedRpcTracker)
};

}  // namespace RAMCloud

#endif // RAMCLOUD_UNSYNCEDRPCTRACKER_H
