#ifndef MOTION_VIDEOPLAYBACKBASE_CPP
#define MOTION_VIDEOPLAYBACKBASE_CPP

#include <Motion/VideoPlaybackBase.hpp>

namespace mt
{
    VideoPlaybackBase::VideoPlaybackBase(DataSource& DataSource) :
        m_dataSource(&DataSource),
        m_initialSetupDone(false),
        m_protectionMutex(),
        m_queuedVideoPackets(),
        m_elapsedTime(),
        m_frameTime(sf::Time::Zero),
        m_frameJump(0),
        m_playedFrameCount(0)
    {
        sf::Lock lock(m_dataSource->m_playbackMutex);

        m_dataSource->m_videoPlaybacks.push_back(this);
    }

    VideoPlaybackBase::~VideoPlaybackBase()
    {
        if (m_dataSource && m_initialSetupDone)
        {
            sf::Lock lock(m_dataSource->m_playbackMutex);

            for (auto& videoplayback : m_dataSource->m_videoPlaybacks)
            {
                if (videoplayback == this)
                {
                    std::swap(videoplayback, m_dataSource->m_videoPlaybacks.back());
                    m_dataSource->m_videoPlaybacks.pop_back();

                    break;
                }
            }

            m_dataSource = nullptr;
        }
    }

    void VideoPlaybackBase::Update(sf::Time DeltaTime)
    {
        if (!m_initialSetupDone)
        {
            SourceReloaded();

            m_initialSetupDone = true;
        }

        if (m_dataSource && m_dataSource->HasVideo())
        {
            if (m_dataSource->GetState() == State::Playing)
            {
                m_elapsedTime += DeltaTime;
                int jumpcount = static_cast<int>(std::floor(m_elapsedTime.asMicroseconds() / m_frameTime.asMicroseconds()));
                m_elapsedTime -= m_frameTime * static_cast<float>(jumpcount);
                m_frameJump += jumpcount;
            }

            priv::VideoPacketPtr packet;

            {
                sf::Lock lock(m_protectionMutex);

                while (m_queuedVideoPackets.size() > 0)
                {
                    if (m_frameJump > 1 && m_queuedVideoPackets.size() > 1)
                    {
                        m_frameJump -= 1;
                        m_playedFrameCount++;

                        m_queuedVideoPackets.pop();
                    }
                    else if (m_frameJump == 1 || (m_frameJump > 1 && m_queuedVideoPackets.size() == 1))
                    {
                        m_frameJump -= 1;
                        m_playedFrameCount++;

                        packet = m_queuedVideoPackets.front();
                        m_queuedVideoPackets.pop();

                        break;
                    }
                    else
                        break;
                }
            }

            if (packet != nullptr)
                UpdateTexture(packet->GetRGBABuffer());
        }
    }

    void VideoPlaybackBase::SourceReloaded()
    {
        if (m_dataSource->HasVideo())
        {
            m_frameTime = m_dataSource->GetVideoFrameTime();

            CreateTexture(m_dataSource->GetVideoSize().x, m_dataSource->GetVideoSize().y);
            ClearTexture();
        }
    }

    void VideoPlaybackBase::StateChanged(State PreviousState, State NewState)
    {
        if (!m_initialSetupDone)
        {
            SourceReloaded();

            m_initialSetupDone = true;
        }

        if (NewState == State::Playing && PreviousState == State::Stopped)
        {
            m_frameJump = 1;
            m_playedFrameCount = 0;
        }
        else if (NewState == State::Stopped)
        {
            m_elapsedTime = sf::Time::Zero;
            m_frameJump = 0;
            m_playedFrameCount = 0;

            if (m_dataSource->HasVideo())
                ClearTexture();

            sf::Lock lock(m_protectionMutex);

            while (m_queuedVideoPackets.size() > 0)
            {
                m_queuedVideoPackets.pop();
            }
        }
    }

    unsigned int VideoPlaybackBase::GetPlayedFrameCount() const
    {
        return m_playedFrameCount;
    }

    DataSource* VideoPlaybackBase::GetDataSource()
    {
        return m_dataSource;
    }
}

#endif
