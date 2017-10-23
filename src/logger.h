#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <iostream>
#include <sstream>
#include <chrono>
#include <string>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#ifdef REACT_PHYSICS_3D
#include <reactphysics3d.h>
#endif

class Logger
{
	bool endLine;
	std::string between;
	std::stringstream buffer;

	static std::ofstream* logFileOut;
	
	void filePrint();

public:
	Logger(bool doEndLine, std::string inBetween);
	Logger();
	explicit Logger(bool doEndLine);
	explicit Logger(std::string inBetween);
	static void initLogger();
	static void close();

	//Print entire string buffer at end of << chain
	//when logger object is destroyed
	~Logger();

	//Function for general types
	template <typename T>
	Logger& operator<<(const T& val)
	{
		//Push input to stringstream
		buffer << val << between;
		return *this;
	}
	Logger& operator<<(glm::vec2 val);
	Logger& operator<<(glm::vec3 val);
	Logger& operator<<(glm::vec4 val);
	Logger& operator<<(glm::quat val);
	Logger& operator<<(glm::mat4 val);
	Logger& operator<<(std::ostream& (*val)(std::ostream &));

#ifdef REACT_PHYSICS_3D
	Logger& operator<<(const rp3d::Vector3 val);
#endif
};

#endif // LOGGER_H_INCLUDED
