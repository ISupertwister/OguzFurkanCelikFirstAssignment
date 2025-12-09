#include "Camera.h"
#include <algorithm> 
#include <cmath>
#include <DirectXMath.h> 

using namespace DirectX;

// Constants
constexpr float kMovementSpeed = 5.0f;
constexpr float kFastSpeedMultiplier = 4.0f;
constexpr float kRotationSpeed = 0.005f;
constexpr float kMaxPitch = XM_PIDIV2 - 0.01f; // Gimbal lock önleme

Camera::Camera() noexcept
    : m_pitch(0.0f), m_yaw(0.0f),
    m_fov(XM_PIDIV4), m_aspect(1.777f), m_nearZ(0.1f), m_farZ(100.0f)
{
    // Baþlangýç pozisyonu
    m_position = { 0.0f, 0.0f, -3.0f };
    RecalculateVectors();
}

void Camera::SetProjection(float fov, float aspect, float nearZ, float farZ) {
    m_fov = fov;
    m_aspect = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

void Camera::RecalculateVectors() {
    // 1. Pitch ve Yaw'a göre yeni forward vektörünü hesapla
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    XMFLOAT3 forward = {
        cosPitch * sinYaw,
        sinPitch,
        cosPitch * cosYaw
    };

    // 2. LookAt hedefini ayarla: Position + Forward Vector
    m_lookAt.x = m_position.x + forward.x;
    m_lookAt.y = m_position.y + forward.y;
    m_lookAt.z = m_position.z + forward.z;

    // 3. Right ve Up vektörlerini hesapla
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR fwd = XMLoadFloat3(&forward);

    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(fwd, right));

    XMStoreFloat3(&m_up, up);
}

void Camera::Update(float dt) {
    float speed = kMovementSpeed * dt;
    if (m_speedMultiplier) {
        speed *= kFastSpeedMultiplier;
    }

    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAtVec = XMLoadFloat3(&m_lookAt);
    XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(lookAtVec, pos));
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));

    // Hareket güncellemeleri
    if (m_moveForward)  pos = XMVectorAdd(pos, XMVectorScale(fwd, speed));
    if (m_moveBackward) pos = XMVectorSubtract(pos, XMVectorScale(fwd, speed));
    if (m_moveRight)    pos = XMVectorAdd(pos, XMVectorScale(right, speed));
    if (m_moveLeft)     pos = XMVectorSubtract(pos, XMVectorScale(right, speed));
    if (m_moveUp)       pos = XMVectorAdd(pos, XMVectorScale(worldUp, speed));
    if (m_moveDown)     pos = XMVectorSubtract(pos, XMVectorScale(worldUp, speed));

    XMStoreFloat3(&m_position, pos);
    RecalculateVectors();
}

void Camera::Rotate(float dx, float dy) {
    m_yaw += dx * kRotationSpeed;
    m_pitch += dy * kRotationSpeed;

    // Manual clamp (std::clamp C++17 zorunluluðunu kaldýrýr)
    if (m_pitch > kMaxPitch) m_pitch = kMaxPitch;
    if (m_pitch < -kMaxPitch) m_pitch = -kMaxPitch;

    // Yaw'ý sarmala
    if (m_yaw > XM_2PI) m_yaw -= XM_2PI;
    if (m_yaw < -XM_2PI) m_yaw += XM_2PI;

    RecalculateVectors();
}

void Camera::SetMovement(bool forward, bool back, bool left, bool right, bool up, bool down, bool speedMultiplier) {
    m_moveForward = forward;
    m_moveBackward = back;
    m_moveLeft = left;
    m_moveRight = right;
    m_moveUp = up;
    m_moveDown = down;
    m_speedMultiplier = speedMultiplier;
}

void Camera::Focus(XMFLOAT3 targetPos, float distance) {
    XMVECTOR target = XMLoadFloat3(&targetPos);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAtVec = XMLoadFloat3(&m_lookAt);

    XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(lookAtVec, pos));
    XMVECTOR newPos = XMVectorSubtract(target, XMVectorScale(fwd, distance));

    XMStoreFloat3(&m_position, newPos);
    m_lookAt = targetPos;

    RecalculateVectors();
}

XMMATRIX Camera::GetViewMatrix() const noexcept {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAt = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    return XMMatrixLookAtLH(pos, lookAt, up);
}

XMMATRIX Camera::GetProjectionMatrix() const noexcept {
    return XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ);
}