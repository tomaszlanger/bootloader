#include "controller.h"
#include "bootloader.h"
#include <string>
#include <mutex>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <functional>
#include <fstream>

std::atomic<bool> initialised(false);

namespace controller {

using namespace std;

controller::controller() {
	bootloader = std::make_unique<bootloader::bootloader>();
	processThread = new std::thread(&controller::process, this);
}

controller::~controller() {
	processThread->join();
	delete processThread;
	if (bootProcessThread) {
		if  (bootProcessThread->joinable())
			bootProcessThread->join();
	}
}

string controller::convertAddressToString(uint32_t address) {
	std::ostringstream stringAddress;
	stringAddress << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << address;
	return stringAddress.str();
}

void controller::writeFirmware() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	serialPortsListChanged(serialPortsList);
	if (!bootloaderBusy) {
		bootloaderBusy = true;
		bootloaderStatusChanged(bootloaderBusy);
		bootProcessThread = make_unique<std::thread>([&] {
			std::ifstream firmwareFile(firmwarePath, std::ios::binary);
			try
			{
				if (!firmwareFile.is_open()) {
					throw bootloader::STBootException("Cannot open firmware file!");
				}
				firmwareFile.seekg(0, firmwareFile.end);
				auto firmwareFileLenght = firmwareFile.tellg();
				if (firmwareFileLenght > 0xFFFF)
					throw bootloader::STBootException("Firmware file is too big");
				firmwareFile.seekg(0, firmwareFile.beg);
				auto dataBuffer = make_unique<uint8_t[]>(firmwareFileLenght);
				firmwareFile.read((char*)(dataBuffer.get()), firmwareFileLenght);
				firmwareFile.close();				
				//switch (deviceStatus()) {
				//	case DeviceStatus::APPLICATION:				
				//		jumpToBootloader();					
				//		break;
				//	case DeviceStatus::BOOTLOADER:									
				//		break;
				//	default:
				//		throw bootloader::STBootException("Firmware write aborted!");					
				//		break;
				//}
				authorize();
				//validateFimware(false);
				//UploadFile(selectedSerialPortName, dataBuffer.get(), firmwareFileLenght, userCodeAddress, userCodeAddress);
				//auto readedDataBuffer = make_unique<uint8_t[]>(firmwareFileLenght);
				//DownloadFile(selectedSerialPortName, readedDataBuffer.get(), firmwareFileLenght, userCodeAddress);
				//for (uint32_t i = 0; i < firmwareFileLenght; i++) {
				//	if (readedDataBuffer[i] != dataBuffer[i]) {
				//		throw bootloader::STBootException("Verification failed!");
				//	}
				//}
				//validateFimware(true);
				//sendMessageToConsole(string("Verifivation passed"));
				//jumpToApplication();
				// Wait some time to allow application code start
				//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			catch (const std::exception &e) {
				firmwareFile.close();
				handleException(e);
			}
			updateBootloaderFinished(true);
		});
	}

}

void controller::eraseFirmware() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (!bootloaderBusy) {
		bootloaderBusy = true;
		bootloaderStatusChanged(bootloaderBusy);
		bootProcessThread = make_unique<std::thread>([&] {
			try {
				switch (deviceStatus()) {
				case DeviceStatus::APPLICATION:
					jumpToBootloader();
					break;
				case DeviceStatus::BOOTLOADER:
					break;
				default:
					throw bootloader::STBootException("Firmware erase aborted!");
					break;
				}
				authorize();
				validateFimware(false);
				bootloader->close();
				bootloader->open(selectedSerialPortName);
				sendMessageToConsole(string("Erasing..."));
				bootloader->GlobalErase();
				sendMessageToConsole(string("Erase complete"));
			}
			catch (const std::exception &e) {
				bootloader->close();
				handleException(e);
			}
			updateBootloaderFinished(true);
		});
	}
}

void controller::updateFirmwarePath(std::string& filePath) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (firmwarePath != filePath) {
		firmwarePath = filePath;
		firmwarePathChanged(firmwarePath);
	}
}

void controller::updateSerialSelectedPort(std::string& selectedPort) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (selectedSerialPortName != selectedPort) {
		handleCloseSerialPort();
		selectedSerialPortName = selectedPort;
	}
}

void controller::openSerialPort() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	handleOpenSerialPort();
}

void controller::closeSerialPort() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	handleCloseSerialPort();
}

void controller::detectDevice() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (!bootloaderBusy) {
		bootloaderBusy = true;
		bootloaderStatusChanged(bootloaderBusy);
		bootProcessThread = make_unique<std::thread>([&] {
			try
			{
				auto deviceMode = deviceStatus();
				switch (deviceMode) {
					case DeviceStatus::APPLICATION:									
						break;
					case DeviceStatus::BOOTLOADER:										
						break;
					default:				
						break;
				}
			}
			catch (const std::exception &e) {
				handleException(e);
				bootloader->close();
			}
			updateBootloaderFinished(true);
		});
	}
}

void controller::detectSerialPorts() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	bootloader->findSerialPorts(serialPortsList);
	serialPortsListChanged(serialPortsList);
}

void controller::switchCode() {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (!bootloaderBusy) {
		bootloaderBusy = true;
		bootloaderStatusChanged(bootloaderBusy);
		bootProcessThread = make_unique<std::thread>([&] {
			try
			{
				switch (deviceStatus()) {
				case DeviceStatus::APPLICATION:
					jumpToBootloader();
					break;
				case DeviceStatus::BOOTLOADER:
					jumpToApplication();
					// Wait some time to allow application code start
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					break;
				default:
					break;
				}
			}
			catch (const std::exception &e) {
				bootloader->close();
				handleException(e);
			}			
			updateBootloaderFinished(true);
		});
	}
}

void controller::updateProgress(const bootloader::STBootProgress& progress) {
	updateProgressPercent(100 * progress.bytesProcessed / progress.bytesTotal);
}

uint8_t controller::updateProgressPercent(uint8_t progress) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (progressPercent != progress) {
		progressPercent = progress;
		progressPercentChanged(progress);
	}
	return 1;
}

void controller::updateBootloaderFinished(bool finished) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (bootloaderFinished != finished) {
		bootloaderFinished = finished;
	}
}

void controller::updatePageSize(std::string& size) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (pageSize != size) {
		pageSize = size;
	}
}

void controller::updateGlobalErase(bool isGlobalErase) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	if (globalErase != isGlobalErase) {
		globalErase = isGlobalErase;
	}
}

void controller::performNewMessage(std::string& message) {
	sendMessageToConsole(message);
}

void controller::updateUserCodeAddress(std::string& address) {
	std::unique_lock<std::mutex> lk(controllerMutex);
	uint32_t newAddress = std::stoul(address, nullptr, 16);
	newAddress = newAddress & 0xffffff00;
	if (newAddress != userCodeAddress) {
		if (newAddress < userCodeBaseAddress)
			newAddress = userCodeBaseAddress;
		userCodeAddress = newAddress;
		userCodePageNumber = (userCodeAddress - userCodeBaseAddress) / 256;
		userCodeAddressChanged(convertAddressToString(newAddress));
	}
}

void controller::process() {
	while (!initialised) {};
	updateUserCodeAddress(convertAddressToString(userCodeBaseAddress));
	detectSerialPorts();
	while (1) {
		{
			std::unique_lock<std::mutex> lk(controllerMutex);
			if (bootloaderFinished == true) {
				bootProcessThread->join();
				bootProcessThread.release();
				bootloaderFinished = false;
				bootloaderBusy = false;
				bootloaderStatusChanged(bootloaderBusy);
			}
		}
	}
}

/* upload a binary image to uC */
void controller::UploadFile(std::string portName, unsigned char* bin, uint32_t size, uint32_t address, uint32_t jumpAddress) {
	using namespace std::placeholders;
	/* get page size */
	uint32_t psize = stoi(pageSize);
	/* close device */
	bootloader->close();
	/* open device */
	bootloader->open(portName);
	/* initialize communication */
	bootloader->Initialize();
	/* update the status */
	std::ostringstream hexProductID;
	hexProductID << std::showbase << std::hex << std::uppercase << bootloader->ProductID;
	sendMessageToConsole("Connected: Ver: " + bootloader->Version + ", PID: " + hexProductID.str());
	/* give some chance see the message */
	std::this_thread::sleep_for(500ms);
	sendMessageToConsole(string("Erasing..."));

	if (globalErase) {
		bootloader->GlobalErase();
	}
	else {
		/* erase operation */
		for (uint32_t i = 0; i < size; i += psize) {
			/* erase page */
			bootloader->ErasePage((i + address - 0x08000000) / psize);
		}
	}
	/* apply new message */
	sendMessageToConsole(string("Programming..."));
	/* progress reporter */
	bootloader::STBootProgress p(0, size, std::bind(&controller::updateProgress, this, _1));
	/* write memory */
	bootloader->WriteMemory(address, bin, 0, size, p);
	/* update the status */
	sendMessageToConsole(string("Success: " + to_string(size) + " bytes written"));
	/* end communication */
	bootloader->close();
}

/* upload a binary image to uC */
void controller::DownloadFile(std::string portName, unsigned char* bin, uint32_t size, uint32_t address) {
	using namespace std::placeholders;
	/* get page size */
	uint32_t psize = stoi(pageSize);
	/* close device */
	bootloader->close();
	/* open device */
	bootloader->open(portName);
	/* initialize communication */
	bootloader->Initialize();
	/* update the status */
	std::ostringstream hexProductID;
	hexProductID << std::showbase << std::hex << std::uppercase << bootloader->ProductID;
	sendMessageToConsole("Connected: Ver: " + bootloader->Version + ", PID: " + hexProductID.str());
	/* give some chance see the message */
	std::this_thread::sleep_for(500ms);
	/* apply new message */
	sendMessageToConsole(string("Reading..."));
	/* progress reporter */
	bootloader::STBootProgress p(0, size, std::bind(&controller::updateProgress, this, _1));
	/* write memory */
	bootloader->ReadMemory(address, bin, 0, size, p);
	/* update the status */
	sendMessageToConsole(string("Success: " + to_string(size) + " bytes readed"));
	/* end communication */
	bootloader->close();
}

void controller::handleException(const std::exception &e) {
	std::string message = e.what() + 0x00;
	performNewMessage(message);
}

void controller::handleOpenSerialPort() {
	try {
		if (bootloader->isOpen()) {
			performNewMessage("Serial port " + selectedSerialPortName + " is already opened");
		}
		else {
			bootloader->open(selectedSerialPortName);
			performNewMessage("Serial port " + selectedSerialPortName + " opened");
		}
	}
	catch (const std::exception &e) {
		handleException(e);
	}
}

void controller::handleCloseSerialPort() {
	try {
		if (bootloader->isOpen()) {
			bootloader->close();
			performNewMessage("Serial port " + selectedSerialPortName + " closed");
		}
	}
	catch (const std::exception &e) {
		handleException(e);
	}
}

DeviceStatus controller::deviceStatus() {
	auto status = DeviceStatus::UNKNOWN;
	try {
		bootloader->close();
		bootloader->open(selectedSerialPortName);
		auto deviceWakedUp = sendCommunicationCommandWithNoException(bootloader::DeviceCommunicationCommands::ENABLE_PRODUCTION_MODE);
		if (deviceWakedUp) {
			bootloader->sendCommunicationCommand(bootloader::DeviceCommunicationCommands::DISABLE_PRODUCTION_MODE);
			sendMessageToConsole(string("Detected device in application mode!"));
			status = DeviceStatus::APPLICATION;
		}
		else {
			bootloader->Initialize();
			sendMessageToConsole(string("Detected device in bootloader mode!"));
			status = DeviceStatus::BOOTLOADER;
		}
		bootloader->close();
	} catch (const std::exception &e) {
		bootloader->close();
		sendMessageToConsole(string("No device detected!"));
	}
	return status;
}

void controller::jumpToBootloader() {
	sendMessageToConsole(string("Send jump to bootloader command"));
	bootloader->close();
	bootloader->open(selectedSerialPortName);
	bootloader->sendCommunicationCommand(bootloader::DeviceCommunicationCommands::ENABLE_PRODUCTION_MODE);
	bootloader->sendCommunicationCommand(bootloader::DeviceCommunicationCommands::JUMP_TO_BOOTLOADER);
	bootloader->close();
	sendMessageToConsole(string("Jump to bootloader success!"));
}

void controller::jumpToApplication() {
	sendMessageToConsole(string("Send jump to application command"));
	bootloader->close();
	bootloader->open(selectedSerialPortName);
	bootloader->Jump(userCodeAddress);
	sendMessageToConsole(string("Jump to application success"));
	bootloader->close();
}

bool controller::sendCommunicationCommandWithNoException(bootloader::DeviceCommunicationCommands command) {
	auto success = true;
	auto portWasOpen = bootloader->isOpen();
	try {
		bootloader->sendCommunicationCommand(command);
	} catch (const std::exception &e) {		
		bootloader->close();
		success = false;
	}
	if (portWasOpen) {
		if (!bootloader->isOpen()) {
			bootloader->open(selectedSerialPortName);
		}
	}
	return success;
}

void controller::validateFimware(bool validate) {
	constexpr uint32_t VALID_FIRMWARE_VALUE = 0x55555555;

	uint32_t commandCode = UINT32_MAX - 1;
	if (validate) {
		commandCode = UINT32_MAX;
	}
	bootloader->close();
	bootloader->open(selectedSerialPortName);
	bootloader->Write(commandCode, (uint8_t*)(&VALID_FIRMWARE_VALUE), 0, 4);
	bootloader->close();
}

void controller::authorize() {
	sendMessageToConsole(string("Send authorize command"));
	bootloader->close();
	bootloader->open(selectedSerialPortName);
	bootloader->Authorize();
	sendMessageToConsole(string("Authorization passed!"));
	bootloader->close();
}

}	// namespace controller