//
// Created by Tim on 30/10/2017.
//

#ifndef VULKANITE_TRANSFORM_H
#define VULKANITE_TRANSFORM_H

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

class Transform
{
public:
	Transform(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale);

	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 right;
	glm::vec3 up;

	glm::mat4 matrix;
	glm::quat rotation;
	glm::vec3 scale;

	void genMatrix();
	void genVectors();
};

#endif //VULKANITE_TRANSFORM_H
