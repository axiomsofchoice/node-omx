#include "AudioThread.h"

AudioThread::AudioThread(ILClient * pClient, ClockComponent * pClock, FFSource * src):
        client(pClient), clock(pClock)
{
    this->arc = new AudioRenderComponent(this->client);
    this->clockAudioTunnel = this->clock->tunnelTo(81, this->arc, 101, 0, 0);
    this->arc->setPCMMode(src->sampleRate, src->channels, AVSampleFormat::AV_SAMPLE_FMT_S16);
    this->arc->changeState(OMX_StateIdle);
    this->arc->enablePortBuffers(100, nullptr, nullptr, nullptr);
    this->arc->setAudioDest("hdmi");
}

void AudioThread::start()
{
    this->audioThread = std::thread(&AudioThread::audioThreadFunc, this);
}

void AudioThread::audioThreadFunc()
{
    this->arc->changeState(OMX_StateExecuting);
    while (!playbackComplete)
    {
        AudioBlock * block = dequeue();
        if (block != nullptr)
        {
            int format = block->audioFormat;
            int readBytes = 0;
            size_t pSize = block->dataSize;
            bool error = false;

            size_t sample_size = pSize / (block->streamCount * block->sampleCount);

            OMX_ERRORTYPE r;
            OMX_BUFFERHEADERTYPE *buff_header = NULL;
            int k, m, n;
            for (k = 0, n = 0; n < block->sampleCount; n++)
            {
                if (k == 0)
                {
                    buff_header = arc->getInputBuffer(100, 1 /* block */);
                }
                memcpy(&buff_header->pBuffer[k], &block->data[n * sample_size], sample_size);
                k += sample_size;
                if (k >= buff_header->nAllocLen)
                {
                    // this buffer is full
                    buff_header->nFilledLen = k;
                    r = arc->emptyBuffer(buff_header);
                    if (r != OMX_ErrorNone)
                    {
                        fprintf(stderr, "Empty buffer error\n");
                    }
                    k = 0;
                    buff_header = NULL;
                }
            }
            if (buff_header != NULL)
            {
                buff_header->nFilledLen = k;

                uint64_t timestamp = (uint64_t)(block->pts != DVD_NOPTS_VALUE ? block->pts : block->dts != DVD_NOPTS_VALUE ? block->dts : 0);

                if (block->looped)
                {
                    baseTime += lastTime;
                }
                lastTime = timestamp;
                timestamp += baseTime;


                buff_header->nTimeStamp = ToOMXTime(timestamp);


                r = arc->emptyBuffer(buff_header);
                if (r != OMX_ErrorNone)
                {
                    fprintf(stderr, "Empty buffer error %s\n");
                }
            }
            delete[] block->data;
            delete block;
        }
    }
    this->arc->changeState(OMX_StateIdle);
}

AudioBlock * AudioThread::dequeue()
{
    std::unique_lock<std::mutex> lk(audioQueueMutex);
    if (audioQueue.empty())
    {
        while (!playbackComplete && audioQueue.empty())
        {
            audioReady.wait_for(lk,  std::chrono::milliseconds(100), [&]()
            {
                return !audioQueue.empty();
            });
        }
    }
    if (playbackComplete || audioQueue.empty())
    {
        return nullptr;
    }
    else
    {
        AudioBlock * b = audioQueue.front();
        audioQueue.pop();
        if (audioQueue.size() < 200 && !bufferReady)
        {
            std::unique_lock<std::mutex> lk(readyForDataMutex);
            bufferReady = true;
            readyForData.notify_one();
        }
        return b;
    }
}
void AudioThread::waitForBuffer()
{
    std::unique_lock<std::mutex> lk(this->readyForDataMutex);
    if (bufferReady)
    {
        return;
    }
    while (!playbackComplete && !bufferReady)
    {
        readyForData.wait_for(lk,  std::chrono::milliseconds(100), [&]()
        {
            return bufferReady;
        });
    }
}
void AudioThread::addData(AudioBlock * ab)
{
    waitForBuffer();
    {
        std::unique_lock<std::mutex> lk(audioQueueMutex);
        audioQueue.push(ab);
        if (audioQueue.size() > 200)
        {
            bufferReady = false;
        }
        audioReady.notify_one();
    }
}
AudioThread::~AudioThread()
{
    this->playbackComplete = true;
    this->audioThread.join();

    if (this->clockAudioTunnel != nullptr)
    {
        delete this->clockAudioTunnel;
        this->clockAudioTunnel = nullptr;
    }
    if (this->arc != nullptr)
    {
        delete this->arc;
        this->arc = nullptr;
    }
}