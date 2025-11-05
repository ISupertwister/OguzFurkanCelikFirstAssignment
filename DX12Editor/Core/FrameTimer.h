#pragma once
#include <windows.h>
#include <iomanip>
#include <sstream>

// Simple frame timer for FPS and dt
class FrameTimer {
public:
    FrameTimer() {
        LARGE_INTEGER f; QueryPerformanceFrequency(&f);
        m_freq = double(f.QuadPart);
        QueryPerformanceCounter(&m_prev);
    }
    void Tick() {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        m_dt = double(now.QuadPart - m_prev.QuadPart) / m_freq;
        m_prev = now;

        m_accumTime += m_dt;
        m_accumFrames++;
    }
    double Delta() const { return m_dt; }

    // Sampling FPS every 'intervalSec'. Returns true when a new sample is ready.
    bool SampleFps(double intervalSec, double& outFps) {
        if (m_accumTime >= intervalSec) {
            outFps = (m_accumFrames / m_accumTime);
            m_accumTime = 0.0;
            m_accumFrames = 0;
            return true;
        }
        return false;
    }
private:
    LARGE_INTEGER m_prev{};
    double m_freq{ 1.0 };
    double m_dt{ 0.0 };
    double m_accumTime{ 0.0 };
    int    m_accumFrames{ 0 };
};


