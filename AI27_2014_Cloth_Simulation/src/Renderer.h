 #pragma once

#include <glad\glad.h>
#include <GLFW\glfw3.h>


#define ASSERT(x) if (!(x)) __debugbreak();
#ifdef DEBUG
	#define GLCall(x) GLClearError(); x; ASSERT(GLLogCall(#x, __FILE__, __LINE__)) 
#else
	#define GLCall(x) x
#endif

void GLClearError();
bool GLCallLog(const char* function, const char* file, int line);
