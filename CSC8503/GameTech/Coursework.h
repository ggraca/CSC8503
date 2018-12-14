#pragma once
#include "GameTechRenderer.h"
#include "TutorialGame.h"
#include "../CSC8503Common/PhysicsSystem.h"

#include "../CSC8503Common/GameServer.h"
#include "../CSC8503Common/GameClient.h"
#include "NetworkedGame.h"

#include "../CSC8503Common/NavigationGrid.h"

enum BotState {
    Following,
    Returning,
    Patrolling,
    Dead // :(
};

namespace NCL {
    namespace CSC8503 {
        struct Player {
            int wins = 0;
            int kills = 0;
            string name;
            GameObject* ball = nullptr;
        };

        struct Bot {
            string name;
            GameObject* cube = nullptr;
            vector<Vector3> path;
            vector<Vector3> patrolSites;
            BotState state = Patrolling;
            float speed = 30.0f;
            float decisionCD = 0.2f;
            float decisionDT = decisionCD;
            float deadCD = 10;
            float deadDT = 0;
            int followDistance = 6;
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
            void UpdateBots(float dt);
            void LoadLevel(string levelName);
            void ServerInput();
            void ResetCamera();
            void UpdateInput(float dt);
            void AddGoalToWorld(Vector3 pos);
            void SpawnPlayer(Player* p, Vector3 pos);
            Player* FindOrCreatePlayer(string name);
            void Shoot(Player* p, Vector3 originalPos, float forceMagnitude);
            void AddBridgeToWorld(Vector3 pos);

            Bot* FindOrCreateBot(string name);
            void SpawnBot(Bot* b, Vector3 pos);
            vector<Vector3> FindPath(Bot* b, Vector3 target);
            void DebugPath(Bot* b);
            void MoveBot(Bot* b, float dt);
            void ChooseBotState(Bot* b, float dt);

            const float ballRadius = 3.0f;
            bool shooting;
            float shootingTimestamp;


            GameServer* server;
            GameClient* client;

            std::vector<Vector3> spawns;
            std::vector<Bot*> bots;
            std::vector<Player*> players;
            std::vector<string> levels;
            std::vector<GameObject*> goals;
            int currentLevel = -1;

            bool debug = true;
        };
    }
}
