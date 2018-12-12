#pragma once
#include "Transform.h"
#include "CollisionVolume.h"

#include "PhysicsObject.h"
#include "RenderObject.h"
#include "NetworkObject.h"

#include <vector>

using std::vector;

namespace NCL {
	namespace CSC8503 {
		class NetworkObject;

		class GameObject	{
		public:
			GameObject(string name = "");
			~GameObject();

			void SetBoundingVolume(CollisionVolume* vol) {
				boundingVolume = vol;
			}

			const CollisionVolume* GetBoundingVolume() const {
				return boundingVolume;
			}

			bool IsActive() const {
				return isActive;
			}

			const Transform& GetConstTransform() const {
				return transform;
			}

			Transform& GetTransform() {
				return transform;
			}

			RenderObject* GetRenderObject() const {
				return renderObject;
			}

			PhysicsObject* GetPhysicsObject() const {
				return physicsObject;
			}

			NetworkObject* GetNetworkObject() const {
				return networkObject;
			}

			void SetRenderObject(RenderObject* newObject) {
				renderObject = newObject;
			}

			void SetPhysicsObject(PhysicsObject* newObject) {
				physicsObject = newObject;
			}

			const string& GetName() const {
				return name;
			}

			void SetName(string s) {
				name = s;
			}

			virtual void OnCollisionBegin(GameObject* otherObject) {
				if (name == "goal") {
					if (otherObject->name == "") return;
					if (winner == nullptr) winner = otherObject;
				} else if (name.substr(0, 3) == "bot") {
					if (otherObject->name.substr(0, 3) == "bot") {
						std::cout << "bot collision with bot" << std::endl;
						killBot = true;
					}
					else if (otherObject->name != "") {
						std::cout << "bot collision with player" << std::endl;

						if (otherObject->GetPhysicsObject()->GetLinearVelocity().Length() < 300) {
							otherObject->GetPhysicsObject()->AddForce(
								Vector3(rand(), 0, rand()).Normalised() * 15000
							);
						} else {
							killBot = true;
							killerName = otherObject->name;
						}

					}
				}
			}

			virtual void OnCollisionEnd(GameObject* otherObject) {
				//std::cout << "OnCollisionEnd event occured!\n";
			}

			bool InsideAABB(const Vector3& pos, const Vector3& halfSize);

			GameObject* GetWinner() const {
				return winner;
			}
			bool killBot = false;
			string killerName = "";
		protected:
			Transform			transform;

			CollisionVolume*	boundingVolume;
			PhysicsObject*		physicsObject;
			RenderObject*		renderObject;
			NetworkObject*		networkObject;

			bool	isActive;
			string	name;
			GameObject* winner = nullptr;
		};
	}
}
