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

    /** Unit tangent of the path at the cube (updated by GameScene). */
    void setPathTangent(const cocos2d::Vec2& t);
    cocos2d::Vec2 getPathTangent() const { return _pathTangent; }

    /** World-space velocity used for collision response (tangent * pathSpeed). */
    cocos2d::Vec2 getVelocity() const;

    /** Rebuild velocity from pathSpeed and current tangent. */
    void syncVelocityFromPath();

    /**
     * Apply a collision impulse in world space; result is projected onto
     * pathTangent to update pathSpeed (scalar along the path).
     */
    void applyVelocity(const cocos2d::Vec2& v);

    /**
     * Per-frame: cruise damping, roll integration, frame + scale update.
     * Call after GameScene sets position and tangent.
     */
    void tick(float dt);

private:
    cocos2d::Sprite* _sprite = nullptr;
    cocos2d::DrawNode* _shadow = nullptr;

    cocos2d::Vector<cocos2d::SpriteFrame*> _frames;
    std::vector<float> _depthScales;

    float _collisionRadius = 18.f;

    float _pathSpeed = 0.f;
    float _targetCruiseSpeed = 120.f;

    cocos2d::Vec2 _pathTangent {1.f, 0.f};

    /** Continuous roll phase (radians); drives sprite frame index. */
    float _rollPhase = 0.f;

    void updateRollAndSprite(float dt);
    void refreshShadowTransform();
};

#endif // __CUBE_SPRITE_H__
