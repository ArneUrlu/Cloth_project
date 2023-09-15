#pragma once

#include "Renderer.h"
#include <iostream>

void GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

bool GLCallLog(const char* function, const char* file, int line)
{
    while (GLenum error = glGetError())
    {
        std::cout << "OpenGL error: " << error << ' ' << function << ' ' << line << std::endl;
        return false;
    }
    return true;
}
