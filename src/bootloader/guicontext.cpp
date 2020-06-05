#include "guicontext.h"
#include "imgui.h"
#include "imgui_extended.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include "portable-file-dialogs.h"
#include <stb_image.h>
#include <GLFW/glfw3.h>


#if _WIN32
#define DEFAULT_PATH "C:\\"
#else
#define DEFAULT_PATH "/tmp"
#endif

namespace guiContext {

guiContext::guiContext(GLFWwindow* parent) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	parentWindow = parent;
	ImGui_ImplGlfw_InitForOpenGL(parentWindow, true);
	ImGui_ImplOpenGL2_Init();

	int my_image_width = 0;
	int my_image_height = 0;

	bool result = loadTextureFromFile("resources/load.png", &loadFileTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/cpu.png", &writeFirmwareTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/eraser.png", &eraseFirmwareTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/chain.png", &openComPortTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/unlink.png", &closeComPortTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/search.png", &detectDeviceTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/check.png", &detectPortsTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
	result = loadTextureFromFile("resources/jogging.png", &switchCodeTexture, &my_image_width, &my_image_height);
	IM_ASSERT(result);
}

guiContext::~guiContext() {
	// Cleanup
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void guiContext::execute() {

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin("Johnson Matthey Batery Systems BMS Bootloader", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);	// Create a window called "Hello, world!" and append into it.
	ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Once);
	int parentWindowWidth, parentWindowHeight;
	glfwGetWindowSize(parentWindow, &parentWindowWidth, &parentWindowHeight);
	ImGui::SetWindowSize(ImVec2(parentWindowWidth, parentWindowHeight));
	float titleBarHeight = ImGui::GetFrameHeight();
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Close")) show_bootloader_window = false;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	bool disabled = false;
	{
		std::unique_lock<std::mutex> lock(quiContextMutex);
		disabled = userInterfaceDisabled;
	}

	std::string helpText = "";

	ImGui::Columns(4, NULL, true);
	ImGui::SetColumnWidth(0,145);
	ImGui::Separator();
	if (disabled)
		ImGuiExtended::DisableWidget();
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)loadFileTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		auto f = pfd::open_file("Choose firmware file to read", DEFAULT_PATH,
			{ "Text Files (.bin)", "*.bin",
				"All Files", "*" },
			true);

		if (!f.result().empty()) {			
			firmwarePathSelected(f.result()[0]);
		}
	}
	if (ImGui::IsItemHovered())	
		helpText = "Import firmware file";	
	ImGui::SameLine(0, 10);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)writeFirmwareTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		writeFirmwareButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Write firmware to BMS";
	ImGui::SameLine(0, 10);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)eraseFirmwareTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		eraseFirmwareButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Erase BMS firmware";
	ImGui::SameLine(0, 10);	
	ImGui::NextColumn();
	ImGui::SetColumnWidth(1, 90);
	static std::list<std::string> comboPortslist;
	{
		std::unique_lock<std::mutex> lock(quiContextMutex);
		if (serialPortsListChanged) {
			serialPortsListChanged = false;
			comboPortslist.clear();
			for (auto& portName : serialPortsList) {
				comboPortslist.push_back(portName);
			}
		}
	}
	if (comboPortslist.empty()) {
		comboPortslist.push_back(" ");
	}
	auto comboCharList = std::unique_ptr<char*[]>{new char*[comboPortslist.size()]()};
	auto comboChars = std::make_unique<std::unique_ptr<char[]>[]>(comboPortslist.size());
	auto serialPortsListIndex = 0;
	for (auto& portName : comboPortslist) {
		auto partNameChar = std::unique_ptr<char[]>{new char[portName.size() + 1]()};
		portName.copy(partNameChar.get(), portName.size());
		partNameChar[portName.size()] = 0;
		comboCharList[serialPortsListIndex] = partNameChar.get();
		comboChars[serialPortsListIndex] = std::move(partNameChar);
		serialPortsListIndex++;
	}
	static int32_t selectedSerialPort = 0;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetFrameHeight() / 2);
	ImGui::SetNextItemWidth(75);
	if (ImGui::Combo("", &selectedSerialPort, comboCharList.get(), comboPortslist.size())) 
	{
		serialPortsListIndex = 0;
		for (auto& element: comboPortslist) {
			if (serialPortsListIndex == selectedSerialPort) {
				serialPortSelected(element);
			}
			serialPortsListIndex++;
		}
	}
	if (ImGui::IsItemHovered())
		helpText = "Choose serial port number";

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetFrameHeight() / 2);
	ImGui::NextColumn();
	ImGui::SetColumnWidth(2, 235);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)openComPortTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		openComPortButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Open Com port";
	ImGui::SameLine(0, 10);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)closeComPortTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		closeComPortButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Close com port";
	ImGui::SameLine(0, 10);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)detectDeviceTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		detectDeviceButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Detect device";
	ImGui::SameLine(0, 10);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)detectPortsTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		detectPortsButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Detect serial ports";	
	ImGui::SameLine(0, 10);
	if (ImGuiExtended::ImageButtonEx((void*)(intptr_t)switchCodeTexture, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1.0, 1.0), 2, ImVec4(0.0f, 0.0f, 0.0f, 0.0f))) {
		switchCodeButtonClicked();
	}
	if (ImGui::IsItemHovered())
		helpText = "Swich bootloader/application code";
	ImGui::Columns(1);
	ImGui::Separator();
	char userAddressInputText[9] = "";
	static std::string lastUserAddressInputText = "";
	{
		std::lock_guard<std::mutex> lock(quiContextMutex);
		userAddressInputText[userCodeAddressText.copy(userAddressInputText, userCodeAddressText.length(), 0)];
	}
	ImGui::SetNextItemWidth(64);
	if (disabled)
		ImGuiExtended::EnableWidget();
	ImGuiExtended::DisableWidget();
	ImGui::InputText(
		" ", 
		userAddressInputText,
		9, 
		ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	if (ImGui::IsItemDeactivated()) {
		if (lastUserAddressInputText.compare(userAddressInputText)) {
			userCodeAddressChanged(std::string(userAddressInputText));
		}
		lastUserAddressInputText = userAddressInputText;
	}
	if (ImGui::IsItemHovered())
		helpText = "Firmare start address (hexadecimal)";

	ImGui::SameLine(0, 0);
	const char* pageSizeOptions[] = { "128", "256", "2048" };
	static int selectedPageSize = 0;
	ImGui::SetNextItemWidth(75);
	if (ImGui::Combo("Page size", &selectedPageSize, pageSizeOptions, IM_ARRAYSIZE(pageSizeOptions))) {
		pageSizeSelected(std::string(pageSizeOptions[selectedPageSize]));
	}
	if (ImGui::IsItemHovered())
		helpText = "Select page size";
	ImGui::SameLine(0, 10);
	static bool globalErase = true;
	if (ImGui::Checkbox("Global erase", &globalErase)) {
		globalEraseChanged(globalErase);
	}
	if (ImGui::IsItemHovered())
		helpText = "Global erase";
	
	ImGuiExtended::EnableWidget();
	ImGui::Separator();
	char firmwarePathBuffer[500];
	{
		std::unique_lock<std::mutex> lock(quiContextMutex);
		if (firmwarePathText.size() < sizeof(firmwarePathBuffer)) {
			std::copy(firmwarePathText.begin(), firmwarePathText.end(), firmwarePathBuffer);
			firmwarePathText.copy(firmwarePathBuffer, firmwarePathText.size() + 1);
			firmwarePathBuffer[firmwarePathText.size()] = '\0';
		}
		else {
			firmwarePathBuffer[0] = '\0';
		}
	}	
	ImGui::PushItemWidth(ImGui::GetWindowSize().x - 130);
	ImGui::InputText("Firmware path", firmwarePathBuffer, sizeof(firmwarePathBuffer), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();
	if (ImGui::IsItemHovered())
		helpText = "Firmware path";
	ImGui::Separator();
	{
		std::unique_lock<std::mutex> lock(quiContextMutex);
		ImGui::ProgressBar(progressBarValue, ImVec2(-1.0f, 0.0f));
	}
	if (ImGui::IsItemHovered())
		helpText = "Firmware write progress";
	ImGui::Separator();
	bool p_open = true;
	{
		std::unique_lock<std::mutex> lock(quiContextMutex);
		if (!messagesQueue.empty()) {
			console.AddLog(messagesQueue.front().c_str());
			messagesQueue.pop();
		}
	}
	console.DrawAsChild("Console", &p_open);
	auto lastCursorPosition = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(0.0, ImGui::GetWindowSize().y - ImGui::GetFrameHeightWithSpacing() - 6));
	ImGui::Separator();
	ImGui::Text(helpText.c_str());
	ImGui::Separator();
	ImGui::SetCursorPos(lastCursorPosition);
	ImGui::End();

	// Rendering
	ImGui::Render();
	int display_w, display_h;
	glfwGetFramebufferSize(parentWindow, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
	glClear(GL_COLOR_BUFFER_BIT);

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

bool guiContext::loadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
	// Load from file
	int image_width = 0;
	int image_height = 0;
	unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
		return false;

	// Create a OpenGL texture identifier
	GLuint image_texture;
	glGenTextures(1, &image_texture);
	glBindTexture(GL_TEXTURE_2D, image_texture);

	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Upload pixels into texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
	stbi_image_free(image_data);

	*out_texture = image_texture;
	*out_width = image_width;
	*out_height = image_height;

	return true;
}

std::string guiContext::openfilename(char *filter, HWND owner) {
	OPENFILENAME ofn;
	char fileName[MAX_PATH] = "";
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = owner;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "";
	std::string fileNameStr;
	if (GetOpenFileName(&ofn))
		fileNameStr = fileName;
	return fileNameStr;
}

void guiContext::updateFirmwarePathText(std::string& firmwarePath) {
	std::lock_guard<std::mutex> lock(quiContextMutex);
	firmwarePathText = firmwarePath;
}

void guiContext::updateSerialPortsListText(std::list<std::string>& portsList) {
	std::lock_guard<std::mutex> lock(quiContextMutex);
	if (serialPortsList != portsList) {
		serialPortsList = portsList;
		serialPortsListChanged = true;
	}

}

void guiContext::addLogMessage(std::string& log) {
	std::lock_guard<std::mutex> lock(quiContextMutex);
	messagesQueue.push(log);
}

void guiContext::updateProgressBar(uint8_t progress) {
	std::lock_guard<std::mutex> lock(quiContextMutex);
	progressBarValue = (float)(progress)/100;
}

void guiContext::updateUserCodeAddressText(std::string& address) {
	std::lock_guard<std::mutex> lock(quiContextMutex);
	userCodeAddressText = address;
}

void guiContext::disableUserInterface(bool disable) {
	std::lock_guard<std::mutex> lock(quiContextMutex);
	userInterfaceDisabled = disable;
}

}	// namespace guiContext