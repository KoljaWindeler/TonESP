#include "DFMiniMp3.h"

DFMiniMp3::DFMiniMp3(){
	_lastSendSpace = c_msSendSpace;
	_isOnline = true;
	mySerial = new SoftwareSerial(12, 13); // RX, TX, gpio 12 == D6, 13 == D7

}

void DFMiniMp3::begin(){
	mySerial->begin(9600);
	mySerial->setTimeout(10000);
	_lastSend = millis();
}


uint16_t DFMiniMp3::loop()
{
		while (mySerial->available() >= DfMp3_Packet_SIZE)
		{
				return listenForReply(0x00);
		}
		return 0;
}

// the track as enumerated across all folders
void DFMiniMp3::playGlobalTrack(uint16_t track)
{
		sendPacket(0x03, track);
}

// sd:/mp3/####track name
void DFMiniMp3::playMp3FolderTrack(uint16_t track)
{
		sendPacket(0x12, track);
}

// sd:/###/###track name
void DFMiniMp3::playFolderTrack(uint8_t folder, uint8_t track)
{
		uint16_t arg = (folder << 8) | track;
		sendPacket(0x0f, arg);
}

// sd:/##/####track name
// track number must be four digits, zero padded
void DFMiniMp3::playFolderTrack16(uint8_t folder, uint16_t track)
{
		uint16_t arg = (((uint16_t)folder) << 12) | track;
		sendPacket(0x14, arg);
}

void DFMiniMp3::playRandomTrackFromAll()
{
		sendPacket(0x18);
}

void DFMiniMp3::nextTrack()
{
		sendPacket(0x01);
}

void DFMiniMp3::prevTrack()
{
		sendPacket(0x02);
}

uint16_t DFMiniMp3::getCurrentTrack()
{
		sendPacket(0x4c);
		return listenForReply(0x4c);
}

// 0- 30
void DFMiniMp3::setVolume(uint8_t volume)
{
		sendPacket(0x06, volume);
}

uint8_t DFMiniMp3::getVolume()
{
		sendPacket(0x43);
		return listenForReply(0x43);
}

void DFMiniMp3::increaseVolume()
{
		sendPacket(0x04);
}

void DFMiniMp3::decreaseVolume()
{
		sendPacket(0x05);
}

// useless, removed
// 0-31
/*
void setVolume(bool mute, uint8_t volume)
{
		uint16_t arg = (!mute << 8) | volume;
		sendPacket(0x10, arg);
}
*/

void DFMiniMp3::loopGlobalTrack(uint16_t globalTrack)
{
		sendPacket(0x08, globalTrack);
}

DfMp3_PlaybackMode DFMiniMp3::getPlaybackMode()
{
		sendPacket(0x45);
		return (DfMp3_PlaybackMode)listenForReply(0x45);
}

void DFMiniMp3::setRepeatPlay(bool repeat)
{
		sendPacket(0x11, !!repeat);
}


void DFMiniMp3::setEq(DfMp3_Eq eq)
{
		sendPacket(0x07, eq);
}

DfMp3_Eq DFMiniMp3::getEq()
{
		sendPacket(0x44);
		return (DfMp3_Eq)listenForReply(0x44);
}


void DFMiniMp3::setPlaybackSource(DfMp3_PlaySource source)
{
		sendPacket(0x09, source, 200);
}

void DFMiniMp3::sleep()
{
		sendPacket(0x0a);
}

void DFMiniMp3::reset()
{
		sendPacket(0x0c, 0, 600);
		_isOnline = false;
}

void DFMiniMp3::start()
{
		sendPacket(0x0d);
}

void DFMiniMp3::pause()
{
		sendPacket(0x0e);
}

void DFMiniMp3::stop()
{
		sendPacket(0x16);
}

uint16_t DFMiniMp3::getStatus()
{
		sendPacket(0x42);
		return listenForReply(0x42);
}

uint16_t DFMiniMp3::getFolderTrackCount(uint16_t folder)
{
		sendPacket(0x4e, folder);
		return listenForReply(0x4e);
}

uint16_t DFMiniMp3::getTotalTrackCount()
{
		sendPacket(0x48);
		return listenForReply(0x48);
}

uint16_t DFMiniMp3::getTotalFolderCount()
{
		sendPacket(0x4F);
		return listenForReply(0x4F);
}

// sd:/advert/####track name
void DFMiniMp3::playAdvertisement(uint16_t track)
{
		sendPacket(0x13, track);
}

void DFMiniMp3::stopAdvertisement()
{
		sendPacket(0x15);
}

void DFMiniMp3::sendPacket(uint8_t command, uint16_t arg, uint16_t sendSpaceNeeded)
{
		uint8_t out[DfMp3_Packet_SIZE] = { 0x7E, 0xFF, 06, command, 00, (uint8_t)(arg >> 8), (uint8_t)(arg & 0x00ff), 00, 00, 0xEF };

		setChecksum(out);

		// wait for spacing since last send
		while (((millis() - _lastSend) < _lastSendSpace) || !_isOnline)
		{
				// check for event messages from the device while
				// we wait
				loop();
				delay(1);
		}

		_lastSendSpace = sendSpaceNeeded;
		mySerial->write(out, DfMp3_Packet_SIZE);

		_lastSend = millis();
}

bool DFMiniMp3::readPacket(uint8_t* command, uint16_t* argument)
{
		uint8_t in[DfMp3_Packet_SIZE] = { 0 };
		uint8_t read;

		// init our out args always
		*command = 0;
		*argument = 0;

		// try to sync our reads to the packet start
		do
		{
				// we use readBytes as it gives us the standard timeout
				read = mySerial->readBytes(&(in[DfMp3_Packet_StartCode]), 1);
				if (read != 1)
				{
						// nothing read
						return false;
				}
		} while (in[DfMp3_Packet_StartCode] != 0x7e);

		read += mySerial->readBytes(in + 1, DfMp3_Packet_SIZE - 1);
		if (read < DfMp3_Packet_SIZE)
		{
				// not enough bytes, corrupted packet
				return false;
		}

		if (in[DfMp3_Packet_Version] != 0xFF ||
				in[DfMp3_Packet_Length] != 0x06 ||
				in[DfMp3_Packet_EndCode] != 0xef )
		{
				// invalid version or corrupted packet
				return false;
		}

		if (!validateChecksum(in))
		{
				// checksum failed, corrupted paket
				return false;
		}

		*command = in[DfMp3_Packet_Command];
		*argument = ((in[DfMp3_Packet_HiByteArgument] << 8) | in[DfMp3_Packet_LowByteArgument]);

		return true;
}

uint16_t DFMiniMp3::listenForReply(uint8_t command)
{
		uint8_t replyCommand = 0;
		uint16_t replyArg = 0;

		do
		{
				if (readPacket(&replyCommand, &replyArg))
				{
						if (command != 0 && command == replyCommand)
						{
								return replyArg;
						}
						else
						{
								switch (replyCommand)
								{
								case 0x3d:
										return DFMP3_PLAY_FINISHED;
										break;

								case 0x3F:
										if (replyArg & 0x02)
										{
												_isOnline = true;
												return DFMP3_CARD_ONLINE;
										}
										break;

								case 0x3A:
										if (replyArg & 0x02)
										{
												return DFMP3_CARD_INSERTED;
										}
										break;

								case 0x3B:
										if (replyArg & 0x02)
										{
												return DFMP3_CARD_REMOVED;
										}
										break;

								case 0x40:
										return DFMP3_ERROR_GENERAL;
										break;

								default:
										// unknown/unsupported command reply
										break;
								}
						}
				}
				else
				{
						if (command != 0)
						{
								return DFMP3_ERROR_GENERAL;
								if (mySerial->available() == 0)
								{
										return 0;
								}
						}
				}
		} while (command != 0);

		return 0;
}

uint16_t DFMiniMp3::calcChecksum(uint8_t* packet)
{
		uint16_t sum = 0;
		for (int i = DfMp3_Packet_Version; i < DfMp3_Packet_HiByteCheckSum; i++)
		{
				sum += packet[i];
		}
		return -sum;
}

void DFMiniMp3::setChecksum(uint8_t* out)
{
		uint16_t sum = calcChecksum(out);

		out[DfMp3_Packet_HiByteCheckSum] = (sum >> 8);
		out[DfMp3_Packet_LowByteCheckSum] = (sum & 0xff);
}

bool DFMiniMp3::validateChecksum(uint8_t* in)
{
		uint16_t sum = calcChecksum(in);
		return (sum == ((in[DfMp3_Packet_HiByteCheckSum] << 8) | in[DfMp3_Packet_LowByteCheckSum]));
}
