#include "window.h"

Window::Window(const char* inTitle, int inWidth, int inHeight)
{
	title = inTitle;
	width = inWidth;
	height = inHeight;

	createGLFWWindow();
}

Window::~Window()
{
	if(glfwWindow)
		destroy();
}

void Window::destroy()
{
	glfwDestroyWindow(glfwWindow);
	glfwWindow = nullptr;
}

void Window::createGLFWWindow()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindow = glfwCreateWindow(width,height,title,nullptr,nullptr);
}

void Window::cursorMode(int mode)
{
	glfwSetInputMode(glfwWindow, GLFW_CURSOR, mode);
}

