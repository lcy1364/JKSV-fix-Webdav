#include "rfs.h"
#include <fstream>

std::vector<uint8_t> rfs::downloadBuffer;

void rfs::writeThread_t(void *a)
{
    rfs::dlWriteThreadStruct *in = (rfs::dlWriteThreadStruct *)a;
    std::vector<uint8_t> localBuff;
    unsigned written = 0;

    FILE *out = fopen(in->cfa->path.c_str(), "wb");

    while(written < in->cfa->size)
    {
        std::unique_lock<std::mutex> dataLock(in->dataLock);
        in->cond.wait(dataLock, [in]{ return in->bufferFull; });
        localBuff.clear();
        localBuff.assign(in->sharedBuffer.begin(), in->sharedBuffer.end());
        in->sharedBuffer.clear();
        in->bufferFull = false;
        dataLock.unlock();
        in->cond.notify_one();

        written += fwrite(localBuff.data(), 1, localBuff.size(), out);
    }
    fclose(out);
    rfs::downloadBuffer.clear();
}

size_t rfs::writeDataBufferThreaded(uint8_t *buff, size_t sz, size_t cnt, void *u)
{
    rfs::dlWriteThreadStruct *in = (rfs::dlWriteThreadStruct *)u;
    rfs::downloadBuffer.insert(rfs::downloadBuffer.end(), buff, buff + (sz * cnt));
    in->downloaded += sz * cnt;

    if(in->downloaded == in->cfa->size || rfs::downloadBuffer.size() == DOWNLOAD_BUFFER_SIZE)
    {
        std::unique_lock<std::mutex> dataLock(in->dataLock);
        in->cond.wait(dataLock, [in]{ return in->bufferFull == false; });
        in->sharedBuffer.assign(rfs::downloadBuffer.begin(), rfs::downloadBuffer.end());
        rfs::downloadBuffer.clear();
        in->bufferFull = true;
        dataLock.unlock();
        in->cond.notify_one();
    }

    if(in->cfa->o)
        *in->cfa->o = in->downloaded;

    return sz * cnt;
}

size_t rfs::writeDataToFile(void* ptr, size_t size, size_t nmemb, void* userdata) {
    std::ofstream* ofs = static_cast<std::ofstream*>(userdata);
    if (ofs->is_open()) {
        ofs->write(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    }
    return 0; // 返回0表示写入失败，curl 会中止下载
}