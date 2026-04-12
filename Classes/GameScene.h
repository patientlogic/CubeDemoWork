/****************************************************************************
 * GameScene — track background, rounded‑rect path, colored cube sheets,
 * collisions, touch to spawn extra cubes.
 ****************************************************************************/

#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include "cocos2d.h"
#include "CubeSprite.h"
#include <array>
#include <vector>

class GameScene : public cocos2d::Scene
{
public:
    static cocos2d::Scene* createScene();

    CREATE_FUNC(GameScene);

    bool init() override;
    void update(float dt) override;
    void onExit() override;

private:
    static constexpr int GRID = 6;
    static constexpr int NUM_FRAMES = 36;
    static constexpr int CELL = 72;
    static constexpr int NUM_PALETTES = 5;
    static constexpr int kMaxCubes = 200;
    /** Cubes spawned per tap/click (each adds one batch until kMaxCubes). */
    static constexpr int kCubesPerTap = 1;
    /** Ignore repeated pointer events within this many frames (guards duplicate Win32 delivery). */
    static constexpr int kSpawnPointerDebounceFrames = 12;

    /** Background sprite (track art); path lives in its local space then scaled. */
    cocos2d::Sprite* _bgSprite = nullptr;
    cocos2d::Vec2 _bgCenter {0.f, 0.f};
    float _bgUniformScale = 1.f;

    /**
     * Rounded track in background local space (anchor center, y up).
     * _rrHalfW = half-width of top/bottom straights (outer, matches yellow there).
     * _rrHalfWSide = |x| of left/right verticals (inset < _rrHalfW pulls sides toward center / yellow).
     */
    float _rrHalfW = 100.f;
    float _rrHalfWSide = 88.f;
    float _rrHalfH = 100.f;
    float _rrCornerR = 30.f;
    float _perimeter = 0.f;
    /**
     * Geometric half-width of blue track band in bg local space (before visual margin + edge margin).
     * maxLateralLocal() = _laneHalfLocal - _lateralVisualHalfLocal - kEdgeMarginLocal.
     */
    float _laneHalfLocal = 24.f;
    /** Bg-local half-width of cube silhouette vs path normal (physics radius is tighter). */
    float _lateralVisualHalfLocal = 12.f;
    /** Extra lateral (toward hole center only) so play bounds match inner blue band vs outer edge. */
    float _innerLaneExtraLocal = 0.f;
    /** Top straight extra on capInner (typically 0; inner inset uses _topInnerPullLocal). */
    float _upperLaneInnerInwardLocal = 0.f;
    /** Top straight extra on capOuter (~px toward outside vs blue). */
    float _upperLaneOuterOutwardLocal = 0.f;
    /** Bottom straight only: pull inner edge slightly toward screen bottom to match inner blue art. */
    float _bottomInnerPullLocal = 0.f;
    /** Top straight only: pull inner edge toward screen top (~px inset vs inner blue art). */
    float _topInnerPullLocal = 0.f;
    /** NW/NE/SW/SE arcs: same ~screen-px inset vs blue (set in setupBackgroundAndPath). */
    float _nwCornerInsetLocal = 0.f;
    float _neCornerInsetLocal = 0.f;
    float _swCornerInsetLocal = 0.f;
    float _seCornerInsetLocal = 0.f;

    std::array<cocos2d::Vector<cocos2d::SpriteFrame*>, NUM_PALETTES> _framesByPalette;
    std::vector<float> _depthScales;
    std::vector<cocos2d::RenderTexture*> _paletteSheets;

    std::vector<CubeSprite*> _cubes;
    std::vector<float> _arc;

    /** World-space; assigned in setupBackgroundAndPath() from baked cube art. */
    float _collisionRadius = 12.f;
    /** >1 exaggerates separation impulse so cube–cube hits read clearly (arcade). */
    float _restitution = 1.12f;
    float _baseCruise = 125.f;

    cocos2d::DrawNode* _pathGizmo = nullptr;
    /** Scrolls arrow markers along the path (matches forward travel sense). */
    float _pathGizmoPhase = 0.f;
    cocos2d::DrawNode* _beltOverlay = nullptr;
    cocos2d::Label* _hint = nullptr;
    int _lastPointerSpawnFrame = -1000000;

    float _swayPhase = 0.f;

    /** Conveyor belt texture phase (arc-length units) for stripes; background UV offset. */
    float _beltPhase = 0.f;
    float _beltTexOffX = 0.f;
    float _beltTexOffY = 0.f;
    cocos2d::Size _bgTexSize {0.f, 0.f};
    bool _bgTexScrollEnabled = false;

    void setupBackgroundAndPath();
    void setupBeltVisual();
    void buildPaletteSpriteSheets();

    static void drawCubeToNode(cocos2d::DrawNode* dn, float yaw, float cx, float cy, float half, float* outDepth,
                               const cocos2d::Color4F* faceColors);

    void bakePaletteToTexture(const cocos2d::Color4F* faceColors, int paletteIndex);

    /** Local point (bg space) → world / scene position. */
    cocos2d::Vec2 localToWorld(const cocos2d::Vec2& local) const;
    /** World → bg local. */
    cocos2d::Vec2 worldToLocal(const cocos2d::Vec2& world) const;

    float wrapS(float s) const;

    /** Arc length s → position on rounded rect, local space. */
    cocos2d::Vec2 roundedRectLocalPos(float s) const;
    cocos2d::Vec2 roundedRectLocalTangent(float s) const;

    cocos2d::Vec2 positionAt(float s) const;
    cocos2d::Vec2 tangentAt(float s) const;
    /** World position on path + lateral offset (local units) along path left normal. */
    cocos2d::Vec2 positionAtLateral(float s, float lateralLocal) const;
    float projectPointToArc(const cocos2d::Vec2& worldPos) const;
    /** Recover arc s and lateral offset from a world position near the track. */
    void reprojectWorldToPath(const cocos2d::Vec2& world, float* outS, float* outLateral) const;
    /** Minimum lateral half-width (straights); see lateralCapsAt(s) for corner-flexible caps. */
    float maxLateralLocal() const;
    /** Toward hole vs outward from hole; corners use smoother ramps + slight outer inset. */
    void lateralCapsAt(float s, float* capInner, float* capOuter) const;

    std::vector<float> scatterArcPositions(int count, float minChord) const;
    float wrappedChordDistance(float a, float b) const;

    void spawnInitialCubes();
    void trySpawnCube(float arc, float targetCruise, int paletteIndex);
    void refreshHintText();
    /** Random arc at least minChord away from every existing cube (wrapped). */
    float pickSpawnArcWithMinGap(float minChord) const;

    void resolveCollisions();
    void drawPathGizmo();
    void refreshPathGizmo();
    void updateBeltVisual(float dt, float avgBeltSpeed);

    bool onTouchBegan(cocos2d::Touch* t, cocos2d::Event* e);

    /** 0..1 smooth weight: 1 on rounded-rect corner arcs (La segments), 0 on straights. */
    float cornerArcWeight(float s) const;
    /** 0..1 on top straight only (not corner arcs) so upper corner radii match lower corners vs art. */
    float upperRegionWeight(float s) const;
    /** 0..1 on bottom horizontal straight [0,c1); smooth at ends. */
    float bottomHorizontalWeight(float s) const;
    /** 0..1 on NW quarter arc [c5,c6) only; smooth at junctions with top/left straights. */
    float northwestCornerWeight(float s) const;
    /** 0..1 on NE quarter arc [c3,c4) only; smooth at junctions with top/right straights. */
    float northeastCornerWeight(float s) const;
    /** 0..1 on SW quarter arc [c7,perimeter); smooth at junctions with left/bottom straights. */
    float southwestCornerWeight(float s) const;
    /** 0..1 on SE quarter arc [c1,c2); smooth at junctions with bottom/right straights. */
    float southeastCornerWeight(float s) const;
};

#endif // __GAME_SCENE_H__
