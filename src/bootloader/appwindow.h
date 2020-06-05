#pragma once
#include "guicontext.h"
#include "controller.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <mutex>

namespace appWindow {
	
class appWindow {

public:
	explicit appWindow(HWND parent);
	~appWindow();
	void execute();

private:
	static void glfw_error_callback(int error, const char* description);

	std::unique_ptr<controller::controller> controller;
	std::unique_ptr<guiContext::guiContext> guiContext;
	GLFWwindow* window;
};

}  // namespace appWindow