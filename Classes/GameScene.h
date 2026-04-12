/****************************************************************************
 * GameScene — quadrilateral path, procedural6×6 sprite sheet, cube flock,
 * circle collisions, and touch / click to spawn extra cubes.
 ****************************************************************************/

#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include "cocos2d.h"
#include "CubeSprite.h"
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

    // Closed path (4 corners, counter‑clockwise in design space).
    std::vector<cocos2d::Vec2> _path;
    std::vector<float> _edgeLen;
    float _perimeter = 0.f;

    /** Distance from a corner over which tangents are blended (smooth turns). */
    float _cornerBlend = 38.f;

    cocos2d::Vector<cocos2d::SpriteFrame*> _frames;
    std::vector<float> _depthScales;

    std::vector<CubeSprite*> _cubes;
    /** Arc‑length parameter for each cube on [0, _perimeter). */
    std::vector<float> _arc;

    float _collisionRadius = 20.f;
    float _restitution = 0.84f;
    int _maxCubes = 16;
    float _baseCruise = 135.f;

    cocos2d::DrawNode* _pathGizmo = nullptr;
    cocos2d::Label* _hint = nullptr;

    /** Keeps GPU atlas alive for sprite frames (procedural sheet). */
    cocos2d::RenderTexture* _sheetRT = nullptr;

    void buildDefaultPath();
    void buildSpriteSheetAndFrames();

    /** Procedurally draws one yaw step into a DrawNode; fills depth hint for scaling. */
    static void drawCubeToNode(cocos2d::DrawNode* dn, float yaw, float cx, float cy, float half, float* outDepth);

    float wrapS(float s) const;
    void locateOnPath(float s, int* segIndex, float* localT) const;
    cocos2d::Vec2 positionAt(float s) const;
    cocos2d::Vec2 tangentAt(float s) const;
    float projectPointToArc(const cocos2d::Vec2& p) const;

    void spawnInitialCubes();
    void trySpawnCube(float arc, float targetCruise);

    void resolveCollisions();
    void drawPathGizmo();

    bool onTouchBegan(cocos2d::Touch* t, cocos2d::Event* e);
};

#endif // __GAME_SCENE_H__
