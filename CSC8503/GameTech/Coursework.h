#pragma once
#include "GameTechRenderer.h"
#include "TutorialGame.h"
#include "../CSC8503Common/PhysicsSystem.h"


namespace NCL {
    namespace CSC8503 {
        class Coursework : public TutorialGame {
        public:
            Coursework();
            ~Coursework();
        protected:
            void LoadObstacles(string levelName);
        };
    }
}
