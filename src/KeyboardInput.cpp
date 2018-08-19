//
// Created by Tim on 30/10/2017.
//

#include "KeyboardInput.h"
#include <GLFW/glfw3.h>

bool KeyboardInput::isKeyPressed(int keyCode)
{
	auto fi = keyList.find(keyCode);
	if(fi != keyList.end())
	{
		if(fi->second == GLFW_PRESS || fi->second == GLFW_REPEAT)
		{
			return true;
		}
	}
	return false;
}