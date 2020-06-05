#include "appwindow.h"
#include "stb_image.h"
#include <stdio.h>
#include <iostream>
#include <exception>
#include <set>

extern std::atomic<bool> initialised;

namespace appWindow {

appWindow::appWindow(HWND parent) {
	// Setup window
	glfwSetErrorCallback(glfw_error_callback);
	try {
		if (!glfwInit()) {
			throw std::exception("GLFW initialisation failed!");
		}
		window = glfwCreateWindow(1280, 720, "Johnson Matthey Batery Systems BMS Bootloader", NULL, NULL);
		if (window == NULL) {
			throw std::exception("App window creration failed!");
		}
		int width, height, nrChannels;
		unsigned char* img = stbi_load("resources/cpu.png", &width, &height, &nrChannels, 4);
		GLFWimage icon = { width,height,img };
		glfwSetWindowIcon(window, 1, &icon);
		stbi_image_free(img);
	} catch (const std::exception& e) {
		std::cout << "AppWindow exception:" << e.what() << std::endl;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	controller = std::make_unique<controller::controller>();
	guiContext = std::make_unique<guiContext::guiContext>(window);

	// Connect signals to slots
	guiContext->firmwarePathSelected.connect(controller.get(), &controller::controller::updateFirmwarePath);
	controller->firmwarePathChanged.connect(guiContext.get(), &guiContext::guiContext::updateFirmwarePathText);
	guiContext->writeFirmwareButtonClicked.connect(controller.get(), &controller::controller::writeFirmware);
	guiContext->eraseFirmwareButtonClicked.connect(controller.get(), &controller::controller::eraseFirmware);

	controller->serialPortsListChanged.connect(guiContext.get(), &guiContext::guiContext::updateSerialPortsListText);
	guiContext->serialPortSelected.connect(controller.get(), &controller::controller::updateSerialSelectedPort);
	
	guiContext->openComPortButtonClicked.connect(controller.get(), &controller::controller::openSerialPort);
	guiContext->closeComPortButtonClicked.connect(controller.get(), &controller::controller::closeSerialPort);
	guiContext->detectDeviceButtonClicked.connect(controller.get(), &controller::controller::detectDevice);
	guiContext->detectPortsButtonClicked.connect(controller.get(), &controller::controller::detectSerialPorts);
	guiContext->switchCodeButtonClicked.connect(controller.get(), &controller::controller::switchCode);

	guiContext->userCodeAddressChanged.connect(controller.get(), &controller::controller::updateUserCodeAddress);
	controller->userCodeAddressChanged.connect(guiContext.get(), &guiContext::guiContext::updateUserCodeAddressText);
	
	guiContext->pageSizeSelected.connect(controller.get(), &controller::controller::updatePageSize);
	guiContext->globalEraseChanged.connect(controller.get(), &controller::controller::updateGlobalErase);

	controller->progressPercentChanged.connect(guiContext.get(), &guiContext::guiContext::updateProgressBar);
	
	controller->bootloaderStatusChanged.connect(guiContext.get(), &guiContext::guiContext::disableUserInterface);
	controller->sendMessageToConsole.connect(guiContext.get(), &guiContext::guiContext::addLogMessage);

	initialised = true;
}

appWindow::~appWindow() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void appWindow::glfw_error_callback(int error, const char* description) {
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void appWindow::execute() {
	while (!glfwWindowShouldClose(window)) {
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		glfwPollEvents();
		if (guiContext) {
			guiContext->execute();
		}
		glfwMakeContextCurrent(window);
		glfwSwapBuffers(window);
	}
}

}	// namespace appWindow