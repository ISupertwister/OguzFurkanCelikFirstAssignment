#pragma once
#include <windows.h>

class FrameTimer {
public:
    FrameTimer() noexcept {
        LARGE_INTEGER freq{};
        QueryPerformanceFrequency(&freq);
        m_freq = double(freq.QuadPart);
        Reset();
    }

    // Call once per frame to update delta time
    void Tick() noexcept {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const double t = double(now.QuadPart) / m_freq;
        m_dt = t - m_prev;
        m_prev = t;
        m_accum += m_dt;
        m_frames++;
    }

    // Seconds between last two ticks
    double DeltaSeconds() const noexcept { return m_dt; }

    // Returns true every 'intervalSec' seconds and outputs FPS in 'outFps'
    bool SampleFps(double intervalSec, double& outFps) noexcept {
        if (m_accum >= intervalSec) {
            outFps = double(m_frames) / m_accum;
            m_accum = 0.0;
            m_frames = 0;
            return true;
        }
        return false;
    }

    void Reset() noexcept {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        m_prev = double(now.QuadPart) / m_freq;
        m_dt = 0.0;
        m_accum = 0.0;
        m_frames = 0;
    }

private:
    double m_freq{ 1.0 };
    double m_prev{ 0.0 };
    double m_dt{ 0.0 };
    double m_accum{ 0.0 };
    unsigned m_frames{ 0 };
};

