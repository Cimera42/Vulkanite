//
// Created by Tim on 8/06/2017.
//

#include "logger.h"

#include <utility>
#include <fstream>

Logger::Logger(bool doEndLine, std::string inBetween)
{
	endLine = doEndLine;
	between = std::move(inBetween);

	if(endLine)
	{
		time_t current_time;
		std::time(&current_time);
		
		struct tm * time_info;
		time_info = localtime(&current_time);
		
		char timeString[9];  // space for "HH:MM:SS\0"
		strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);
		buffer << "[" << timeString << "] ";
	}
}
Logger::Logger() : Logger(true,"") {}
Logger::Logger(bool doEndLine) : Logger(doEndLine, ""){}
Logger::Logger(std::string inBetween) : Logger(true, std::move(inBetween)){}

Logger::~Logger()
{
	if(endLine)
		buffer << std::endl;
	
	std::cout << buffer.rdbuf();
	filePrint();
}

std::ofstream* Logger::logFileOut;
void Logger::initLogger()
{
	time_t current_time;
	std::time(&current_time);

	struct tm * time_info;
	time_info = localtime(&current_time);

	char timeString[20];  // space for "0000-00-00_00-00-00\0"
	strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S", time_info);
	std::string logFilename;
	logFilename += "logs/log_";
	logFilename.append(timeString);
	logFilename += ".log";
	
	logFileOut = new std::ofstream(logFilename, std::ios::out);
}

void Logger::close()
{
	logFileOut->close();
	delete logFileOut;
}

void Logger::filePrint()
{
	(*logFileOut) << buffer.rdbuf()->str();
	(*logFileOut).flush();
}

Logger& Logger::operator<<(const glm::vec2 val)
{
	buffer << val.x << ", " << val.y << between;
	return *this;
}

Logger& Logger::operator<<(const glm::vec3 val)
{
	buffer << val.x << ", " << val.y << ", " << val.z << between;
	return *this;
}

Logger& Logger::operator<<(const glm::vec4 val)
{
	buffer << val.w << ", " << val.x << ", " << val.y << ", " << val.z << between;
	return *this;
}

Logger& Logger::operator<<(const glm::quat val)
{
	//Call as vec4
	(*this) << glm::vec4(val.w,val.x,val.y,val.z);
	return *this;
}

Logger& Logger::operator<<(const glm::mat4 val)
{
	buffer << val[0][0] << "," << val[1][0] << "," << val[2][0] << "," << val[3][0] << " | ";
	buffer << val[0][1] << "," << val[1][1] << "," << val[2][1] << "," << val[3][1] << " | ";
	buffer << val[0][2] << "," << val[1][2] << "," << val[2][2] << "," << val[3][2] << " | ";
	buffer << val[0][3] << "," << val[1][3] << "," << val[2][3] << "," << val[3][3] << " | ";
	return *this;
}

//Function for special stream types, eg endl
Logger& Logger::operator<<(std::ostream& (*val)(std::ostream &))
{
	buffer << val << between;
	return *this;
}

#ifdef REACT_PHYSICS_3D
Logger &Logger::operator<<(const rp3d::Vector3 val)
{
	buffer << val.x << ", " << val.y << ", " << val.z << between;
	return *this;
}
#endif
