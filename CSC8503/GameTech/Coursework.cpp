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
    physics->SetGravity(Vector3(0, -2000, 0));
    physics->UseGravity(false);
    world->ShuffleConstraints(true);
    world->ShuffleObjects(true);

    Window::GetWindow()->ShowOSPointer(false);
    Window::GetWindow()->LockMouseToWindow(true);

    InitNetwork();

    levels.push_back("level3.txt");
    levels.push_back("level1.txt");
    levels.push_back("level2.txt");

    if (isServer) {
        currentLevel = 0;
        LoadLevel(levels[currentLevel]);
        me = FindOrCreatePlayer("server");
        SpawnPlayer(me, spawns[0]);
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
    goal = nullptr;

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
                b->patrolSites.push_back(Vector3(pos.x - 10, 0, pos.z - 10));
                if (b->patrolSites.size() == 1) SpawnBot(b, pos);
            }
        }
    }
}

Coursework::~Coursework() {
    NetworkBase::Destroy();
    if (isServer) {
        string filename = Assets::DATADIR + "networkstatus.txt";
        std::ofstream outfile(filename);
        outfile << 0;
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

        if (goal->GetWinner() != nullptr) {
            std::cout << goal->GetName() << " won the game!" << std::endl;

            currentLevel++;
            if (currentLevel > levels.size() - 1) currentLevel = 0;

            LoadLevel(levels[currentLevel]);

            for(int i = 0; i < players.size(); i++)
                SpawnPlayer(players[i], spawns[i]);
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

    if (me != nullptr) UpdateInput(dt);

    for(int i = 0; i < players.size(); i++) {
        renderer->DrawString(
            players[i]->name + ": " + to_string(players[i]->wins) + " | " + to_string(players[i]->kills),
            Vector2(10, 30 * i + 10)
        );
    }


    Debug::FlushRenderables();
    renderer->Render();
}

void Coursework::UpdateInput(float dt) {
    // TODO: reset level
    // TODO: reset camera

    // if (Window::GetKeyboard()->KeyPressed(NCL::KeyboardKeys::KEYBOARD_G)) {
    //  useGravity = !useGravity; //Toggle gravity!
    //  physics->UseGravity(useGravity);
    // }

    if (!shooting && Window::GetMouse()->ButtonDown(NCL::MouseButtons::MOUSE_LEFT)){
        shooting = true;
        shootingTimestamp = rand();
    }
    else if (Window::GetMouse()->ButtonDown(NCL::MouseButtons::MOUSE_LEFT)) {
        shootingTimestamp += dt;

        int speed = 3, magnitude = 8000;
        forceMagnitude = (sin(shootingTimestamp * speed) + 1) * magnitude;
    }
    else if (shooting && !Window::GetMouse()->ButtonDown(NCL::MouseButtons::MOUSE_LEFT)) {
        shooting = false;

        if (isServer)
            Shoot(me, world->GetMainCamera()->GetPosition(), forceMagnitude);
        else
            client->SendPacket(StringPacket(SerializePlay()));
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
    std::vector<string> words = split_string(msg, ' ');

    int serverLevel = stoi(words[index++]);
    if (currentLevel != serverLevel) {
        currentLevel = serverLevel;
        LoadLevel(levels[currentLevel]);
    }

    int totalPlayers = stoi(words[index++]);
    for(int i = 0; i < totalPlayers; i++) {
        string name = words[index++];

        float x = stof(words[index++]);
        float y = stof(words[index++]);
        float z = stof(words[index++]);

        Player* p = FindOrCreatePlayer(name);
        SpawnPlayer(p, Vector3(x, y, z));
    }
    if (me == nullptr && players.size() > 0) me = players[players.size() - 1];

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
}

string Coursework::SerializeState() {
    stringstream buffer;

    buffer << currentLevel << " " << players.size() << " ";
    for(auto p : players) {
        Vector3 pos = p->ball->GetTransform().GetWorldPosition();
        buffer << p->name << " "
               << pos.x << " "
               << pos.y << " "
               << pos.z << " ";
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
    goal = AddSphereToWorld(Vector3(pos.x, 5, pos.z), 10, 0.0f);
    goal->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1));
    goal->SetName("goal");
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
	while(outPath.PopWaypoint(pos)) path.push_back(pos);

    // Remove initialPos
    if (path.size() > 0) path.erase(path.begin() + 0);

    return path;
}

void Coursework::DebugPath(Bot* b) {
    if (!debug) return;

    for(int i = 1; i < b->path.size(); i++) {
        Vector3 p1 = b->path[i - 1];
        Vector3 p2 = b->path[i];

        Vector3 offset = Vector3(10, 0, 10);

        Debug::DrawLine(p1 + offset, p2 + offset, Vector4(0, 1, 0, 1));
    }
}

void Coursework::MoveBot(Bot* b, float dt) {
    if (b->path.size() == 0) return;

    Vector3 offset = Vector3(10, 14, 10);
    Vector3 currPos = b->cube->GetTransform().GetWorldPosition();
    Vector3 target = b->path[0] + offset;

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
    vector<Vector3> pathToPlayer =
        FindPath(b, players[0]->ball->GetTransform().GetWorldPosition());

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

            if (b->path.size() < 1) {
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

            b->deadDT += b->decisionCD;
            if (b->deadDT < b->deadCD) return;

            b->deadDT = 0;
            Vector3 offset = Vector3(10, 0, 10);
            SpawnBot(b, b->patrolSites[0] + offset);
            b->state = Patrolling;
        break;
    }
}
