//
// Created by Tim on 30/10/2017.
//

#include "Transform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

Transform::Transform(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale) :
		position(position),
		rotation(rotation),
		scale(scale)
{
	genMatrix();
	genVectors();
}

void Transform::genMatrix()
{
	matrix = glm::mat4();
	matrix *= glm::translate(position);
	matrix *= glm::toMat4(rotation);
	matrix *= glm::scale(scale);
}

void Transform::genVectors()
{
	forward = glm::vec3(0,0,1) * rotation;
	right = glm::vec3(1,0,0) * rotation;
	up = glm::vec3(0,1,0) * rotation;
}