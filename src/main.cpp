#include <iostream>
//#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "window.h"
#include "vulkanInterface.h"
#include "logger.h"

void errCallback(int inCode, const char* descrip)
{
	Logger() << inCode << " -- #GLFW ERROR# -- " << descrip;
}

void windowResize(GLFWwindow* window, int width, int height)
{
	if(width == 0 || height == 0) return;

	VulkanInterface* vulkanInterface = static_cast<VulkanInterface *>(glfwGetWindowUserPointer(window));
	vulkanInterface->recreateSwapchain();
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

	auto vulkanInterface = new VulkanInterface();
	try
	{
		vulkanInterface->initVulkan(window);
	} catch(const std::runtime_error& e) {
		Logger() << " -- #VULKAN ERROR# -- " << e.what();
		return EXIT_FAILURE;
	}

	glfwSetWindowSizeCallback(window->glfwWindow, windowResize);
	glfwSetWindowUserPointer(window->glfwWindow, vulkanInterface);

	while(!glfwWindowShouldClose(window->glfwWindow))
	{
		vulkanInterface->draw();
		glfwPollEvents();
	}

	vulkanInterface->waitForIdle();

	Logger() << "Begin destruction";
	delete window;
	Logger() << "Window destroyed";
	glfwTerminate();
	Logger() << "GLFW destroyed";
	delete vulkanInterface;
	Logger() << "Vulkan destroyed";

	Logger::close();

	return EXIT_SUCCESS;
}