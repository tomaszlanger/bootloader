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
				/* read file */
				uint8_t bin[200];
				uint32_t length;
				
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
				UploadFile(selectedSerialPortName, dataBuffer.get(), firmwareFileLenght, userCodeAddress, userCodeAddress);		
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
				// wake up BMS
				// sett bootloader mode with confirmation

				selectedSerialPortName = "COM11";
				/* read file */
				uint8_t bin[200];
				uint32_t length;

				/* close device */
				bootloader->close();
				/* open device */
				bootloader->open(selectedSerialPortName);

				bootloader->wakeUp(bin, &length);

				std::stringstream message;
				for (int i = 0; i < length; ++i)
					message << std::hex << std::uppercase << (int)bin[i] << " ";
				sendMessageToConsole("Wake up response: " + message.str());

				bootloader->status(bin, &length);
				for (int i = 0; i < length; ++i)
					message << std::hex << std::uppercase << (int)bin[i] << " ";
				sendMessageToConsole("RX: " + message.str());

				//UploadFile(selectedSerialPortName, bin, sizeof(bin), userCodeAddress, userCodeAddress);
			}
			catch (const std::exception &e) {
				handleException(e);
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

void controller::runCode() {
	std::unique_lock<std::mutex> lk(controllerMutex);
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

		//	std::this_thread::sleep_for(10s);


		{
			//std::string message = "";
			//{
			//	std::unique_lock<std::mutex> lk(controllerMutex);
			//	if (!otherThreadsMessages.empty()) {
			//		message = otherThreadsMessages.front();
			//		otherThreadsMessages.pop();
			//	}
			//}

			//switch (message)
			//{
			//	case MSG_POST_USER_DATA:
			//		break;


			//	case MSG_TIMER:
			//		break;

			//	case MSG_EXIT_THREAD:
			//		break;

			//	default:
			//		break;
			//}


			//{
				//while {otherThreadsMessages.}


				//std::this_thread::sleep_for(10s);

				//std::unique_lock<std::mutex> lk(controllerMutex);

				//if (writeFirmwareFinished == true) {
				//	bootProcessThread->join();
				//	bootProcessThread.release();
				//	writeFirmwareFinished = false;
				//	bootloaderBusy = false;
				//	bootloaderStatusChanged(bootloaderBusy);
				//}
		}

		//while (iiii < 100)
		//{
		//	iiii++;
		//	info = "Example info Example info Example info Example info Example info Example info" +std::to_string(iiii);
		//	total += info.size();
		//	sendMessageToConsole(info);
		//}	
		//std::this_thread::sleep_for(std::chrono::seconds(10));
		//iiii = 0;
		uint8_t ssss = 44;
		//sendFirmwareUpdateProgress(ssss);
		{
			// Wait for a message to be added to the queue
			//std::unique_lock<std::mutex> lk(controllerMutex);
			//while (m_queue.empty())
			//	m_cv.wait(lk);

			//if (m_queue.empty())
			//	continue;

			//msg = m_queue.front();
			//m_queue.pop();
		}

		//	addConsoleInfo(info);
		//	iiii++;
		//}

		//if (sendInfo) {
		//	sendInfo = false;

		//	addConsoleInfo(info);
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
			/* update progress bar */
			sendMessageToConsole(to_string((int32_t)i * 100 / size));
		}
	}

	/* apply new message */
	sendMessageToConsole(string("Programming..."));
	/* progress reporter */
	bootloader::STBootProgress p(0, 0, std::bind(&controller::updateProgress, this, _1));
	/* write memory */
	bootloader->WriteMemory(address, bin, 0, size, p);
	/* update the status */
	sendMessageToConsole(string("Success: " + to_string(size) + " bytes written"));
	/* go! */
	bootloader->Jump(jumpAddress);
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

}	// namespace controller