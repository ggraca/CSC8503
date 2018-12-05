#include "Coursework.h"
#include "../CSC8503Common/GameWorld.h"
#include "../../Plugins/OpenGLRendering/OGLMesh.h"
#include "../../Plugins/OpenGLRendering/OGLShader.h"
#include "../../Plugins/OpenGLRendering/OGLTexture.h"
#include "../../Common/TextureLoader.h"

#include "../CSC8503Common/PositionConstraint.h"

using namespace NCL;
using namespace CSC8503;


Coursework::Coursework() : TutorialGame() {
  physics->SetGravity(Vector3(0, -2000, 0));
    AddFloorToWorld(Vector3(10, -100, 1));
    AddSphereToWorld(Vector3(0, 10, 0), 10.0f, 10.0f);
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
