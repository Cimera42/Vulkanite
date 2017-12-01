#include <iostream>
//#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "window.h"
#include "vulkanInterface.h"
#include "logger.h"
#include "Camera.h"
#include "Transform.h"
#include "KeyboardInput.h"
#include "SpecificThreadPool.h"

bool shouldExit = false;
glm::vec2 storedLastPos = glm::vec2(0,0);
static Transform* cameraTransform;
static KeyboardInput* keyboardInput;

void errCallback(int inCode, const char* descrip)
{
	Logger() << inCode << " -- #GLFW ERROR# -- " << descrip;
}

void windowCloseEvent(GLFWwindow *closingWindow)
{
	shouldExit = true;
}

void windowResize(GLFWwindow* window, int width, int height)
{
	if(width == 0 || height == 0) return;

	auto vulkanInterface = static_cast<VulkanInterface *>(glfwGetWindowUserPointer(window));
	vulkanInterface->recreateSwapchain();
}
void mouseMoveInputEvent(GLFWwindow *window, double xpos, double ypos)
{
	glm::vec2 lastPos = storedLastPos;
	if(lastPos.x == 0 && lastPos.y == 0)
		lastPos = glm::vec2(xpos, ypos);

	double pitch = (lastPos.y - ypos)/100.0f;
	double yaw = (lastPos.x - xpos)/100.0f;

	glm::quat rotation = cameraTransform->rotation;
	glm::quat y = glm::angleAxis((float) (yaw*(50.0f/100)), glm::vec3(0,1,0));
	glm::quat p = glm::angleAxis((float) (-pitch*(50.0f/100)), cameraTransform->right);
	rotation *= p*y;
	cameraTransform->rotation = rotation;
	cameraTransform->genVectors();

	storedLastPos = glm::vec2(xpos, ypos);
}

void keyboardInputEvent(GLFWwindow* inWindow, int keyCode, int scanCode, int action, int modifiers)
{
	keyboardInput->keyList[keyCode] = action;
}

int main()
{
	Logger::initLogger();
	Logger() << "First Line of Program";

	if(!glfwInit())
	{
		Logger() << "GLFW init failed";
		return false;
	}
	glfwSetErrorCallback(errCallback);

	auto window = new Window("Vulkanite", 640,480);
	window->cursorMode(GLFW_CURSOR_DISABLED);
	glfwSetWindowCloseCallback(window->glfwWindow, windowCloseEvent);
	glfwSetCursorPosCallback(window->glfwWindow, mouseMoveInputEvent);
	glfwSetKeyCallback(window->glfwWindow, keyboardInputEvent);

	auto vulkanInterface = new VulkanInterface();
	try
	{
		vulkanInterface->initVulkan(window);

		glfwSetWindowSizeCallback(window->glfwWindow, windowResize);
		glfwSetWindowUserPointer(window->glfwWindow, vulkanInterface);

		auto camera = new Camera(90.0f, static_cast<float>(window->width / window->height), 0.001f, 100.0f);
		cameraTransform = new Transform(glm::vec3(0.0f,0.0f,-3.0f), glm::quat(0.0f,0.0f,0.0f,1.0f), glm::vec3(1.0f,1.0f,1.0f));
		keyboardInput = new KeyboardInput();

		//Timekeeping for deltatime and fps counter
		std::chrono::time_point<std::chrono::steady_clock> start, previous, current, then;
		start = std::chrono::steady_clock::now();
		previous = start;
		then = start;
		int frames = 0;
		while(!shouldExit)
		{
			current = std::chrono::steady_clock::now();
			std::chrono::duration<double> dt = (current - previous);
			previous = current;
			double dtf = dt.count();

			glm::vec3 displaced = cameraTransform->position;
			if(keyboardInput->isKeyPressed('W'))
				displaced += cameraTransform->forward * ((float)dtf) * 5.0f;

			if(keyboardInput->isKeyPressed('S'))
				displaced -= cameraTransform->forward * ((float)dtf) * 5.0f;

			if(keyboardInput->isKeyPressed('D'))
				displaced -= cameraTransform->right * ((float)dtf) * 5.0f;

			if(keyboardInput->isKeyPressed('A'))
				displaced += cameraTransform->right * ((float)dtf) * 5.0f;

			if(keyboardInput->isKeyPressed(' '))
				displaced += glm::vec3(0, 1, 0) * ((float)dtf) * 5.0f;

			if(keyboardInput->isKeyPressed(340))
				displaced -= glm::vec3(0, 1, 0) * ((float)dtf) * 5.0f;

			cameraTransform->position = displaced;

			camera->lookAt(cameraTransform->position, cameraTransform->position + cameraTransform->forward, cameraTransform->up);

			vulkanInterface->update(camera);
			vulkanInterface->draw();

			frames++;
			if(((std::chrono::duration<double>)(current - then)).count() > 1.0)
			{
				glfwSetWindowTitle(window->glfwWindow, std::to_string(frames).c_str());
				frames = 0;
				then = current;
			}
			if(glfwGetKey(window->glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				shouldExit = true;
			glfwPollEvents();
		}

		vulkanInterface->waitForIdle();

		Logger() << "Begin destruction";
		delete camera;
		delete cameraTransform;
		delete keyboardInput;
		delete window;
		Logger() << "Window destroyed";
		glfwTerminate();
		Logger() << "GLFW destroyed";
		delete vulkanInterface;
		Logger() << "Vulkan destroyed";

		Logger::close();

	} catch(const std::runtime_error& e) {
		Logger() << " -- #RUNTIME ERROR# -- " << e.what();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}