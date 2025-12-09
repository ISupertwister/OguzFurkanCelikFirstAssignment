#include "Camera.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

// Constants
constexpr float kMovementSpeed = 5.0f;
constexpr float kFastSpeedMultiplier = 4.0f;
constexpr float kRotationSpeed = 0.005f;
constexpr float kMaxPitch = XM_PIDIV2 - 0.01f; // Prevent gimbal lock

constexpr float kZoomStep = 1.0f;   // Base zoom step per wheel "tick"
constexpr float kMinOrbitDistance = 0.5f;
constexpr float kMaxOrbitDistance = 100.0f;

Camera::Camera() noexcept
    : m_pitch(0.0f)
    , m_yaw(0.0f)
    , m_fov(XM_PIDIV4)
    , m_aspect(1.777f)
    , m_nearZ(0.1f)
    , m_farZ(100.0f)
{
    // Initial position
    m_position = { 0.0f, 0.0f, -3.0f };
    m_lookAt = { 0.0f, 0.0f,  0.0f };
    m_up = { 0.0f, 1.0f,  0.0f };

    // Default orbit settings
    m_orbitTarget = { 0.0f, 0.0f, 0.0f };
    m_orbitDistance = 3.0f;

    RecalculateVectors();
}

void Camera::SetProjection(float fov, float aspect, float nearZ, float farZ) {
    m_fov = fov;
    m_aspect = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

void Camera::RecalculateVectors() {
    // Compute forward from pitch/yaw
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    XMFLOAT3 forward = {
        cosPitch * sinYaw,
        sinPitch,
        cosPitch * cosYaw
    };

    if (m_isOrbitMode) {
        // In orbit mode, camera position is derived from pivot + distance
        m_lookAt = m_orbitTarget;

        m_position.x = m_orbitTarget.x - forward.x * m_orbitDistance;
        m_position.y = m_orbitTarget.y - forward.y * m_orbitDistance;
        m_position.z = m_orbitTarget.z - forward.z * m_orbitDistance;
    }
    else {
        // In free-fly mode, look-at is derived from position + forward
        m_lookAt.x = m_position.x + forward.x;
        m_lookAt.y = m_position.y + forward.y;
        m_lookAt.z = m_position.z + forward.z;
    }

    // Compute up vector from forward and world up
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

    // In orbit mode we ignore WASD-style movement and only update from orbit
    if (m_isOrbitMode) {
        RecalculateVectors();
        return;
    }

    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAtVec = XMLoadFloat3(&m_lookAt);
    XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(lookAtVec, pos));
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));

    // Movement updates
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

    // Clamp pitch
    if (m_pitch > kMaxPitch)  m_pitch = kMaxPitch;
    if (m_pitch < -kMaxPitch) m_pitch = -kMaxPitch;

    // Wrap yaw
    if (m_yaw > XM_2PI)  m_yaw -= XM_2PI;
    if (m_yaw < -XM_2PI) m_yaw += XM_2PI;

    RecalculateVectors();
}

void Camera::SetMovement(bool forward, bool back, bool left, bool right,
    bool up, bool down, bool speedMultiplier) {
    m_moveForward = forward;
    m_moveBackward = back;
    m_moveLeft = left;
    m_moveRight = right;
    m_moveUp = up;
    m_moveDown = down;
    m_speedMultiplier = speedMultiplier;
}

void Camera::Focus(XMFLOAT3 targetPos, float distance) {
    // Enable orbit mode around the given target
    m_isOrbitMode = true;
    m_orbitTarget = targetPos;
    m_orbitDistance = std::clamp(distance, kMinOrbitDistance, kMaxOrbitDistance);

    // Compute yaw/pitch from vector target -> camera
    XMVECTOR target = XMLoadFloat3(&m_orbitTarget);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(target, pos));

    XMFLOAT3 d;
    XMStoreFloat3(&d, dir);

    // Derive yaw and pitch from direction
    m_pitch = asinf(d.y);
    m_yaw = atan2f(d.x, d.z);

    RecalculateVectors();
}

void Camera::Zoom(float amount) {
    if (amount == 0.0f)
        return;

    float zoomStep = kZoomStep * amount;

    if (m_isOrbitMode) {
        // In orbit mode we change the orbit distance
        m_orbitDistance = std::clamp(m_orbitDistance - zoomStep, kMinOrbitDistance, kMaxOrbitDistance);
        RecalculateVectors();
    }
    else {
        // In free-fly mode we move along the forward direction
        XMVECTOR pos = XMLoadFloat3(&m_position);
        XMVECTOR lookAtVec = XMLoadFloat3(&m_lookAt);
        XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(lookAtVec, pos));

        pos = XMVectorAdd(pos, XMVectorScale(fwd, zoomStep));
        XMStoreFloat3(&m_position, pos);

        RecalculateVectors();
    }
}

void Camera::SetOrbitMode(bool enabled, XMFLOAT3 target) {
    m_isOrbitMode = enabled;

    if (enabled) {
        m_orbitTarget = target;

        // Compute initial orbit distance from current position
        XMVECTOR pos = XMLoadFloat3(&m_position);
        XMVECTOR pivot = XMLoadFloat3(&m_orbitTarget);
        XMVECTOR diff = XMVectorSubtract(pivot, pos);
        float dist = XMVectorGetX(XMVector3Length(diff));
        m_orbitDistance = std::clamp(dist, kMinOrbitDistance, kMaxOrbitDistance);
    }

    RecalculateVectors();
}

XMMATRIX Camera::GetViewMatrix() const noexcept {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAt = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMLoadFloat3(&m_up);

    return XMMatrixLookAtLH(pos, lookAt, up);
}

XMMATRIX Camera::GetProjectionMatrix() const noexcept {
    return XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ);
}
