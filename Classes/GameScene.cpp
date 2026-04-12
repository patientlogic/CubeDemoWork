#include "GameScene.h"

#include "base/ccRandom.h"
#include "math/Vec3.h"
#include "2d/CCRenderTexture.h"

#include <algorithm>
#include <cmath>

USING_NS_CC;

namespace
{
const float PI = 3.14159265f;
const float TWO_PI = PI * 2.f;

inline float smooth01(float t)
{
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    return t * t * (3.f - 2.f * t);
}

inline Vec3 rotY(const Vec3& v, float yaw)
{
    float c = cosf(yaw);
    float s = sinf(yaw);
    return Vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}
}

Scene* GameScene::createScene()
{
    return GameScene::create();
}

void GameScene::onExit()
{
    Scene::onExit();
    CC_SAFE_RELEASE_NULL(_sheetRT);
}

bool GameScene::init()
{
    if (!Scene::init())
        return false;

    buildDefaultPath();
    buildSpriteSheetAndFrames();
    drawPathGizmo();

    _hint = Label::createWithSystemFont("Tap / click: add cube (max 16)", "Arial", 15);
    if (_hint)
    {
        _hint->setAnchorPoint(Vec2::ZERO);
        _hint->setPosition(Vec2(10.f, 10.f));
        _hint->setColor(Color3B::WHITE);
        _hint->enableOutline(Color4B(0, 0, 0, 200), 1);
        addChild(_hint, 100);
    }

    spawnInitialCubes();

    auto listener = EventListenerTouchOneByOne::create();
    listener->setSwallowTouches(true);
    listener->onTouchBegan = CC_CALLBACK_2(GameScene::onTouchBegan, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

    scheduleUpdate();
    return true;
}

void GameScene::buildDefaultPath()
{
    auto vs = Director::getInstance()->getVisibleSize();
    auto vo = Director::getInstance()->getVisibleOrigin();
    Vec2 center = vo + Vec2(vs.width * 0.5f, vs.height * 0.5f);

    float hw = vs.width * 0.38f;
    float hh = vs.height * 0.36f;
    float skewX = 55.f;
    float skewY = 40.f;

    _path.clear();
    // Counter‑clockwise custom quadrilateral (mildly skewed rectangle).
    _path.push_back(Vec2(center.x - hw, center.y - hh));
    _path.push_back(Vec2(center.x + hw, center.y - hh));
    _path.push_back(Vec2(center.x + hw + skewX * 0.25f, center.y + hh));
    _path.push_back(Vec2(center.x - hw + skewX * 0.1f, center.y + hh + skewY * 0.2f));

    _edgeLen.resize(4);
    _perimeter = 0.f;
    for (int i = 0; i < 4; i++)
    {
        float len = _path[(i + 1) % 4].distance(_path[i]);
        _edgeLen[i] = len;
        _perimeter += len;
    }
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

void GameScene::locateOnPath(float s, int* segIndex, float* localT) const
{
    s = wrapS(s);
    float acc = 0.f;
    for (int i = 0; i < 4; i++)
    {
        float L = _edgeLen[i];
        if (s < acc + L || i == 3)
        {
            *segIndex = i;
            *localT = (L > 0.0001f) ? ((s - acc) / L) : 0.f;
            if (*localT < 0.f)
                *localT = 0.f;
            if (*localT > 1.f)
                *localT = 1.f;
            return;
        }
        acc += L;
    }
    *segIndex = 3;
    *localT = 1.f;
}

Vec2 GameScene::positionAt(float s) const
{
    int seg = 0;
    float t = 0.f;
    locateOnPath(s, &seg, &t);
    Vec2 a = _path[seg];
    Vec2 b = _path[(seg + 1) % 4];
    return a + (b - a) * t;
}

Vec2 GameScene::tangentAt(float s) const
{
    int seg = 0;
    float t = 0.f;
    locateOnPath(s, &seg, &t);

    const Vec2& a = _path[seg];
    const Vec2& b = _path[(seg + 1) % 4];
    const Vec2& c = _path[(seg + 2) % 4];
    const Vec2& prev = _path[(seg + 3) % 4];

    Vec2 t0 = (b - a);
    t0.normalize();
    Vec2 t1 = (c - b);
    t1.normalize();

    float L = _edgeLen[seg];
    float distFromStart = t * L;
    float distToEnd = (1.f - t) * L;

    if (distFromStart < _cornerBlend)
    {
        Vec2 tIncoming = (a - prev);
        tIncoming.normalize();
        float alpha = smooth01(distFromStart / _cornerBlend);
        Vec2 tv = tIncoming * (1.f - alpha) + t0 * alpha;
        tv.normalize();
        return tv;
    }
    if (distToEnd < _cornerBlend)
    {
        float alpha = smooth01(distToEnd / _cornerBlend);
        Vec2 tv = t1 * (1.f - alpha) + t0 * alpha;
        tv.normalize();
        return tv;
    }
    return t0;
}

float GameScene::projectPointToArc(const Vec2& p) const
{
    float bestS = 0.f;
    float bestD2 = 1.e20f;
    float acc = 0.f;
    for (int i = 0; i < 4; i++)
    {
        const Vec2& a = _path[i];
        const Vec2& b = _path[(i + 1) % 4];
        Vec2 ab = b - a;
        float len2 = ab.lengthSquared();
        float tt = Vec2::dot(p - a, ab) / std::max(len2, 0.0001f);
        if (tt < 0.f)
            tt = 0.f;
        else if (tt > 1.f)
            tt = 1.f;
        Vec2 q = a + ab * tt;
        float d2 = q.distanceSquared(p);
        if (d2 < bestD2)
        {
            bestD2 = d2;
            float segLen = sqrtf(len2);
            bestS = acc + tt * segLen;
        }
        acc += _edgeLen[i];
    }
    return wrapS(bestS);
}

void GameScene::drawCubeToNode(DrawNode* dn, float yaw, float cx, float cy, float half, float* outDepth)
{
    static const int faces[6][4] = {
        {4, 5, 6, 7},
        {0, 3, 2, 1},
        {3, 2, 6, 7},
        {0, 4, 5, 1},
        {1, 2, 6, 5},
        {0, 3, 7, 4}
    };

    static const Vec3 nraw[6] = {
        Vec3(0, 0, 1), Vec3(0, 0, -1), Vec3(0, 1, 0), Vec3(0, -1, 0), Vec3(1, 0, 0), Vec3(-1, 0, 0)
    };

    static const Color4F cols[6] = {
        Color4F(0.72f, 0.55f, 0.95f, 1.f), Color4F(0.36f, 0.22f, 0.58f, 1.f),
        Color4F(0.92f, 0.82f, 1.0f, 1.f), Color4F(0.30f, 0.17f, 0.46f, 1.f),
        Color4F(0.58f, 0.38f, 0.90f, 1.f), Color4F(0.42f, 0.27f, 0.72f, 1.f)
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
        Vec3 rn = rotY(nraw[f], yaw);
        if (Vec3::dot(rn, Vec3(0.f, 0.f, 1.f)) <= 0.02f)
            continue;
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
        dn->drawPolygon(poly, 4, cols[f], 1.2f, Color4F(0.08f, 0.04f, 0.15f, 0.65f));
    }
}

void GameScene::buildSpriteSheetAndFrames()
{
    _depthScales.resize(NUM_FRAMES);

    auto holder = Node::create();
    holder->setAnchorPoint(Vec2::ZERO);
    holder->setPosition(Vec2::ZERO);

    const float drawHalf = 26.f;

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        auto dn = DrawNode::create();
        float yaw = (float)i / (float)NUM_FRAMES * TWO_PI;
        int col = i % GRID;
        int row = i / GRID;
        float cx = col * CELL + CELL * 0.5f;
        float cy = row * CELL + CELL * 0.5f;
        float depthZ = 0.f;
        drawCubeToNode(dn, yaw, cx, cy, drawHalf, &depthZ);
        float nz = (depthZ / 1.414f + 1.f) * 0.5f;
        _depthScales[static_cast<size_t>(i)] = 0.86f + 0.16f * nz;
        holder->addChild(dn);
    }

    const int W = GRID * CELL;
    const int H = GRID * CELL;

    CC_SAFE_RELEASE_NULL(_sheetRT);
    _sheetRT = RenderTexture::create(W, H);
    if (!_sheetRT)
    {
        CCLOG("GameScene: RenderTexture create failed");
        return;
    }
    _sheetRT->retain();

    _sheetRT->beginWithClear(0.f, 0.f, 0.f, 0.f);
    holder->visit();
    _sheetRT->end();

    Director::getInstance()->getRenderer()->render();

    Texture2D* tex = _sheetRT->getSprite()->getTexture();
    tex->setAntiAliasTexParameters();

    _frames.clear();
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        int col = i % GRID;
        int row = i / GRID;
        Rect rect(col * CELL, row * CELL, CELL, CELL);
        auto sf = SpriteFrame::createWithTexture(tex, rect);
        _frames.pushBack(sf);
    }
}

void GameScene::drawPathGizmo()
{
    _pathGizmo = DrawNode::create();
    for (int i = 0; i < 4; i++)
    {
        Vec2 a = _path[i];
        Vec2 b = _path[(i + 1) % 4];
        _pathGizmo->drawSegment(a, b, 2.2f, Color4F(0.22f, 0.32f, 0.5f, 0.45f));
    }
    addChild(_pathGizmo, -5);
}

void GameScene::trySpawnCube(float arc, float targetCruise)
{
    if ((int)_cubes.size() >= _maxCubes)
        return;
    if (_frames.size() != CubeSprite::FRAME_COUNT)
        return;

    CubeSprite* cube = CubeSprite::create(_frames, _depthScales, _collisionRadius);
    if (!cube)
        return;

    cube->setTargetCruiseSpeed(targetCruise);
    cube->setPathSpeed(targetCruise * cocos2d::random(0.92f, 1.08f));

    _arc.push_back(wrapS(arc));
    _cubes.push_back(cube);
    addChild(cube, 10);
}

void GameScene::spawnInitialCubes()
{
    const int starter = 5;
    for (int i = 0; i < starter; ++i)
    {
        float frac = (float)i / (float)starter;
        float cruise = _baseCruise * cocos2d::random(0.88f, 1.12f);
        trySpawnCube(frac * _perimeter, cruise);
    }
}

void GameScene::resolveCollisions()
{
    const int n = (int)_cubes.size();
    for (int pass = 0; pass < 4; ++pass)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = i + 1; j < n; ++j)
            {
                CubeSprite* a = _cubes[i];
                CubeSprite* b = _cubes[j];
                Vec2 pa = a->getPosition();
                Vec2 pb = b->getPosition();
                float r = a->getCollisionRadius() + b->getCollisionRadius();
                Vec2 d = pb - pa;
                float dist2 = d.lengthSquared();
                if (dist2 > r * r || dist2 < 1e-5f)
                    continue;

                float dist = sqrtf(dist2);
                Vec2 n = d * (1.f / dist);
                float pen = r - dist;
                pa -= n * (pen * 0.5f);
                pb += n * (pen * 0.5f);

                _arc[i] = projectPointToArc(pa);
                _arc[j] = projectPointToArc(pb);
                a->setPosition(positionAt(_arc[i]));
                b->setPosition(positionAt(_arc[j]));

                Vec2 ta = tangentAt(_arc[i]);
                Vec2 tb = tangentAt(_arc[j]);
                a->setPathTangent(ta);
                b->setPathTangent(tb);

                Vec2 va = ta * a->getPathSpeed();
                Vec2 vb = tb * b->getPathSpeed();
                float relN = Vec2::dot(va - vb, n);
                if (relN < 0.f)
                {
                    float j = -(1.f + _restitution) * relN * 0.5f;
                    va += n * j;
                    vb -= n * j;
                }
                a->applyVelocity(va);
                b->applyVelocity(vb);
            }
        }
    }
}

void GameScene::update(float dt)
{
    const int n = (int)_cubes.size();
    if (n == 0)
        return;

    const int substeps = 2;
    const float h = dt / (float)substeps;

    for (int step = 0; step < substeps; ++step)
    {
        for (int i = 0; i < n; ++i)
        {
            CubeSprite* c = _cubes[i];
            _arc[i] = wrapS(_arc[i] + c->getPathSpeed() * h);
        }

        for (int i = 0; i < n; ++i)
        {
            CubeSprite* c = _cubes[i];
            c->setPosition(positionAt(_arc[i]));
            c->setPathTangent(tangentAt(_arc[i]));
        }

        resolveCollisions();

        for (int i = 0; i < n; ++i)
            _cubes[i]->tick(h);
    }
}

bool GameScene::onTouchBegan(Touch* /*t*/, Event* /*e*/)
{
    trySpawnCube(cocos2d::random(0.f, _perimeter), _baseCruise * cocos2d::random(0.9f, 1.1f));
    return true;
}
