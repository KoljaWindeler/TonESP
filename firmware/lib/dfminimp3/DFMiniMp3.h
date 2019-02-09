#ifndef DFMINI_h
#define DFMINI_h
/*-------------------------------------------------------------------------
DFMiniMp3 library

Written by Michael C. Miller.

I invest time and resources providing this open source code,
please support me by dontating (see https://github.com/Makuna/DFMiniMp3)

-------------------------------------------------------------------------
This file is part of the Makuna/DFMiniMp3 library.

DFMiniMp3 is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

NeoPixelBus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with NeoPixel.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/

#include <Arduino.h>
#include "../espsoftwareserial/SoftwareSerial.h"


#define DFMP3_PLAY_FINISHED 1
#define DFMP3_CARD_ONLINE		2
#define DFMP3_CARD_INSERTED	3
#define DFMP3_CARD_REMOVED	4
#define DFMP3_ERROR_GENERAL	5

enum DfMp3_Error
{
    DfMp3_Error_Busy = 1,
    DfMp3_Error_Sleeping,
    DfMp3_ErrormySoftwareSerialWrongStack,
    DfMp3_Error_CheckSumNotMatch,
    DfMp3_Error_FileIndexOut,
    DfMp3_Error_FileMismatch,
    DfMp3_Error_Advertise,
    DfMp3_Error_General = 0xff
};


enum DfMp3_PlaybackMode
{
    DfMp3_PlaybackMode_Repeat,
    DfMp3_PlaybackMode_FolderRepeat,
    DfMp3_PlaybackMode_SingleRepeat,
    DfMp3_PlaybackMode_Random
};


enum DfMp3_Eq
{
    DfMp3_Eq_Normal,
    DfMp3_Eq_Pop,
    DfMp3_Eq_Rock,
    DfMp3_Eq_Jazz,
    DfMp3_Eq_Classic,
    DfMp3_Eq_Bass
};


enum DfMp3_PlaySource
{
    DfMp3_PlaySource_U,
    DfMp3_PlaySource_Sd,
    DfMp3_PlaySource_Aux,
    DfMp3_PlaySource_Sleep,
    DfMp3_PlaySource_Flash
};


class DFMiniMp3
{
public:
    DFMiniMp3();
    void begin();
    uint16_t loop();
		void playGlobalTrack(uint16_t track = 0);
    void playMp3FolderTrack(uint16_t track);
    void playFolderTrack(uint8_t folder, uint8_t track);
    void playFolderTrack16(uint8_t folder, uint16_t track);
		void playRandomTrackFromAll();
    void nextTrack();
    void prevTrack();
    uint16_t getCurrentTrack();
    void setVolume(uint8_t volume);
    uint8_t getVolume();
    void increaseVolume();
    void decreaseVolume();
    void loopGlobalTrack(uint16_t globalTrack);
    DfMp3_PlaybackMode getPlaybackMode();
    void setRepeatPlay(bool repeat);
    void setEq(DfMp3_Eq eq);
    DfMp3_Eq getEq();
    void setPlaybackSource(DfMp3_PlaySource source);
    void sleep();
    void reset();
    void start();
    void pause();
    void stop();
    uint16_t getStatus();
    uint16_t getFolderTrackCount(uint16_t folder);
    uint16_t getTotalTrackCount();
    uint16_t getTotalFolderCount();
    void playAdvertisement(uint16_t track);
    void stopAdvertisement();

		uint8_t max_folder = 0;
		uint16_t max_file_in_folder = 0;

		uint8_t status = 0;

private:
    static const uint16_t c_msSendSpace = 50;
		SoftwareSerial* mySerial; // RX, TX
    // 7E FF 06 0F 00 01 01 xx xx EF
    // 0	->	7E is start code
    // 1	->	FF is version
    // 2	->	06 is length
    // 3	->	0F is command
    // 4	->	00 is no receive
    // 5~6	->	01 01 is argument
    // 7~8	->	checksum = 0 - ( FF+06+0F+00+01+01 )
    // 9	->	EF is end code
    enum DfMp3_Packet
    {
        DfMp3_Packet_StartCode,
        DfMp3_Packet_Version,
        DfMp3_Packet_Length,
        DfMp3_Packet_Command,
        DfMp3_Packet_RequestAck,
        DfMp3_Packet_HiByteArgument,
        DfMp3_Packet_LowByteArgument,
        DfMp3_Packet_HiByteCheckSum,
        DfMp3_Packet_LowByteCheckSum,
        DfMp3_Packet_EndCode,
        DfMp3_Packet_SIZE
    };

    uint32_t _lastSend;
    uint16_t _lastSendSpace;
    bool _isOnline;

    void sendPacket(uint8_t command, uint16_t arg = 0, uint16_t sendSpaceNeeded = c_msSendSpace);
    bool readPacket(uint8_t* command, uint16_t* argument);
    uint16_t listenForReply(uint8_t command);
    uint16_t calcChecksum(uint8_t* packet);
    void setChecksum(uint8_t* out);
    bool validateChecksum(uint8_t* in);
};

#endif
