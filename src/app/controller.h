#pragma once
#include "sigslot.h"
#include "bootloader.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <list>
#include <list>


namespace controller{

using namespace sigslot;

class controller : public has_slots<multi_threaded_global> {

public:
	controller();
	~controller();

	static std::string convertAddressToString(uint32_t address);

	// Signals
	signal1<std::string&, multi_threaded_global> firmwarePathChanged;
	signal1<std::list<std::string>&, multi_threaded_global> serialPortsListChanged;
	signal1<std::string&, multi_threaded_global> userCodeAddressChanged;
	signal1<uint8_t, multi_threaded_global> progressPercentChanged;
	signal1<bool, multi_threaded_global> bootloaderStatusChanged;
	signal1<std::string&, multi_threaded_global> sendMessageToConsole;

	// Slots
	void writeFirmware();
	void eraseFirmware();
	void updateFirmwarePath(std::string& filePath);
	void updateSerialSelectedPort(std::string& selectedPort);
	void openSerialPort();
	void closeSerialPort();
	void detectDevice();
	void detectSerialPorts();
	void runCode();

	void updateProgress(const bootloader::STBootProgress& progress);
	uint8_t updateProgressPercent(uint8_t progress);
	void updateBootloaderFinished(bool finished);
	void updatePageSize(std::string& size);
	void updateGlobalErase(bool isGlobalErase);
	void performNewMessage(std::string& message);
	void updateUserCodeAddress(std::string& address);	

private:
	void process();
	void UploadFile(std::string portName, unsigned char* bin, uint32_t size, uint32_t address, uint32_t jumpAddress);
	void handleException(const std::exception &e);
	void handleOpenSerialPort();
	void handleCloseSerialPort();	

	std::thread* processThread;
	std::mutex controllerMutex;
	std::unique_ptr<bootloader::bootloader> bootloader;
	std::unique_ptr<std::thread> bootProcessThread;

	std::string firmwarePath = "";
	std::string selectedSerialPortName = "";
	std::list<std::string> serialPortsList;
	const uint32_t userCodeBaseAddress = 0x8000000;
	uint32_t userCodeAddress = 0;
	uint32_t userCodePageNumber = 0;
	bool globalErase = false;
	std::string pageSize = "256";

	uint8_t progressPercent = 0;
	bool bootloaderBusy = false;
	bool bootloaderFinished = false;
};

}  // namespace guiContext
