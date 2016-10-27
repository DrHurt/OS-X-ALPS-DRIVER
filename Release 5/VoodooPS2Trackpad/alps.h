/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLEPS2SYNAPTICSTOUCHPAD_H
#define _APPLEPS2SYNAPTICSTOUCHPAD_H

#include "ApplePS2MouseDevice.h"
#include "ApplePS2Device.h"
#include "VoodooPS2TouchPadBase.h"

#define ALPS_PROTO_V1	0x100
#define ALPS_PROTO_V2	0x200
#define ALPS_PROTO_V3	0x300
#define ALPS_PROTO_V3_RUSHMORE	0x310
#define ALPS_PROTO_V4	0x400
#define ALPS_PROTO_V5	0x500

#define MAX_TOUCHES     4

/**
 * struct alps_model_info - touchpad ID table
 * @signature: E7 response string to match.
 * @command_mode_resp: For V3/V4 touchpads, the final byte of the EC response
 *   (aka command mode response) identifies the firmware minor version.  This
 *   can be used to distinguish different hardware models which are not
 *   uniquely identifiable through their E7 responses.
 * @proto_version: Indicates V1/V2/V3/...
 * @byte0: Helps figure out whether a position report packet matches the
 *   known format for this model.  The first byte of the report, ANDed with
 *   mask0, should match byte0.
 * @mask0: The mask used to check the first byte of the report.
 * @flags: Additional device capabilities (passthrough port, trackstick, etc.).
 *
 * Many (but not all) ALPS touchpads can be identified by looking at the
 * values returned in the "E7 report" and/or the "EC report."  This table
 * lists a number of such touchpads.
 */
struct alps_model_info {
    UInt8 signature[3];
    UInt8 command_mode_resp;
    UInt16 proto_version;
    UInt8 byte0, mask0;
    UInt8 flags;
};

/**
 * struct alps_nibble_commands - encodings for register accesses
 * @command: PS/2 command used for the nibble
 * @data: Data supplied as an argument to the PS/2 command, if applicable
 *
 * The ALPS protocol uses magic sequences to transmit binary data to the
 * touchpad, as it is generally not OK to send arbitrary bytes out the
 * PS/2 port.  Each of the sequences in this table sends one nibble of the
 * register address or (write) data.  Different versions of the ALPS protocol
 * use slightly different encodings.
 */
struct alps_nibble_commands {
    SInt32 command;
    UInt8 data;
};

struct alps_bitmap_point {
    int start_bit;
    int num_bits;
};

struct input_mt_pos {
    UInt32 x;
    UInt32 y;
};

/**
 * struct alps_fields - decoded version of the report packet
 * @x_map: Bitmap of active X positions for MT.
 * @y_map: Bitmap of active Y positions for MT.
 * @fingers: Number of fingers for MT.
 * @pressure: Pressure.
 * @st: position for ST.
 * @mt: position for MT.
 * @first_mp: Packet is the first of a multi-packet report.
 * @is_mp: Packet is part of a multi-packet report.
 * @left: Left touchpad button is active.
 * @right: Right touchpad button is active.
 * @middle: Middle touchpad button is active.
 * @ts_left: Left trackstick button is active.
 * @ts_right: Right trackstick button is active.
 * @ts_middle: Middle trackstick button is active.
 */
struct alps_fields {
    UInt32 x_map;
    UInt32 y_map;
    UInt32 fingers;
    
    int pressure;
    struct input_mt_pos st;
    struct input_mt_pos mt[MAX_TOUCHES];
    
    UInt32 first_mp:1;
    UInt32 is_mp:1;
    
    UInt32 left:1;
    UInt32 right:1;
    UInt32 middle:1;
    
    UInt32 ts_left:1;
    UInt32 ts_right:1;
    UInt32 ts_middle:1;
};

class ApplePS2ALPSGlidePoint;

/**
 * struct alps_data - private data structure for the ALPS driver
 * @nibble_commands: Command mapping used for touchpad register accesses.
 * @addr_command: Command used to tell the touchpad that a register address
 *   follows.
 * @proto_version: Indicates V1/V2/V3/...
 * @byte0: Helps figure out whether a position report packet matches the
 *   known format for this model.  The first byte of the report, ANDed with
 *   mask0, should match byte0.
 * @mask0: The mask used to check the first byte of the report.
 * @fw_ver: cached copy of firmware version (EC report)
 * @flags: Additional device capabilities (passthrough port, trackstick, etc.).
 * @x_max: Largest possible X position value.
 * @y_max: Largest possible Y position value.
 * @x_bits: Number of X bits in the MT bitmap.
 * @y_bits: Number of Y bits in the MT bitmap.
 * @prev_fin: Finger bit from previous packet.
 * @multi_packet: Multi-packet data in progress.
 * @multi_data: Saved multi-packet data.
 * @f: Decoded packet data fields.
 * @quirks: Bitmap of ALPS_QUIRK_*.
 */
struct alps_data {
    /* these are autodetected when the device is identified */
    const struct alps_nibble_commands *nibble_commands;
    SInt32 addr_command;
    UInt16 proto_version;
    UInt8 byte0, mask0;
    UInt8 flags;
    SInt32 x_max;
    SInt32 y_max;
    SInt32 x_bits;
    SInt32 y_bits;
    unsigned int x_res;
    unsigned int y_res;
    
    SInt32 prev_fin;
    SInt32 multi_packet;
    int second_touch;
    UInt8 multi_data[6];
    struct alps_fields f;
    UInt8 quirks;
    
    int pktsize = 6;
};

// Pulled out of alps_data, now saved as vars on class
// makes invoking a little easier
typedef bool (ApplePS2ALPSGlidePoint::*hw_init)();
typedef void (ApplePS2ALPSGlidePoint::*decode_fields)(struct alps_fields *f, UInt8 *p);
typedef void (ApplePS2ALPSGlidePoint::*process_packet)(UInt8 *packet);
//typedef void (ApplePS2ALPSGlidePoint::*set_abs_params)();

#define ALPS_QUIRK_TRACKSTICK_BUTTONS	1 /* trakcstick buttons in trackstick packet */

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ApplePS2ALPSGlidePoint Class Declaration
//

typedef struct ALPSStatus {
    UInt8 bytes[3];
} ALPSStatus_t;

#define kPacketLengthSmall  3
#define kPacketLengthLarge  6
#define kPacketLengthMax    6
#define kDP_CommandNibble10 0xf2
#define BITS_PER_BYTE 8

// predeclure stuff
struct alps_data;

class EXPORT ApplePS2ALPSGlidePoint : public VoodooPS2TouchPadBase {
    typedef VoodooPS2TouchPadBase super;
    OSDeclareDefaultStructors( ApplePS2ALPSGlidePoint );
    
private:
    alps_data priv;
    
    hw_init hw_init;
    decode_fields decode_fields;
    process_packet process_packet;
    //    set_abs_params set_abs_params;
    
protected:
    int _multiPacket;
    UInt8 _multiData[6];
    IOGBounds _bounds;
    
    virtual void dispatchRelativePointerEventWithPacket(UInt8 *packet,
                                                        UInt32 packetSize);
    
    bool getStatus(ALPSStatus_t *status);
    
    virtual bool deviceSpecificInit();
    
    bool enterCommandMode();
    
    bool exitCommandMode();
    
    bool hwInitV3();
    
    int commandModeReadReg(int addr);
    
    bool commandModeWriteReg(int addr, UInt8 value);
    
    bool commandModeWriteReg(UInt8 value);
    
    bool commandModeSendNibble(int value);
    
    bool commandModeSetAddr(int addr);
    
    bool passthroughModeV3(int regBase, bool enable);
    
    bool passthroughModeV2(bool enable);
    
    bool absoluteModeV3();
    
    bool absoluteModeV1V2();
    
    bool absoluteModeV4();
    
    bool resetMouse();
    
    bool setSampleRateAndEnable(UInt8 rate);
    
    void processPacketV3(UInt8 *packet);
    
    void processTrackstickPacketV3(UInt8 * packet);
    
    void processTouchpadPacketV3(UInt8 * packet);
    
    int processBitmap(struct alps_data *priv,
                      struct alps_fields *f);
    
    PS2InterruptResult interruptOccurred(UInt8 data);
    
    void packetReady();
    
    void setTouchPadEnable(bool enable);
    
    void dispatchEventsWithInfo(int xraw, int yraw, int z, int fingers, UInt32 buttonsraw);
    
    void calculateMovement(int x, int y, int z, int fingers, int & dx, int & dy);
    
    void processPacketV1V2(UInt8 *packet);
    
    void decodeButtonsV3(struct alps_fields *f, UInt8 *p);
    
    void decodePinnacle(struct alps_fields *f, UInt8 *p);
    
    void decodeRushmore(struct alps_fields *f, UInt8 *p);
    
    void decodeDolphin(struct alps_fields *f, UInt8 *p);
    
    void processPacketV4(UInt8 *packet);
    
    bool repeatCmd(SInt32 init_command, SInt32 init_arg, SInt32 repeated_command, ALPSStatus_t *report);
    
    bool tapMode(bool enable);
    
    bool hwInitV1V2();
    
    bool hwInitV4();
    
    IOReturn probeTrackstickV3(int regBase);
    
    IOReturn setupTrackstickV3(int regBase);
    
    bool hwInitRushmoreV3();
    
    bool hwInitDolphinV1();
    
    bool alps_hw_init_dolphin_v2();
    
    bool alps_short_cmd_v5(UInt8 init_command, UInt8 cmd);
    
    bool alps_request_v5(UInt8 ahmad);
    
    void setDefaults();
    
    bool matchTable(ALPSStatus_t *e7, ALPSStatus_t *ec);
    
    IOReturn identify();
    
    void setupMaxes();
    
    bool v1v2MagicEnable();
    
    int alps_get_v3_v7_resolution(int reg_pitch);
        
public:
    virtual ApplePS2ALPSGlidePoint * probe(IOService *provider,
                                           SInt32 *score);
    
    void stop(IOService *provider);
    
    bool init(OSDictionary * dict);
};

#endif /* _APPLEPS2SYNAPTICSTOUCHPAD_H */
