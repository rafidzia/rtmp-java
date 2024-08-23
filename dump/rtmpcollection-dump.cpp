#include <jni.h>
#include <map>
#include <librtmp/rtmp.h>
#include <librtmp/log.h>
#include <mutex>
#include <queue>
#include <fcntl.h>
#include <iostream>
#include "util.cpp"

class RTMPCollection
{
public:
    RTMPCollection() : counter(0) {}

    ~RTMPCollection()
    {
        for (auto &pair : collection)
        {
            RTMP_Close(pair.second);
            RTMP_Free(pair.second);
        }
    }

    int addRTMPConnection(const char *url)
    {
        std::lock_guard<std::mutex> lock(collectionMutex);
        RTMP *rtmp = RTMP_Alloc();
        RTMP_Init(rtmp);
        rtmp->Link.timeout = 5;
        if (!RTMP_SetupURL(rtmp, (char *)url))
        {
            RTMP_Log(RTMP_LOGERROR, "SetupURL Err\n");
            RTMP_Free(rtmp);
            return -1;
        }
        RTMP_EnableWrite(rtmp);
        if (!RTMP_Connect(rtmp, NULL))
        {
            RTMP_Log(RTMP_LOGERROR, "Connect Err\n");
            RTMP_Free(rtmp);
            return -1;
        }
        if (!RTMP_ConnectStream(rtmp, 0))
        {
            RTMP_Log(RTMP_LOGERROR, "ConnectStream Err\n");
            RTMP_Close(rtmp);
            RTMP_Free(rtmp);
            return -1;
        }
        int fd = RTMP_Socket(rtmp);
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        int32_t id = ++counter;
        collection.insert(std::make_pair(id, rtmp));

        RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket));
        RTMPPacket_Alloc(packet, 1024 * 64);
        RTMPPacket_Reset(packet);
        packet->m_hasAbsTimestamp = 0;
        packet->m_nChannel = 0x04;
        packet->m_nInfoField2 = rtmp->m_stream_id;
        packetBuffer.insert(std::make_pair(id, packet));

        std::queue<BufferHolder*> q;
        bufferQueue.insert(std::make_pair(id, q));
        return id;
    }

    RTMP *getRTMPConnection(int id)
    {
        std::lock_guard<std::mutex> lock(collectionMutex);
        if (collection.find(id) != collection.end())
        {
            return collection[id];
        }
        return NULL;
    }

    RTMPPacket *getRTMPPacket(int id)
    {
        std::lock_guard<std::mutex> lock(packetBufferMutex);
        if (packetBuffer.find(id) != packetBuffer.end())
        {
            return packetBuffer[id];
        }
        return NULL;
    }

    BufferHolder *dequeBuffer(int id)
    {
        std::lock_guard<std::mutex> lock(bufferQueueMutex);
        if (bufferQueue.find(id) != bufferQueue.end())
        {
            BufferHolder *holder = bufferQueue[id].front();
            bufferQueue[id].pop();
            return holder;
        }
        return NULL;
    }

    void enqueueBuffer(int id, BufferHolder *holder)
    {
        std::lock_guard<std::mutex> lock(bufferQueueMutex);
        if (bufferQueue.find(id) != bufferQueue.end())
        {
            bufferQueue[id].push(holder);
        }
    }

    bool isQueueEmpty(int id)
    {
        std::lock_guard<std::mutex> lock(bufferQueueMutex);
        if (bufferQueue.find(id) != bufferQueue.end())
        {
            return bufferQueue[id].empty();
        }
        return true;
    }

    int closeRTMPConnection(int id)
    {
        std::lock_guard<std::mutex> lock(collectionMutex);
        auto it = collection.find(id);
        int result = -1;
        if (it != collection.end())
        {
            RTMP_Close(it->second);
            RTMP_Free(it->second);
            collection.erase(it);
            result = 0;
        }
        auto it2 = packetBuffer.find(id);
        if (it2 != packetBuffer.end())
        {
            RTMPPacket_Free(it2->second);
            free(it2->second);
            packetBuffer.erase(it2);
        }
        auto it3 = bufferQueue.find(id);
        if (it3 != bufferQueue.end())
        {
            while (!it3->second.empty()) {
                BufferHolder *holder = it3->second.front();
                free(holder->data);
                delete holder;
                it3->second.pop();
            }
            bufferQueue.erase(it3);
        }
        return result;
    }

private:
    const char *url;
    std::map<int, RTMP *> collection;
    std::mutex collectionMutex;
    std::map<int, RTMPPacket *> packetBuffer;
    std::mutex packetBufferMutex;
    std::map<int, std::queue<BufferHolder*>> bufferQueue;
    std::mutex bufferQueueMutex;
    int32_t counter;
};
