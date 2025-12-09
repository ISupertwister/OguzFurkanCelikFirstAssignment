#pragma once
#include <DirectXMath.h>

// Camera class definition
class Camera {
public:
    Camera() noexcept;

    // Matrices
    DirectX::XMMATRIX GetViewMatrix() const noexcept;
    DirectX::XMMATRIX GetProjectionMatrix() const noexcept;

    // Settings & movement
    void SetProjection(float fov, float aspect, float nearZ, float farZ);
    void Update(float dt);
    void Rotate(float dx, float dy);
    void SetMovement(bool forward, bool back, bool left, bool right,
        bool up, bool down, bool speedMultiplier);

    // Focus camera on a target at given distance (also prepares orbit pivot)
    void Focus(DirectX::XMFLOAT3 targetPos, float distance);

    // Zoom along view direction (mouse wheel)
    void Zoom(float amount);

    // Orbit mode: when enabled, camera rotates around a pivot instead of moving freely
    void SetOrbitMode(bool enabled, DirectX::XMFLOAT3 target = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
    bool IsOrbitMode() const noexcept { return m_isOrbitMode; }

    // Optional helpers
    DirectX::XMFLOAT3 GetPosition() const noexcept { return m_position; }
    DirectX::XMFLOAT3 GetLookAt() const noexcept { return m_lookAt; }

private:
    void RecalculateVectors();

private:
    // Transform
    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_lookAt;
    DirectX::XMFLOAT3 m_up;

    // Euler angles
    float m_pitch;
    float m_yaw;

    // Projection
    float m_fov;
    float m_aspect;
    float m_nearZ;
    float m_farZ;

    // Movement flags
    bool m_moveForward{ false };
    bool m_moveBackward{ false };
    bool m_moveLeft{ false };
    bool m_moveRight{ false };
    bool m_moveUp{ false };
    bool m_moveDown{ false };
    bool m_speedMultiplier{ false };

    // Orbit settings
    bool m_isOrbitMode{ false };
    DirectX::XMFLOAT3 m_orbitTarget{ 0.0f, 0.0f, 0.0f };
    float m_orbitDistance{ 5.0f };
};
