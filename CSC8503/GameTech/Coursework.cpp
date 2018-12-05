#include "Coursework.h"
#include "../CSC8503Common/GameWorld.h"
#include "../../Plugins/OpenGLRendering/OGLMesh.h"
#include "../../Plugins/OpenGLRendering/OGLShader.h"
#include "../../Plugins/OpenGLRendering/OGLTexture.h"
#include "../../Common/TextureLoader.h"

#include "../CSC8503Common/PositionConstraint.h"
#include "../../Common/Assets.h"
#include <fstream>

using namespace NCL;
using namespace CSC8503;


Coursework::Coursework() : TutorialGame() {
    physics->SetGravity(Vector3(0, -2000, 0));

    // Floor
    AddFloorToWorld(Vector3(1, 0, 1));

    // TODO: Obstacles
    LoadObstacles("level1.txt");

    // Player
    AddSphereToWorld(Vector3(40, 20, 340), 3.0f, 3.0f);
}

void Coursework::LoadObstacles(string levelName) {
    int width, height, scale;
    char val;
    std::ifstream infile(Assets::DATADIR + levelName);

    scale = 10;
	infile >> width;
	infile >> height;

    for(int x = 0; x < width; x++) {
        for(int z = 0; z < height; z++) {
            infile >> val;

            if (val == '.') continue;
            else if (val == 'x') {
                AddCubeToWorld(
                    Vector3(x * scale * 2, scale * 2, z * scale * 2),
                    Vector3(scale, scale, scale),
                    1.0f
                );
            }
        }
    }
}

Coursework::~Coursework() {
    delete cubeMesh;
    delete sphereMesh;
    delete basicTex;
    delete basicShader;

    delete physics;
    delete renderer;
    delete world;
}
