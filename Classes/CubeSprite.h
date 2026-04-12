/****************************************************************************
 * CubeSprite — 2D sprite-sheet “fake 3D” cube with rolling animation,
 * optional shadow, circle collision radius, and scalar path speed with
 * inertia toward a cruise value.
 ****************************************************************************/

#ifndef __CUBE_SPRITE_H__
#define __CUBE_SPRITE_H__

#include "cocos2d.h"
#include <vector>

/**
 * Visual + lightweight physics state for one rolling cube.
 * The parent GameScene owns path integration; this class consumes tangent,
 * path speed, and dt to advance roll phase, pick sprite frames, and apply
 * depth scaling.
 */
class CubeSprite : public cocos2d::Node
{
public:
    /** Grid is 6×6 = 36 frames; must match GameScene sheet builder. */
    static const int FRAME_COUNT = 36;

    /**
     * @param frames   Sprite frames in row-major order (0..35).
     * @param scales   Per-frame fake-depth scale (same order as frames).
     * @param collisionRadius  Circle radius for overlap tests (design units).
     */
    static CubeSprite* create(const cocos2d::Vector<cocos2d::SpriteFrame*>& frames,
 const std::vector<float>& scales,
                              float collisionRadius);

    bool init(const cocos2d::Vector<cocos2d::SpriteFrame*>& frames,
              const std::vector<float>& scales,
              float collisionRadius);

    /** Arc-length speed along the path (positive = forward along tangent). */
    float getPathSpeed() const { return _pathSpeed; }
    void setPathSpeed(float v) { _pathSpeed = v; }

    /** Scene drags speed toward this value each frame (inertia / motor). */
    float getTargetCruiseSpeed() const { return _targetCruiseSpeed; }
    void setTargetCruiseSpeed(float v) { _targetCruiseSpeed = v; }

    float getCollisionRadius() const { return _collisionRadius; }

    /** Unit tangent of the path at the cube (updated by GameScene). Left normal is derived. */
    void setPathTangent(const cocos2d::Vec2& t);
    cocos2d::Vec2 getPathTangent() const { return _pathTangent; }
    cocos2d::Vec2 getPathNormal() const { return _pathNormal; }

    /** Lateral offset along path left normal in background local units (GameScene integrates). */
    float getLateralOffset() const { return _lateralOffset; }
    void setLateralOffset(float u) { _lateralOffset = u; }
    float getLateralVel() const { return _lateralVel; }
    void setLateralVel(float v) { _lateralVel = v; }

    /** Uniform bg scale so local lateral velocity maps to world (same as GameScene path). */
    void setWorldScaleUniform(float s) { _worldScaleUniform = (s > 1e-5f) ? s : 1.f; }

    /** Belt cruise in path-local units (same as pathSpeed); used for motor / diagnostics. */
    void setBeltCarrySpeed(float v) { _beltCarrySpeed = v; }

    /** World-space velocity: along-path + across-path (scaled from local lateral speed). */
    cocos2d::Vec2 getVelocity() const;

    /** Rebuild velocity from pathSpeed and current tangent. */
    void syncVelocityFromPath();

    /** Apply post-collision velocity in world space; splits into path and lateral (local). */
    void applyVelocity(const cocos2d::Vec2& v);

    /** Bump visual roll spin from impact (|normal impulse|); rolling accelerates on collision. */
    void boostRollSpinFromCollision(float normalImpulseMagnitude);

    /**
     * Call once per physics step with path corner weight [0,1] (0 on straights).
     * Adds impulsive spin when entering / ramping into corners — not continuous cruise roll.
     */
    void integrateCornerRoll(float cornerArcWeight, float pathSpeedLocal, float dt);

    /**
     * Accelerate path speed toward cruise with a max rate (units/s²) for natural pickup / slowdown.
     * Call before integrating arc length along the track.
     */
    void integratePathSpeed(float dt, float maxAccel);

    /** Prevent collision impulses from exploding path / lateral speeds (world/local units). */
    void clampSpeeds(float maxPathSpeed, float maxLateralVelLocal);

    /** Randomize roll phase so cubes are not visually synchronized. */
    void seedRollPhase(float radians) { _rollPhase = radians; }
    void seedVisualHeading(float radians) { _visualHeading = radians; }

    /**
     * Roll + sprite frame; path speed easing is handled by integratePathSpeed in GameScene.
     */
    void tick(float dt);

private:
    cocos2d::Sprite* _sprite = nullptr;
    cocos2d::DrawNode* _shadow = nullptr;

    cocos2d::Vector<cocos2d::SpriteFrame*> _frames;
    std::vector<float> _depthScales;

    float _collisionRadius = 9.f;

    float _pathSpeed = 0.f;
    float _targetCruiseSpeed = 120.f;

    cocos2d::Vec2 _pathTangent {1.f, 0.f};
    cocos2d::Vec2 _pathNormal {0.f, 1.f};

    float _lateralOffset = 0.f;
    float _lateralVel = 0.f;
    float _worldScaleUniform = 1.f;

    float _beltCarrySpeed = 0.f;

    /** Continuous roll phase (radians); drives sprite frame index. */
    float _rollPhase = 0.f;
    /** Visual spin rate (rad/s); only from corner/collision impulses, decays at rest. */
    float _rollSpinRate = 0.f;
    float _prevCornerRollWeight = 0.f;
    /** Smoothed travel heading (rad, CCW from +X); drives node rotation vs baked yaw strip. */
    float _visualHeading = 0.f;

    void updateRollAndSprite(float dt);
    void refreshShadowTransform();
};

#endif // __CUBE_SPRITE_H__
