#include "GameScene.h"

#include "base/ccRandom.h"
#include "math/Vec3.h"
#include "2d/CCRenderTexture.h"
#include "platform/CCGL.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

USING_NS_CC;

namespace
{
const float PI = 3.14159265f;
const float TWO_PI = PI * 2.f;

/** Reusable buffer for rounded face outlines (baked once per palette). */
static std::vector<Vec2> s_roundPoly;

/**
 * Convex quad with one cut per corner (8 vertices): small constant-radius chamfer, no arc tessellation.
 * Keeps edges clean; falls back to the original quad if a corner cannot be cut safely.
 */
static void buildChamferedQuadPolygon(std::vector<Vec2>& out, const Vec2 poly[4], float cornerR)
{
    out.clear();
    float minEdge = 1.e9f;
    for (int i = 0; i < 4; ++i)
        minEdge = std::min(minEdge, poly[i].distance(poly[(i + 1) % 4]));
    cornerR = std::min(cornerR, minEdge * 0.28f);

    Vec2 A[4];
    Vec2 B[4];
    bool ok[4] = {false, false, false, false};

    for (int i = 0; i < 4; ++i)
    {
        const Vec2& V = poly[i];
        const Vec2& Vprev = poly[(i + 3) % 4];
        const Vec2& Vnext = poly[(i + 1) % 4];
        Vec2 e1 = Vprev - V;
        Vec2 e2 = Vnext - V;
        float len1 = e1.length();
        float len2 = e2.length();
        if (len1 < 1e-4f || len2 < 1e-4f)
            continue;
        e1 *= (1.f / len1);
        e2 *= (1.f / len2);
        float cosTheta = Vec2::dot(e1, e2);
        cosTheta = std::max(-1.f, std::min(1.f, cosTheta));
        float theta = acosf(cosTheta);
        if (theta < 0.04f)
            continue;
        float d = cornerR / tanf(theta * 0.5f);
        float maxD = std::min(len1, len2) * 0.5f - 0.35f;
        if (maxD < 0.25f)
            continue;
        d = std::min(d, maxD);
        A[i] = V + e1 * d;
        B[i] = V + e2 * d;
        ok[i] = true;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (!ok[i])
        {
            out.assign(poly, poly + 4);
            return;
        }
    }

    // Perimeter: A0–B0 along cut, then edge B0–A1, … (closed to A0).
    out.reserve(8);
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(A[i]);
        out.push_back(B[i]);
    }
}

inline Vec3 rotY(const Vec3& v, float yaw)
{
    float c = cosf(yaw);
    float s = sinf(yaw);
    return Vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

// Six faces (front +Z, back −Z, top +Y, bottom −Y, right +X, left −X) — distinct shades so the cube reads as a hex.
const Color4F kPalBlue[6] = {
    Color4F(0.35f, 0.55f, 1.0f, 1.f), Color4F(0.12f, 0.2f, 0.48f, 1.f),  Color4F(0.65f, 0.82f, 1.0f, 1.f),
    Color4F(0.1f, 0.18f, 0.5f, 1.f),  Color4F(0.28f, 0.5f, 0.98f, 1.f),  Color4F(0.18f, 0.38f, 0.85f, 1.f),
};
const Color4F kPalGreen[6] = {
    Color4F(0.22f, 0.88f, 0.42f, 1.f), Color4F(0.08f, 0.35f, 0.16f, 1.f), Color4F(0.5f, 1.0f, 0.62f, 1.f),
    Color4F(0.06f, 0.28f, 0.12f, 1.f), Color4F(0.18f, 0.72f, 0.36f, 1.f), Color4F(0.12f, 0.55f, 0.3f, 1.f),
};
const Color4F kPalRed[6] = {
    Color4F(1.0f, 0.32f, 0.3f, 1.f), Color4F(0.42f, 0.1f, 0.1f, 1.f),  Color4F(1.0f, 0.65f, 0.62f, 1.f),
    Color4F(0.35f, 0.06f, 0.08f, 1.f), Color4F(0.95f, 0.2f, 0.2f, 1.f),  Color4F(0.78f, 0.14f, 0.18f, 1.f),
};
const Color4F kPalYellow[6] = {
    Color4F(1.0f, 0.85f, 0.22f, 1.f), Color4F(0.5f, 0.38f, 0.06f, 1.f), Color4F(1.0f, 0.96f, 0.55f, 1.f),
    Color4F(0.4f, 0.3f, 0.05f, 1.f), Color4F(0.98f, 0.75f, 0.15f, 1.f), Color4F(0.8f, 0.58f, 0.1f, 1.f),
};
const Color4F kPalPurple[6] = {
    Color4F(0.68f, 0.42f, 1.0f, 1.f), Color4F(0.26f, 0.1f, 0.45f, 1.f), Color4F(0.88f, 0.72f, 1.0f, 1.f),
    Color4F(0.2f, 0.08f, 0.36f, 1.f), Color4F(0.55f, 0.28f, 0.95f, 1.f), Color4F(0.42f, 0.2f, 0.75f, 1.f),
};

const Color4F* const kAllPalettes[5] = {kPalBlue, kPalGreen, kPalRed, kPalYellow, kPalPurple};
}

Scene* GameScene::createScene()
{
    return GameScene::create();
}

void GameScene::onExit()
{
    Scene::onExit();
    for (auto* rt : _paletteSheets)
        CC_SAFE_RELEASE_NULL(rt);
    _paletteSheets.clear();
}

Vec2 GameScene::localToWorld(const Vec2& local) const
{
    return _bgCenter + Vec2(local.x * _bgUniformScale, local.y * _bgUniformScale);
}

Vec2 GameScene::worldToLocal(const Vec2& world) const
{
    return Vec2((world.x - _bgCenter.x) / _bgUniformScale, (world.y - _bgCenter.y) / _bgUniformScale);
}

void GameScene::setupBackgroundAndPath()
{
    auto vs = Director::getInstance()->getVisibleSize();
    auto vo = Director::getInstance()->getVisibleOrigin();
    _bgCenter = vo + Vec2(vs.width * 0.5f, vs.height * 0.5f);

    _bgSprite = Sprite::create("track_background.png");
    if (!_bgSprite || !_bgSprite->getTexture())
    {
        CCLOG("GameScene: track_background.png missing — add it to Resources.");
        auto layer = LayerColor::create(Color4B(238, 236, 232, 255), vs.width, vs.height);
        layer->setPosition(vo);
        addChild(layer, -300);
        // Default path in design space if no art.
        _bgUniformScale = 1.f;
        _rrHalfW = vs.width * 0.34f;
        _rrHalfWSide = _rrHalfW * 0.75f;
        _rrHalfH = vs.height * 0.34f;
        _rrCornerR = std::min(_rrHalfW, _rrHalfH) * 0.28f;
    }
    else
    {
        Size cs = _bgSprite->getContentSize();
        _bgSprite->setAnchorPoint(Vec2(0.5f, 0.5f));
        _bgSprite->setPosition(_bgCenter);
        float scl = std::min(vs.width / cs.width, vs.height / cs.height);
        _bgSprite->setScale(scl);
        _bgUniformScale = scl;
        addChild(_bgSprite, -200);

        // Outer half-width matches top/bottom yellow lane; sides use a smaller |x| so cubes hug the arrows there.
        const float kHalfNormOuter = 0.335f;
        const float kHalfNormSide = 0.200f;
        const float kCornerNorm = 0.115f;
        _rrHalfW = cs.width * kHalfNormOuter;
        _rrHalfWSide = cs.width * kHalfNormSide;
        _rrHalfH = cs.height * kHalfNormOuter;
        _rrCornerR = std::min(cs.width, cs.height) * kCornerNorm;
    }

    float maxR = std::min(_rrHalfWSide, _rrHalfH) * 0.95f;
    if (_rrCornerR > maxR)
        _rrCornerR = maxR;
    if (_rrHalfWSide >= _rrHalfW - 0.5f)
        _rrHalfWSide = _rrHalfW * 0.75f;
    if (_rrHalfWSide <= _rrCornerR + 0.5f)
        _rrHalfWSide = _rrCornerR + 1.f;

    float aH = _rrHalfW;
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lx = aH - aV;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;
    // Inner bottom only (Lm): s=0 at (-aV+r,-b) matches SW end — no extra outer-left Lx so start/end coincide.
    _perimeter = 2.f * Lm + 4.f * La + 2.f * Lr;

    // World-space hit radius from bake (drawHalf=13) + projection; then tightened so contact
    // matches visible overlap (full corner+maxDepth sphere was too large / cluttered).
    const float kBakeHalf = 13.f;
    const float kProj = 0.52f;
    const float kTilt = 0.48f;
    const float kMaxAxisCombo = std::sqrt(1.f + (1.f + kTilt) * (1.f + kTilt));
    const float kTypicalDepthScale = 0.92f;
    const float kHitRadiusTighten = 0.76f;
    float rWorld = kBakeHalf * kProj * kMaxAxisCombo * kTypicalDepthScale * kHitRadiusTighten;

    float th = std::min(_rrHalfW, _rrHalfH);
    float maxLaneGeom = th * 0.48f;
    float rLocalMax = std::max(10.f, maxLaneGeom - 18.f);
    float rLocal = rWorld / std::max(0.001f, _bgUniformScale);
    if (rLocal > rLocalMax)
    {
        rLocal = rLocalMax;
        rWorld = rLocal * _bgUniformScale;
    }

    _collisionRadius = rWorld;

    // Physics uses a tightened circle; lane clamping uses projected sprite extent so visuals do not
    // read past the blue band when the node (anchor center) is on the path centerline.
    float rLocalPhys = _collisionRadius / std::max(0.001f, _bgUniformScale);
    const float kVisualLateralFudge = 1.05f;
    _lateralVisualHalfLocal = (rLocalPhys / kHitRadiusTighten) * kVisualLateralFudge;

    // Inner blue track (between white and outer blue). Inset slightly so cube visuals stay in blue.
    float rLocalForLane = std::max(rLocalPhys, _lateralVisualHalfLocal);
    const float kInnerBlueTrackHalfFactor = 0.275f;
    _laneHalfLocal = th * kInnerBlueTrackHalfFactor;
    _laneHalfLocal = std::max(_laneHalfLocal, rLocalForLane + 5.f);
    _laneHalfLocal = std::min(_laneHalfLocal, th * 0.315f);
    // Extend toward hole center so play/debug band matches inner blue (past inner white line).
    _innerLaneExtraLocal = std::max(10.f, th * 0.042f);
    // Top straight: outer edge boost; inner inset uses _topInnerPullLocal only (see below).
    const float kUpperPx = 10.f;
    float invS = 1.f / std::max(0.001f, _bgUniformScale);
    _upperLaneInnerInwardLocal = 0.f;
    _upperLaneOuterOutwardLocal = kUpperPx * invS;
    // Top straight: ~20 px toward screen top so inner edge sits inset vs inner blue art.
    _topInnerPullLocal = 20.f * invS;
    // Bottom straight: ~20 px toward screen bottom so inner edge sits inset vs inner blue art.
    _bottomInnerPullLocal = 20.f * invS;
    const float kCornerBlueInsetPx = 20.f;
    _nwCornerInsetLocal = kCornerBlueInsetPx * invS;
    _neCornerInsetLocal = kCornerBlueInsetPx * invS;
    _swCornerInsetLocal = kCornerBlueInsetPx * invS;
    _seCornerInsetLocal = kCornerBlueInsetPx * invS;
}

void GameScene::setupBeltVisual()
{
    _bgTexScrollEnabled = false;
    if (_bgSprite && _bgSprite->getTexture())
    {
        Texture2D* tex = _bgSprite->getTexture();
        _bgTexSize = tex->getContentSize();
        int iw = (int)floorf(_bgTexSize.width + 0.5f);
        int ih = (int)floorf(_bgTexSize.height + 0.5f);
        auto isPow2 = [](int x) { return x > 0 && (x & (x - 1)) == 0; };
        if (isPow2(iw) && isPow2(ih))
        {
            Texture2D::TexParams tp = { GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT };
            tex->setTexParameters(tp);
            _bgTexScrollEnabled = true;
        }
    }

    _beltOverlay = DrawNode::create();
    addChild(_beltOverlay, -199);
}

void GameScene::updateBeltVisual(float dt, float avgBeltSpeed)
{
    _beltPhase = wrapS(_beltPhase + avgBeltSpeed * dt);

    Vec2 avgVel(0.f, 0.f);
    const int n = (int)_cubes.size();
    if (n > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            Vec2 t = tangentAt(_arc[i]);
            avgVel += t * _cubes[i]->getPathSpeed();
        }
        avgVel *= (1.f / (float)n);
    }
    else
    {
        avgVel = Vec2(_baseCruise * 0.5f, 0.f);
    }

    if (_bgTexScrollEnabled && _bgSprite)
    {
        const float k = 0.08f;
        _beltTexOffX += avgVel.x * dt * k;
        _beltTexOffY += avgVel.y * dt * k;
        float w = _bgTexSize.width;
        float h = _bgTexSize.height;
        float ox = fmodf(_beltTexOffX, w);
        float oy = fmodf(_beltTexOffY, h);
        if (ox < 0.f)
            ox += w;
        if (oy < 0.f)
            oy += h;
        _bgSprite->setTextureRect(Rect(ox, oy, w, h));
    }

    if (!_beltOverlay || _perimeter <= 0.f)
        return;

    _beltOverlay->clear();
    const int N = 240;
    const float dashLen = std::max(12.f, _perimeter / 48.f);
    Vec2 prev = positionAt(0.f);
    for (int i = 1; i <= N; ++i)
    {
        float s = (float)i / (float)N * _perimeter;
        Vec2 q = positionAt(s);
        float seg = (float)((int)floorf((s + _beltPhase) / dashLen) & 1);
        Color4F c = seg > 0.5f ? Color4F(1.f, 1.f, 1.f, 0.09f) : Color4F(0.85f, 0.82f, 0.75f, 0.07f);
        _beltOverlay->drawSegment(prev, q, 2.2f, c);
        prev = q;
    }
}

Vec2 GameScene::roundedRectLocalPos(float s) const
{
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;

    float c1 = Lm;
    float c2 = c1 + La;
    float c3 = c2 + Lr;
    float c4 = c3 + La;
    float c5 = c4 + Lm;
    float c6 = c5 + La;
    float c7 = c6 + Lr;

    s = wrapS(s);

    if (s < c1)
        return Vec2(-aV + r + s, -b);
    if (s < c2)
    {
        float u = s - c1;
        float th = -PI * 0.5f + (u / La) * (PI * 0.5f);
        Vec2 C(aV - r, -b + r);
        return C + Vec2(r * cosf(th), r * sinf(th));
    }
    if (s < c3)
        return Vec2(aV, -b + r + (s - c2));
    if (s < c4)
    {
        float u = s - c3;
        float th = 0.f + (u / La) * (PI * 0.5f);
        Vec2 C(aV - r, b - r);
        return C + Vec2(r * cosf(th), r * sinf(th));
    }
    if (s < c5)
        return Vec2(aV - r - (s - c4), b);
    if (s < c6)
    {
        float u = s - c5;
        float th = PI * 0.5f + (u / La) * (PI * 0.5f);
        Vec2 C(-aV + r, b - r);
        return C + Vec2(r * cosf(th), r * sinf(th));
    }
    if (s < c7)
        return Vec2(-aV, b - r - (s - c6));
    {
        float u = s - c7;
        float th = PI + (u / La) * (PI * 0.5f);
        Vec2 C(-aV + r, -b + r);
        return C + Vec2(r * cosf(th), r * sinf(th));
    }
}

Vec2 GameScene::roundedRectLocalTangent(float s) const
{
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;

    float c1 = Lm;
    float c2 = c1 + La;
    float c3 = c2 + Lr;
    float c4 = c3 + La;
    float c5 = c4 + Lm;
    float c6 = c5 + La;
    float c7 = c6 + Lr;

    s = wrapS(s);

    if (s < c1)
        return Vec2(1.f, 0.f);
    if (s < c2)
    {
        float u = s - c1;
        float th = -PI * 0.5f + (u / La) * (PI * 0.5f);
        Vec2 t(-sinf(th), cosf(th));
        t.normalize();
        return t;
    }
    if (s < c3)
        return Vec2(0.f, 1.f);
    if (s < c4)
    {
        float u = s - c3;
        float th = 0.f + (u / La) * (PI * 0.5f);
        Vec2 t(-sinf(th), cosf(th));
        t.normalize();
        return t;
    }
    if (s < c5)
        return Vec2(-1.f, 0.f);
    if (s < c6)
    {
        float u = s - c5;
        float th = PI * 0.5f + (u / La) * (PI * 0.5f);
        Vec2 t(-sinf(th), cosf(th));
        t.normalize();
        return t;
    }
    if (s < c7)
        return Vec2(0.f, -1.f);
    {
        float u = s - c7;
        float th = PI + (u / La) * (PI * 0.5f);
        Vec2 t(-sinf(th), cosf(th));
        t.normalize();
        return t;
    }
}

bool GameScene::init()
{
    if (!Scene::init())
        return false;

    setupBackgroundAndPath();
    buildPaletteSpriteSheets();
    drawPathGizmo();
    setupBeltVisual();

    _hint = Label::createWithSystemFont(" ", "Arial", 14);
    if (_hint)
    {
        _hint->setAnchorPoint(Vec2::ZERO);
        _hint->setPosition(Vec2(10.f, 10.f));
        _hint->setColor(Color3B(40, 40, 45));
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
        _hint->enableOutline(Color4B(255, 255, 255, 120), 1);
#endif
        addChild(_hint, 100);
    }

    spawnInitialCubes();
    refreshHintText();

    auto listener = EventListenerTouchOneByOne::create();
    listener->setSwallowTouches(true);
    listener->onTouchBegan = CC_CALLBACK_2(GameScene::onTouchBegan, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

    scheduleUpdate();
    return true;
}

float GameScene::wrapS(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    float r = fmodf(s, _perimeter);
    if (r < 0.f)
        r += _perimeter;
    return r;
}

Vec2 GameScene::positionAt(float s) const
{
    return localToWorld(roundedRectLocalPos(s));
}

Vec2 GameScene::tangentAt(float s) const
{
    Vec2 t = roundedRectLocalTangent(s);
    t.normalize();
    return t;
}

float GameScene::maxLateralLocal() const
{
    const float kEdgeMarginLocal = 5.f;
    return std::max(2.f, _laneHalfLocal - _lateralVisualHalfLocal - kEdgeMarginLocal);
}

namespace
{
inline void innerOuterDirsFromCenter(const Vec2& p, const Vec2& n, Vec2* innerDir, Vec2* outerDir)
{
    Vec2 toCenter = -p;
    if (toCenter.lengthSquared() < 1e-4f)
        toCenter = n;
    else
        toCenter.normalize();
    float d = Vec2::dot(n, toCenter);
    if (d >= 0.f)
    {
        *innerDir = n;
        *outerDir = -n;
    }
    else
    {
        *innerDir = -n;
        *outerDir = n;
    }
}

inline float smoothStep01Lane(float x)
{
    if (x <= 0.f)
        return 0.f;
    if (x >= 1.f)
        return 1.f;
    return x * x * (3.f - 2.f * x);
}

/** Perlin-style smootherstep for gentler corner in/out ramps. */
inline float smootherStep01Lane(float x)
{
    if (x <= 0.f)
        return 0.f;
    if (x >= 1.f)
        return 1.f;
    return x * x * x * (x * (x * 6.f - 15.f) + 10.f);
}
}

float GameScene::cornerArcWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;
    float c1 = Lm;
    float c2 = c1 + La;
    float c3 = c2 + Lr;
    float c4 = c3 + La;
    float c5 = c4 + Lm;
    float c6 = c5 + La;
    float c7 = c6 + Lr;
    // Plateau on each quarter-circle with smooth ramps at straights (smootherstep = flexible rounding).
    const float kCornerEndBlend = 0.13f;

    auto segWeightPlateau = [s, kCornerEndBlend](float lo, float hi) -> float {
        if (s < lo || s >= hi - 1e-5f)
            return 0.f;
        float u = (s - lo) / std::max(1e-5f, hi - lo);
        float rampIn = smootherStep01Lane(u / kCornerEndBlend);
        float rampOut = smootherStep01Lane((1.f - u) / kCornerEndBlend);
        return std::min(rampIn, rampOut);
    };

    float w = segWeightPlateau(c1, c2);
    w = std::max(w, segWeightPlateau(c3, c4));
    w = std::max(w, segWeightPlateau(c5, c6));
    w = std::max(w, segWeightPlateau(c7, _perimeter));
    return w;
}

float GameScene::upperRegionWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;
    float c1 = Lm;
    float c2 = c1 + La;
    float c3 = c2 + Lr;
    float c4 = c3 + La;
    float c5 = c4 + Lm;
    // Top straight [c4, c5) only — excludes NE (c3–c4) and NW (c5–c6) so top-right / top-left
    // corner arcs use the same lateral corner math as bottom-right / bottom-left (matches blue art).
    const float lo = c4;
    const float hi = c5;
    if (s < lo || s >= hi - 1e-5f)
        return 0.f;
    float u = (s - lo) / std::max(1e-5f, hi - lo);
    const float kUpperBlend = 0.14f;
    float rampIn = smootherStep01Lane(u / kUpperBlend);
    float rampOut = smootherStep01Lane((1.f - u) / kUpperBlend);
    return std::min(rampIn, rampOut);
}

float GameScene::bottomHorizontalWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float r = _rrCornerR;
    float c1 = 2.f * aV - 2.f * r;
    if (s >= c1 - 1e-5f)
        return 0.f;
    float u = s / std::max(1e-5f, c1);
    const float kBlend = 0.1f;
    float rampIn = smootherStep01Lane(u / kBlend);
    float rampOut = smootherStep01Lane((1.f - u) / kBlend);
    return std::min(rampIn, rampOut);
}

float GameScene::northwestCornerWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;
    float c4 = Lm + La + Lr + La;
    float c5 = c4 + Lm;
    float c6 = c5 + La;
    const float lo = c5;
    const float hi = c6;
    if (s < lo || s >= hi - 1e-5f)
        return 0.f;
    float u = (s - lo) / std::max(1e-5f, hi - lo);
    const float kBlend = 0.13f;
    float rampIn = smootherStep01Lane(u / kBlend);
    float rampOut = smootherStep01Lane((1.f - u) / kBlend);
    return std::min(rampIn, rampOut);
}

float GameScene::northeastCornerWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;
    float c2 = Lm + La;
    float c3 = c2 + Lr;
    float c4 = c3 + La;
    const float lo = c3;
    const float hi = c4;
    if (s < lo || s >= hi - 1e-5f)
        return 0.f;
    float u = (s - lo) / std::max(1e-5f, hi - lo);
    const float kBlend = 0.13f;
    float rampIn = smootherStep01Lane(u / kBlend);
    float rampOut = smootherStep01Lane((1.f - u) / kBlend);
    return std::min(rampIn, rampOut);
}

float GameScene::southwestCornerWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float b = _rrHalfH;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float Lr = 2.f * b - 2.f * r;
    float c7 = Lm + La + Lr + La + Lm + La + Lr;
    const float lo = c7;
    const float hi = _perimeter;
    if (s < lo || s >= hi - 1e-5f)
        return 0.f;
    float u = (s - lo) / std::max(1e-5f, hi - lo);
    const float kBlend = 0.13f;
    float rampIn = smootherStep01Lane(u / kBlend);
    float rampOut = smootherStep01Lane((1.f - u) / kBlend);
    return std::min(rampIn, rampOut);
}

float GameScene::southeastCornerWeight(float s) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    s = wrapS(s);
    float aV = _rrHalfWSide;
    float r = _rrCornerR;
    float Lm = 2.f * aV - 2.f * r;
    float La = PI * r * 0.5f;
    float c1 = Lm;
    float c2 = c1 + La;
    const float lo = c1;
    const float hi = c2;
    if (s < lo || s >= hi - 1e-5f)
        return 0.f;
    float u = (s - lo) / std::max(1e-5f, hi - lo);
    const float kBlend = 0.13f;
    float rampIn = smootherStep01Lane(u / kBlend);
    float rampOut = smootherStep01Lane((1.f - u) / kBlend);
    return std::min(rampIn, rampOut);
}

void GameScene::lateralCapsAt(float s, float* capInner, float* capOuter) const
{
    float base = maxLateralLocal();
    // Moderate inward bulge on corners (was 0.28 — less push toward hole at apex).
    const float kCornerLateralExpand = 0.2f;
    // Tuck outer boundary slightly inward on corners so the band does not read as extending too far outward.
    const float kCornerOuterInset = 0.07f;
    float w = cornerArcWeight(s);
    float atInner = base * (1.f + kCornerLateralExpand * w);
    float uw = upperRegionWeight(s);
    float bw = bottomHorizontalWeight(s);
    float nw = northwestCornerWeight(s);
    float ne = northeastCornerWeight(s);
    float sw = southwestCornerWeight(s);
    float se = southeastCornerWeight(s);
    float artCornerInset = nw * _nwCornerInsetLocal + ne * _neCornerInsetLocal + sw * _swCornerInsetLocal
        + se * _seCornerInsetLocal;
    if (capInner)
    {
        float ci = atInner + _innerLaneExtraLocal + uw * _upperLaneInnerInwardLocal;
        // Top horizontal: move inner boundary toward screen top vs inner blue art.
        ci -= uw * _topInnerPullLocal;
        // Bottom horizontal: move inner boundary toward screen bottom vs inner blue art.
        ci -= bw * _bottomInnerPullLocal;
        // All four quarter arcs: slight inset vs blue (NE/SE aligned same scale).
        ci -= artCornerInset;
        *capInner = std::max(2.f, ci);
    }
    if (capOuter)
    {
        float co = base * (1.f - kCornerOuterInset * w) + uw * _upperLaneOuterOutwardLocal;
        co -= artCornerInset;
        *capOuter = std::max(2.f, co);
    }
}

Vec2 GameScene::positionAtLateral(float s, float lateralLocal) const
{
    Vec2 t = roundedRectLocalTangent(s);
    t.normalize();
    Vec2 n(-t.y, t.x);
    Vec2 p = roundedRectLocalPos(s) + n * lateralLocal;
    return localToWorld(p);
}

void GameScene::reprojectWorldToPath(const Vec2& world, float* outS, float* outLateral) const
{
    float s = projectPointToArc(world);
    Vec2 L = worldToLocal(world);
    Vec2 p0 = roundedRectLocalPos(s);
    Vec2 t = roundedRectLocalTangent(s);
    t.normalize();
    Vec2 n(-t.y, t.x);
    float lat = Vec2::dot(L - p0, n);
    float capIn = 0.f;
    float capOut = 0.f;
    lateralCapsAt(s, &capIn, &capOut);
    Vec2 toCenter = -p0;
    if (toCenter.lengthSquared() < 1e-4f)
        toCenter = n;
    else
        toCenter.normalize();
    float dotN = Vec2::dot(n, toCenter);
    if (dotN >= 0.f)
    {
        if (lat > capIn)
            lat = capIn;
        if (lat < -capOut)
            lat = -capOut;
    }
    else
    {
        if (lat > capOut)
            lat = capOut;
        if (lat < -capIn)
            lat = -capIn;
    }
    if (outS)
        *outS = wrapS(s);
    if (outLateral)
        *outLateral = lat;
}

float GameScene::wrappedChordDistance(float a, float b) const
{
    float d = fabsf(a - b);
    d = fmodf(d, _perimeter);
    if (d > _perimeter * 0.5f)
        d = _perimeter - d;
    return d;
}

float GameScene::pickSpawnArcWithMinGap(float minChord) const
{
    if (_perimeter <= 0.f)
        return 0.f;
    const int maxAttempts = 160;
    for (int a = 0; a < maxAttempts; ++a)
    {
        float s = cocos2d::random(0.f, _perimeter);
        bool ok = true;
        for (float existing : _arc)
        {
            if (wrappedChordDistance(s, existing) < minChord)
            {
                ok = false;
                break;
            }
        }
        if (ok)
            return s;
    }
    return cocos2d::random(0.f, _perimeter);
}

float GameScene::projectPointToArc(const Vec2& worldPos) const
{
    Vec2 L = worldToLocal(worldPos);
    const int NS = 512;
    float bestS = 0.f;
    float bestD2 = 1.e20f;
    for (int i = 0; i <= NS; ++i)
    {
        float si = (float)i / (float)NS * _perimeter;
        Vec2 pi = roundedRectLocalPos(si);
        float d2 = L.distanceSquared(pi);
        if (d2 < bestD2)
        {
            bestD2 = d2;
            bestS = si;
        }
    }
    // Local refinement
    float step = _perimeter / (float)NS;
    for (int k = 0; k < 8; ++k)
    {
        float sL = wrapS(bestS - step);
        float sR = wrapS(bestS + step);
        float d0 = L.distanceSquared(roundedRectLocalPos(bestS));
        float dL = L.distanceSquared(roundedRectLocalPos(sL));
        float dR = L.distanceSquared(roundedRectLocalPos(sR));
        if (dL < d0 && dL <= dR)
        {
            bestS = sL;
            step *= 0.5f;
        }
        else if (dR < d0)
        {
            bestS = sR;
            step *= 0.5f;
        }
        else
            step *= 0.5f;
    }
    return wrapS(bestS);
}

std::vector<float> GameScene::scatterArcPositions(int count, float minChord) const
{
    std::vector<float> out;
    out.reserve(static_cast<size_t>(count));
    int attempts = 0;
    const int maxAttempts = count * 220;
    while ((int)out.size() < count && attempts < maxAttempts)
    {
        ++attempts;
        float s = cocos2d::random(0.f, _perimeter);
        bool ok = true;
        for (float t : out)
        {
            if (wrappedChordDistance(s, t) < minChord)
            {
                ok = false;
                break;
            }
        }
        if (ok)
            out.push_back(s);
    }
    while ((int)out.size() < count)
        out.push_back(cocos2d::random(0.f, _perimeter));
    return out;
}

void GameScene::drawCubeToNode(DrawNode* dn, float yaw, float cx, float cy, float half, float* outDepth, const Color4F* cols)
{
    static const int faces[6][4] = {
        {4, 5, 6, 7},
        {0, 3, 2, 1},
        {3, 2, 6, 7},
        {0, 4, 5, 1},
        {1, 2, 6, 5},
        {0, 3, 7, 4}
    };

    static const Vec3 base[8] = {
        Vec3(-1.f, -1.f, -1.f), Vec3(1.f, -1.f, -1.f), Vec3(1.f, 1.f, -1.f), Vec3(-1.f, 1.f, -1.f),
        Vec3(-1.f, -1.f, 1.f), Vec3(1.f, -1.f, 1.f), Vec3(1.f, 1.f, 1.f), Vec3(-1.f, 1.f, 1.f)
    };

    Vec3 wv[8];
    float maxZ = -1.e9f;
    for (int i = 0; i < 8; i++)
    {
        wv[i] = rotY(base[i], yaw);
        maxZ = std::max(maxZ, wv[i].z);
    }
    if (outDepth)
        *outDepth = maxZ;

    auto proj = [&](const Vec3& w) -> Vec2 {
        const float tilt = 0.48f;
        return Vec2(cx + w.x * half * 0.52f, cy + (w.y + w.z * tilt) * half * 0.52f);
    };

    struct FaceOrder
    {
        float z;
        int id;
    };
    std::vector<FaceOrder> order;
    order.reserve(6);

    for (int f = 0; f < 6; f++)
    {
        float az = 0.f;
        for (int k = 0; k < 4; k++)
            az += wv[faces[f][k]].z;
        order.push_back({az * 0.25f, f});
    }

    std::sort(order.begin(), order.end(), [](const FaceOrder& A, const FaceOrder& B) { return A.z < B.z; });

    for (const auto& fo : order)
    {
        int f = fo.id;
        Vec2 poly[4];
        for (int k = 0; k < 4; k++)
            poly[k] = proj(wv[faces[f][k]]);
        // Small radius per corner (chamfer / single cut): 8 verts, no dense arcs or heavy outline (less noise).
        const float cornerR = std::max(0.75f, half * 0.09f);
        buildChamferedQuadPolygon(s_roundPoly, poly, cornerR);
        const Vec2* verts = s_roundPoly.data();
        int nv = (int)s_roundPoly.size();
        if (nv < 3)
        {
            verts = poly;
            nv = 4;
        }
        dn->drawPolygon(verts, nv, cols[f], 0.f, Color4F(0.f, 0.f, 0.f, 0.f));
    }
}

void GameScene::bakePaletteToTexture(const Color4F* faceColors, int paletteIndex)
{
    _depthScales.resize(NUM_FRAMES);

    auto holder = Node::create();
    holder->setAnchorPoint(Vec2::ZERO);
    const float drawHalf = 13.f;

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        auto dn = DrawNode::create();
        float yaw = (float)i / (float)NUM_FRAMES * TWO_PI;
        int col = i % GRID;
        int row = i / GRID;
        float cx = col * CELL + CELL * 0.5f;
        float cy = row * CELL + CELL * 0.5f;
        float depthZ = 0.f;
        drawCubeToNode(dn, yaw, cx, cy, drawHalf, &depthZ, faceColors);
        float nz = (depthZ / 1.414f + 1.f) * 0.5f;
        _depthScales[static_cast<size_t>(i)] = 0.86f + 0.16f * nz;
        holder->addChild(dn);
    }

    const int W = GRID * CELL;
    const int H = GRID * CELL;
    auto* rt = RenderTexture::create(W, H);
    if (!rt)
    {
        CCLOG("GameScene: RenderTexture failed for palette %d", paletteIndex);
        return;
    }
    rt->retain();
    _paletteSheets.push_back(rt);

    rt->beginWithClear(0.f, 0.f, 0.f, 0.f);
    holder->visit();
    rt->end();
    Director::getInstance()->getRenderer()->render();

    Texture2D* tex = rt->getSprite()->getTexture();
    tex->setAntiAliasTexParameters();

    _framesByPalette[static_cast<size_t>(paletteIndex)].clear();
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        int col = i % GRID;
        int row = i / GRID;
        Rect rect(col * CELL, row * CELL, CELL, CELL);
        auto sf = SpriteFrame::createWithTexture(tex, rect);
        _framesByPalette[static_cast<size_t>(paletteIndex)].pushBack(sf);
    }
}

void GameScene::buildPaletteSpriteSheets()
{
    for (int p = 0; p < NUM_PALETTES; ++p)
        bakePaletteToTexture(kAllPalettes[p], p);
}

void GameScene::drawPathGizmo()
{
    _pathGizmo = DrawNode::create();
    _pathGizmo->setCascadeOpacityEnabled(true);
    addChild(_pathGizmo, -50);
    refreshPathGizmo();
}

void GameScene::refreshPathGizmo()
{
    if (!_pathGizmo || _perimeter <= 0.f)
        return;

    _pathGizmo->clear();

    const Color4F lineCol(1.f, 0.88f, 0.08f, 0.5f);
    const Color4F arrowCol(1.f, 0.92f, 0.12f, 0.5f);

    const int N = 220;
    Vec2 prev = positionAt(0.f);
    for (int i = 1; i <= N; ++i)
    {
        float s = (float)i / (float)N * _perimeter;
        Vec2 q = positionAt(s);
        _pathGizmo->drawSegment(prev, q, 2.0f, lineCol);
        prev = q;
    }

    const int numArrows = 32;
    const float arrowLen = std::max(5.5f, _perimeter * 0.011f);
    const float wing = arrowLen * 0.38f;

    for (int k = 0; k < numArrows; ++k)
    {
        float s = wrapS((float)k / (float)numArrows * _perimeter + _pathGizmoPhase);
        Vec2 pos = positionAt(s);
        Vec2 t = tangentAt(s);
        float tl = t.length();
        if (tl < 1e-4f)
            continue;
        t *= (1.f / tl);
        Vec2 n(-t.y, t.x);
        Vec2 tip = pos + t * arrowLen;
        Vec2 b1 = pos - t * (arrowLen * 0.42f) + n * wing;
        Vec2 b2 = pos - t * (arrowLen * 0.42f) - n * wing;
        _pathGizmo->drawTriangle(tip, b1, b2, arrowCol);
    }
}

void GameScene::trySpawnCube(float arc, float targetCruise, int paletteIndex)
{
    if ((int)_cubes.size() >= kMaxCubes)
        return;
    if (paletteIndex < 0 || paletteIndex >= NUM_PALETTES)
        paletteIndex = 0;
    if (_framesByPalette[static_cast<size_t>(paletteIndex)].size() != CubeSprite::FRAME_COUNT)
        return;

    CubeSprite* cube = CubeSprite::create(_framesByPalette[static_cast<size_t>(paletteIndex)], _depthScales, _collisionRadius);
    if (!cube)
        return;

    cube->setTargetCruiseSpeed(targetCruise);
    cube->setPathSpeed(targetCruise * cocos2d::random(0.96f, 1.04f));
    cube->setWorldScaleUniform(_bgUniformScale);
    float capIn = 0.f, capOut = 0.f;
    lateralCapsAt(arc, &capIn, &capOut);
    capIn *= 0.92f;
    capOut *= 0.92f;
    Vec2 p0 = roundedRectLocalPos(arc);
    Vec2 t = roundedRectLocalTangent(arc);
    t.normalize();
    Vec2 n(-t.y, t.x);
    Vec2 toCenter = -p0;
    if (toCenter.lengthSquared() < 1e-4f)
        toCenter = n;
    else
        toCenter.normalize();
    float lo, hi;
    if (Vec2::dot(n, toCenter) >= 0.f)
    {
        lo = -capOut;
        hi = capIn;
    }
    else
    {
        lo = -capIn;
        hi = capOut;
    }
    cube->setLateralOffset(cocos2d::random(lo, hi));
    cube->setLateralVel(cocos2d::random(-22.f, 22.f));
    cube->seedRollPhase(cocos2d::random(0.f, TWO_PI));
    cube->seedVisualHeading(atan2f(t.y, t.x));

    _arc.push_back(wrapS(arc));
    _cubes.push_back(cube);
    addChild(cube, 10);
}

void GameScene::refreshHintText()
{
    if (!_hint)
        return;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%d cubes — tap or click to add 1 (max %d)", (int)_cubes.size(), kMaxCubes);
    _hint->setString(buf);
}

void GameScene::spawnInitialCubes()
{
    std::vector<int> palettes;
    palettes.reserve(30);
    for (int c = 0; c < NUM_PALETTES; ++c)
        for (int k = 0; k < 6; ++k)
            palettes.push_back(c);

    for (int i = (int)palettes.size() - 1; i >= 0; --i)
    {
        int j = cocos2d::random<int>(0, i);
        std::swap(palettes[static_cast<size_t>(i)], palettes[static_cast<size_t>(j)]);
    }

    // Along-path spacing: as large as feasible for 30 cubes on the loop (≤ perimeter/30 between neighbors).
    const int kInitialCount = 30;
    float targetGap = std::max(_collisionRadius * 3.1f, _perimeter * 0.028f);
    float maxFeasible = _perimeter / (float)kInitialCount * 0.94f;
    float minChord = std::min(
        std::max(_collisionRadius * 2.15f, std::min(targetGap, maxFeasible)),
        maxFeasible);
    std::vector<float> arcs = scatterArcPositions(kInitialCount, minChord);

    for (int i = 0; i < kInitialCount; ++i)
    {
        float cruise = _baseCruise * cocos2d::random(0.96f, 1.04f);
        trySpawnCube(arcs[static_cast<size_t>(i)], cruise, palettes[static_cast<size_t>(i)]);
    }
}

void GameScene::resolveCollisions()
{
    const int n = (int)_cubes.size();
    const float tangentialFriction = 0.035f;
    const float normalImpulseScale = 1.45f;
    /** Slightly past touching; keep small so collisions don’t fire before cubes look close. */
    const float kSoftShell = 1.04f;
    const float restingRepel = 11.f;
    for (int pass = 0; pass < 8; ++pass)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = i + 1; j < n; ++j)
            {
                CubeSprite* a = _cubes[i];
                CubeSprite* b = _cubes[j];
                Vec2 pa = a->getPosition();
                Vec2 pb = b->getPosition();
                float rSum = a->getCollisionRadius() + b->getCollisionRadius();
                float rContact = rSum * kSoftShell;
                Vec2 d = pb - pa;
                float dist2 = d.lengthSquared();
                if (dist2 > rContact * rContact || dist2 < 1e-5f)
                    continue;

                float dist = sqrtf(dist2);
                Vec2 nrm = d * (1.f / dist);
                float penHard = rSum - dist;
                float penSoft = 0.f;
                if (penHard < 0.f)
                    penSoft = rContact - dist;
                float sep = penHard > 0.f ? penHard * 0.5f : penSoft * 0.42f;
                pa -= nrm * sep;
                pb += nrm * sep;

                float si = 0.f, sj = 0.f, latA = 0.f, latB = 0.f;
                reprojectWorldToPath(pa, &si, &latA);
                reprojectWorldToPath(pb, &sj, &latB);
                _arc[i] = si;
                _arc[j] = sj;
                a->setLateralOffset(latA);
                b->setLateralOffset(latB);
                a->setPosition(positionAtLateral(si, latA));
                b->setPosition(positionAtLateral(sj, latB));

                Vec2 ta = tangentAt(si);
                Vec2 tb = tangentAt(sj);
                a->setPathTangent(ta);
                b->setPathTangent(tb);

                Vec2 va = a->getVelocity();
                Vec2 vb = b->getVelocity();
                float relN = Vec2::dot(va - vb, nrm);
                float jn = 0.f;
                if (relN > 1e-5f)
                {
                    jn = -(1.f + _restitution) * relN * 0.5f * normalImpulseScale;
                }
                else
                {
                    float drive = (penHard > 0.f) ? penHard : (penSoft * 0.55f);
                    if (drive > 1e-4f)
                        jn = -drive * restingRepel;
                }
                va += nrm * jn;
                vb -= nrm * jn;

                Vec2 tang(-nrm.y, nrm.x);
                float relT = Vec2::dot(va - vb, tang);
                float jt = -relT * tangentialFriction * 0.5f;
                va += tang * jt;
                vb -= tang * jt;

                a->applyVelocity(va);
                b->applyVelocity(vb);

                float impMag = fabsf(jn);
                if (impMag > 1e-4f)
                {
                    a->boostRollSpinFromCollision(impMag);
                    b->boostRollSpinFromCollision(impMag);
                }
            }
        }
    }
}

void GameScene::update(float dt)
{
    const int n = (int)_cubes.size();
    float avgBelt = _baseCruise;
    if (n > 0)
    {
        float sum = 0.f;
        for (int i = 0; i < n; ++i)
            sum += _cubes[i]->getPathSpeed();
        avgBelt = sum / (float)n;
    }
    updateBeltVisual(dt, avgBelt);

    if (_pathGizmo)
    {
        // Same arc-length rate as cubes (avg pathSpeed); arrows march with the belt.
        _pathGizmoPhase = wrapS(_pathGizmoPhase + avgBelt * dt);
        refreshPathGizmo();
    }

    if (n == 0)
        return;

    for (int i = 0; i < n; ++i)
        _cubes[i]->setBeltCarrySpeed(_baseCruise);

    const int substeps = 3;
    const float h = dt / (float)substeps;
    const float pathMaxAccel = 210.f;

    for (int step = 0; step < substeps; ++step)
    {
        _swayPhase += h * 2.1f;
        // Lateral: damping + shared sway only — no spring toward lat=0 or cubes drift to the centerline.
        const float latDamp = 6.5f;
        const float swayAmp = 26.f;

        for (int i = 0; i < n; ++i)
        {
            CubeSprite* c = _cubes[i];
            float capIn = 0.f, capOut = 0.f;
            lateralCapsAt(_arc[i], &capIn, &capOut);
            float lat = c->getLateralOffset();
            float lv = c->getLateralVel();
            float sway = swayAmp * sinf(_swayPhase + (float)i * 0.73f);
            float acc = -latDamp * lv + sway;
            lv += acc * h;
            lat += lv * h;
            Vec2 p0 = roundedRectLocalPos(_arc[i]);
            Vec2 t = roundedRectLocalTangent(_arc[i]);
            t.normalize();
            Vec2 n(-t.y, t.x);
            Vec2 toCenter = -p0;
            if (toCenter.lengthSquared() < 1e-4f)
                toCenter = n;
            else
                toCenter.normalize();
            if (Vec2::dot(n, toCenter) >= 0.f)
            {
                if (lat > capIn)
                {
                    lat = capIn;
                    lv *= -0.35f;
                }
                else if (lat < -capOut)
                {
                    lat = -capOut;
                    lv *= -0.35f;
                }
            }
            else
            {
                if (lat > capOut)
                {
                    lat = capOut;
                    lv *= -0.35f;
                }
                else if (lat < -capIn)
                {
                    lat = -capIn;
                    lv *= -0.35f;
                }
            }
            c->setLateralOffset(lat);
            c->setLateralVel(lv);
        }

        for (int i = 0; i < n; ++i)
        {
            CubeSprite* c = _cubes[i];
            _arc[i] = wrapS(_arc[i] + c->getPathSpeed() * h);
        }

        for (int i = 0; i < n; ++i)
        {
            CubeSprite* c = _cubes[i];
            float lat = c->getLateralOffset();
            c->setPosition(positionAtLateral(_arc[i], lat));
            c->setPathTangent(tangentAt(_arc[i]));
        }

        resolveCollisions();

        const float kMaxPathSpeed = _baseCruise * 3.5f;
        const float kMaxLateralVel = 240.f;
        for (int i = 0; i < n; ++i)
            _cubes[i]->clampSpeeds(kMaxPathSpeed, kMaxLateralVel);

        // Motor toward cruise after contacts so impulses are not erased at the start of the next substep.
        for (int i = 0; i < n; ++i)
            _cubes[i]->integratePathSpeed(h, pathMaxAccel);

        for (int i = 0; i < n; ++i)
        {
            CubeSprite* c = _cubes[i];
            c->integrateCornerRoll(cornerArcWeight(_arc[i]), c->getPathSpeed(), h);
            c->tick(h);
        }
    }
}

bool GameScene::onTouchBegan(Touch* /*t*/, Event* /*e*/)
{
    int frame = Director::getInstance()->getTotalFrames();
    if (frame - _lastPointerSpawnFrame < kSpawnPointerDebounceFrames)
        return true;
    _lastPointerSpawnFrame = frame;

    float touchGap = std::max(_collisionRadius * 2.6f, _perimeter * 0.02f);
    for (int k = 0; k < kCubesPerTap && (int)_cubes.size() < kMaxCubes; ++k)
    {
        int pal = cocos2d::random<int>(0, NUM_PALETTES - 1);
        float arc = pickSpawnArcWithMinGap(touchGap);
        trySpawnCube(arc, _baseCruise * cocos2d::random(0.96f, 1.04f), pal);
    }
    refreshHintText();
    return true;
}
