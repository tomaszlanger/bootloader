#include "bootloader.h"
#include <string>
#include <mutex>
#include <chrono>
#include <thread>

namespace bootloader {


bootloader::bootloader() : sp(std::string(""), 38400, serial::Timeout::simpleTimeout(100)) {
}

bootloader::~bootloader() {
	close();
}

void bootloader::cancelOperation() {
	cancel = true;
}

void bootloader::findSerialPorts(std::list<std::string>& serialPortsList) {
	string currentOpenedPort = "";
	if (sp.isOpen()) {
		sp.close();
		currentOpenedPort = sp.getPort();
	}
	serialPortsList.clear();
	for (uint8_t i = 0; i < 255; i++) {
		sp.setPort("COM" + std::to_string(i));
		try {
			sp.open();
			sp.close();
			serialPortsList.push_back(sp.getPort());
		}
		catch (...) {}

	}
	if (!currentOpenedPort.empty()) {
		sp.setPort(currentOpenedPort);
		sp.open();
	}
}

/* open serial port */
void bootloader::open(string portName) {
	/* initialize serial port */
	sp.setPort(portName);
	sp.setParity(parity_even);
	sp.setBytesize(eightbits);
	/* open serial port */
	sp.open();
	/* discard input and output buffers */
	sp.flush();
}
bool bootloader::isOpen() {
	return sp.isOpen();

}
/* close */
void bootloader::close() {
	/* close permitted? */
	if (sp.isOpen())
		sp.close();
}
/* initialize communication */
void bootloader::Initialize()
{
	/* get version and command list */
	Get();

	/* no support for get id? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::GET_ID) == Commands.end()) {
		throw STBootException("Command not supported");
	}

	/* get product id */
	GetID();
}

/* unprotect memory */
void bootloader::Unprotect() {
	/* no support for unprotect? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::WR_UNPROTECT) == Commands.end()) {
		throw STBootException("Command not supported");
	}
	/* no support for unprotect? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::RD_UNPROTECT) == Commands.end()) {
		throw STBootException("Command not supported");
	}
}

/* read memory */
void bootloader::ReadMemory(uint32_t address, unsigned char* buf, int32_t offset,
	int32_t size, STBootProgress& p) {
	/* number of bytes read */
	int32_t bread = 0, btotal = size;
	/* no support for read? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::READ) == Commands.end()) {
		throw STBootException("Command not supported");
	}

	cancel = false;

	/* data is read in chunks */
	while (size > 0 && !cancel) {
		/* chunk size */
		int32_t csize = min(size, 256);
		/* read a single chunk */
		Read(address, buf, offset, csize);

		/* update iterators */
		size -= csize; offset += csize; address += (uint32_t)csize;
		/* update number of bytes read */
		bread += csize;

		/* report progress */
		p.update(bread, btotal);
	}

	/* throw exception if operation was cancelled */
	if (cancel)
		throw STBootException("Read cancelled");
}

/* write memory */
void bootloader::WriteMemory(uint32_t address, unsigned char* buf, int32_t offset,
	int32_t size, STBootProgress& p) {
	/* number of bytes written */
	int32_t bwritten = 0, btotal = size;

	cancel = false;

	/* no support for write? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::WRITE) == Commands.end()) {
		throw STBootException("Command not supported");
	}

	/* data is read in chunks */
	while (size > 0 && !cancel) {
		/* chunk size */
		int32_t csize = min(size, 256);
		/* read a single chunk */
		Write(address, buf, offset, csize);

		/* update iterators */
		size -= csize; offset += csize; address += (uint32_t)csize;
		/* update number of bytes read */
		bwritten += csize;

		/* report progress */
		p.update(bwritten, btotal);
	}

	/* throw exception if operation was cancelled */
	if (cancel)
		throw STBootException("Write cancelled");
}

/* erase page */
void bootloader::ErasePage(uint32_t pageNumber) {
	/* 'classic' erase operation supported? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::ERASE) != Commands.end()) {
		Erase(pageNumber);
		/* 'extended' erase operation supported? */
	}
	else if (std::find(Commands.begin(), Commands.end(), STCmds::EXT_ERASE) != Commands.end()) {
		ExtendedErase(pageNumber);
		/* no operation supported */
	}
	else {
		throw STBootException("Command not supported");
	}
}

/* perform global erase */
void bootloader::GlobalErase() {
	/* 'classic' erase operation supported? */

	/* 'classic' erase operation supported? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::ERASE) != Commands.end()) {
		EraseSpecial(STEraseMode::GLOBAL);
		/* 'extended' erase operation supported? */
	}
	else if (std::find(Commands.begin(), Commands.end(), STCmds::EXT_ERASE) != Commands.end()) {
		ExtendedEraseSpecial(STExtendedEraseMode::GLOBAL);
		/* no operation supported */
	}
	else {
		throw STBootException("Command not supported");
	}
}

/* jump to user code */
void bootloader::Jump(uint32_t address) {
	/* no support for go? */
	if (std::find(Commands.begin(), Commands.end(), STCmds::GO) == Commands.end()) {
		throw STBootException("Command not supported");
	}
	/* go! */
	Go(address);
}

void bootloader::sendCommunicationCommand(DeviceCommunicationCommands command) {
	const uint8_t enableProductionModeCommand[8] = { 0xAA,0xAA,0xAA,0x08,0x01,0x00,0x98,0xA1 };
	const uint8_t disableProductionModeCommand[8] = { 0xAA,0xAA,0xAA,0x08,0x02,0x00,0x68,0xA1 };
	const uint8_t jumpToBootloaderCommand[8] = { 0xAA,0xAA,0xAA,0x08,0x0A,0x00,0xA8,0xA6 };
	const uint8_t wakeUpChar[1] = { 0x00 };
	uint8_t rx[1];
	sp.write(wakeUpChar, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	switch (command) {
	case DeviceCommunicationCommands::ENABLE_PRODUCTION_MODE:
		sp.write(enableProductionModeCommand, 8);
		break;
	case DeviceCommunicationCommands::DISABLE_PRODUCTION_MODE:
		sp.write(disableProductionModeCommand, 8);
		break;
	case DeviceCommunicationCommands::JUMP_TO_BOOTLOADER:
		sp.write(jumpToBootloaderCommand, 8);
		break;
	default:
		break;
	}
	if (read(rx, 1) == 0)
		throw STBootException("No BMS command response!");
	if ((rx[0] != 0xE2))
		throw STBootException(" Bad BMS command response!");
}

const int16_t cryptoKeyBmsN = 2651;
const int16_t cryptoKeyBmsE = 7;
const int16_t cryptoKeyBmsD = 2743;

const int16_t cryptoKeyHostN = 1243;
const int16_t cryptoKeyHostE = 3;
const int16_t cryptoKeyHostD = 1867;

/* Authorize procedure */
void bootloader::Authorize()
{
	uint8_t tx[16];
	/* temporary storage for response bytes */
	uint8_t tmp[17];
	/* rx buffer */
	string rx = "";

	/* store code */
	tx[0] = (uint8_t)STCmds::RANDOMIZE;
	/* set checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);

	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* wait for random value */
	read(tmp, 8);

	/* decrypt received random value */
	uint32_t randomValue = 0;
	decrypt((int16_t*)(tmp), (uint8_t*)(&randomValue), 4, cryptoKeyBmsN, cryptoKeyBmsD);
	/* encrypt received random value with host keys */
	encrypt((uint8_t*)(&randomValue), (int16_t*)(tx), 4, cryptoKeyHostN, cryptoKeyHostE);

	/* set checksum from encrypted random value */
	crc = ComputeChecksum(tx, 0, 8);
	tx[8] = (uint8_t)(crc >> 8);
	tx[9] = (uint8_t)(crc & 0x00FF);

	/* send bytes */
	sp.write(tx, 10);
	/* wait for response code */
	auto currentTimeout = sp.getTimeout();
	sp.setTimeout(serial::Timeout::simpleTimeout(1000));
	/* wait for response code */
	read(tmp, 1);
	/* rocever timeout to previous value */
	sp.setTimeout(currentTimeout);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");
}

void bootloader::Get() {
	uint8_t tx[3];
	/* temporary storage for response bytes */
	uint8_t tmp[1];
	/* numbe or response bytes */
	int nbytes;
	/* rx buffer */
	string rx = "";

	/* store code */
	tx[0] = (uint8_t)STCmds::GET;
	/* set checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);
	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);

	/* wait for response code */
	auto  dddd = read(tmp, 1);

	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* wait for number of bytes */
	read(tmp, 1);
	/* assign number of bytes that will follow (add for acks) */
	nbytes = tmp[0] + 2;
	/* nbytes must be equal to 13 for stm32 products */
	if (nbytes != 10)
		throw STBootException("Invalid length");

	/* receive response */
	read(rx, nbytes - 1);

	///* store version information */
	Version = to_string((rx[0] >> 4)) + "." + to_string((rx[0] & 0xf));

	///* add all commands */
	Commands.clear();
	for (uint16_t i = 1; i < nbytes - 1; i++)
		Commands.push_back((STCmds)(uint8_t)(rx[i]));
}

/* get id command */
void bootloader::GetID()
{
	uint8_t tx[3];
	/* temporary storage for response bytes */
	uint8_t tmp[1];
	/* numbe or response bytes */
	int nbytes;
	/* rx buffer */
	string rx = "";

	/* store code */
	tx[0] = (uint8_t)STCmds::GET_ID;
	/* set checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);

	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* wait for number of bytes */
	read(tmp, 1);
	/* assign number of bytes that will follow (add for acks) */
	nbytes = tmp[0] + 2;
	/* nbytes must be equal to 3 for stm32 products */
	if (nbytes != 3)
		throw STBootException("Invalid length");

	/* receive response */
	read(rx, nbytes - 1);

	/* store product id */
	ProductID = (USHORT)((rx[0] << 8) | (rx[1]));
}

void bootloader::Read(uint32_t address, unsigned char* buf, int32_t offset, int32_t length) {
	uint8_t tx[12];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::READ;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* store address */
	tx[3] = (uint8_t)((address >> 24) & 0xff);
	tx[4] = (uint8_t)((address >> 16) & 0xff);
	tx[5] = (uint8_t)((address >> 8) & 0xff);
	tx[6] = (uint8_t)(address & 0xff);
	/* address checksum (needs to be not negated. why? because ST!
	 * that's why. */
	crc = ComputeChecksum(tx, 3, 4);
	tx[7] = (uint8_t)(crc >> 8);
	tx[8] = (uint8_t)(crc & 0x00FF);

	/* store number of bytes */
	tx[9] = (uint8_t)(length - 1);
	/* size checksum */
	crc = ComputeChecksum(tx, 9, 1);
	tx[10] = (uint8_t)(crc >> 8);
	tx[11] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* send address */
	sp.write(tx + 3, 6);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Address Rejected");

	sp.flush();
	/* send address */
	sp.write(tx + 9, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Size Rejected");

	/* receive response */
	read(buf + offset, length);
}

/* go command */
void bootloader::Go(uint32_t address) {
	uint8_t tx[9];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::GO;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* store address */
	tx[3] = (uint8_t)((address >> 24) & 0xff);
	tx[4] = (uint8_t)((address >> 16) & 0xff);
	tx[5] = (uint8_t)((address >> 8) & 0xff);
	tx[6] = (uint8_t)(address & 0xff);
	/* address checksum (needs to be not negated. why? because ST!
	 * that's why. */
	crc = ComputeChecksum(tx, 3, 4);
	tx[7] = (uint8_t)(crc >> 8);
	tx[8] = (uint8_t)(crc & 0x00FF);

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* send address */
	sp.write(tx + 3, 6);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Address Rejected");
}

/* write memory */
void bootloader::Write(uint32_t address, unsigned char* data, int32_t offset, int32_t length) {
	bool success = false;
	uint8_t writeTries = 3;
	do {
		/* command word */
		uint8_t tx[12];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::WRITE;
		/* checksum */
		auto crc = ComputeChecksum(tx, 0, 1);
		tx[1] = (uint8_t)(crc >> 8);
		tx[2] = (uint8_t)(crc & 0x00FF);

		/* store address */
		tx[3] = (uint8_t)((address >> 24) & 0xff);
		tx[4] = (uint8_t)((address >> 16) & 0xff);
		tx[5] = (uint8_t)((address >> 8) & 0xff);
		tx[6] = (uint8_t)(address & 0xff);
		/* address checksum (needs to be not negated. why? because ST!
		 * that's why. */
		crc = ComputeChecksum(tx, 3, 4);
		tx[7] = (uint8_t)(crc >> 8);
		tx[8] = (uint8_t)(crc & 0x00FF);

		/* number of bytes */
		tx[9] = (uint8_t)(length - 1);
		/* data checksum */
		crc = ComputeChecksumWithPrecedingByte(data, offset, length, tx[9]);
		tx[10] = (uint8_t)(crc >> 8);
		tx[11] = (uint8_t)(crc & 0x00FF);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 3);
		/* wait for response code */
		read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw STBootException("Command Rejected");

		/* send address */
		sp.write(tx + 3, 6);
		/* wait for response code */
		read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw STBootException("Address Rejected");

		/* send the number of bytes */
		sp.write(tx + 9, 1);
		/* send data */
		sp.write(data + offset, length);
		/* send checksum */
		sp.write(tx + 10, 2);
		auto currentTimeout = sp.getTimeout();
		sp.setTimeout(serial::Timeout::simpleTimeout(2000));
		/* wait for response code */
		read(tmp, 1);
		/* rocever timeout to previous value */
		sp.setTimeout(currentTimeout);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK) {
			writeTries--;
		}
		else {
			success = true;
		}
	} while (writeTries && !success);

	if (!success) {
		throw STBootException("Data Rejected");
	}
}

/* erase memory page */
void bootloader::Erase(uint32_t pageNumber) {
	uint8_t tx[7];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::ERASE;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* erase single page */
	tx[3] = 0;
	/* set page number */
	tx[4] = (uint8_t)pageNumber;
	/* checksum */
	crc = ComputeChecksum(tx, 3, 2);
	tx[5] = (uint8_t)(crc >> 8);
	tx[6] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* send address */
	sp.write(tx + 3, 4);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Page Rejected");
}

/* erase memory page */
void bootloader::EraseSpecial(STEraseMode mode) {
	uint8_t tx[6];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::ERASE;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* erase single page */
	tx[3] = (uint8_t)((int)mode);
	/* checksum */
	crc = ComputeChecksum(tx, 3, 1);
	tx[4] = (uint8_t)(crc >> 8);
	tx[5] = (uint8_t)(crc & 0x00FF);
	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* send address */;
	sp.write(tx + 3, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Special Code Rejected");
}

/* extended erase memory page */
void bootloader::ExtendedErase(uint32_t pageNumber) {
	uint8_t tx[9];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::EXT_ERASE;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* erase single page */
	tx[3] = 0;
	tx[4] = 0;
	/* set page number */
	tx[5] = (uint8_t)(pageNumber >> 8);
	tx[6] = (uint8_t)(pageNumber >> 0);
	/* checksum */
	crc = ComputeChecksum(tx, 3, 4);
	tx[7] = (uint8_t)(crc >> 8);
	tx[8] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* send address */
	sp.write(tx + 3, 6);
	/* wait for response code. use longer timeout, erase might
		* take a while or two. */
	read(tmp, 1 /*,3000*/);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Page Rejected");
}

/* extended erase memory page */
void bootloader::ExtendedEraseSpecial(STExtendedEraseMode mode) {
	uint8_t tx[7];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::EXT_ERASE;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* erase single page */
	tx[3] = (uint8_t)((int)mode >> 8);
	tx[4] = (uint8_t)((int)mode >> 0);
	/* checksum */
	crc = ComputeChecksum(tx, 3, 2);
	tx[5] = (uint8_t)(crc >> 8);
	tx[6] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Command Rejected");

	/* send address */
	sp.write(tx + 3, 4);
	/* wait for response code. use longer timeout, erase might
		* take a while or two. */
	auto currentTimeout = sp.getTimeout();
	sp.setTimeout(serial::Timeout::simpleTimeout(2000));
	read(tmp, 1);
	/* rocever timeout to previous value */
	sp.setTimeout(currentTimeout);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Special code Rejected");
}

/* unprotect flash before writing */
void bootloader::WriteUnprotect() {
	/* command word */
	uint8_t tx[3];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::WR_UNPROTECT;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code */
	read(tmp, 1);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Write Unprotect Rejected");
}

/* unprotect flash before reading */
void bootloader::ReadUnprotect() {
	/* command word */
	uint8_t tx[3];
	/* temporary storage for response bytes */
	uint8_t tmp[1];

	/* command code */
	tx[0] = (uint8_t)STCmds::RD_UNPROTECT;
	/* checksum */
	auto crc = ComputeChecksum(tx, 0, 1);
	tx[1] = (uint8_t)(crc >> 8);
	tx[2] = (uint8_t)(crc & 0x00FF);

	/* try to send command and wait for response */

	/* send bytes */
	sp.write(tx, 3);
	/* wait for response code. use longer timeout, erase might
		* take a while or two. */
	read(tmp, 1 /*,10000*/);
	/* check response code */
	if (tmp[0] != (uint8_t)STResps::ACK)
		throw STBootException("Read Unprotect Rejected");
}

uint16_t bootloader::ComputeChecksum(const unsigned char* data, int32_t offset, int32_t count) {
	uint16_t crc = 0x0000;
	for (int32_t i = offset; i < count + offset; i++) {
		crc = crc ^ data[i];
		for (int32_t i = 0; i < 8; i++) {
			if (crc & 0x01) crc = (crc >> 1) ^ 0xA001;
			else crc = (crc >> 1);
		}
	}
	return crc;
}

uint16_t bootloader::ComputeChecksumWithPrecedingByte(const unsigned char* data, int32_t offset, int32_t count, uint8_t lastByte) {
	uint16_t crc = 0x0000;
	crc = crc ^ lastByte;
	for (int32_t i = 0; i < 8; i++) {
		if (crc & 0x01) crc = (crc >> 1) ^ 0xA001;
		else crc = (crc >> 1);
	}
	for (int32_t i = offset; i < count + offset; i++) {
		crc = crc ^ data[i];
		for (int32_t i = 0; i < 8; i++) {
			if (crc & 0x01) crc = (crc >> 1) ^ 0xA001;
			else crc = (crc >> 1);
		}
	}
	return crc;
}

uint16_t bootloader::ComputeChecksum(uint16_t crc, uint8_t byte) {
	uint16_t i = 0;
	crc = crc ^ byte;
	for (i = 0; i < 8; i++) {
		if (crc & 0x01) crc = (crc >> 1) ^ 0xA001;
		else crc = (crc >> 1);
	}
	return  crc;
}

/* read from serial port and throw exception in case of timeout */
size_t bootloader::read(uint8_t *buffer, size_t size) {
	if (sp.read(buffer, size) != size) {
		throw STBootException("Read Timeout");
	}
	return size;
}

size_t bootloader::read(std::string &buffer, size_t size) {
	auto bytesBuffer = make_unique<uint8_t[]>(size);
	size_t bytesRead = 0;
	bytesRead = read(bytesBuffer.get(), size);
	buffer.append(reinterpret_cast<const char*>(bytesBuffer.get()), bytesRead);
	return bytesRead;
}

void bootloader::encrypt(uint8_t* source, int16_t* destination, uint8_t length, int16_t n, int16_t e) {
	int32_t pt, ct, k;
	int32_t i = 0, j = 0;
	while (i != length) {
		pt = source[i];
		pt = pt - SEED_OFFSET;
		k = 1;
		for (j = 0; j < e; j++) {
			k = k * pt;
			k = k % n;
		}
		ct = k + SEED_OFFSET;
		destination[i] = ct;
		i++;
	}
}

void bootloader::decrypt(int16_t* source, uint8_t* destination, uint8_t length, int16_t n, int16_t d) {
	int32_t pt, ct, k;
	int32_t i = 0, j = 0;
	while (i != length) {
		ct = source[i] - SEED_OFFSET;
		k = 1;
		for (j = 0; j < d; j++) {
			k = k * ct;
			k = k % n;
		}
		pt = k + SEED_OFFSET;
		destination[i] = pt;
		i++;
	}
}

}  // namespace bootloader