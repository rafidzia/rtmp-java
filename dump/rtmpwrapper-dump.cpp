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

    // int sending(int connection, BufferHolder *holder)
    // {
    //     RTMP *rtmp = collection.getRTMPConnection(connection);
    //     if (rtmp == NULL || !RTMP_IsConnected(rtmp))
    //     {
    //         free(holder->data);
    //         delete holder;
    //         return -1;
    //     }

    //     RTMPPacket *packet = collection.getRTMPPacket(connection);
    //     packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    //     packet->m_nTimeStamp = holder->timestamp;
    //     packet->m_packetType = holder->type;
    //     packet->m_nBodySize = holder->datalength;
    //     memcpy(packet->m_body, holder->data + 11, holder->datalength);

    //     if (!RTMP_SendPacket(rtmp, packet, 0))
    //     {
    //         RTMP_Log(RTMP_LOGERROR, "Failed to send packet.\n");
    //         free(holder->data);
    //         delete holder;
    //         return -1;
    //     }
    //     int size = 11 + holder->datalength + 4;
    //     free(holder->data);
    //     delete holder;
    //     return size;
    // }

    // void run(int connection)
    // {
    //     RTMP *rtmp = collection.getRTMPConnection(connection);
    //     if (rtmp == NULL || !RTMP_IsConnected(rtmp))
    //     {
    //         collection.closeRTMPConnection(connection);
    //         return;
    //     }

    //     int timeoutStep = 0;

    //     while (1)
    //     {
    //         if (collection.isQueueEmpty(connection))
    //         {
    //             std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //             timeoutStep++;
    //             if (timeoutStep > 50)
    //             {
    //                 collection.closeRTMPConnection(connection);
    //                 break;
    //             }
    //             continue;
    //         }
    //         timeoutStep = 0;
    //         BufferHolder *holder = collection.dequeBuffer(connection);
    //         if (!sending(connection, holder))
    //         {
    //             collection.closeRTMPConnection(connection);
    //             break;
    //         }
    //     }
    // }

    JNIEXPORT jint JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_connect(JNIEnv *env, jobject obj, jstring url)
    {
        const char *urlstr = env->GetStringUTFChars(url, 0);
        jint connection = (jint)collection.addRTMPConnection(urlstr);
        env->ReleaseStringUTFChars(url, urlstr);
        return connection;
    }

    JNIEXPORT jint JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_close(JNIEnv *env, jobject obj, jint connection)
    {
        return (jint)collection.closeRTMPConnection(connection);
    }

    JNIEXPORT jboolean JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_isConnected(JNIEnv *env, jobject obj, jint connection)
    {
        RTMP *rtmp = collection.getRTMPConnection(connection);
        if (rtmp == NULL || !RTMP_IsConnected(rtmp))
        {
            return false;
        }
        return true;
    }

    // JNIEXPORT void JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_start(JNIEnv *env, jobject obj, jint connection, jbyteArray byteArray)
    // {
    //     std::thread t(run, connection);
    //     t.detach();
    // }

    // BufferHolder *processBuffer(JNIEnv *env, jbyteArray byteArray) {
    //     jbyte *byteData = env->GetByteArrayElements(byteArray, NULL);
    //     BufferHolder *holder = new BufferHolder();

    //     memcpy(&holder->type, byteData, 1);
    //     if (holder->type != 0x09 && holder->type != 0x08)
    //     {
    //         env->ReleaseByteArrayElements(byteArray, byteData, 0);
    //         return NULL;
    //     }

    //     memcpy(&holder->datalength, byteData + 1, 3);
    //     holder->datalength = HTON24(holder->datalength);

    //     memcpy(&holder->timestamp, byteData + 4, 4);
    //     holder->timestamp = HTONTIME(holder->timestamp);

    //     holder->data = (char *)malloc(11 + holder->datalength + 4);
    //     memcpy(holder->data, byteData, 11 + holder->datalength + 4);

    //     env->ReleaseByteArrayElements(byteArray, byteData, 0);

    //     return holder;
    // }

    JNIEXPORT jint JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_send(JNIEnv *env, jobject obj, jint connection, jbyteArray byteArray)
    {
        RTMP *rtmp = collection.getRTMPConnection(connection);
        if (rtmp == NULL || !RTMP_IsConnected(rtmp))
        {
            return -1;
        }

        jbyte *byteData = env->GetByteArrayElements(byteArray, NULL);

        uint8_t type;
        memcpy(&type, byteData, 1);
        if (type != 0x09 && type != 0x08)
        {
            env->ReleaseByteArrayElements(byteArray, byteData, 0);
            return NULL;
        }

        uint32_t datalength;
        memcpy(&datalength, byteData + 1, 3);
        datalength = HTON24(datalength);
    
        uint8_t timestamp;
        memcpy(&timestamp, byteData + 4, 4);
        timestamp = HTONTIME(timestamp);

        RTMPPacket *packet = collection.getRTMPPacket(connection);
        packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet->m_packetType = type;
        packet->m_nBodySize = datalength;
        packet->m_nTimeStamp = timestamp;

        memcpy(packet->m_body, byteData + 11, datalength);

        env->ReleaseByteArrayElements(byteArray, byteData, 0);

        if (!RTMP_SendPacket(rtmp, packet, 0))
        {
            RTMP_Log(RTMP_LOGERROR, "Failed to send packet.\n");
            return -1;
        }
        return 11 + datalength + 4;
    }
    // JNIEXPORT void JNICALL Java_com_mceasy_jt1078_util_RTMPWrapper_enqueue(JNIEnv *env, jobject obj, jint connection, jbyteArray byteArray)
    // {
    //     BufferHolder *holder = processBuffer(env, byteArray);
    //     if (holder == NULL)
    //         return;
    //     collection.enqueueBuffer(connection, holder);
    // }
}