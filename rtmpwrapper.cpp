#include <jni.h>
#include <librtmp/rtmp.h>
#include <unistd.h>
#include <string.h>
#include <rtmpcollection.cpp>
#include <thread>
#include <iostream>

extern "C"
{
    RTMPCollection collection;

    int sending(RTMP *rtmp, RTMPPacket *packet, char *buffer)
    {
        RTMPPacket_Reset(packet);

        packet->m_hasAbsTimestamp = 0;
        packet->m_nChannel = 0x04;
        packet->m_nInfoField2 = rtmp->m_stream_id;
        packet->m_headerType = RTMP_PACKET_SIZE_LARGE;

        uint8_t type;
        memcpy(&type, buffer, 1);
        if (type == 0x08)
            packet->m_nChannel = 0x03;

        uint32_t datalength;
        memcpy(&datalength, buffer + 1, 3);
        datalength = HTON24(datalength);

        uint32_t timestamp;
        memcpy(&timestamp, buffer + 4, 4);
        timestamp = HTONTIME(timestamp);

        packet->m_packetType = type;
        packet->m_nBodySize = datalength;
        packet->m_nTimeStamp = timestamp;
        memcpy(packet->m_body, buffer + 11, datalength);

        free(buffer);

        int result = RTMP_SendPacket(rtmp, packet, 0);
        return result;
    }

    void run(int connection)
    {
        RTMP *rtmp = collection.getRTMPConnection(connection);
        if (rtmp == NULL || !RTMP_IsConnected(rtmp))
        {
            collection.closeRTMPConnection(connection);
            return;
        }

        RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket));
        RTMPPacket_Alloc(packet, 1024 * 512);

        int timeoutStep = 0;

        while (1)
        {
            if (collection.isQueueEmpty(connection))
            {
                timeoutStep++;
                if (timeoutStep > 500)
                {
                    RTMP_Log(RTMP_LOGERROR, "Timeouted. Exit Loop.\n");
                    collection.closeRTMPConnection(connection);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            timeoutStep = 0;
            char *buffer = collection.dequeBuffer(connection);
            if (buffer == NULL)
                continue;
            if (sending(rtmp, packet, buffer) <= 0)
            {
                RTMP_Log(RTMP_LOGERROR, "Packet Unsent. Exit Loop.\n");
                collection.closeRTMPConnection(connection);
                break;
            }
        }

        RTMPPacket_Free(packet);
    }

    JNIEXPORT jint JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_connect(JNIEnv *env, jobject obj, jstring url)
    {
        const char *urlstr = env->GetStringUTFChars(url, 0);
        jint connection = (jint)collection.addRTMPConnection(urlstr);
        env->ReleaseStringUTFChars(url, urlstr);
        std::thread t(run, connection);
        t.detach();
        return connection;
    }

    JNIEXPORT jint JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_close(JNIEnv *env, jobject obj, jint connection)
    {
        return (jint)collection.closeRTMPConnection(connection);
    }

    JNIEXPORT jboolean JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_isConnected(JNIEnv *env, jobject obj, jint connection)
    {
        RTMP *rtmp = collection.getRTMPConnection(connection);
        if (rtmp == NULL)
            return false;
        return true;
    }

    JNIEXPORT jint JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_send(JNIEnv *env, jobject obj, jint connection, jbyteArray byteArray)
    {
        RTMP *rtmp = collection.getRTMPConnection(connection);
        if (rtmp == NULL)
            return -1;

        jbyte *byteData = env->GetByteArrayElements(byteArray, NULL);

        uint8_t type;
        memcpy(&type, byteData, 1);
        if (type != 0x09 && type != 0x08)
        {
            env->ReleaseByteArrayElements(byteArray, byteData, 0);
            return 0;
        }

        uint32_t datalength;
        memcpy(&datalength, byteData + 1, 3);
        datalength = HTON24(datalength);

        int size = 11 + datalength + 4;

        char *buffer = (char *)malloc(size);
        memcpy(buffer, byteData, size);

        env->ReleaseByteArrayElements(byteArray, byteData, 0);

        collection.enqueueBuffer(connection, buffer);

        return size;
    }
}