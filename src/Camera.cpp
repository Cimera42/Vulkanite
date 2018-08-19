//
// Created by Tim on 30/10/2017.
//

#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>


Camera::Camera(float fov, float aspectRatio, float nearDist, float farDist)	:
		fov(fov),
		aspectRatio(aspectRatio),
		nearDist(nearDist),
		farDist(farDist)
{
	perspective();
	lookAt(glm::vec3(0,0,0), glm::vec3(0,0,1), glm::vec3(0,1,0));
}

void Camera::lookAt(glm::vec3 eye, glm::vec3 centre, glm::vec3 up)
{
	viewMatrix = glm::lookAt(eye, centre, up);
}

void Camera::perspective()
{
	projectionMatrix = glm::perspective(fov, aspectRatio, nearDist, farDist);
}
