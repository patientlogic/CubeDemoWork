#include "CubeSprite.h"

#include "base/ccRandom.h"

#include <algorithm>
#include <cmath>

USING_NS_CC;

namespace{
const float PI = 3.14159265f;
const float TWO_PI = PI * 2.f;

inline float clamp01(float x)
{
    if (x < 0.f) return 0.f;
    if (x > 1.f) return 1.f;
    return x;
}

/** Smoothstep for nicer easing. */
inline float smooth01(float t)
{
    t = clamp01(t);
    return t * t * (3.f - 2.f * t);
}

/** Shortest-path angle lerp factor t in [0,1]. */
inline float lerpAngle(float a, float b, float t)
{
    float d = b - a;
    while (d > PI)
        d -= TWO_PI;
    while (d < -PI)
        d += TWO_PI;
    return a + d * t;
}
}

CubeSprite* CubeSprite::create(const Vector<SpriteFrame*>& frames,
                               const std::vector<float>& scales,
                               float collisionRadius)
{
    CubeSprite* p = new (std::nothrow) CubeSprite();
    if (p && p->init(frames, scales, collisionRadius))
    {
        p->autorelease();
        return p;
    }
    CC_SAFE_DELETE(p);
    return nullptr;
}

bool CubeSprite::init(const Vector<SpriteFrame*>& frames,
                      const std::vector<float>& scales,
                      float collisionRadius)
{
    if (!Node::init())
        return false;

    CCASSERT(frames.size() == FRAME_COUNT, "CubeSprite expects 36 frames");
    CCASSERT(scales.size() == static_cast<size_t>(FRAME_COUNT), "CubeSprite expects 36 scales");

    _frames = frames;
    _depthScales = scales;
    _collisionRadius = collisionRadius;

    _sprite = Sprite::createWithSpriteFrame(frames.at(0));
    _sprite->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    addChild(_sprite, 1);

    _shadow = DrawNode::create();
    // Flattened ellipse in drawSolidCircle only — never use non-uniform Node scale (getScale() asserts).
    const float sr = collisionRadius * 0.72f;
    _shadow->drawSolidCircle(Vec2::ZERO, sr, 0.f, 28, 1.f, 0.42f, Color4F(0.f, 0.f, 0.f, 0.18f));
    _shadow->setPosition(Vec2(1.5f, -collisionRadius * 0.55f));
    addChild(_shadow, 0);

    setCascadeOpacityEnabled(true);
    _sprite->setCascadeOpacityEnabled(true);

    return true;
}

void CubeSprite::setPathTangent(const Vec2& t)
{
    float len2 = t.lengthSquared();
    if (len2 > 0.0001f)
    {
        _pathTangent = t * (1.f / sqrtf(len2));
        _pathNormal = Vec2(-_pathTangent.y, _pathTangent.x);
    }
}

Vec2 CubeSprite::getVelocity() const
{
    const float s = std::max(1e-5f, _worldScaleUniform);
    // Path/lateral speeds are in background-local units; scale to world for physics and roll.
    return _pathTangent * (_pathSpeed * s) + _pathNormal * (_lateralVel * s);
}

void CubeSprite::syncVelocityFromPath()
{
    // Velocity is fully defined by scalar speed and tangent.
}

void CubeSprite::applyVelocity(const Vec2& v)
{
    float invS = 1.f / std::max(1e-5f, _worldScaleUniform);
    _pathSpeed = v.dot(_pathTangent) * invS;
    _lateralVel = v.dot(_pathNormal) * invS;
}

void CubeSprite::boostRollSpinFromCollision(float normalImpulseMagnitude)
{
    if (normalImpulseMagnitude <= 0.f)
        return;
    const float R = std::max(1e-5f, _collisionRadius);
    const float gain = 0.55f / R;
    const float sign = (cocos2d::random(0.f, 1.f) < 0.5f) ? -1.f : 1.f;
    const float jitter = cocos2d::random(0.65f, 1.15f);
    _rollSpinRate += sign * normalImpulseMagnitude * gain * jitter;
    const float cap = 14.f;
    if (_rollSpinRate > cap)
        _rollSpinRate = cap;
    else if (_rollSpinRate < -cap)
        _rollSpinRate = -cap;
}

void CubeSprite::integrateCornerRoll(float cornerArcWeight, float pathSpeedLocal, float /*dt*/)
{
    const float enterThresh = 0.28f;
    const bool entered = cornerArcWeight > enterThresh && _prevCornerRollWeight <= enterThresh;
    _prevCornerRollWeight = cornerArcWeight;

    if (entered)
    {
        const float R = std::max(1e-5f, _collisionRadius);
        const float s = std::max(1e-5f, _worldScaleUniform);
        const float vWorld = fabsf(pathSpeedLocal) * s;
        const float omega0 = vWorld / R;
        const float sign = (cocos2d::random(0.f, 1.f) < 0.5f) ? -1.f : 1.f;
        const float jitter = cocos2d::random(0.4f, 1.05f);
        _rollSpinRate += sign * omega0 * jitter * 0.42f;
    }

    const float cap = 14.f;
    if (_rollSpinRate > cap)
        _rollSpinRate = cap;
    else if (_rollSpinRate < -cap)
        _rollSpinRate = -cap;
}

void CubeSprite::integratePathSpeed(float dt, float maxAccel)
{
    float diff = _targetCruiseSpeed - _pathSpeed;
    float lim = maxAccel * dt;
    if (diff > lim)
        diff = lim;
    else if (diff < -lim)
        diff = -lim;
    _pathSpeed += diff;
}

void CubeSprite::clampSpeeds(float maxPathSpeed, float maxLateralVelLocal)
{
    if (_pathSpeed > maxPathSpeed)
        _pathSpeed = maxPathSpeed;
    else if (_pathSpeed < -maxPathSpeed)
        _pathSpeed = -maxPathSpeed;
    if (_lateralVel > maxLateralVelLocal)
        _lateralVel = maxLateralVelLocal;
    else if (_lateralVel < -maxLateralVelLocal)
        _lateralVel = -maxLateralVelLocal;
}

void CubeSprite::tick(float dt)
{
    updateRollAndSprite(dt);
    refreshShadowTransform();
}

void CubeSprite::updateRollAndSprite(float dt)
{
    // Sheets bake rotY(yaw); node rotation aligns bake forward with travel.
    // No continuous “belt roll” — cubes stay on a stable frame while cruising; _rollSpinRate
    // (corners + collisions only) advances _rollPhase, then decays for calm straight motion.
    const float spinDecaySec = 0.62f;
    _rollSpinRate *= expf(-dt / spinDecaySec);

    _rollPhase += _rollSpinRate * dt;

    // Wrap accumulated spin to a stable range (avoid float drift).
    _rollPhase = fmodf(_rollPhase, TWO_PI);
    if (_rollPhase < 0.f)
        _rollPhase += TWO_PI;

    Vec2 v = getVelocity();
    float speed2 = v.lengthSquared();
    float targetHeading = atan2f(_pathTangent.y, _pathTangent.x);
    if (speed2 > 25.f)
    {
        float vHeading = atan2f(v.y, v.x);
        float blend = clamp01(speed2 / (speed2 + 12000.f));
        targetHeading = lerpAngle(targetHeading, vHeading, blend * 0.35f);
    }
    const float alignRate = 10.f;
    float k = 1.f - expf(-alignRate * dt);
    _visualHeading = lerpAngle(_visualHeading, targetHeading, k);

    // Cocos rotation is clockwise degrees; atan2 is CCW from +X — negate to align travel with bake.
    setRotation(-_visualHeading * (180.f / PI));

    float combined = _rollPhase;
    combined = fmodf(combined, TWO_PI);
    if (combined < 0.f)
        combined += TWO_PI;

    float frameF = (combined / TWO_PI) * FRAME_COUNT;
    int idx = static_cast<int>(frameF) % FRAME_COUNT;
    if (idx < 0)
        idx += FRAME_COUNT;

    _sprite->setSpriteFrame(_frames.at(idx));

    float frac = frameF - floorf(frameF);
    float s0 = _depthScales[static_cast<size_t>(idx)];
    int idx1 = (idx + 1) % FRAME_COUNT;
    float s1 = _depthScales[static_cast<size_t>(idx1)];
    float blend = smooth01(frac);
    float sc = s0 * (1.f - blend) + s1 * blend;

    _sprite->setScale(sc);
}

void CubeSprite::refreshShadowTransform()
{
    float h = _sprite->getScaleX();
    _shadow->setScale(0.82f + 0.1f * h);
}
