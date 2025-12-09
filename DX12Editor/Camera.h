#pragma once
#include <DirectXMath.h>

// Camera sýnýfý tanýmý
class Camera {
public:
    Camera() noexcept;

    // Kamera matrislerini alma
    DirectX::XMMATRIX GetViewMatrix() const noexcept;
    DirectX::XMMATRIX GetProjectionMatrix() const noexcept;

    // Ayarlar ve Hareket
    void SetProjection(float fov, float aspect, float nearZ, float farZ);
    void Update(float dt);
    void Rotate(float dx, float dy);
    void SetMovement(bool forward, bool back, bool left, bool right, bool up, bool down, bool speedMultiplier);
    void Focus(DirectX::XMFLOAT3 targetPos, float distance);

private:
    void RecalculateVectors();

private:
    // Kamera Pozisyon ve Yön Bilgileri
    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_lookAt;
    DirectX::XMFLOAT3 m_up;

    // Euler Açýlarý
    float m_pitch;
    float m_yaw;

    // Projeksiyon Ayarlarý
    float m_fov;
    float m_aspect;
    float m_nearZ;
    float m_farZ;

    // Hareket Durumlarý (Tuþlara basýlý mý?)
    bool m_moveForward{ false };
    bool m_moveBackward{ false };
    bool m_moveLeft{ false };
    bool m_moveRight{ false };
    bool m_moveUp{ false };
    bool m_moveDown{ false };
    bool m_speedMultiplier{ false };
};