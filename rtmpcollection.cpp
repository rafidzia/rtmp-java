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

        std::queue<char*> q;
        bufferQueue.insert(std::make_pair(id, q));

        RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket));
        RTMPPacket_Alloc(packet, 1024 * 512);
        RTMPPacket_Reset(packet);
        packetBuffer.insert(std::make_pair(id, packet));

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

    int closeRTMPConnection(int id)
    {
        std::lock_guard<std::mutex> lock1(collectionMutex);
        auto it = collection.find(id);
        int result = -1;
        if (it != collection.end())
        {
            RTMP_Close(it->second);
            RTMP_Free(it->second);
            collection.erase(it);
            result = 0;
        }
        std::lock_guard<std::mutex> lock2(bufferQueueMutex);
        auto it2 = bufferQueue.find(id);
        if (it2 != bufferQueue.end())
        {
            while (!it2->second.empty())
            {
                free(it2->second.front());
                it2->second.pop();
            }
            bufferQueue.erase(it2);
        }
        std::lock_guard<std::mutex> lock3(packetBufferMutex);
        auto it3 = packetBuffer.find(id);
        if (it3 != packetBuffer.end())
        {
            RTMPPacket_Free(it3->second);
            packetBuffer.erase(it3);
        }

        return result;
    }

    char *dequeBuffer(int id)
    {
        std::lock_guard<std::mutex> lock(bufferQueueMutex);
        if (bufferQueue.find(id) != bufferQueue.end())
        {
            char *buffer = bufferQueue[id].front();
            bufferQueue[id].pop();
            return buffer;
        }
        return NULL;
    }

    void enqueueBuffer(int id, char *buffer)
    {
        std::lock_guard<std::mutex> lock(bufferQueueMutex);
        if (bufferQueue.find(id) != bufferQueue.end())
        {
            bufferQueue[id].push(buffer);
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

private:
    const char *url;
    std::map<int, RTMP *> collection;
    std::mutex collectionMutex;
    std::map<int, std::queue<char*>> bufferQueue;
    std::mutex bufferQueueMutex;
    std::map<int, RTMPPacket *> packetBuffer;
    std::mutex packetBufferMutex;
    int32_t counter;
};
