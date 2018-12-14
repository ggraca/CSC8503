#include "NavigationGrid.h"
#include "../../Common/Assets.h"

#include <fstream>
#include <string>


using namespace NCL;
using namespace CSC8503;

const int LEFT_NODE		= 0;
const int RIGHT_NODE	= 1;
const int TOP_NODE		= 2;
const int BOTTOM_NODE	= 3;

const char WALL_NODE	= 'x';
const char FLOOR_NODE	= '.';

NavigationGrid::NavigationGrid()	{
	nodeSize	= 0;
	gridWidth	= 0;
	gridHeight	= 0;
	allNodes	= nullptr;
}

NavigationGrid::NavigationGrid(const std::string&filename) : NavigationGrid() {
	std::ifstream infile(Assets::DATADIR + filename);

	infile >> nodeSize;
	infile >> gridWidth;
	infile >> gridHeight;

	allNodes = new GridNode[gridWidth * gridHeight];

	for(int y = 0; y < gridHeight; ++y) {
		for(int x = 0; x < gridWidth; ++x) {
			GridNode&n = allNodes[(gridWidth * y) + x];
			std::string type;
			infile >> type;
			n.type = type[0];
			n.position = Vector3(x * gridWidth, 0, y * gridHeight);
		}
	}

	//now to build the connectivity between the nodes
	for(int y = 0; y < gridHeight; ++y) {
		for(int x = 0; x < gridWidth; ++x) {
			GridNode&n = allNodes[(gridWidth * y) + x];

			if (y > 0) { //get the above node
				n.connected[0] = &allNodes[(gridWidth * (y - 1)) + x];
			}
			if (y < gridHeight - 1) { //get the below node
				n.connected[1] = &allNodes[(gridWidth * (y + 1)) + x];
			}
			if (x > 0) { //get left node
				n.connected[2] = &allNodes[(gridWidth * (y)) + (x - 1)];
			}
			if (x < gridWidth - 1) { //get right node
				n.connected[3] = &allNodes[(gridWidth * (y)) + (x + 1)];
			}
			for(int i = 0; i < 4; ++i) {
				if (n.connected[i]) {
					if (n.connected[i]->type == 'x' || n.connected[i]->type == 'c') {
						n.connected[i] = nullptr; //actually a wall, disconnect!
					}
					else {
						n.costs[i]		= 1;
					}
				}
			}
		}
	}
}

NavigationGrid::~NavigationGrid()	{
	delete[] allNodes;
}

bool NavigationGrid::FindPath(const Vector3& from, const Vector3& to, NavigationPath& outPath) {
	int fromX = (from.x / nodeSize);
	int fromZ = (from.z / nodeSize);

	int toX = (to.x / nodeSize);
	int toZ = (to.z / nodeSize);

	if (fromX < 0 || fromX > gridWidth - 1 ||
		fromZ < 0 || fromZ > gridHeight - 1) {
		return false;
	}
	if (toX < 0 || toX > gridWidth - 1 ||
		toZ < 0 || toZ > gridHeight - 1) {
		return false;
	}

	GridNode* startNode = &allNodes[(fromZ * gridWidth) + fromX];
	GridNode* endNode = &allNodes[(toZ * gridWidth) + toX];

	std::vector<GridNode*> openList;
	std::vector<GridNode*> closedList;

	openList.emplace_back(startNode);

	startNode->f = 0;
	startNode->g = 0;
	startNode->parent = nullptr;

	GridNode* currentBestNode = nullptr;

	while (!openList.empty()) {
		currentBestNode = RemoveBestNode(openList);

		// we ’ve found the path !
		if (currentBestNode == endNode) {
			GridNode* node = endNode;
			while(node != nullptr) {
				outPath.PushWaypoint(node->position);
				// Build up the waypoints
				node = node->parent;
			}
			return true;
		}
		else {
			for(int i = 0; i < 4; i++) {
				GridNode* neighbour = currentBestNode->connected[i];
				// might not be connected ...
				if (!neighbour) continue;

				bool inClosed = NodeInList(neighbour, closedList);
				// already discarded this neighbour ...
				if (inClosed) continue;

				float h = Heuristic(neighbour, endNode);
				float g = currentBestNode->g + currentBestNode->costs[i];
				float f = h + g;

				bool inOpen = NodeInList(neighbour, openList);
				// first time we ’ve seen this neighbour
				if (!inOpen) openList.emplace_back(neighbour);

				// might be a better route to this node !
				if (!inOpen || f < neighbour->f) {
					neighbour->parent = currentBestNode;
					neighbour->f = f;
					neighbour->g = g;
				}
			}
			closedList.emplace_back(currentBestNode);
		}
	}

	// open list emptied out with no path !
	return false;
}

bool NavigationGrid::NodeInList(GridNode* n, std::vector<GridNode*>& list) const {
	std::vector<GridNode*>::iterator i = std::find(list.begin(),list.end(), n);
	return i == list.end() ? false : true;
}

GridNode*  NavigationGrid::RemoveBestNode(std::vector<GridNode*>& list) const {
	std::vector<GridNode*>::iterator bestI = list.begin();

	GridNode* bestNode = *list.begin();

	for(auto i = list.begin(); i != list.end(); i++) {
		if ((*i)->f < bestNode->f) {
			bestNode = (*i);
			bestI = i;
		}
	}

	list.erase(bestI);
	return bestNode;
}

float NavigationGrid::Heuristic(GridNode* n1, GridNode* n2) const {
	return (n1->position - n2->position).Length();
}
