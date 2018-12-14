#include "Coursework.h"
#include "../CSC8503Common/GameWorld.h"
#include "../../Plugins/OpenGLRendering/OGLMesh.h"
#include "../../Plugins/OpenGLRendering/OGLShader.h"
#include "../../Plugins/OpenGLRendering/OGLTexture.h"
#include "../../Common/TextureLoader.h"

#include "../CSC8503Common/PositionConstraint.h"
#include "../../Common/Assets.h"

#include <fstream>
#include "utils.h"


using namespace NCL;
using namespace CSC8503;


Coursework::Coursework() : TutorialGame() {
    physics->SetGravity(Vector3(0, -4, 0));
    physics->UseGravity(false);
    world->ShuffleConstraints(true);
    world->ShuffleObjects(true);

    Window::GetWindow()->ShowOSPointer(false);
    Window::GetWindow()->LockMouseToWindow(true);

    InitNetwork();

    levels.push_back("menu.txt");
    levels.push_back("level1.txt");
    levels.push_back("level2.txt");
    levels.push_back("level3.txt");

    if (isServer) {
        currentLevel = 0;
        LoadLevel(levels[currentLevel]);
        me = FindOrCreatePlayer("server");
        SpawnPlayer(me, spawns[0]);
        ResetCamera();
    } else {
      ;
    }
}

void Coursework::InitNetwork() {
    NetworkBase::Initialise();

    int status;
    int port = NetworkBase::GetDefaultPort();
    string filename = Assets::DATADIR + "networkstatus.txt";

    std::ifstream infile(filename);
    infile >> status;
    infile.close();

    if (status == 0) {
        isServer = true;
        std::ofstream outfile(filename);
        outfile << 1;
        outfile.close();
    }

    if (isServer) {
        server = new GameServer(port, 5);
        server->RegisterPacketHandler(StringMessage, this);
    } else {
        client = new GameClient();
        client->RegisterPacketHandler(StringMessage, this);
        bool canConnect = client->Connect(127, 0, 0, 1, port);
    }
}

void Coursework::ResetWorld() {
    world->ClearAndErase();
    physics->Clear();

    for (auto p : players) p->ball = nullptr;
    bots.clear();
    spawns.clear();
    goals.clear();

    shooting = false;

    // Floor
    AddFloorToWorld(Vector3(200, 0, 200));
}

void Coursework::LoadLevel(string levelName) {
    ResetWorld();

    int width, height, scale;
    string val;
    std::ifstream infile(Assets::DATADIR + levelName);

    infile >> scale;
    scale /= 2;
    infile >> width;
    infile >> height;

    for(int z = 0; z < height; z++) {
        for(int x = 0; x < width; x++) {
            infile >> val;

            Vector3 pos = Vector3(10 + x * scale * 2, scale * 2, 10 + z * scale * 2);
            if (val[0] == 'x') {
                AddCubeToWorld(
                    pos,
                    Vector3(scale * 1.01, scale * 1.01, scale * 1.01),
                    0.0f
                );
            } else if (val[0] == 'g') {
                AddGoalToWorld(pos);
            } else if (val[0] == 's') {
                spawns.push_back(pos);
            } else if (val[0] == 'b') {
                Bot* b = FindOrCreateBot("bot" + val.substr(1, 1));

                Vector3 patrolPos = Vector3(pos.x, 14, pos.z);
                int index = stoi(val.substr(2, 1));

                while (b->patrolSites.size() < index)
                    b->patrolSites.push_back(patrolPos);

                b->patrolSites[index - 1] = patrolPos;
            } else if (val[0] == 'c') {
                AddBridgeToWorld(pos);
            }
        }
    }

    for(auto b : bots) {
        if (b->patrolSites.size() > 0) SpawnBot(b, b->patrolSites[0]);
    }
}

Coursework::~Coursework() {
    NetworkBase::Destroy();
    if (!isServer) return;

    // Reset Status
    string filename = Assets::DATADIR + "networkstatus.txt";
    std::ofstream outfile(filename);
    outfile << 0;
    outfile.close();

    // Save Highscore
    string playerName;
    int mostWins = -1;
    int mostKills = -1;

    for(auto p : players) {
        if (p->wins > mostWins || (p->wins == mostWins && p->kills > mostKills)) {
            mostWins = p->wins;
            mostKills = p->kills;
            playerName = p->name;
        }
    }

    string buffer;
    int savedWins, savedKills;

    filename = Assets::DATADIR + "highscore.txt";
    std::ifstream infile(filename);
    infile >> buffer >> savedWins >> savedKills;
    outfile.close();

    if (mostWins > savedWins || (mostWins == savedWins && mostKills > savedKills)) {
        std::ofstream outfile(filename);
        outfile << playerName << " " << mostWins << " " << mostKills;
        outfile.close();
    }
}

void Coursework::UpdateGame(float dt) {
    if (isServer) {
        server->UpdateServer();
        if (server->newClient) {
            int id = players.size();
            SpawnPlayer(
                FindOrCreatePlayer("player" + to_string(id)),
                spawns[id]
            );
            server->SendGlobalPacket(StringPacket(SerializeState()));
            server->newClient = false;
        }
    } else {
        client->UpdateClient();
    }


    world->GetMainCamera()->UpdateCamera(dt);
    world->UpdateWorld(dt);
    renderer->Update(dt);

    if (isServer) {
        physics->Update(dt);

        // Lock ball in Y pos
        for(auto p : players) {
            Vector3 ballPos = p->ball->GetTransform().GetWorldPosition();
            p->ball->GetTransform().SetWorldPosition(
                Vector3(ballPos.x, 10 + ballRadius, ballPos.z)
            );
        }

        UpdateBots(dt);
        ServerInput();

        for(int i = 0; i < goals.size(); i++) {
            GameObject* goal = goals[i];

            if (goal->GetWinner() == nullptr) continue;

            // Menu
            if (goals.size() > 1) {
                currentLevel = i + 1;
            } else {
                std::cout << goal->GetName() << " won the game!" << std::endl;
                Player* p = FindOrCreatePlayer(goal->GetWinner()->GetName());
                p->wins++;

                currentLevel++;
                if (currentLevel > levels.size() - 1) currentLevel = 1;
            }

            LoadLevel(levels[currentLevel]);

            for(int i = 0; i < players.size(); i++)
                SpawnPlayer(players[i], spawns[i]);

            ResetCamera();
        }

        for(auto b : bots) {
            if (b->cube->killBot) {
                b->state = Dead;
                b->cube->killBot = false;

                if (b->cube->killerName != "") {
                    Player* p = FindOrCreatePlayer(b->cube->killerName);
                    p->kills++;
                    b->cube->killerName = "";
                }
            }
        }

        server->SendGlobalPacket(StringPacket(SerializeState()));
    }

    if (me != nullptr) {
        UpdateInput(dt);

        Vector3 ballPos = me->ball->GetTransform().GetWorldPosition();
        Vector3 cameraPos = world->GetMainCamera()->GetPosition();
        Vector3 dir = ballPos - cameraPos;
        dir.y = 0;
        dir.Normalise();

        Debug::DrawLine(ballPos, ballPos + dir * forceMagnitude / 200, Vector4(1, 0, 0, 1));
    }

    for(int i = 0; i < players.size(); i++) {
        renderer->DrawString(
            players[i]->name + ": " + to_string(players[i]->wins) + " | " + to_string(players[i]->kills),
            Vector2(10, 30 * i + 10)
        );
    }



    Debug::FlushRenderables();
    renderer->Render();
}

void Coursework::ServerInput() {
    if (Window::GetKeyboard()->KeyPressed(KEYBOARD_RIGHT)) {
        currentLevel++;
        if (currentLevel > levels.size() - 1) currentLevel = 1;

        LoadLevel(levels[currentLevel]);

        for(int i = 0; i < players.size(); i++)
            SpawnPlayer(players[i], spawns[i]);

        ResetCamera();
	}

    if (Window::GetKeyboard()->KeyPressed(KEYBOARD_LEFT)) {
        currentLevel--;
        if (currentLevel < 1) currentLevel = levels.size() - 1;

        LoadLevel(levels[currentLevel]);

        for(int i = 0; i < players.size(); i++)
            SpawnPlayer(players[i], spawns[i]);

        ResetCamera();
	}

    if (Window::GetKeyboard()->KeyPressed(KEYBOARD_R)) {
        LoadLevel(levels[currentLevel]);

        for(int i = 0; i < players.size(); i++)
            SpawnPlayer(players[i], spawns[i]);

        ResetCamera();
	}
}

void Coursework::UpdateInput(float dt) {
    if (Window::GetKeyboard()->KeyPressed(KEYBOARD_R))
        ResetCamera();

    // TODO: reset level
    // TODO: reset camera

    // if (Window::GetKeyboard()->KeyPressed(NCL::KeyboardKeys::KEYBOARD_G)) {
    //  useGravity = !useGravity; //Toggle gravity!
    //  physics->UseGravity(useGravity);
    // }

    if (!shooting && Window::GetMouse()->ButtonDown(NCL::MouseButtons::MOUSE_LEFT)){
        shooting = true;
        shootingTimestamp = 1;
    }
    else if (Window::GetMouse()->ButtonDown(NCL::MouseButtons::MOUSE_LEFT)) {
        shootingTimestamp += dt;

        int speed = 3, magnitude = 8000;
        forceMagnitude = (cos(shootingTimestamp * speed) + 1) * magnitude + 1000;
    }
    else if (shooting && !Window::GetMouse()->ButtonDown(NCL::MouseButtons::MOUSE_LEFT)) {
        shooting = false;

        if (isServer)
            Shoot(me, world->GetMainCamera()->GetPosition(), forceMagnitude);
        else
            client->SendPacket(StringPacket(SerializePlay()));

        forceMagnitude = 1000;
    }

    renderer->DrawString("Power: " + std::to_string(int(forceMagnitude) / 1000), Vector2(990, 10));
}

void Coursework::ReceivePacket(int type, GamePacket* payload, int source) {
    if (type != StringMessage) return;

    StringPacket* realPacket = (StringPacket*)payload;
    string msg = realPacket->GetStringFromData();
    //std::cout << "received message: " << msg << std::endl;

    isServer ? ProcessClientMessage(msg) : ProcessServerMessage(msg);
}

void Coursework::ProcessClientMessage(string msg) {
    std::vector<string> words = split_string(msg, ' ');

    Player* p = FindOrCreatePlayer(words[0]);
    Vector3 pos(
        stof(words[1]),
        stof(words[2]),
        stof(words[3])
    );
    Shoot(p, pos, stof(words[4]));
}

void Coursework::ProcessServerMessage(string msg) {
    int index = 0;
    bool resetLevel = false;
    std::vector<string> words = split_string(msg, ' ');

    int serverLevel = stoi(words[index++]);
    if (currentLevel != serverLevel) {
        currentLevel = serverLevel;
        LoadLevel(levels[currentLevel]);
        resetLevel = true;
    }

    int totalPlayers = stoi(words[index++]);
    for(int i = 0; i < totalPlayers; i++) {
        string name = words[index++];

        float x = stof(words[index++]);
        float y = stof(words[index++]);
        float z = stof(words[index++]);

        Player* p = FindOrCreatePlayer(name);
        SpawnPlayer(p, Vector3(x, y, z));

        p->wins = stoi(words[index++]);
        p->kills = stoi(words[index++]);
    }
    if (me == nullptr && players.size() > 0)
        me = players[players.size() - 1];

    // for(auto b : bots) delete b;
    // bots.clear();

    int totalBots = stoi(words[index++]);
    for(int i = 0; i < totalBots; i++) {
        string name = words[index++];
        float x = stof(words[index++]);
        float y = stof(words[index++]);
        float z = stof(words[index++]);

        Bot* b = FindOrCreateBot(name);
        SpawnBot(b, Vector3(x, y, z));
    }

    if (resetLevel) ResetCamera();
}

string Coursework::SerializeState() {
    stringstream buffer;

    buffer << currentLevel << " " << players.size() << " ";
    for(auto p : players) {
        Vector3 pos = p->ball->GetTransform().GetWorldPosition();
        buffer << p->name << " "
               << pos.x << " "
               << pos.y << " "
               << pos.z << " "
               << p->wins << " "
               << p->kills << " ";
    }

    buffer << bots.size() << " ";
    for(auto b : bots) {
        Vector3 pos = b->cube->GetTransform().GetWorldPosition();
        buffer << b->name << " "
               << pos.x << " "
               << pos.y << " "
               << pos.z << " ";
    }
    //std::cout << buffer.str() << std::endl;
	return buffer.str();
}

string Coursework::SerializePlay() {
    stringstream buffer;

    Vector3 pos = world->GetMainCamera()->GetPosition();
    buffer << me->name << " "
           << pos.x << " "
           << pos.y << " "
           << pos.z << " "
           << forceMagnitude;

    //std::cout << buffer.str() << std::endl;
	return buffer.str();
}

Player* Coursework::FindOrCreatePlayer(string name) {
    for(auto p : players) {
        if (p->name == name) return p;
    }

    Player* p = new Player;
    p->name = name;
    players.push_back(p);

    return p;
}

void Coursework::SpawnPlayer(Player* p, Vector3 pos) {
    if (p->ball == nullptr) {
        p->ball = AddSphereToWorld(Vector3(pos.x, 10 + ballRadius, pos.z), ballRadius, 3.0f);
        p->ball->GetRenderObject()->SetColour(Vector4(1, 1, 1, 1));
        p->ball->SetName(p->name);
    } else {
        p->ball->GetTransform().SetWorldPosition(Vector3(pos.x, 10 + ballRadius, pos.z));
    }
}

void Coursework::AddGoalToWorld(Vector3 pos) {
    GameObject* goal = AddSphereToWorld(Vector3(pos.x, 5, pos.z), 10, 0.0f);
    goal->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1));
    goal->SetName("goal");

    goals.push_back(goal);
}

Bot* Coursework::FindOrCreateBot(string name) {
    for(auto b : bots) {
        if (b->name == name) return b;
    }

    Bot* b = new Bot;
    b->name = name;
    bots.push_back(b);

    return b;
}

void Coursework::SpawnBot(Bot* b, Vector3 pos) {
    int scale = 4;

    if (b->cube == nullptr) {
        b->cube = AddCubeToWorld(
            Vector3(pos.x, 10 + scale, pos.z),
            Vector3(scale, scale, scale),
            0.01f
        );
        b->cube->GetRenderObject()->SetColour(Vector4(0, 0, 1, 1));
        b->cube->SetName(b->name);
        OBBVolume* volume = new OBBVolume(Vector3(scale, scale, scale));
    	b->cube->SetBoundingVolume((CollisionVolume*)volume);
    } else {
        b->cube->GetTransform().SetWorldPosition(
            Vector3(pos.x, 10 + scale, pos.z)
        );
    }
}

void Coursework::Shoot(Player* p, Vector3 originalPos, float power) {
    Vector3 ballPos = p->ball->GetTransform().GetWorldPosition();

    // Hit the top of the ball to get some spinning effect
    originalPos.y = ballPos.y = ballPos.y + 0.1;

    Ray ray(originalPos, originalPos - ballPos);
    RayCollision closestCollision;
    world->Raycast(ray, closestCollision, true);

    p->ball->GetPhysicsObject()->AddForceAtPosition(
        ray.GetDirection().Normalised() * -power,
        closestCollision.collidedAt
    );
}

void Coursework::UpdateBots(float dt) {
    for(auto b : bots) {
        b->decisionDT += dt;
        if (b->decisionDT > b->decisionCD) {
            b->decisionDT = 0;
            ChooseBotState(b, dt);
        }
        DebugPath(b);
        MoveBot(b, dt);
    }
}

vector<Vector3> Coursework::FindPath(Bot* b, Vector3 target) {
    vector<Vector3> path;

    NavigationGrid grid(levels[currentLevel]);
    NavigationPath outPath;

    bool found = grid.FindPath(
        b->cube->GetTransform().GetWorldPosition(),
        target,
        outPath
    );

	Vector3 pos;
    Vector3 offset = Vector3(10, 14, 10);
	while(outPath.PopWaypoint(pos)) path.push_back(pos + offset);

    // Remove initialPos
    if (path.size() > 0) path.erase(path.begin());

    // Improve last position
    path.push_back(target);

    return path;
}

void Coursework::DebugPath(Bot* b) {
    if (!debug) return;

    for(int i = 1; i < b->path.size(); i++) {
        Vector3 p1 = b->path[i - 1];
        Vector3 p2 = b->path[i];

        Debug::DrawLine(p1, p2, Vector4(0, 1, 0, 1));
    }
}

void Coursework::MoveBot(Bot* b, float dt) {
    if (b->path.size() == 0) return;

    Vector3 currPos = b->cube->GetTransform().GetWorldPosition();
    Vector3 target = b->path[0];
    Vector3 direction = target - currPos;

    if (direction.Length() < 0.2) {
        b->path.erase(b->path.begin() + 0);
    } else {
        direction.Normalise();
        b->cube->GetTransform().SetWorldPosition(currPos + direction * dt * b->speed);
        b->cube->GetTransform().SetLocalOrientation(Quaternion(Vector3(0, 0, 0), 1));
    }
}

void Coursework::ChooseBotState(Bot* b, float dt) {
    vector<Vector3> pathToPlayer;
    int bestSize = 1000;

    for(auto p : players) {
        vector<Vector3> newPath =
            FindPath(b, p->ball->GetTransform().GetWorldPosition());

        if (newPath.size() < bestSize) {
            pathToPlayer = vector<Vector3>(newPath);
            bestSize = newPath.size();
        }
    }

    switch (b->state) {
        case Following:
            if (pathToPlayer.size() > b->followDistance) {
                b->state = Returning;
                b->path.clear();
                return;
            }

            b->path.clear();
            b->path = vector<Vector3>(pathToPlayer);
        break;
        case Returning:
            if (pathToPlayer.size() <= b->followDistance) {
                b->state = Following;
                b->path.clear();
                return;
            }

            b->path.clear();
            b->path = FindPath(b, b->patrolSites[0]);

            if (b->path.size() < 2) {
                b->path.clear();
                b->state = Patrolling;
            }
        break;
        case Patrolling:
            if (pathToPlayer.size() <= b->followDistance) {
                b->state = Following;
                b->path.clear();
                return;
            }

            if (b->path.size() == 0)
                b->path = vector<Vector3>(b->patrolSites);
        break;
        case Dead:
            SpawnBot(b, b->patrolSites[0] + Vector3(-1000, 0, -1000));
            b->path.clear();

            b->deadDT += b->decisionCD;
            if (b->deadDT < b->deadCD) return;

            b->deadDT = 0;
            SpawnBot(b, b->patrolSites[0]);
            b->state = Patrolling;
        break;
    }
}

void Coursework::ResetCamera() {
    Vector3 offset = Vector3(0, 200, 100);
    world->GetMainCamera()->SetPosition(me->ball->GetTransform().GetWorldPosition() + offset);
    world->GetMainCamera()->SetYaw(0);
    world->GetMainCamera()->SetPitch(-45);
}

void Coursework::AddBridgeToWorld(Vector3 pos) {
    Vector3 cubeSize = Vector3 (3, 3, 3);

    float invCubeMass = 5; // how heavy the middle pieces are
    int numLinks = 4;
    float maxDistance = 12; // constraint distance
    float cubeDistance = 10; // distance between links
    Vector3 startPos = Vector3(pos.x, 13, pos.z);

    GameObject* start = AddCubeToWorld(startPos + Vector3 (0, 0, 0), cubeSize, 0);
    GameObject* end = AddCubeToWorld(startPos + Vector3 ((numLinks + 2) * cubeDistance, 0, 0), cubeSize, 0);
    GameObject* previous = start ;

    for (int i = 0; i < numLinks; ++i) {
        GameObject* block = AddCubeToWorld(startPos + Vector3 ((i + 1) * cubeDistance, 0, 0), cubeSize, invCubeMass);
        PositionConstraint* constraint =new PositionConstraint(previous, block, maxDistance);
        world->AddConstraint(constraint);
        previous = block;
    }

    PositionConstraint* constraint = new PositionConstraint(previous, end, maxDistance);
    world->AddConstraint(constraint);
}
