#pragma once
#include "GameTechRenderer.h"
#include "TutorialGame.h"
#include "../CSC8503Common/PhysicsSystem.h"

#include "../CSC8503Common/GameServer.h"
#include "../CSC8503Common/GameClient.h"
#include "NetworkedGame.h"


namespace NCL {
    namespace CSC8503 {
        struct Player {
            int wins = 0;
            int kills = 0;
            string name;
            GameObject* ball = nullptr;
        };

        class Coursework : public TutorialGame, public PacketReceiver {
        public:
            Coursework();
            ~Coursework();
            void UpdateGame(float dt);

            bool isServer = false;
            Player* me;
        protected:
            void ReceivePacket(int type, GamePacket* payload, int source);
            void ProcessClientMessage(string msg);
            void ProcessServerMessage(string msg);
            string SerializeState();
            string SerializePlay();
            void InitNetwork();
            void ResetWorld();
            void LoadLevel(string levelName);
            void UpdateInput(float dt);
            void AddGoalToWorld(Vector3 pos);
            void SpawnPlayer(Player* p, Vector3 pos);
            Player* FindOrCreatePlayer(string name);
            void Shoot(Player* p, Vector3 originalPos, float forceMagnitude);

            const float ballRadius = 3.0f;
            bool shooting;
            float shootingTimestamp;

            GameObject* goal;

            GameServer* server;
            GameClient* client;

            std::vector<Vector3> spawns;
            std::vector<Player*> players;
            std::vector<string> levels;
            int currentLevel = -1;

        };
    }
}
