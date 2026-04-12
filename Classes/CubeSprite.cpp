#include "CubeSprite.h"

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
    const float sr = collisionRadius * 1.05f;
    _shadow->drawSolidCircle(Vec2::ZERO, sr, 0.f, 32, 1.12f, 0.52f, Color4F(0.f, 0.f, 0.f, 0.34f));
    _shadow->setPosition(Vec2(2.f, -collisionRadius * 0.65f));
    addChild(_shadow, 0);

    setCascadeOpacityEnabled(true);
    _sprite->setCascadeOpacityEnabled(true);

    return true;
}

void CubeSprite::setPathTangent(const Vec2& t)
{
    float len2 = t.lengthSquared();
    if (len2 > 0.0001f)
        _pathTangent = t * (1.f / sqrtf(len2));
}

Vec2 CubeSprite::getVelocity() const
{
    return _pathTangent * _pathSpeed;
}

void CubeSprite::syncVelocityFromPath()
{
    // Velocity is fully defined by scalar speed and tangent.
}

void CubeSprite::applyVelocity(const Vec2& v)
{
    float s = v.dot(_pathTangent);
    _pathSpeed = s;
}

void CubeSprite::tick(float dt)
{
    // Inertia: ease path speed toward cruise (motor + light drag feel).
    const float tau = 0.35f;
    float k = 1.f - expf(-dt / tau);
    _pathSpeed += (_targetCruiseSpeed - _pathSpeed) * k;

    updateRollAndSprite(dt);
    refreshShadowTransform();
}

void CubeSprite::updateRollAndSprite(float dt)
{
    // Roll rate proportional to linear speed; sign follows motion along tangent.
    const float rollScale = 0.045f;
    _rollPhase += _pathSpeed * dt * rollScale;

    // Wrap phase to [0, TWO_PI)
    _rollPhase = fmodf(_rollPhase, TWO_PI);
    if (_rollPhase < 0.f)
        _rollPhase += TWO_PI;

    float frameF = (_rollPhase / TWO_PI) * FRAME_COUNT;
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
    float h = _sprite->getScale();
    _shadow->setScaleX(1.05f + 0.08f * h);
    _shadow->setScaleY(0.55f + 0.05f * h);
}
