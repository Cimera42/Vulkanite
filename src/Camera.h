//
// Created by Tim on 30/10/2017.
//

#ifndef VULKANITE_CAMERA_H
#define VULKANITE_CAMERA_H

#include <glm/mat4x4.hpp>

class Camera
{
public:
	Camera(float fov, float aspectRatio, float nearDist, float farDist);

	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;

	void lookAt(glm::vec3 eye, glm::vec3 centre, glm::vec3 up);

private:

	float fov;
	float aspectRatio;
	float nearDist;
	float farDist;

	void perspective();
};

#endif //VULKANITE_CAMERA_H
