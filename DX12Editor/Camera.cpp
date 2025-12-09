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

constexpr float kZoomStep = 1.0f;
constexpr float kMinOrbitDistance = 1.0f; // Increased slightly for better stability
constexpr float kMaxOrbitDistance = 100.0f;

Camera::Camera() noexcept :
    m_pitch(-0.30f),     // look slightly down at the origin
    m_yaw(DirectX::XM_PI),
    m_fov(XM_PIDIV4),    // 45-degree FOV
    m_aspect(1.777f),
    m_nearZ(0.1f),
    m_farZ(1000.0f)
{
    // CStart a bit above and behind the origin.
    m_position = { 0.0f, 3.0f, 8.0f };
    m_lookAt = { 0.0f, 0.0f,  0.0f };
    m_orbitTarget = m_lookAt;
    m_orbitDistance = 8.5f;

    RecalculateVectors();
}



void Camera::SetProjection(float fov, float aspect, float nearZ, float farZ) {
    m_fov = fov;
    m_aspect = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

void Camera::RecalculateVectors() {
    // 1. Calculate forward vector based on Euler angles
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    // Standard FPS forward vector calculation
    XMFLOAT3 forward = {
        cosPitch * sinYaw,
        sinPitch,
        cosPitch * cosYaw
    };

    // 2. Update Position and LookAt based on mode
    if (m_isOrbitMode) {
        // Orbit Mode: Camera position rotates around the target
        // Calculate new position based on the "forward" vector (inverted) and distance
        m_lookAt = m_orbitTarget;

        // In orbit, we are 'distance' units away from target in the opposite direction of 'forward'
        // Note: We negate forward components to look AT the target
        m_position.x = m_orbitTarget.x - (forward.x * m_orbitDistance);
        m_position.y = m_orbitTarget.y - (forward.y * m_orbitDistance);
        m_position.z = m_orbitTarget.z - (forward.z * m_orbitDistance);
    }
    else {
        // Free-Fly Mode: LookAt is derived from Position + Forward direction
        m_lookAt.x = m_position.x + forward.x;
        m_lookAt.y = m_position.y + forward.y;
        m_lookAt.z = m_position.z + forward.z;
    }

    // 3. Calculate Up and Right vectors
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR fwdVec = XMLoadFloat3(&forward);

    // Create view matrix basis vectors
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(worldUp, fwdVec));

    // Re-calculate local up to ensure orthogonality
    // Note: In standard FPS cameras, we often just use (0,1,0) for view matrix, 
    // but calculating the real up vector allows for banking/rolling if needed later.
    XMVECTOR upVec = XMVector3Normalize(XMVector3Cross(fwdVec, rightVec));

    XMStoreFloat3(&m_up, upVec);
}

void Camera::Update(float dt) {
    // If in Orbit Mode, we don't handle WASD movement, only rotation updates via RecalculateVectors
    if (m_isOrbitMode) {
        RecalculateVectors();
        return;
    }

    // Free-fly movement logic
    float speed = kMovementSpeed * dt;
    if (m_speedMultiplier) {
        speed *= kFastSpeedMultiplier;
    }

    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAtVec = XMLoadFloat3(&m_lookAt);

    // Get direction vectors
    XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(lookAtVec, pos));
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));

    // Apply movement
    if (m_moveForward)  pos = XMVectorAdd(pos, XMVectorScale(fwd, speed));
    if (m_moveBackward) pos = XMVectorSubtract(pos, XMVectorScale(fwd, speed));
    if (m_moveRight)    pos = XMVectorAdd(pos, XMVectorScale(right, speed));
    if (m_moveLeft)     pos = XMVectorSubtract(pos, XMVectorScale(right, speed));
    if (m_moveUp)       pos = XMVectorAdd(pos, XMVectorScale(worldUp, speed));
    if (m_moveDown)     pos = XMVectorSubtract(pos, XMVectorScale(worldUp, speed));

    XMStoreFloat3(&m_position, pos);

    // Recalculate LookAt based on new position
    RecalculateVectors();
}

void Camera::Rotate(float dx, float dy) {
    m_yaw -= dx * kRotationSpeed;
    m_pitch += dy * kRotationSpeed;

    // Clamp pitch to avoid flipping upside down    
    if (m_pitch > kMaxPitch)  m_pitch = kMaxPitch;
    if (m_pitch < -kMaxPitch) m_pitch = -kMaxPitch;

    // Keep yaw within reasonable bounds (0-360)
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
    m_isOrbitMode = true;
    m_orbitTarget = targetPos;
    m_orbitDistance = std::clamp(distance, kMinOrbitDistance, kMaxOrbitDistance);

    // Calculate the direction vector from Target to current Position
    // This preserves the current viewing angle relative to the new target
    XMVECTOR target = XMLoadFloat3(&m_orbitTarget);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR dirToCamera = XMVector3Normalize(XMVectorSubtract(pos, target));

    XMFLOAT3 d;
    XMStoreFloat3(&d, dirToCamera);

    // Re-calculate Pitch and Yaw based on the direction vector
    m_pitch = asinf(d.y);           // Pitch from Y component
    m_yaw = atan2f(d.x, d.z);       // Yaw from X and Z components

    RecalculateVectors();
}

void Camera::Zoom(float amount) {
    if (amount == 0.0f) return;

    float zoomStep = kZoomStep * amount;

    if (m_isOrbitMode) {
        m_orbitDistance = std::clamp(m_orbitDistance - zoomStep, kMinOrbitDistance, kMaxOrbitDistance);
    }
    else {
        // In free-fly, zoom moves the camera forward/backward
        XMVECTOR pos = XMLoadFloat3(&m_position);
        XMVECTOR lookAtVec = XMLoadFloat3(&m_lookAt);
        XMVECTOR fwd = XMVector3Normalize(XMVectorSubtract(lookAtVec, pos));
        pos = XMVectorAdd(pos, XMVectorScale(fwd, zoomStep));
        XMStoreFloat3(&m_position, pos);
    }
    RecalculateVectors();
}

void Camera::SetOrbitMode(bool enabled, XMFLOAT3 target) {
    // Only update state if changing modes to prevent snapping
    if (m_isOrbitMode != enabled) {
        m_isOrbitMode = enabled;
        if (enabled) {
            m_orbitTarget = target;
            // Calculate current distance to maintain smooth transition
            XMVECTOR pos = XMLoadFloat3(&m_position);
            XMVECTOR t = XMLoadFloat3(&target);
            float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(pos, t)));
            m_orbitDistance = std::clamp(dist, kMinOrbitDistance, kMaxOrbitDistance);
        }
    }
    RecalculateVectors();
}

XMMATRIX Camera::GetViewMatrix() const noexcept {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAt = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMLoadFloat3(&m_up);
    return XMMatrixLookAtRH(pos, lookAt, up);
}

XMMATRIX Camera::GetProjectionMatrix() const noexcept {
    return XMMatrixPerspectiveFovRH(m_fov, m_aspect, m_nearZ, m_farZ);
}