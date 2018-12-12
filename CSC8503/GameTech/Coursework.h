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
    Patrolling
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
            float cooldown = 0.2;
            float dtsum = cooldown;
            int followDistance = 8;
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
            void UpdateInput(float dt);
            void AddGoalToWorld(Vector3 pos);
            void SpawnPlayer(Player* p, Vector3 pos);
            Player* FindOrCreatePlayer(string name);
            void Shoot(Player* p, Vector3 originalPos, float forceMagnitude);

            Bot* FindOrCreateBot(string name);
            void SpawnBot(Bot* b, Vector3 pos);
            vector<Vector3> FindPath(Bot* b, Vector3 target);
            void DebugPath(Bot* b);
            void MoveBot(Bot* b, float dt);
            void ChooseBotState(Bot* b);

            const float ballRadius = 3.0f;
            bool shooting;
            float shootingTimestamp;

            GameObject* goal;

            GameServer* server;
            GameClient* client;

            std::vector<Vector3> spawns;
            std::vector<Bot*> bots;
            std::vector<Player*> players;
            std::vector<string> levels;
            int currentLevel = -1;

            bool debug = true;
        };
    }
}
