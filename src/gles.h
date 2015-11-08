#ifndef _gles_h_
#define _gles_h_
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define glGetAttribLocationARB glGetAttribLocation
#define glCreateShaderObjectARB glCreateShader
#define glShaderSourceARB       glShaderSource
#define glCompileShaderARB      glCompileShader
#define glDeleteObjectARB       glDeleteObject
#define glCreateProgramObjectARB glCreateProgram
#define glAttachObjectARB       glAttachShader
#define glLinkProgramARB        glLinkProgram
#define glVertexAttrib2fARB     glVertexAttrib2f

#define GLhandleARB            GLuint
#define GLcharARB              GLchar

#define GL_FRAGMENT_SHADER_ARB GL_FRAGMENT_SHADER
#define GL_VERTEX_SHADER_ARB   GL_VERTEX_SHADER

#define GL_ALPHA_TEST          GL_ALPHA_TEST_QCOM
#define GL_RGBA8               GL_RGBA8_OES
#define GL_RGB8                GL_RGB8_OES
#define GL_BGRA                GL_BGRA_EXT
#define GL_RGB5                GL_RGB5_A1 //FIXME
#define GL_BGR                 GL_BGRA_EXT //FIXME
#define GL_UNSIGNED_SHORT_1_5_5_5_REV GL_UNSIGNED_SHORT //FIXME

#define GL_LINE_WIDTH_RANGE    GL_ALIASED_LINE_WIDTH_RANGE 
#define GL_POINT_SIZE_RANGE    GL_ALIASED_POINT_SIZE_RANGE

#define glewIsSupported(x)     1 //FIXME
#define glGetObjectParameterivARB(x, y, z) (*(z) = 1) //FIXME
#define GLEW_EXT_paletted_texture 0 //FIXME
#endif
