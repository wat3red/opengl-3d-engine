#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <print>
#include <string_view>

inline void APIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length,
	const GLchar* msg, const void* data)
{
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;

	std::string_view sourceStr = "UNKNOWN";
	switch (source) {
	case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "WINDOW SYSTEM"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "SHADER COMPILER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "APPLICATION"; break;
	case GL_DEBUG_SOURCE_OTHER:           sourceStr = "OTHER"; break;
	default:                              break;
	}

	std::string_view typeStr = "UNKNOWN";
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:               typeStr = "ERROR"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "DEPRECATED BEHAVIOR"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "UNDEFINED BEHAVIOR"; break;
	case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "PORTABILITY"; break;
	case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "PERFORMANCE"; break;
	case GL_DEBUG_TYPE_MARKER:              typeStr = "MARKER"; break;
	case GL_DEBUG_TYPE_OTHER:               typeStr = "OTHER"; break;
	default:                                break;
	}

	std::string_view severityStr = "UNKNOWN";
	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:   severityStr = "HIGH"; break;
	case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "MEDIUM"; break;
	case GL_DEBUG_SEVERITY_LOW:    severityStr = "LOW"; break;
	default:                       break;
	}

	std::println("{}: {} of {} severity, raised from {}: {}",
		id, typeStr, severityStr, sourceStr, msg ? msg : "");
}