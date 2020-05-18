#pragma once
#include <string>
#include "windows.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "sigslot.h"
#include "console.h"
#include <queue>
#include <mutex>

namespace guiContext{

using namespace sigslot;

class guiContext : public has_slots<multi_threaded_global> {

public:
	explicit guiContext(GLFWwindow* parent);
	~guiContext();

	void execute();
	static bool loadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
	static std::string guiContext::openfilename(char *filter = "All Files (*.*)\0*.*\0", HWND owner = NULL);
	void addLogMessage(std::string& log);

	bool show_demo_window = true;
	bool show_another_window = false;
	bool show_bootloader_window = true;

	// Signals
	signal1<std::string&, multi_threaded_global> firmwarePathSelected;
	signal0<multi_threaded_global> writeFirmwareButtonClicked;
	signal0<multi_threaded_global> eraseFirmwareButtonClicked;
	signal1<std::string&, multi_threaded_global> serialPortSelected;
	signal0<multi_threaded_global> openComPortButtonClicked;
	signal0<multi_threaded_global> closeComPortButtonClicked;
	signal0<multi_threaded_global> detectDeviceButtonClicked;
	signal0<multi_threaded_global> detectPortsButtonClicked;
	signal0<multi_threaded_global> runCodeButtonClicked;
	signal1<std::string&, multi_threaded_global> userCodeAddressChanged;
	signal1<bool, multi_threaded_global> globalEraseChanged;
	signal1<std::string&, multi_threaded_global> pageSizeSelected;

	//Slots
	void updateFirmwarePathText(std::string& firmwarePath);
	void updateSerialPortsListText(std::list<std::string>& portsList);
	void updateUserCodeAddressText(std::string& address);

	void updateProgressBar(uint8_t progress);
	void disableUserInterface(bool disable);
	

private:

	RECT  lpRect;
	ImVec2 winSize_old = ImVec2(0, 0);
	ImVec2 winPosition_old = ImVec2(0, 0);
	ImVec2 dragDelta_old = ImVec2(0, 0);

	ImVec2 moveDelta = ImVec2(0, 0);
	ImVec2 moveDeltaOld = ImVec2(0, 0);

	ImVec2 referencePos = ImVec2(0, 0);
	bool positionChanged = false;

	ImVec2 moveStartPosition = ImVec2(0, 0);

	int moveOffsetX = 0;
	int moveOffsetY = 0;

	uint16_t counter = 0;
	uint16_t counter1 = 0;
	
	bool userInterfaceDisabled = false;

	/// my vals
	GLFWwindow* parentWindow;
	GLuint loadFileTexture = 0;
	GLuint writeFirmwareTexture = 0;
	GLuint eraseFirmwareTexture = 0;
	GLuint openComPortTexture = 0;
	GLuint closeComPortTexture = 0;
	GLuint detectDeviceTexture = 0;
	GLuint detectPortsTexture = 0;
	GLuint runCodeTexture = 0;

	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	std::queue<std::string> messagesQueue;
	std::mutex quiContextMutex;
	console console;

	std::list<std::string> serialPortsList;
	std::string firmwarePathText = "";
	std::string userCodeAddressText = "";
	float progressBarValue = 0;
	
	bool serialPortsListChanged = false;
};

}  // namespace guiContext
