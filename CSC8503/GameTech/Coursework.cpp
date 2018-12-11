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

    for (auto p : players) {
        p->ball = nullptr;
    }
    spawns.clear();
    goal = nullptr;

    shooting = false;

    // Floor
    AddFloorToWorld(Vector3(200, 0, 200));
}

void Coursework::LoadLevel(string levelName) {
    ResetWorld();

    int width, height, scale;
    char val;
    std::ifstream infile(Assets::DATADIR + levelName);

    scale = 10;
    infile >> width;
    infile >> height;

    for(int z = 0; z < height; z++) {
        for(int x = 0; x < width; x++) {
            infile >> val;

            Vector3 pos = Vector3(x * scale * 2, scale * 2, z * scale * 2);
            if (val == 'x') {
                AddCubeToWorld(
                    pos,
                    Vector3(scale * 1.01, scale * 1.01, scale * 1.01),
                    0.0f
                );
            } else if (val == 'g') {
                AddGoalToWorld(pos);
            } else if (val == 's') {
                spawns.push_back(pos);
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

        if (goal->GetWinner() != nullptr) {
            std::cout << goal->GetName() << " won the game!" << std::endl;

            currentLevel++;
            if (currentLevel > levels.size() - 1) currentLevel = 0;

            LoadLevel(levels[currentLevel]);

            for(int i = 0; i < players.size(); i++)
                SpawnPlayer(players[i], spawns[i]);
        }

        server->SendGlobalPacket(StringPacket(SerializeState()));
    }

    if (me != nullptr) UpdateInput(dt);

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

    renderer->DrawString("Click Force:" + std::to_string(int(forceMagnitude) / 1000), Vector2(10, 20));
}

void Coursework::ReceivePacket(int type, GamePacket* payload, int source) {
    if (type != StringMessage) return;

    StringPacket* realPacket = (StringPacket*)payload;
    string msg = realPacket->GetStringFromData();
    std::cout << "received message: " << msg << std::endl;

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
    std::vector<string> words = split_string(msg, ' ');

    if (currentLevel != stoi(words[0])) {
        currentLevel = stoi(words[0]);
        LoadLevel(levels[currentLevel]);
    }

    int totalPlayers = stoi(words[1]);
    int attributes = 4;

    for(int i = 0; i < totalPlayers; i++) {
        string name = words[i * attributes + 2];
        Vector3 pos(
            stof(words[i * attributes + 3]),
            stof(words[i * attributes + 4]),
            stof(words[i * attributes + 5])
        );

        Player* p = FindOrCreatePlayer(name);
        SpawnPlayer(p, pos);
    }
    if (me == nullptr && players.size() > 0) me = players[players.size() - 1];
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
