/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License
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

#include <IOKit/hidsystem/IOHIDParameter.h>
#include "VoodooPS2Controller.h"
#include "alps.h"

enum {
    kTapEnabled = 0x01
};

#define ARRAY_SIZE(x)    (sizeof(x)/sizeof(x[0]))
#define MAX(X,Y)         ((X) > (Y) ? (X) : (Y))
#define abs(x) ((x) < 0 ? -(x) : (x))
#define BIT(x) (1 << (x))


/*
 * Definitions for ALPS version 3 and 4 command mode protocol
 */
#define ALPS_CMD_NIBBLE_10  0x01f2

#define ALPS_REG_BASE_RUSHMORE  0xc2c0
#define ALPS_REG_BASE_V7	0xc2c0
#define ALPS_REG_BASE_PINNACLE  0x0000

static const struct alps_nibble_commands alps_v3_nibble_commands[] = {
    { kDP_MouseSetPoll,                 0x00 }, /* 0 no send/recv */
    { kDP_SetDefaults,                  0x00 }, /* 1 no send/recv */
    { kDP_SetMouseScaling2To1,          0x00 }, /* 2 no send/recv */
    { kDP_SetMouseSampleRate | 0x1000,  0x0a }, /* 3 send=1 recv=0 */
    { kDP_SetMouseSampleRate | 0x1000,  0x14 }, /* 4 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x28 }, /* 5 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x3c }, /* 6 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x50 }, /* 7 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x64 }, /* 8 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0xc8 }, /* 9 ..*/
    { kDP_CommandNibble10    | 0x0100,  0x00 }, /* a send=0 recv=1 */
    { kDP_SetMouseResolution | 0x1000,  0x00 }, /* b send=1 recv=0 */
    { kDP_SetMouseResolution | 0x1000,  0x01 }, /* c ..*/
    { kDP_SetMouseResolution | 0x1000,  0x02 }, /* d ..*/
    { kDP_SetMouseResolution | 0x1000,  0x03 }, /* e ..*/
    { kDP_SetMouseScaling1To1,          0x00 }, /* f no send/recv */
};

static const struct alps_nibble_commands alps_v4_nibble_commands[] = {
    { kDP_Enable,                       0x00 }, /* 0 no send/recv */
    { kDP_SetDefaults,                  0x00 }, /* 1 no send/recv */
    { kDP_SetMouseScaling2To1,          0x00 }, /* 2 no send/recv */
    { kDP_SetMouseSampleRate | 0x1000,  0x0a }, /* 3 send=1 recv=0 */
    { kDP_SetMouseSampleRate | 0x1000,  0x14 }, /* 4 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x28 }, /* 5 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x3c }, /* 6 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x50 }, /* 7 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x64 }, /* 8 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0xc8 }, /* 9 ..*/
    { kDP_CommandNibble10    | 0x0100,  0x00 }, /* a send=0 recv=1 */
    { kDP_SetMouseResolution | 0x1000,  0x00 }, /* b send=1 recv=0 */
    { kDP_SetMouseResolution | 0x1000,  0x01 }, /* c ..*/
    { kDP_SetMouseResolution | 0x1000,  0x02 }, /* d ..*/
    { kDP_SetMouseResolution | 0x1000,  0x03 }, /* e ..*/
    { kDP_SetMouseScaling1To1,          0x00 }, /* f no send/recv */
};


#define ALPS_DUALPOINT          0x02    /* touchpad has trackstick */
#define ALPS_PASS               0x04    /* device has a pass-through port */

#define ALPS_WHEEL              0x08    /* hardware wheel present */
#define ALPS_FW_BK_1            0x10    /* front & back buttons present */
#define ALPS_FW_BK_2            0x20    /* front & back buttons present */
#define ALPS_FOUR_BUTTONS       0x40    /* 4 direction button present */
#define ALPS_PS2_INTERLEAVED    0x80    /* 3-byte PS/2 packet interleaved with
6-byte ALPS packet */
#define ALPS_STICK_BITS		0x100	/* separate stick button bits */
#define ALPS_BUTTONPAD		0x200	/* device is a clickpad */


static const struct alps_model_info alps_model_data[] = {
    { { 0x32, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT },
    /* Toshiba Salellite Pro M10 */
    { { 0x33, 0x02, 0x0a }, 0x00, ALPS_PROTO_V1, 0x88, 0xf8, 0 },               /* UMAX-530T */
    { { 0x53, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },
    { { 0x53, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },
    { { 0x60, 0x03, 0xc8 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },               /* HP ze1115 */
    { { 0x63, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },
    { { 0x63, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },
    { { 0x63, 0x02, 0x28 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_FW_BK_2 },    /* Fujitsu Siemens S6010 */
    { { 0x63, 0x02, 0x3c }, 0x00, ALPS_PROTO_V2, 0x8f, 0x8f, ALPS_WHEEL },      /* Toshiba Satellite S2400-103 */
    { { 0x63, 0x02, 0x50 }, 0x00, ALPS_PROTO_V2, 0xef, 0xef, ALPS_FW_BK_1 },    /* NEC Versa L320 */
    { { 0x63, 0x02, 0x64 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },
    { { 0x63, 0x03, 0xc8 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT },
    /* Dell Latitude D800 */
    { { 0x73, 0x00, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_DUALPOINT },  /* ThinkPad R61 8918-5QG */
    { { 0x73, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 },
    { { 0x73, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_FW_BK_2 },    /* Ahtec Laptop */
    { { 0x20, 0x02, 0x0e }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT },
    /* XXX */
    { { 0x22, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT },
    { { 0x22, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xff, 0xff, ALPS_PASS | ALPS_DUALPOINT },
    /* Dell Latitude D600 */
    /* Dell Latitude E5500, E6400, E6500, Precision M4400 */
    { { 0x62, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xcf, 0xcf,
        ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED },
    { { 0x73, 0x02, 0x50 }, 0x00, ALPS_PROTO_V2, 0xcf, 0xcf, ALPS_FOUR_BUTTONS },
    /* Dell Vostro 1400 */
    { { 0x52, 0x01, 0x14 }, 0x00, ALPS_PROTO_V2, 0xff, 0xff,
        ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED },
    /* Toshiba Tecra A11-11L */
    { { 0x73, 0x02, 0x64 }, 0x8a, ALPS_PROTO_V4, 0x8f, 0x8f, 0 },
};

// =============================================================================
// ApplePS2ALPSGlidePoint Class Implementation
//

OSDefineMetaClassAndStructors(ApplePS2ALPSGlidePoint, VoodooPS2TouchPadBase);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApplePS2ALPSGlidePoint *ApplePS2ALPSGlidePoint::probe(IOService *provider, SInt32 *score) {
    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe entered...\n");
    bool success;
    
    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //
    
    _device = (ApplePS2MouseDevice *) provider;
    
    _device->lock();
    resetMouse();
    
    if (identify() != 0) {
        success = false;
    } else {
        success = true;
        //_bounds.maxx = priv.x_max;
        //_bounds.maxy = priv.y_max;
    }
    _device->unlock();
    
    _device = 0;
    
    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe leaving.\n");
    
    return success ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::deviceSpecificInit() {
    
    resetMouse();
    
    if (identify() != 0) {
        goto init_fail;
    }
    
    // Setup expected packet size
    priv.pktsize = priv.proto_version == ALPS_PROTO_V4 ? 8 : 6;
    
    IOLog("ALPS: Touchpad driver started\n");
    
    if (!(this->*hw_init)()) {
        goto init_fail;
    }
    
    return true;
    
init_fail:
    IOLog("%s: Device initialization failed. Touchpad probably won't work\n", getName());
    resetMouse();
    return false;
}

bool ApplePS2ALPSGlidePoint::init(OSDictionary *dict) {
    if (!super::init(dict)) {
        return false;
    }
    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::stop(IOService *provider) {
    resetMouse();
    
    super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::resetMouse() {
    TPS2Request<3> request;
    
    // Reset mouse
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_Reset;
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commandsCount = 3;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    // Verify the result
    if (request.commands[1].inOrOut != kSC_Reset && request.commands[2].inOrOut != kSC_ID) {
        DEBUG_LOG("Failed to reset mouse, return values did not match. [0x%02x, 0x%02x]\n", request.commands[1].inOrOut, request.commands[2].inOrOut);
        return false;
    }
    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

PS2InterruptResult ApplePS2ALPSGlidePoint::interruptOccurred(UInt8 data) {
    //
    // This will be invoked automatically from our device when asynchronous
    // events need to be delivered. Process the trackpad data. Do NOT issue
    // any BLOCKING commands to our device in this context.
    //
    // Ignore all bytes until we see the start of a packet, otherwise the
    // packets may get out of sequence and things will get very confusing.
    //
    
    if (priv.proto_version == ALPS_PROTO_V7){
        // Right now this checks if the packet is either a PS/2 packet (data & 0xc8)
        // or if the first packet matches the specific trackpad first packet
        if (0 == _packetByteCount && (data & priv.mask0) != priv.byte0) {
            IOLog("%s: Unexpected byte0 data (%02x) from PS/2 controller\n", getName(), data);
            return kPS2IR_packetBuffering;
        } else if (0 == _packetByteCount && ((data & 0xc8) == 0x08))
            IOLog("%s: PS/2 Packet Received\n", getName());
        
        UInt8 *packet = _ringBuffer.head();
        packet[_packetByteCount++] = data;
        
        if (priv.pktsize == _packetByteCount) {
            // complete 6/8 or 3-byte packet received...
            // 3-byte packet is bare PS/2 packet instead of ALPS specific packet
            _ringBuffer.advanceHead(priv.pktsize);
            _packetByteCount = 0;
            return kPS2IR_packetReady;
        }
        return kPS2IR_packetBuffering;
    }
    
    // Right now this checks if the packet is either a PS/2 packet (data & 0xc8)
    // or if the first packet matches the specific trackpad first packet
    if (0 == _packetByteCount && (data & 0xc8) != 0x08 && (data & priv.mask0) != priv.byte0) {
        DEBUG_LOG("%s: Unexpected byte0 data (%02x) from PS/2 controller\n", getName(), data);
        return kPS2IR_packetBuffering;
    }
    
    /* Bytes 2 - packet size should have 0 in highest bit */
    if (_packetByteCount >= 1 && data == 0x80) {
        DEBUG_LOG("%s: Unexpected byte%d data (%02x) from PS/2 controller\n", getName(), _packetByteCount, data);
        _packetByteCount = 0;
        return kPS2IR_packetBuffering;
    }
    
    UInt8 *packet = _ringBuffer.head();
    packet[_packetByteCount++] = data;
    
    if (priv.pktsize == _packetByteCount ||
        (kPacketLengthSmall == _packetByteCount && (packet[0] & 0xc8) == 0x08)) {
        // complete 6/8 or 3-byte packet received...
        // 3-byte packet is bare PS/2 packet instead of ALPS specific packet
        _ringBuffer.advanceHead(priv.pktsize);
        _packetByteCount = 0;
        return kPS2IR_packetReady;
    }
    return kPS2IR_packetBuffering;
}

void ApplePS2ALPSGlidePoint::packetReady() {
    // empty the ring buffer, dispatching each packet...
    while (_ringBuffer.count() >= priv.pktsize) {
        UInt8 *packet = _ringBuffer.tail();
        // now we have complete packet, either 6-byte or 3-byte
        if ((packet[0] & priv.mask0) == priv.byte0) {
            DEBUG_LOG("ps2: Got pointer event with packet = { %02x, %02x, %02x, %02x, %02x, %02x }\n", packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
            (this->*process_packet)(packet);
            _ringBuffer.advanceTail(priv.pktsize);
        } else {
            DEBUG_LOG("ps2: Intercepted bare PS/2 packet..ignoring\n");
            // Ignore bare PS/2 packet for now...messes with the actual full 6-byte ALPS packet above
            //            dispatchRelativePointerEventWithPacket(packet, kPacketLengthSmall);
            _ringBuffer.advanceTail(kPacketLengthSmall);
        }
    }
}

void ApplePS2ALPSGlidePoint::processPacketV1V2(UInt8 *packet) {
    int x, y, z, ges, fin, left, right, middle, buttons = 0, fingers = 0;
    int back = 0, forward = 0;
    uint64_t now_abs;
    
    clock_get_uptime(&now_abs);
    
    if (priv.proto_version == ALPS_PROTO_V1) {
        left = packet[2] & 0x10;
        right = packet[2] & 0x08;
        middle = 0;
        x = packet[1] | ((packet[0] & 0x07) << 7);
        y = packet[4] | ((packet[3] & 0x07) << 7);
        z = packet[5];
    } else {
        left = packet[3] & 1;
        right = packet[3] & 2;
        middle = packet[3] & 4;
        x = packet[1] | ((packet[2] & 0x78) << (7 - 3));
        y = packet[4] | ((packet[3] & 0x70) << (7 - 4));
        z = packet[5];
    }
    
    if (priv.flags & ALPS_FW_BK_1) {
        back = packet[0] & 0x10;
        forward = packet[2] & 4;
    }
    
    if (priv.flags & ALPS_FW_BK_2) {
        back = packet[3] & 4;
        forward = packet[2] & 4;
        if ((middle = forward && back)) {
            forward = back = 0;
        }
    }
    
    ges = packet[2] & 1;
    fin = packet[2] & 2;
    
    /* To make button reporting compatible with rest of driver */
    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;
    
    
    if ((priv.flags & ALPS_DUALPOINT) && z == 127) {
        int dx, dy;
        dx = x > 383 ? (x - 768) : x;
        dy = -(y > 255 ? (y - 512) : y);
        
        dispatchRelativePointerEventX(dx, dy, buttons, now_abs);
        return;
    }
    
    /* Some models have separate stick button bits */
    if (priv.flags & ALPS_STICK_BITS) {
        left |= packet[0] & 1;
        right |= packet[0] & 2;
        middle |= packet[0] & 4;
    }
    
    /* Convert hardware tap to a reasonable Z value */
    if (ges && !fin) {
        z = 40;
    }
    
    /*
     * A "tap and drag" operation is reported by the hardware as a transition
     * from (!fin && ges) to (fin && ges). This should be translated to the
     * sequence Z>0, Z==0, Z>0, so the Z==0 event has to be generated manually.
     */
    if (ges && fin && !priv.prev_fin) {
        touchmode = MODE_DRAG;
    }
    priv.prev_fin = fin;
    
    if (z > 30) {
        fingers = 1;
    }
    
    if (z < 25) {
        fingers = 0;
    }
    
    dispatchEventsWithInfo(x, y, z, fingers, buttons);
    
    if (priv.flags & ALPS_WHEEL) {
        int scrollAmount = ((packet[2] << 1) & 0x08) - ((packet[0] >> 4) & 0x07);
        if (scrollAmount) {
            dispatchScrollWheelEventX(scrollAmount, 0, 0, now_abs);
        }
    }
    
    /* OS X incompatible */
    
    /*if (priv->flags & (ALPS_FW_BK_1 | ALPS_FW_BK_2)) {
     input_report_key(dev, BTN_FORWARD, forward);
     input_report_key(dev, BTN_BACK, back);
     }
     
     if (priv->flags & ALPS_FOUR_BUTTONS) {
     input_report_key(dev, BTN_0, packet[2] & 4);
     input_report_key(dev, BTN_1, packet[0] & 0x10);
     input_report_key(dev, BTN_2, packet[3] & 4);
     input_report_key(dev, BTN_3, packet[0] & 0x20);
     }*/
}

static void alps_get_bitmap_points(unsigned int map,
                                   struct alps_bitmap_point *low,
                                   struct alps_bitmap_point *high,
                                   int *fingers)
{
    struct alps_bitmap_point *point;
    int i, bit, prev_bit = 0;
    
    point = low;
    for (i = 0; map != 0; i++, map >>= 1) {
        bit = map & 1;
        if (bit) {
            if (!prev_bit) {
                point->start_bit = i;
                point->num_bits = 0;
                (*fingers)++;
            }
            point->num_bits++;
        } else {
            if (prev_bit)
                point = high;
        }
        prev_bit = bit;
    }
}

/*
 * Process bitmap data from semi-mt protocols. Returns the number of
 * fingers detected. A return value of 0 means at least one of the
 * bitmaps was empty.
 *
 * The bitmaps don't have enough data to track fingers, so this function
 * only generates points representing a bounding box of all contacts.
 * These points are returned in fields->mt when the return value
 * is greater than 0.
 */
int ApplePS2ALPSGlidePoint::processBitmap(struct alps_data *priv,
                                          struct alps_fields *fields)
{
    
    int i, fingers_x = 0, fingers_y = 0, fingers, closest;
    struct alps_bitmap_point x_low = {0,}, x_high = {0,};
    struct alps_bitmap_point y_low = {0,}, y_high = {0,};
    struct input_mt_pos corner[4];
    
    
    if (!fields->x_map || !fields->y_map) {
        return 0;
    }
    
    alps_get_bitmap_points(fields->x_map, &x_low, &x_high, &fingers_x);
    alps_get_bitmap_points(fields->y_map, &y_low, &y_high, &fingers_y);
    
    /*
     * Fingers can overlap, so we use the maximum count of fingers
     * on either axis as the finger count.
     */
    fingers = max(fingers_x, fingers_y);
    
    /*
     * If an axis reports only a single contact, we have overlapping or
     * adjacent fingers. Divide the single contact between the two points.
     */
    if (fingers_x == 1) {
        i = x_low.num_bits / 2;
        x_low.num_bits = x_low.num_bits - i;
        x_high.start_bit = x_low.start_bit + i;
        x_high.num_bits = max(i, 1);
    }
    
    if (fingers_y == 1) {
        i = y_low.num_bits / 2;
        y_low.num_bits = y_low.num_bits - i;
        y_high.start_bit = y_low.start_bit + i;
        y_high.num_bits = max(i, 1);
    }
    
    /* top-left corner */
    corner[0].x = (priv->x_max * (2 * x_low.start_bit + x_low.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[0].y = (priv->y_max * (2 * y_low.start_bit + y_low.num_bits - 1)) /
    (2 * (priv->y_bits - 1));
    
    /* top-right corner */
    corner[1].x = (priv->x_max * (2 * x_high.start_bit + x_high.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[1].y = (priv->y_max * (2 * y_low.start_bit + y_low.num_bits - 1)) /
    (2 * (priv->y_bits - 1));
    
    /* bottom-right corner */
    corner[2].x = (priv->x_max * (2 * x_high.start_bit + x_high.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[2].y = (priv->y_max * (2 * y_high.start_bit + y_high.num_bits - 1)) /
    (2 * (priv->y_bits - 1));
    
    /* bottom-left corner */
    corner[3].x = (priv->x_max * (2 * x_low.start_bit + x_low.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[3].y = (priv->y_max * (2 * y_high.start_bit + y_high.num_bits - 1)) /
    (2 * (priv->y_bits - 1));
    
    /* x-bitmap order is reversed on v5 touchpads  */
    if (priv->proto_version == ALPS_PROTO_V5) {
        for (i = 0; i < 4; i++)
            corner[i].x = priv->x_max - corner[i].x;
    }
    
    /* y-bitmap order is reversed on v3 and v4 touchpads  */
    if (priv->proto_version == ALPS_PROTO_V3 || priv->proto_version == ALPS_PROTO_V4) {
        for (i = 0; i < 4; i++)
            corner[i].y = priv->y_max - corner[i].y;
    }
    
    /*
     * We only select a corner for the second touch once per 2 finger
     * touch sequence to avoid the chosen corner (and thus the coordinates)
     * jumping around when the first touch is in the middle.
     */
    if (priv->second_touch == -1) {
        /* Find corner closest to our st coordinates */
        closest = 0x7fffffff;
        for (i = 0; i < 4; i++) {
            int dx = fields->st.x - corner[i].x;
            int dy = fields->st.y - corner[i].y;
            int distance = dx * dx + dy * dy;
            
            if (distance < closest) {
                priv->second_touch = i;
                closest = distance;
            }
        }
        /* And select the opposite corner to use for the 2nd touch */
        priv->second_touch = (priv->second_touch + 2) % 4;
    }
    
    fields->mt[0] = fields->st;
    fields->mt[1] = corner[priv->second_touch];
    
    //IOLog("ALPS: Process Bitmap, Corner=%d, Fingers=%d, x1=%d, x2=%d, y1=%d, y2=%d\n", priv->second_touch, fingers, fields->mt[0].x, fields->mt[1].x, fields->mt[0].y, fields->mt[1].y);
    return fingers;
}

void ApplePS2ALPSGlidePoint::processTrackstickPacketV3(UInt8 *packet) {
    int x, y, z, left, right, middle;
    uint64_t now_abs;
    UInt32 buttons = 0, raw_buttons = 0;
    
    /* It should be a DualPoint when received trackstick packet */
    if (!(priv.flags & ALPS_DUALPOINT)) {
        return;
    }
    
    /* Sanity check packet */
    if (!(packet[0] & 0x40)) {
        DEBUG_LOG("ps2: bad trackstick packet, disregarding...\n");
        return;
    }
    
    /* There is a special packet that seems to indicate the end
     * of a stream of trackstick data. Filter these out
     */
    if (packet[1] == 0x7f && packet[2] == 0x7f && packet[3] == 0x7f) {
        return;
    }
    
    x = (SInt8) (((packet[0] & 0x20) << 2) | (packet[1] & 0x7f));
    y = (SInt8) (((packet[0] & 0x10) << 3) | (packet[2] & 0x7f));
    z = (packet[4] & 0x7c) >> 2;
    
    /* Prevent pointer jump on finger lift */
    if ((abs(x) >= 0x7f) && (abs(y) >= 0x7f)) {
        x = y = 0;
    }
    
    /*
     * The x and y values tend to be quite large, and when used
     * alone the trackstick is difficult to use. Scale them down
     * to compensate.
     */
    x /= 3;
    y /= 3;
    
    /* To get proper movement direction */
    y = -y;
    
    clock_get_uptime(&now_abs);
    
    /*
     * Most ALPS models report the trackstick buttons in the touchpad
     * packets, but a few report them here. No reliable way has been
     * found to differentiate between the models upfront, so we enable
     * the quirk in response to seeing a button press in the trackstick
     * packet.
     */
    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;
    
    if (!(priv.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS) &&
        (left || middle || right)) {
        priv.quirks |= ALPS_QUIRK_TRACKSTICK_BUTTONS;
    }
    
    if (priv.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS) {
        raw_buttons |= left ? 0x01 : 0;
        raw_buttons |= right ? 0x02 : 0;
        raw_buttons |= middle ? 0x04 : 0;
    }
    
    /* Button status can appear in normal packet */
    if (0 == raw_buttons) {
        buttons = lastbuttons;
    } else {
        buttons = raw_buttons;
        lastbuttons = buttons;
    }
    
    /* If middle button is pressed, switch to scroll mode. Else, move pointer normally */
    if (0 == (buttons & 0x04)) {
        dispatchRelativePointerEventX(x, y, buttons, now_abs);
    } else {
        dispatchScrollWheelEventX(-y, -x, 0, now_abs);
    }
}

bool ApplePS2ALPSGlidePoint::decodeButtonsV3(struct alps_fields *f, unsigned char *p) {
    f->left = !!(p[3] & 0x01);
    f->right = !!(p[3] & 0x02);
    f->middle = !!(p[3] & 0x04);
    
    f->ts_left = !!(p[3] & 0x10);
    f->ts_right = !!(p[3] & 0x20);
    f->ts_middle = !!(p[3] & 0x40);
    return true;
}

bool ApplePS2ALPSGlidePoint::decodePinnacle(struct alps_fields *f, UInt8 *p) {
    f->first_mp = !!(p[4] & 0x40);
    f->is_mp = !!(p[0] & 0x40);
    
    if (f->is_mp) {
        f->fingers = (p[5] & 0x3) + 1;
        f->x_map = ((p[4] & 0x7e) << 8) |
        ((p[1] & 0x7f) << 2) |
        ((p[0] & 0x30) >> 4);
        f->y_map = ((p[3] & 0x70) << 4) |
        ((p[2] & 0x7f) << 1) |
        (p[4] & 0x01);
    } else {
        f->st.x = ((p[1] & 0x7f) << 4) | ((p[4] & 0x30) >> 2) |
        ((p[0] & 0x30) >> 4);
        f->st.y = ((p[2] & 0x7f) << 4) | (p[4] & 0x0f);
        f->pressure = p[5] & 0x7f;
        
        decodeButtonsV3(f, p);
    }
    return true;
}

bool ApplePS2ALPSGlidePoint::decodeRushmore(struct alps_fields *f, UInt8 *p) {
    f->first_mp = !!(p[4] & 0x40);
    f->is_mp = !!(p[5] & 0x40);
    
    if (f->is_mp) {
        f->fingers = max((p[5] & 0x3), ((p[5] >> 2) & 0x3)) + 1;
        f->x_map = ((p[5] & 0x10) << 11) |
        ((p[4] & 0x7e) << 8) |
        ((p[1] & 0x7f) << 2) |
        ((p[0] & 0x30) >> 4);
        f->y_map = ((p[5] & 0x20) << 6) |
        ((p[3] & 0x70) << 4) |
        ((p[2] & 0x7f) << 1) |
        (p[4] & 0x01);
    } else {
        f->st.x = ((p[1] & 0x7f) << 4) | ((p[4] & 0x30) >> 2) |
        ((p[0] & 0x30) >> 4);
        f->st.y = ((p[2] & 0x7f) << 4) | (p[4] & 0x0f);
        f->pressure = p[5] & 0x7f;
        
        decodeButtonsV3(f, p);
    }
    return true;
}

unsigned char ApplePS2ALPSGlidePoint::alps_get_packet_id_v7(UInt8 *byte)
{
    unsigned char packet_id;
    
    if (byte[4] & 0x40)
        packet_id = V7_PACKET_ID_TWO;
    else if (byte[4] & 0x01)
        packet_id = V7_PACKET_ID_MULTI;
    else if ((byte[0] & 0x10) && !(byte[4] & 0x43))
        packet_id = V7_PACKET_ID_NEW;
    else if (byte[1] == 0x00 && byte[4] == 0x00)
        packet_id = V7_PACKET_ID_IDLE;
    else
        packet_id = V7_PACKET_ID_UNKNOWN;
    
    return packet_id;
}

void ApplePS2ALPSGlidePoint::alps_get_finger_coordinate_v7(struct input_mt_pos *mt,
                                          UInt8 *pkt,
                                          UInt8 pkt_id)
{
    mt[0].x = ((pkt[2] & 0x80) << 4);
    mt[0].x |= ((pkt[2] & 0x3F) << 5);
    mt[0].x |= ((pkt[3] & 0x30) >> 1);
    mt[0].x |= (pkt[3] & 0x07);
    mt[0].y = (pkt[1] << 3) | (pkt[0] & 0x07);
    
    mt[1].x = ((pkt[3] & 0x80) << 4);
    mt[1].x |= ((pkt[4] & 0x80) << 3);
    mt[1].x |= ((pkt[4] & 0x3F) << 4);
    mt[1].y = ((pkt[5] & 0x80) << 3);
    mt[1].y |= ((pkt[5] & 0x3F) << 4);
    
    switch (pkt_id) {
        case V7_PACKET_ID_TWO:
            mt[1].x &= ~0x000F;
            mt[1].y |= 0x000F;
            /* Detect false-postive touches where x & y report max value */
            if (mt[1].y == 0x7ff && mt[1].x == 0xff0)
                mt[1].x = 0;
                /* y gets set to 0 at the end of this function */
            break;
            
        case V7_PACKET_ID_MULTI:
            mt[1].x &= ~0x003F;
            mt[1].y &= ~0x0020;
            mt[1].y |= ((pkt[4] & 0x02) << 4);
            mt[1].y |= 0x001F;
            break;
            
        case V7_PACKET_ID_NEW:
            mt[1].x &= ~0x003F;
            mt[1].x |= (pkt[0] & 0x20);
            mt[1].y |= 0x000F;
            break;
    }
    
    mt[0].y = 0x7FF - mt[0].y;
    mt[1].y = 0x7FF - mt[1].y;
}

int ApplePS2ALPSGlidePoint::alps_get_mt_count(struct input_mt_pos *mt)
{
    int i, fingers = 0;
    
    for (i = 0; i < MAX_TOUCHES; i++) {
        if (mt[i].x != 0 || mt[i].y != 0)
            fingers++;
    }
    
    return fingers;
}

bool ApplePS2ALPSGlidePoint::decodeV7(struct alps_fields *f, UInt8 *p){
    //IOLog("Decode V7 touchpad Packet... 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", p[0], p[1], p[2], p[3], p[4], p[5]);
    
    unsigned char pkt_id;
    
    pkt_id = alps_get_packet_id_v7(p);
    if (pkt_id == V7_PACKET_ID_IDLE)
        return true;
    if (pkt_id == V7_PACKET_ID_UNKNOWN)
        return false;
    
    /*
     * NEW packets are send to indicate a discontinuity in the finger
     * coordinate reporting. Specifically a finger may have moved from
     * slot 0 to 1 or vice versa. INPUT_MT_TRACK takes care of this for
     * us.
     *
     * NEW packets have 3 problems:
     * 1) They do not contain middle / right button info (on non clickpads)
     *    this can be worked around by preserving the old button state
     * 2) They do not contain an accurate fingercount, and they are
     *    typically send when the number of fingers changes. We cannot use
     *    the old finger count as that may mismatch with the amount of
     *    touch coordinates we've available in the NEW packet
     * 3) Their x data for the second touch is inaccurate leading to
     *    a possible jump of the x coordinate by 16 units when the first
     *    non NEW packet comes in
     * Since problems 2 & 3 cannot be worked around, just ignore them.
     */
    if (pkt_id == V7_PACKET_ID_NEW)
        return true;
    
    alps_get_finger_coordinate_v7(f->mt, p, pkt_id);
    
    if (pkt_id == V7_PACKET_ID_TWO)
        f->fingers = alps_get_mt_count(f->mt);
    else /* pkt_id == V7_PACKET_ID_MULTI */
        f->fingers = 3 + (p[5] & 0x03);
    
    f->left = (p[0] & 0x80) >> 7;
    if (priv.flags & ALPS_BUTTONPAD) {
        if (p[0] & 0x20)
            f->fingers++;
        if (p[0] & 0x10)
            f->fingers++;
    } else {
        f->right = (p[0] & 0x20) >> 5;
        f->middle = (p[0] & 0x10) >> 4;
    }
    
    /* Sometimes a single touch is reported in mt[1] rather then mt[0] */
    if (f->fingers == 1 && f->mt[0].x == 0 && f->mt[0].y == 0) {
        f->mt[0].x = f->mt[1].x;
        f->mt[0].y = f->mt[1].y;
        f->mt[1].x = 0;
        f->mt[1].y = 0;
    }
    return true;
}

bool ApplePS2ALPSGlidePoint::decodeDolphin(struct alps_fields *f, UInt8 *p) {
    uint64_t palm_data = 0;
    
    f->first_mp = !!(p[0] & 0x02);
    f->is_mp = !!(p[0] & 0x20);
    
    if (!f->is_mp) {
        f->st.x = ((p[1] & 0x7f) | ((p[4] & 0x0f) << 7));
        f->st.y = ((p[2] & 0x7f) | ((p[4] & 0xf0) << 3));
        f->pressure = (p[0] & 4) ? 0 : p[5] & 0x7f;
        decodeButtonsV3(f, p);
    } else {
        f->fingers = ((p[0] & 0x6) >> 1 |
                      (p[0] & 0x10) >> 2);
        
        palm_data = (p[1] & 0x7f) |
        ((p[2] & 0x7f) << 7) |
        ((p[4] & 0x7f) << 14) |
        ((p[5] & 0x7f) << 21) |
        ((p[3] & 0x07) << 28) |
        (((uint64_t)p[3] & 0x70) << 27) |
        (((uint64_t)p[0] & 0x01) << 34);
        
        /* Y-profile is stored in P(0) to p(n-1), n = y_bits; */
        f->y_map = palm_data & (BIT(priv.y_bits) - 1);
        
        /* X-profile is stored in p(n) to p(n+m-1), m = x_bits; */
        f->x_map = (palm_data >> priv.y_bits) &
        (BIT(priv.x_bits) - 1);
    }
    return true;
}

void ApplePS2ALPSGlidePoint::alps_process_touchpad_packet_v3_v5(UInt8 *packet) {
    int fingers = 0;
    UInt32 buttons = 0;
    uint64_t now_abs;
    struct alps_fields f;
    //ffff
    
    clock_get_uptime(&now_abs);
    
    (this->*decode_fields)(&f, packet);
    
    /*
     * There's no single feature of touchpad position and bitmap packets
     * that can be used to distinguish between them. We rely on the fact
     * that a bitmap packet should always follow a position packet with
     * bit 6 of packet[4] set.
     */
    if (priv.multi_packet) {
        /*
         * Sometimes a position packet will indicate a multi-packet
         * sequence, but then what follows is another position
         * packet. Check for this, and when it happens process the
         * position packet as usual.
         */
        if (f.is_mp) {
            fingers = f.fingers;
            /*
             * Bitmap processing uses position packet's coordinate
             * data, so we need to do decode it first.
             */
            (this->*decode_fields)(&f, priv.multi_data);
            if (processBitmap(&priv, &f) == 0) {
                fingers = 0; /* Use st data */
            }
        } else {
            priv.multi_packet = 0;
        }
    }
    
    /*
     * Bit 6 of byte 0 is not usually set in position packets. The only
     * times it seems to be set is in situations where the data is
     * suspect anyway, e.g. a palm resting flat on the touchpad. Given
     * this combined with the fact that this bit is useful for filtering
     * out misidentified bitmap packets, we reject anything with this
     * bit set.
     */
    if (f.is_mp) {
        return;
    }
    
    if (!priv.multi_packet && (f.first_mp)) {
        priv.multi_packet = 1;
        memcpy(priv.multi_data, packet, sizeof(priv.multi_data));
        return;
    }
    
    priv.multi_packet = 0;
    
    /*
     * Sometimes the hardware sends a single packet with z = 0
     * in the middle of a stream. Real releases generate packets
     * with x, y, and z all zero, so these seem to be flukes.
     * Ignore them.
     */
    if (f.st.x && f.st.y && !f.pressure) {
        //return;
    }
    
    /* Use st data when we don't have mt data */
    if (fingers < 2) {
        f.mt[0].x = f.st.x;
        f.mt[0].y = f.st.y;
        fingers = f.pressure > 0 ? 1 : 0;
        priv.second_touch = -1;
    }
    
    buttons |= f.left ? 0x01 : 0;
    buttons |= f.right ? 0x02 : 0;
    buttons |= f.middle ? 0x04 : 0;
    
    if ((priv.flags & ALPS_DUALPOINT) &&
        !(priv.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS)) {
        buttons |= f.ts_left ? 0x01 : 0;
        buttons |= f.ts_right ? 0x02 : 0;
        buttons |= f.ts_middle ? 0x04 : 0;
    }
    
    /* Reverse y co-ordinates to have 0 at bottom for gestures to work */
    f.mt[0].y = priv.y_max - f.mt[0].y;
    f.mt[1].y = priv.y_max - f.mt[1].y;
    
    /* Improve multifinger accuacy */
    if (last_fingers > fingers && fingers != 0 && f.pressure > 0) {
        fingers = last_fingers;
    }
    
    dispatchEventsWithInfo(f.mt[0].x, f.mt[0].y, f.pressure, fingers, buttons);
}

void ApplePS2ALPSGlidePoint::processPacketV3(UInt8 *packet) {
    /*
     * v3 protocol packets come in three types, two representing
     * touchpad data and one representing trackstick data.
     * Trackstick packets seem to be distinguished by always
     * having 0x3f in the last byte. This value has never been
     * observed in the last byte of either of the other types
     * of packets.
     */
    if (packet[5] == 0x3f) {
        processTrackstickPacketV3(packet);
        return;
    }
    
    alps_process_touchpad_packet_v3_v5(packet);
}

void ApplePS2ALPSGlidePoint::processPacketV4(UInt8 *packet) {
    SInt32 offset;
    //SInt32 left, right = 0;
    SInt32 fingers = 0;
    UInt32 buttons = 0;
    struct alps_fields f;
    
    /*
     * v4 has a 6-byte encoding for bitmap data, but this data is
     * broken up between 3 normal packets. Use priv.multi_packet to
     * track our position in the bitmap packet.
     */
    if (packet[6] & 0x40) {
        /* sync, reset position */
        priv.multi_packet = 0;
    }
    
    if (priv.multi_packet > 2) {
        return;
    }
    
    offset = 2 * priv.multi_packet;
    priv.multi_data[offset] = packet[6];
    priv.multi_data[offset + 1] = packet[7];
    
    f.left = packet[4] & 0x01;
    f.right = packet[4] & 0x02;
    
    f.st.x = ((packet[1] & 0x7f) << 4) | ((packet[3] & 0x30) >> 2) |
    ((packet[0] & 0x30) >> 4);
    f.st.y = ((packet[2] & 0x7f) << 4) | (packet[3] & 0x0f);
    f.pressure = packet[5] & 0x7f;
    
    if (++priv.multi_packet > 2) {
        priv.multi_packet = 0;
        
        f.x_map = ((priv.multi_data[2] & 0x1f) << 10) |
        ((priv.multi_data[3] & 0x60) << 3) |
        ((priv.multi_data[0] & 0x3f) << 2) |
        ((priv.multi_data[1] & 0x60) >> 5);
        f.y_map = ((priv.multi_data[5] & 0x01) << 10) |
        ((priv.multi_data[3] & 0x1f) << 5) |
        (priv.multi_data[1] & 0x1f);
        
        fingers = processBitmap(&priv, &f);
        
    }
    
    buttons |= f.left ? 0x01 : 0;
    buttons |= f.right ? 0x02 : 0;
    
    dispatchEventsWithInfo(f.st.x, f.st.y, f.pressure, fingers, buttons);
}

void ApplePS2ALPSGlidePoint::processTrackstickPacketV7(UInt8 *packet){
    //TODO: Add Trackstick support (No trackstick on Lenovo Flex 3-1480)
}

void ApplePS2ALPSGlidePoint::processTouchpadPacketV7(UInt8 *packet){
    int fingers = 0;
    UInt32 buttons = 0;
    struct alps_fields f;
    
    memset(&f, 0, sizeof(alps_fields));
    
    if (!(this->*decode_fields)(&f, packet))
        return;
    
    buttons |= f.left ? 0x01 : 0;
    buttons |= f.right ? 0x02 : 0;
    buttons |= f.middle ? 0x04 : 0;
    
    if ((priv.flags & ALPS_DUALPOINT) &&
        !(priv.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS)) {
        buttons |= f.ts_left ? 0x01 : 0;
        buttons |= f.ts_right ? 0x02 : 0;
        buttons |= f.ts_middle ? 0x04 : 0;
    }
    
    /* Reverse y co-ordinates to have 0 at bottom for gestures to work */
    f.mt[0].y = priv.y_max - f.mt[0].y;
    f.mt[1].y = priv.y_max - f.mt[1].y;
    
    fingers = f.fingers;
    
    //Hack because V7 doesn't report pressure
    if (fingers != 0 && (f.mt[0].x != 0 && f.mt[0].y != 0))
        f.pressure = 40;
    else
        f.pressure = 0;
    
    dispatchEventsWithInfo(f.mt[0].x, f.mt[0].y, f.pressure, fingers, buttons);
}

void ApplePS2ALPSGlidePoint::processPacketV7(UInt8 *packet){
    if (packet[0] == 0x48 && (packet[4] & 0x47) == 0x06)
        processTrackstickPacketV7(packet);
    else
        processTouchpadPacketV7(packet);
}

void ApplePS2ALPSGlidePoint::dispatchEventsWithInfo(int xraw, int yraw, int z, int fingers, UInt32 buttonsraw) {
    uint64_t now_abs;
    clock_get_uptime(&now_abs);
    uint64_t now_ns;
    absolutetime_to_nanoseconds(now_abs, &now_ns);
    
    DEBUG_LOG("%s::dispatchEventsWithInfo: x=%d, y=%d, z=%d, fingers=%d, buttons=%d\n",
              getName(), xraw, yraw, z, fingers, buttonsraw);
    
    // scale x & y to the axis which has the most resolution
    if (xupmm < yupmm) {
        xraw = xraw * yupmm / xupmm;
    } else if (xupmm > yupmm) {
        yraw = yraw * xupmm / yupmm;
    }
    
    /* Dr Hurt: Scale all touchpads' x axis to 6000 to be able to the same divisor for all models */
    if (priv.proto_version == ALPS_PROTO_V2) {
        xraw *= 6;
        yraw *= 6;
    }
    
    if (priv.proto_version > ALPS_PROTO_V2 && priv.proto_version < ALPS_PROTO_V5) {
        xraw *= 3;
        yraw *= 3;
    }
    
    if (priv.proto_version == ALPS_PROTO_V5) {
        xraw *= 4.4;
        yraw *= 4.4;
    }
    
    if (priv.proto_version == ALPS_PROTO_V7) {
        xraw *= 1.5;
        yraw *= 1.5;
    }
    
    int x = xraw;
    int y = yraw;
    
    // allow middle click to be simulated the other two physical buttons
    UInt32 buttons = buttonsraw;
    lastbuttons = buttons;
    
    // allow middle button to be simulated with two buttons down
    if (!clickpadtype || fingers == 3) {
        buttons = middleButton(buttons, now_abs, fingers == 3 ? fromPassthru : fromTrackpad);
        DEBUG_LOG("New buttons value after check for middle click: %d\n", buttons);
    }
    
    // recalc middle buttons if finger is going down
    if (0 == last_fingers && fingers > 0) {
        buttons = middleButton(buttonsraw | passbuttons, now_abs, fromCancel);
    }
    
    if (last_fingers > 0 && fingers > 0 && last_fingers != fingers) {
        DEBUG_LOG("Start ignoring delta with finger change\n");
        // ignore deltas for a while after finger change
        ignoredeltas = ignoredeltasstart;
    }
    
    if (last_fingers != fingers) {
        DEBUG_LOG("Finger change, reset averages\n");
        // reset averages after finger change
        x_undo.reset();
        y_undo.reset();
        x_avg.reset();
        y_avg.reset();
    }
    
    // unsmooth input (probably just for testing)
    // by default the trackpad itself does a simple decaying average (1/2 each)
    // we can undo it here
    if (unsmoothinput) {
        x = x_undo.filter(x);
        y = y_undo.filter(y);
    }
    
    // smooth input by unweighted average
    if (smoothinput) {
        x = x_avg.filter(x);
        y = y_avg.filter(y);
        
    }
    
    if (ignoredeltas) {
        DEBUG_LOG("Still ignoring deltas. Value=%d\n", ignoredeltas);
        lastx = x;
        lasty = y;
        if (--ignoredeltas == 0) {
            x_undo.reset();
            y_undo.reset();
            x_avg.reset();
            y_avg.reset();
        }
    }
    
    // deal with "OutsidezoneNoAction When Typing"
    if (outzone_wt && z > z_finger && now_ns - keytime < maxaftertyping &&
        (x < zonel || x > zoner || y < zoneb || y > zonet)) {
        DEBUG_LOG("Ignore touch input after typing\n");
        // touch input was shortly after typing and outside the "zone"
        // ignore it...
        return;
    }
    
    // if trackpad input is supposed to be ignored, then don't do anything
    if (ignoreall) {
        DEBUG_LOG("ignoreall is set, returning\n");
        return;
    }
    
#ifdef DEBUG_VERBOSE
    int tm1 = touchmode;
#endif
    if (z < z_finger && isTouchMode()) {
        // Finger has been lifted
        DEBUG_LOG("finger lifted after touch\n");
        xrest = yrest = scrollrest = 0;
        inSwipeLeft = inSwipeRight = inSwipeUp = inSwipeDown = 0;
        inSwipe4Left = inSwipe4Right = inSwipe4Up = inSwipe4Down = 0;
        xmoved = ymoved = 0;
        untouchtime = now_ns;
        tracksecondary = false;
        
        if (dy_history.count()) {
            DEBUG_LOG("ps2: newest=%llu, oldest=%llu, diff=%llu, avg: %d/%d=%d\n", time_history.newest(), time_history.oldest(), time_history.newest() - time_history.oldest(), dy_history.sum(), dy_history.count(), dy_history.average());
        }
        else {
            DEBUG_LOG("ps2: no time/dy history\n");
        }
        
        // check for scroll momentum start
        if ((MODE_MTOUCH == touchmode || MODE_VSCROLL == touchmode) && momentumscroll && momentumscrolltimer) {
            // releasing when we were in touchmode -- check for momentum scroll
            if (dy_history.count() > momentumscrollsamplesmin &&
                (momentumscrollinterval = time_history.newest() - time_history.oldest())) {
                momentumscrollsum = dy_history.sum();
                momentumscrollcurrent = momentumscrolltimer * momentumscrollsum;
                momentumscrollrest1 = 0;
                momentumscrollrest2 = 0;
                setTimerTimeout(scrollTimer, momentumscrolltimer);
            }
        }
        time_history.reset();
        dy_history.reset();
        DEBUG_LOG("ps2: now_ns-touchtime=%lld (%s). touchmode=%d\n", (uint64_t) (now_ns - touchtime) / 1000, now_ns - touchtime < maxtaptime ? "true" : "false", touchmode);
        if (now_ns - touchtime < maxtaptime && clicking) {
            switch (touchmode) {
                case MODE_DRAG:
                    if (!immediateclick) {
                        buttons &= ~0x7;
                        dispatchRelativePointerEventX(0, 0, buttons | 0x1, now_abs);
                        dispatchRelativePointerEventX(0, 0, buttons, now_abs);
                    }
                    if (wastriple && rtap) {
                        buttons |= !swapdoubletriple ? 0x4 : 0x02;
                    } else if (wasdouble && rtap) {
                        buttons |= !swapdoubletriple ? 0x2 : 0x04;
                    } else {
                        buttons |= 0x1;
                    }
                    touchmode = MODE_NOTOUCH;
                    break;
                    
                case MODE_DRAGLOCK:
                    touchmode = MODE_NOTOUCH;
                    break;
                    
                default: //dispatch taps
                    if (wastriple && rtap)
                    {
                        buttons |= !swapdoubletriple ? 0x4 : 0x02;
                        touchmode=MODE_NOTOUCH;
                    }
                    else if (wasdouble && rtap)
                    {
                        buttons |= !swapdoubletriple ? 0x2 : 0x04;
                        touchmode=MODE_NOTOUCH;
                    }
                    else
                    {
                        buttons |= 0x1;
                        touchmode=dragging ? MODE_PREDRAG : MODE_NOTOUCH;
                    }
                    break;
            }
        }
        else {
            if ((touchmode==MODE_DRAG || touchmode==MODE_DRAGLOCK)
                && (draglock || draglocktemp || (dragTimer && dragexitdelay)))
            {
                touchmode=MODE_DRAGNOTOUCH;
                if (!draglock && !draglocktemp)
                {
                    cancelTimer(dragTimer);
                    setTimerTimeout(dragTimer, dragexitdelay);
                }
            } else {
                touchmode = MODE_NOTOUCH;
                draglocktemp = 0;
            }
        }
        wasdouble = false;
        wastriple = false;
    }
    
    // cancel pre-drag mode if second tap takes too long
    if (touchmode == MODE_PREDRAG && now_ns - untouchtime >= maxdragtime) {
        DEBUG_LOG("cancel pre-drag since second tap took too long\n");
        touchmode = MODE_NOTOUCH;
    }
    
    // Note: This test should probably be done somewhere else, especially if to
    // implement more gestures in the future, because this information we are
    // erasing here (time of touch) might be useful for certain gestures...
    
    // cancel tap if touch point moves too far
    if (isTouchMode() && isFingerTouch(z)) {
        int dx = xraw > touchx ? xraw - touchx : touchx - xraw;
        int dy = yraw > touchy ? touchy - yraw : yraw - touchy;
        if (!wasdouble && !wastriple && (dx > tapthreshx || dy > tapthreshy)) {
            touchtime = 0;
        }
        else if (dx > dblthreshx || dy > dblthreshy) {
            touchtime = 0;
        }
    }
    
#ifdef DEBUG_VERBOSE
    int tm2 = touchmode;
#endif
    int dx = 0, dy = 0;
    
    DEBUG_LOG("ps2: touchmode=%d, buttons = %d\n", touchmode, buttons);
    switch (touchmode) {
        case MODE_DRAG:
        case MODE_DRAGLOCK:
            if (MODE_DRAGLOCK == touchmode || (!immediateclick || now_ns - touchtime > maxdbltaptime)) {
                buttons |= 0x1;
            }
            // fall through
        case MODE_MOVE:
            if (last_fingers == fingers)
            {
                if (now_ns - touchtime > 100000000) {
                    dx = x-lastx+xrest;
                    dy = lasty-y+yrest;
                    xrest = dx % divisorx;
                    yrest = dy % divisory;
                    if (abs(dx) > bogusdxthresh || abs(dy) > bogusdythresh)
                        dx = dy = xrest = yrest = 0;
                }
            }
            break;
            
        case MODE_MTOUCH:
            switch (fingers) {
                case 1:
                    // transition from multitouch to single touch
                    // continue moving with the primary finger
                    if (last_fingers == fingers && !wsticky)
                    {
                        dy_history.reset();
                        time_history.reset();
                        //tracksecondary=false;
                        touchmode=MODE_MOVE;
                        break;
                    }
                    
                case 2: // two finger
                    if (last_fingers != fingers) {
                        break;
                    }
                    if (palm && z > zlimit) {
                        break;
                    }
                    if (palm_wt && now_ns - keytime < maxaftertyping) {
                        break;
                    }
                    dy = (wvdivisor) ? (y-lasty+yrest) : 0;
                    dx = (whdivisor&&hscroll) ? (x-lastx+xrest) : 0;
                    yrest = (wvdivisor) ? dy % wvdivisor : 0;
                    xrest = (whdivisor&&hscroll) ? dx % whdivisor : 0;
                    // check for stopping or changing direction
                    if ((dy < 0) != (dy_history.newest() < 0) || dy == 0) {
                        // stopped or changed direction, clear history
                        dy_history.reset();
                        time_history.reset();
                    }
                    // put movement and time in history for later
                    dy_history.filter(dy);
                    time_history.filter(now_ns);
                    //REVIEW: filter out small movements (Mavericks issue)
                    if (abs(dx) < scrolldxthresh)
                    {
                        xrest = dx;
                        dx = 0;
                    }
                    if (abs(dy) < scrolldythresh)
                    {
                        yrest = dy;
                        dy = 0;
                    }
                    if (0 != dy || 0 != dx)
                    {
                        
                        dispatchScrollWheelEventX(wvdivisor ? dy / wvdivisor : 0, (whdivisor && hscroll) ? -dx / whdivisor : 0, 0, now_abs);
                        dx = dy = 0;
                    }
                    break;
                    
                case 3: // three finger
                    // Now calculate total movement since 3 fingers down (add to total)
                    xmoved += lastx-x;
                    ymoved += y-lasty;
                    
                    // dispatching 3 finger movement
                    if (ymoved > swipedy && !inSwipeUp && !inSwipe4Up) {
                        inSwipeUp = 1;
                        inSwipeDown = 0;
                        ymoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipeUp, &now_abs);
                        break;
                    }
                    if (ymoved < -swipedy && !inSwipeDown && !inSwipe4Down) {
                        inSwipeDown = 1;
                        inSwipeUp = 0;
                        ymoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipeDown, &now_abs);
                        break;
                    }
                    if (xmoved < -swipedx && !inSwipeRight && !inSwipe4Right) {
                        inSwipeRight = 1;
                        inSwipeLeft = 0;
                        xmoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipeRight, &now_abs);
                        break;
                    }
                    if (xmoved > swipedx && !inSwipeLeft && !inSwipe4Left) {
                        inSwipeLeft = 1;
                        inSwipeRight = 0;
                        xmoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipeLeft, &now_abs);
                        break;
                    }
                    break;
                    
                case 4: // four fingers
                    // Now calculate total movement since 4 fingers down (add to total)
                    xmoved += lastx-x;
                    ymoved += y-lasty;
                    
                    // dispatching 4 finger movement
                    if (ymoved > swipedy && !inSwipe4Up) {
                        inSwipe4Up = 1; inSwipeUp = 0;
                        inSwipe4Down = 0;
                        ymoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Up, &now_abs);
                        break;
                    }
                    if (ymoved < -swipedy && !inSwipe4Down) {
                        inSwipe4Down = 1; inSwipeDown = 0;
                        inSwipe4Up = 0;
                        ymoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Down, &now_abs);
                        break;
                    }
                    if (xmoved < -swipedx && !inSwipe4Right) {
                        inSwipe4Right = 1; inSwipeRight = 0;
                        inSwipe4Left = 0;
                        xmoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Right, &now_abs);
                        break;
                    }
                    if (xmoved > swipedx && !inSwipe4Left) {
                        inSwipe4Left = 1; inSwipeLeft = 0;
                        inSwipe4Right = 0;
                        xmoved = 0;
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Left, &now_abs);
                        break;
                    }
            }
            break;
            
        case MODE_VSCROLL:
            if (!vsticky && (x < redge || fingers > 1 || z > zlimit)) {
                touchmode = MODE_NOTOUCH;
                break;
            }
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                break;
            }
            dy = y-lasty+scrollrest;
            scrollrest = dy % vscrolldivisor;
            //REVIEW: filter out small movements (Mavericks issue)
            if (abs(dy) < scrolldythresh)
            {
                scrollrest = dy;
                dy = 0;
            }
            if ((dy < 0) != (dy_history.newest() < 0) || dy == 0) {
                // stopped or changed direction, clear history
                dy_history.reset();
                time_history.reset();
            }
            // put movement and time in history for later
            dy_history.filter(dy);
            time_history.filter(now_ns);
            if (dy)
            {
                dispatchScrollWheelEventX(dy / vscrolldivisor, 0, 0, now_abs);
                dy = 0;
            }
            break;
            
        case MODE_HSCROLL:
            if (!hsticky && (y > bedge || fingers > 1 || z > zlimit)) {
                touchmode = MODE_NOTOUCH;
                break;
            }
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                break;
            }
            dx = lastx-x+scrollrest;
            scrollrest = dx % hscrolldivisor;
            //REVIEW: filter out small movements (Mavericks issue)
            if (abs(dx) < scrolldxthresh)
            {
                scrollrest = dx;
                dx = 0;
            }
            if (dx)
            {
                dispatchScrollWheelEventX(0, dx / hscrolldivisor, 0, now_abs);
                dx = 0;
            }
            break;
            
        case MODE_CSCROLL:
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                break;
            }
            
            if (y < centery) {
                dx = x - lastx;
            }
            else {
                dx = lastx - x;
            }
            
            if (x < centerx) {
                dx += lasty - y;
            }
            else {
                dx += y - lasty;
                dx += scrollrest;
                scrollrest = dx % cscrolldivisor;
            }
            //REVIEW: filter out small movements (Mavericks issue)
            if (abs(dx) < scrolldxthresh)
            {
                scrollrest = dx;
                dx = 0;
            }
            if (dx)
            {
                dispatchScrollWheelEventX(dx / cscrolldivisor, 0, 0, now_abs);
                dx = 0;
            }
            break;
            
        case MODE_DRAGNOTOUCH:
            buttons |= 0x1;
            // fall through
        case MODE_PREDRAG:
            if (!immediateclick && (!palm_wt || now_ns - keytime >= maxaftertyping)) {
                buttons |= 0x1;
            }
        case MODE_NOTOUCH:
            break;
            
        default:; // nothing
    }
    
    // capture time of tap, and watch for double/triple tap
    if (isFingerTouch(z)) {
        // taps don't count if too close to typing or if currently in momentum scroll
        if ((!palm_wt || now_ns - keytime >= maxaftertyping) && !momentumscrollcurrent) {
            if (!isTouchMode()) {
                touchtime = now_ns;
                touchx = x;
                touchy = y;
            }
            if (fingers == 2) {
                wasdouble = true;
            } else if (fingers == 3) {
                wastriple = true;
            }
        }
        // any touch cancels momentum scroll
        momentumscrollcurrent = 0;
    }
    // switch modes, depending on input
    if (touchmode == MODE_PREDRAG && isFingerTouch(z)) {
        touchmode = MODE_DRAG;
        draglocktemp = _modifierdown & draglocktempmask;
    }
    if (touchmode == MODE_DRAGNOTOUCH && isFingerTouch(z)) {
        if (dragTimer)
            cancelTimer(dragTimer);
        touchmode=MODE_DRAGLOCK;
    }
    ////if ((w>wlimit || w<3) && isFingerTouch(z) && scroll && (wvdivisor || (hscroll && whdivisor)))
    if (MODE_MTOUCH != touchmode && (fingers > 1) && isFingerTouch(z)) {
        touchmode = MODE_MTOUCH;
        tracksecondary = false;
    }
    
    if (scroll && cscrolldivisor) {
        if (touchmode == MODE_NOTOUCH && z > z_finger && y > tedge && (ctrigger == 1 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && y > tedge && x > redge && (ctrigger == 2))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x > redge && (ctrigger == 3 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x > redge && y < bedge && (ctrigger == 4))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && y < bedge && (ctrigger == 5 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && y < bedge && x < ledge && (ctrigger == 6))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x < ledge && (ctrigger == 7 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x < ledge && y > tedge && (ctrigger == 8))
            touchmode = MODE_CSCROLL;
    }
    if ((MODE_NOTOUCH == touchmode || (MODE_HSCROLL == touchmode && y >= bedge)) &&
        z > z_finger && x > redge && vscrolldivisor && scroll) {
        touchmode = MODE_VSCROLL;
        scrollrest = 0;
    }
    if ((MODE_NOTOUCH == touchmode || (MODE_VSCROLL == touchmode && x <= redge)) &&
        z > z_finger && y < bedge && hscrolldivisor && scroll) {
        touchmode = MODE_HSCROLL;
        scrollrest = 0;
    }
    if (touchmode == MODE_NOTOUCH && z > z_finger && last_fingers != 2) {
        touchmode = MODE_MOVE;
    }
    
    // dispatch dx/dy and current button status
    dispatchRelativePointerEventX(dx / divisorx, dy / divisory, buttons, now_abs);
    
    // always save last seen position for calculating deltas later
    lastx = x;
    lasty = y;
    last_fingers = fingers;
    
#ifdef DEBUG_VERBOSE
    DEBUG_LOG("ps2: dx=%d, dy=%d (%d,%d) z=%d mode=(%d,%d,%d) buttons=%d wasdouble=%d wastriple=%d\n", dx, dy, x, y, z, tm1, tm2, touchmode, buttons, wasdouble, wastriple);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::
dispatchRelativePointerEventWithPacket(UInt8 *packet,
                                       UInt32 packetSize) {
    //
    // Process the three byte relative format packet that was retrieved from the
    // trackpad. The format of the bytes is as follows:
    //
    //  7  6  5  4  3  2  1  0
    // -----------------------
    // YO XO YS XS  1  M  R  L
    // X7 X6 X5 X4 X3 X3 X1 X0  (X delta)
    // Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0  (Y delta)
    //
    
    UInt32 buttons = 0;
    SInt32 dx, dy;
    
    if ((packet[0] & 0x1)) buttons |= 0x1;  // left button   (bit 0 in packet)
    if ((packet[0] & 0x2)) buttons |= 0x2;  // right button  (bit 1 in packet)
    if ((packet[0] & 0x4)) buttons |= 0x4;  // middle button (bit 2 in packet)
    
    dx = packet[1];
    if (dx) {
        dx = packet[1] - ((packet[0] << 4) & 0x100);
    }
    
    dy = packet[2];
    if (dy) {
        dy = ((packet[0] << 3) & 0x100) - packet[2];
    }
    
    uint64_t now_abs;
    clock_get_uptime(&now_abs);
    DEBUG_LOG("Dispatch relative PS2 packet: dx=%d, dy=%d, buttons=%d\n", dx, dy, buttons);
    dispatchRelativePointerEventX(dx, dy, buttons, now_abs);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setTouchPadEnable(bool enable) {
    DEBUG_LOG("setTouchpadEnable enter\n");
    //
    // Instructs the trackpad to start or stop the reporting of data packets.
    // It is safe to issue this request from the interrupt/completion context.
    //
    
    if (enable) {
        initTouchPad();
    } else {
        // to disable just reset the mouse
        resetMouse();
    }
}

bool ApplePS2ALPSGlidePoint::getStatus(ALPSStatus_t *status) {
    return repeatCmd(NULL, NULL, kDP_SetDefaultsAndDisable, status);
}

/*
 * Turn touchpad tapping on or off. The sequences are:
 * 0xE9 0xF5 0xF5 0xF3 0x0A to enable,
 * 0xE9 0xF5 0xF5 0xE8 0x00 to disable.
 * My guess that 0xE9 (GetInfo) is here as a sync point.
 * For models that also have stickpointer (DualPoints) its tapping
 * is controlled separately (0xE6 0xE6 0xE6 0xF3 0x14|0x0A) but
 * we don't fiddle with it.
 */
bool ApplePS2ALPSGlidePoint::tapMode(bool enable) {
    int cmd = enable ? kDP_SetMouseSampleRate : kDP_SetMouseResolution;
    UInt8 tapArg = enable ? 0x0A : 0x00;
    TPS2Request<8> request;
    ALPSStatus_t result;
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_GetMouseInformation;
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commands[3].command = kPS2C_ReadDataPort;
    request.commands[3].inOrOut = 0;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[6].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[6].inOrOut = cmd;
    request.commands[7].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[7].inOrOut = tapArg;
    request.commandsCount = 8;
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 8) {
        DEBUG_LOG("Enabling tap mode failed before getStatus call, command count=%d\n",
                  request.commandsCount);
        return false;
    }
    
    return getStatus(&result);
}

bool ApplePS2ALPSGlidePoint::enterCommandMode() {
    DEBUG_LOG("enter command mode\n");
    TPS2Request<4> request;
    ALPSStatus_t status;
    
    if (!repeatCmd(NULL, NULL, kDP_MouseResetWrap, &status)) {
        IOLog("ALPS: Failed to enter command mode!\n");
        return false;
    }
    return true;
}

bool ApplePS2ALPSGlidePoint::exitCommandMode() {
    DEBUG_LOG("exit command mode\n");
    TPS2Request<1> request;
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseStreamMode;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return true;
}

bool ApplePS2ALPSGlidePoint::hwInitV3() {
    int regVal;
    
    if ((priv.flags & ALPS_DUALPOINT) &&
        setupTrackstickV3(ALPS_REG_BASE_PINNACLE) == kIOReturnIOError)
        goto error;
    
    if (!(enterCommandMode() &&
          absoluteModeV3())) {
        IOLog("ERROR: Failed to enter absolute mode\n");
        goto error;
    }
    
    regVal = commandModeReadReg(0x0006);
    if (regVal == -1)
        goto error;
    if (!commandModeWriteReg(regVal | 0x01))
        goto error;
    
    regVal = commandModeReadReg(0x0007);
    if (regVal == -1)
        goto error;
    if (!commandModeWriteReg(regVal | 0x01))
        goto error;
    
    if (commandModeReadReg(0x0144) == -1)
        goto error;
    if (!commandModeWriteReg(0x04))
        goto error;
    
    if (commandModeReadReg(0x0159) == -1)
        goto error;
    if (!commandModeWriteReg(0x03))
        goto error;
    
    if (commandModeReadReg(0x0163) == -1)
        goto error;
    if (!commandModeWriteReg(0x0163, 0x03))
        goto error;
    
    if (commandModeReadReg(0x0162) == -1)
        goto error;
    if (!commandModeWriteReg(0x0162, 0x04))
        goto error;
    
    exitCommandMode();
    
    /* Set rate and enable data reporting */
    DEBUG_LOG("set sample rate\n");
    if (!setSampleRateAndEnable(0x28)) {
        return false;
    }
    
    return true;
    
error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::alps_get_v3_v7_resolution(int reg_pitch)
{
    int reg, x_pitch, y_pitch, x_electrode, y_electrode, x_phys, y_phys;
    
    reg = commandModeReadReg(reg_pitch);
    if (reg < 0)
        return reg;
    
    x_pitch = (char)(reg << 4) >> 4; /* sign extend lower 4 bits */
    x_pitch = 50 + 2 * x_pitch; /* In 0.1 mm units */
    
    y_pitch = (char)reg >> 4; /* sign extend upper 4 bits */
    y_pitch = 36 + 2 * y_pitch; /* In 0.1 mm units */
    
    reg = commandModeReadReg(reg_pitch + 1);
    if (reg < 0)
        return reg;
    
    x_electrode = (char)(reg << 4) >> 4; /* sign extend lower 4 bits */
    x_electrode = 17 + x_electrode;
    
    y_electrode = (char)reg >> 4; /* sign extend upper 4 bits */
    y_electrode = 13 + y_electrode;
    
    x_phys = x_pitch * (x_electrode - 1); /* In 0.1 mm units */
    y_phys = y_pitch * (y_electrode - 1); /* In 0.1 mm units */
    
    priv.x_res = priv.x_max * 10 / x_phys; /* units / mm */
    priv.y_res = priv.y_max * 10 / y_phys; /* units / mm */
    
    IOLog("pitch %dx%d num-electrodes %dx%d physical size %dx%d mm res %dx%d\n",
                x_pitch, y_pitch, x_electrode, y_electrode,
                x_phys / 10, y_phys / 10, priv.x_res, priv.y_res);
    
    return true;
}

bool ApplePS2ALPSGlidePoint::hwInitRushmoreV3() {
    
    
    int regVal;
    TPS2Request<1> request;
    
    if (priv.flags & ALPS_DUALPOINT) {
        regVal = setupTrackstickV3(ALPS_REG_BASE_RUSHMORE);
        if (regVal == kIOReturnIOError) {
            goto error;
        }
        if (regVal == kIOReturnNoDevice) {
            priv.flags &= ~ALPS_DUALPOINT;
        }
    }
    
    if (!enterCommandMode() ||
        commandModeReadReg(0xc2d9) == -1 ||
        !commandModeWriteReg(0xc2cb, 0x00)) {
        goto error;
    }
    
    regVal = commandModeReadReg(0xc2c6);
    if (regVal == -1)
        goto error;
    if (!commandModeWriteReg(regVal & 0xfd))
        goto error;
    
    if (!commandModeWriteReg(0xc2c9, 0x64))
        goto error;
    
    /* enter absolute mode */
    regVal = commandModeReadReg(0xc2c4);
    if (regVal == -1)
        goto error;
    if (!commandModeWriteReg(regVal | 0x02))
        goto error;
    
    exitCommandMode();
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_Enable;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == 1;
    
error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::setSampleRateAndEnable(UInt8 rate) {
    TPS2Request<3> request;
    UInt8 commandNum = 0;
    
    DEBUG_LOG("setSampleRateAndEnable %d\n", (int) rate);
    
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = kDP_SetMouseSampleRate;                // 0xF3
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = rate;                                // 100
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = kDP_Enable;                            // 0xF4, Enable Data Reporting
    request.commandsCount = commandNum;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == commandNum;
}


int ApplePS2ALPSGlidePoint::commandModeReadReg(int addr) {
    TPS2Request<4> request;
    ALPSStatus_t status;
    
    if (!commandModeSetAddr(addr)) {
        DEBUG_LOG("Failed to set addr to read register\n");
        return -1;
    }
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_GetMouseInformation; //sync..
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commands[3].command = kPS2C_ReadDataPort;
    request.commands[3].inOrOut = 0;
    request.commandsCount = 4;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 4) {
        return -1;
    }
    
    status.bytes[0] = request.commands[1].inOrOut;
    status.bytes[1] = request.commands[2].inOrOut;
    status.bytes[2] = request.commands[3].inOrOut;
    
    DEBUG_LOG("ApplePS2ALPSGlidePoint read reg result: { 0x%02x, 0x%02x, 0x%02x }\n", status.bytes[0], status.bytes[1], status.bytes[2]);
    
    /* The address being read is returned in the first 2 bytes
     * of the result. Check that the address matches the expected
     * address.
     */
    if (addr != ((status.bytes[0] << 8) | status.bytes[1])) {
        DEBUG_LOG("ApplePS2ALPSGlidePoint ERROR: read wrong registry value, expected: %x\n", addr);
        return -1;
    }
    
    return status.bytes[2];
}

bool ApplePS2ALPSGlidePoint::commandModeWriteReg(int addr, UInt8 value) {
    
    if (!commandModeSetAddr(addr)) {
        return false;
    }
    
    return commandModeWriteReg(value);
}

bool ApplePS2ALPSGlidePoint::commandModeWriteReg(UInt8 value) {
    if (!commandModeSendNibble((value >> 4) & 0xf)) {
        return false;
    }
    if (!commandModeSendNibble(value & 0xf)) {
        return false;
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::commandModeSendNibble(int nibble) {
    SInt32 command;
    // The largest amount of requests we will have is 2 right now
    // 1 for the initial command, and 1 for sending data OR 1 for receiving data
    // If the nibble commands at the top change then this will need to change as
    // well. For now we will just validate that the request will not overload
    // this object.
    TPS2Request<2> request;
    int cmdCount = 0, send = 0, receive = 0, i;
    
    if (nibble > 0xf) {
        IOLog("%s::commandModeSendNibble ERROR: nibble value is greater than 0xf, command may fail\n", getName());
    }
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    command = priv.nibble_commands[nibble].command;
    request.commands[cmdCount++].inOrOut = command & 0xff;
    
    send = (command >> 12 & 0xf);
    receive = (command >> 8 & 0xf);
    
    // Validate that the number of requests will not exceed our buffer as
    // defined above
    // Also, send can never be > 1 since all we have available is the data
    // from the alps_nibble_commands which is 1 byte
    if ((send > 1) || ((send + receive + 1) > 2)) {
        IOLog("%s::commandModeSendNibble: ERROR: Nibble commands have changed. Cannot process nibble that sends or receives more than 1 byte of data.\n", getName());
        return false;
    }
    
    //DEBUG_LOG("%s: send nibble: nibble=%x command info=%x command=0x%02x send=%d, receive=%d, data=0x%02x\n",
    //          getName(), nibble, command, request.commands[0].inOrOut, send, receive, priv.nibble_commands[nibble].data);
    
    if (send > 0) {
        request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[cmdCount++].inOrOut = priv.nibble_commands[nibble].data;
    }
    
    // Receive the amount of data for the given command
    // Even though we don't read the data, we should drain the data port to follow protocol
    for (i = 0; i < receive; i++) {
        request.commands[cmdCount].command = kPS2C_ReadDataPort;
        request.commands[cmdCount++].inOrOut = 0;
    }
    
    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));
    
    _device->submitRequestAndBlock(&request);
    
    //DEBUG_LOG("%s: num nibble commands=%d, expected=%d\n", getName(), request.commandsCount, cmdCount);
    
    return request.commandsCount == cmdCount;
}

bool ApplePS2ALPSGlidePoint::commandModeSetAddr(int addr) {
    
    TPS2Request<1> request;
    int i, nibble;
    
    //    DEBUG_LOG("command mode set addr with addr command: 0x%02x\n", priv.addr_command);
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = priv.addr_command;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 1) {
        return false;
    }
    
    for (i = 12; i >= 0; i -= 4) {
        nibble = (addr >> i) & 0xf;
        if (!commandModeSendNibble(nibble)) {
            return false;
        }
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::passthroughModeV3(int regBase, bool enable) {
    int regVal;
    bool ret = false;
    
    DEBUG_LOG("passthrough mode enable=%d\n", enable);
    
    if (!enterCommandMode()) {
        IOLog("ERROR: Failed to enter command mode while enabling passthrough mode\n");
        return false;
    }
    
    regVal = commandModeReadReg(regBase + 0x0008);
    if (regVal == -1) {
        IOLog("Failed to read register while setting up passthrough mode\n");
        goto error;
    }
    
    if (enable) {
        regVal |= 0x01;
    } else {
        regVal &= ~0x01;
    }
    
    ret = commandModeWriteReg(regVal);
    
error:
    if (!exitCommandMode()) {
        IOLog("ERROR: failed to exit command mode while enabling passthrough mode v3\n");
        return false;
    }
    
    return ret;
};

bool ApplePS2ALPSGlidePoint::passthroughModeV2(bool enable) {
    int cmd = enable ? kDP_SetMouseScaling2To1 : kDP_SetMouseScaling1To1;
    TPS2Request<4> request;
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = cmd;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = cmd;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = cmd;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetDefaultsAndDisable;
    request.commandsCount = 4;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == 4;
};

bool ApplePS2ALPSGlidePoint::absoluteModeV3() {
    
    int regVal;
    
    regVal = commandModeReadReg(0x0004);
    if (regVal == -1) {
        return false;
    }
    
    regVal |= 0x06;
    if (!commandModeWriteReg(regVal)) {
        return false;
    }
    
    return true;
}

IOReturn ApplePS2ALPSGlidePoint::probeTrackstickV3(int regBase) {
    int ret = kIOReturnIOError;
    int regVal;
    
    if (!enterCommandMode()) {
        goto error;
    }
    
    regVal = commandModeReadReg(regBase + 0x08);
    
    if (regVal == -1) {
        // On linux this is reported as an IO error
        // however, here it can also mean that the device
        // doesn't exist. So I lean on the side that it
        // doesn't exist. If there was an IO error here
        // it doesn't matter too much anyway, the trackstick
        // just won't work or there will be another IO error
        // later on that will break out of the init as well
        ret = kIOReturnNoDevice;
        goto error;
    }
    
    /* bit 7: trackstick is present */
    ret = regVal & 0x80 ? 0 : kIOReturnNoDevice;
    
error:
    exitCommandMode();
    return ret;
}

IOReturn ApplePS2ALPSGlidePoint::setupTrackstickV3(int regBase) {
    IOReturn ret = 0;
    ALPSStatus_t report;
    TPS2Request<3> request;
    
    if (!passthroughModeV3(regBase, true)) {
        return kIOReturnIOError;
    }
    
    if (!repeatCmd(NULL, NULL, kDP_SetMouseScaling2To1, &report)) {
        IOLog("WARN: trackstick E7 report failed\n");
        ret = kIOReturnNoDevice;
    } else {
        /*
         * Not sure what this does, but it is absolutely
         * essential. Without it, the touchpad does not
         * work at all and the trackstick just emits normal
         * PS/2 packets.
         */
        request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[0].inOrOut = kDP_SetMouseScaling1To1;
        request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[1].inOrOut = kDP_SetMouseScaling1To1;
        request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[2].inOrOut = kDP_SetMouseScaling1To1;
        request.commandsCount = 3;
        assert(request.commandsCount <= countof(request.commands));
        _device->submitRequestAndBlock(&request);
        if (request.commandsCount != 3) {
            IOLog("ERROR: error sending magic E6 scaling sequence\n");
            ret = kIOReturnIOError;
            goto error;
        }
        if (!(commandModeSendNibble(0x9) && commandModeSendNibble(0x4))) {
            IOLog("ERROR: error sending magic E6 nibble sequence\n");
            ret = kIOReturnIOError;
            goto error;
        }
        DEBUG_LOG("Sent magic E6 sequence\n");
        
        /* Ensures trackstick packets are in the correct format */
        if (!(enterCommandMode() &&
              commandModeWriteReg(regBase + 0x0008, 0x82) &&
              exitCommandMode())) {
            ret = kIOReturnIOError;
            goto error;
        }
    }
error:
    if (!passthroughModeV3(regBase, false)) {
        ret = kIOReturnIOError;
    }
    
    return ret;
}

/*
 * Used during both passthrough mode initialization and touchpad enablement
 */
bool ApplePS2ALPSGlidePoint::v1v2MagicEnable() {
    TPS2Request<5> request;
    
    /* Try ALPS magic knock - 4 disable before enable */
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_Enable;
    request.commandsCount = 5;
    
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == 5;
}

bool ApplePS2ALPSGlidePoint::absoluteModeV1V2() {
    
    TPS2Request<1> request;
    
    if (!v1v2MagicEnable()) {
        IOLog("Failed to enter absolute mode with magic knock\n");
        return false;
    }
    
    /*
     * Switch mouse to poll (remote) mode so motion data will not
     * get in our way
     */
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_MouseSetPoll;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == 1;
}

bool ApplePS2ALPSGlidePoint::repeatCmd(SInt32 init_command, SInt32 init_arg, SInt32 repeated_command, ALPSStatus_t *report) {
    TPS2Request<9> request;
    int byte0, cmd;
    cmd = byte0 = 0;
    
    if (init_command) {
        request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[cmd++].inOrOut = kDP_SetMouseResolution;
        request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[cmd++].inOrOut = init_arg;
    }
    
    
    // 3X run command
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    
    // Get info/result
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = kDP_GetMouseInformation;
    byte0 = cmd;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commandsCount = cmd;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    report->bytes[0] = request.commands[byte0].inOrOut;
    report->bytes[1] = request.commands[byte0+1].inOrOut;
    report->bytes[2] = request.commands[byte0+2].inOrOut;
    
    DEBUG_LOG("%02x report: [0x%02x 0x%02x 0x%02x]\n",
              repeated_command,
              report->bytes[0],
              report->bytes[1],
              report->bytes[2]);
    
    return request.commandsCount == cmd;
}

bool ApplePS2ALPSGlidePoint::hwInitV1V2() {
    TPS2Request<1> request;
    
    if (priv.flags & ALPS_PASS) {
        if (!passthroughModeV2(true)) {
            IOLog("ERROR: Failed to enter passthrough mode\n");
            return false;
        }
    }
    
    if (!tapMode(true)) {
        IOLog("ERROR: Failed to enable hardware tapping\n");
        return false;
    }
    
    if (!absoluteModeV1V2()) {
        IOLog("ERROR: Failed to enable absolute mode\n");
        return false;
    }
    
    if (priv.flags & ALPS_PASS) {
        if (!passthroughModeV2(false)) {
            IOLog("ERROR: Failed to exit passthrough mode\n");
            return false;
        }
    }
    
    // Enable data reporting
    v1v2MagicEnable();
    
    // Not sure if this is needed or not...previous version did not have
    // it but the linux driver does, need to figure out why its not reporting
    // data right now
    /* ALPS needs stream mode, otherwise it won't report any data */
    /* request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
     request.commands[0].inOrOut = kDP_SetMouseStreamMode;
     request.commandsCount = 1;
     _device->submitRequestAndBlock(&request);
     
     if (request.commandsCount != 1) {
     IOLog("ERROR: Failed to set stream mode on touchpad\n");
     return false;
     }*/
    
    return true;
}

/* Must be in command mode when calling this function */
bool ApplePS2ALPSGlidePoint::absoluteModeV4() {
    int regVal;
    
    regVal = commandModeReadReg(0x0004);
    if (regVal == -1) {
        return false;
    }
    
    regVal |= 0x02;
    if (!commandModeWriteReg(regVal)) {
        return false;
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::hwInitV4() {
    TPS2Request<7> request;
    
    if (!enterCommandMode()) {
        goto error;
    }
    
    if (!absoluteModeV4()) {
        IOLog("ERROR: Failed to enter absolute mode\n");
        goto error;
    }
    
    DEBUG_LOG("now setting a bunch of regs\n");
    
    if (!commandModeWriteReg(0x0007, 0x8c)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0149, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0160, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x017f, 0x15)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0151, 0x01)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0168, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x014a, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0161, 0x03)) {
        goto error;
    }
    
    exitCommandMode();
    
    /*
     * This sequence changes the output from a 9-byte to an
     * 8-byte format. All the same data seems to be present,
     * just in a more compact format.
     */
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseSampleRate;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = 0xc8;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetMouseSampleRate;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = 0x64;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetMouseSampleRate;
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = 0x50;
    request.commands[6].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[6].inOrOut = kDP_GetId;
    request.commandsCount = 7;
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 7) {
        return false;
    }
    
    /* Set rate and enable data reporting */
    DEBUG_LOG("set sample rate\n");
    if (!setSampleRateAndEnable(0x64)) {
        return false;
    }
    
    return true;
    
error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::hwInitV7(){
    TPS2Request<16> request;
    int reg_val;
    
    if (!enterCommandMode())
        goto error;
    
    if (commandModeReadReg(0xc2d9) == -1)
        goto error;
    
    if (!alps_get_v3_v7_resolution(0xc397))
        goto error;
    
    if (!commandModeWriteReg(0xc2c9, 0x64))
        goto error;
    
    reg_val = commandModeReadReg(0xc2c4);
    if (reg_val == -1)
        goto error;
    
    if (!commandModeWriteReg(reg_val | 0x02))
        goto error;
    
    exitCommandMode();
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_Enable;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == 1;
error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::hwInitDolphinV1() {
    TPS2Request<16> request;
    int cmdCount = 0;
    /*
     int array[]={0xE8, 0x00, 0xE7, 0xE7,
     0xE7, 0xE9, 0xEC, 0xEC,
     0xEC, 0xE9, 0xEA, 0xEA,
     0xF3, 0x64, 0xF3, 0x28};
     */
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE8;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0x00;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE7;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE7;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE7;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE9;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEC;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEC;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEC;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE9;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEA;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = kDP_SetMouseStreamMode;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = kDP_SetMouseSampleRate;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0x64;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = kDP_SetMouseSampleRate;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0x28;
    request.commandsCount = cmdCount;
    
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != cmdCount) {
        return false;
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::alps_short_cmd_v5(UInt8 init_command, UInt8 cmd)
{
    TPS2Request<2> request;
    int cmdCount = 0;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = init_command;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = init_command;
    
    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    commandModeWriteReg(cmd);
    
    return true;
}

bool ApplePS2ALPSGlidePoint::alps_request_v5(UInt8 ahmad)
{
    TPS2Request<1> request;
    int cmdCount = 0;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = ahmad;
    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount = cmdCount;
    
}

bool ApplePS2ALPSGlidePoint::alps_hw_init_dolphin_v2()
{
    TPS2Request<3> request;
    
    /* sample rate */
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x55);
    
    /* idle count */
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x64);
    
    /* button swap */
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x47);
    
    /* data priority */
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x43);
    
    /* set power mode */
    alps_short_cmd_v5(kDP_MouseSetPoll, 0x57);
    
    
    enterCommandMode();
    
    commandModeWriteReg(0x001f, 0x09);
    
    commandModeWriteReg(0x0020, 0x09);
    
    commandModeWriteReg(0x0022, 0x80);
    
    exitCommandMode();
    
    
    /* select V1 packet format */
    alps_request_v5(kDP_SetDefaultsAndDisable);
    
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x85);
    
    /* XY and gesture */
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x73);
    
    /* profile sequence */
    alps_short_cmd_v5(kDP_SetMouseStreamMode, 0x94);
    
    /* final init */
    alps_request_v5(kDP_SetMouseStreamMode);
    
    commandModeSendNibble(0x7);
    commandModeSendNibble(0xf);
    commandModeSendNibble(0xe);
    
    alps_request_v5(kDP_Enable);
    
    return true;
    
}

void ApplePS2ALPSGlidePoint::setDefaults() {
    priv.byte0 = 0x8f;
    priv.mask0 = 0x8f;
    priv.flags = ALPS_DUALPOINT;
    
    priv.x_max = 2000;
    priv.y_max = 1400;
    priv.x_bits = 15;
    priv.y_bits = 11;
    
    switch (priv.proto_version) {
        case ALPS_PROTO_V1:
        case ALPS_PROTO_V2:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV1V2;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV1V2;
            priv.x_max = 1023;
            priv.y_max = 767;
            //            set_abs_params = alps_set_abs_params_st;
            break;
        case ALPS_PROTO_V3:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV3;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV3;
            //            set_abs_params = alps_set_abs_params_mt;
            decode_fields = &ApplePS2ALPSGlidePoint::decodePinnacle;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            
            if (probeTrackstickV3(ALPS_REG_BASE_PINNACLE)) {
                priv.flags &= ~ALPS_DUALPOINT;
            }
            break;
        case ALPS_PROTO_V3_RUSHMORE:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitRushmoreV3;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV3;
            //            set_abs_params = alps_set_abs_params_mt;
            decode_fields = &ApplePS2ALPSGlidePoint::decodeRushmore;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.x_bits = 16;
            priv.y_bits = 12;
            
            if (probeTrackstickV3(ALPS_REG_BASE_RUSHMORE)) {
                priv.flags &= ~ALPS_DUALPOINT;
            }
            break;
        case ALPS_PROTO_V4:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV4;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV4;
            //            set_abs_params = alps_set_abs_params_mt;
            priv.nibble_commands = alps_v4_nibble_commands;
            priv.addr_command = kDP_SetDefaultsAndDisable;
            break;
        case ALPS_PROTO_V5:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitDolphinV1;
            process_packet = &ApplePS2ALPSGlidePoint::alps_process_touchpad_packet_v3_v5;
            decode_fields = &ApplePS2ALPSGlidePoint::decodeDolphin;
            //            set_abs_params = alps_set_abs_params_mt;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.byte0 = 0xc8;
            priv.mask0 = 0xc8;
            priv.flags = 0;
            priv.x_max = 1360;
            priv.y_max = 660;
            priv.x_bits = 23;
            priv.y_bits = 12;
            break;
            
        case ALPS_PROTO_V7:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV7;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV7;
            decode_fields = &ApplePS2ALPSGlidePoint::decodeV7;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.byte0 = 0x48;
            priv.mask0 = 0x48;
            priv.flags = ALPS_DUALPOINT;
            
            priv.x_max = 0xfff;
            priv.y_max = 0x7ff;
            
            if (priv.fw_ver[1] != 0xba){
                priv.flags |= ALPS_BUTTONPAD;
                IOLog("ButtonPad Detected!\n");
            }
            
            if (probeTrackstickV3(ALPS_REG_BASE_V7) < 0){
                priv.flags &= ~ALPS_DUALPOINT;
                IOLog("Trackstick Detected!\n");
            }
            
            break;
    }
}

bool ApplePS2ALPSGlidePoint::matchTable(ALPSStatus_t *e7, ALPSStatus_t *ec) {
    const struct alps_model_info *model;
    int i;
    
    for (i = 0; i < ARRAY_SIZE(alps_model_data); i++) {
        model = &alps_model_data[i];
        
        if (!memcmp(e7->bytes, model->signature, sizeof(model->signature)) &&
            (!model->command_mode_resp ||
             model->command_mode_resp == ec->bytes[2])) {
                
                priv.proto_version = model->proto_version;
                setDefaults();
                
                priv.flags = model->flags;
                priv.byte0 = model->byte0;
                priv.mask0 = model->mask0;
                
                return true;
            }
    }
    
    return false;
}

IOReturn ApplePS2ALPSGlidePoint::identify() {
    ALPSStatus_t e6, e7, ec;
    
    /*
     * First try "E6 report".
     * ALPS should return 0,0,10 or 0,0,100 if no buttons are pressed.
     * The bits 0-2 of the first byte will be 1s if some buttons are
     * pressed.
     */
    
    if (!repeatCmd(kDP_SetMouseResolution, NULL, kDP_SetMouseScaling1To1, &e6)) {
        IOLog("%s::identify: not an ALPS device. Error getting E6 report\n", getName());
        return kIOReturnIOError;
    }
    
    if ((e6.bytes[0] & 0xf8) != 0 || e6.bytes[1] != 0 || (e6.bytes[2] != 10 && e6.bytes[2] != 100)) {
        IOLog("%s::identify: not an ALPS device. Invalid E6 report\n", getName());
        return kIOReturnInvalid;
    }
    
    /*
     * Now get the "E7" and "EC" reports.  These will uniquely identify
     * most ALPS touchpads.
     */
    if (!(repeatCmd(kDP_SetMouseResolution, NULL, kDP_SetMouseScaling2To1, &e7) &&
          repeatCmd(kDP_SetMouseResolution, NULL, kDP_MouseResetWrap, &ec) &&
          exitCommandMode())) {
        IOLog("%s::identify: not an ALPS device. Error getting E7/EC report\n", getName());
        return kIOReturnIOError;
    }
    
    if (matchTable(&e7, &ec)) {
        return 0;
        
    } else if (e7.bytes[0] == 0x73 && e7.bytes[1] == 0x03 && e7.bytes[2] == 0x50 &&
               ec.bytes[0] == 0x73 && (ec.bytes[1] == 0x01 || ec.bytes[1] == 0x02)) {
        priv.proto_version = ALPS_PROTO_V5;
        setDefaults();
        IOLog("ALPS: Found a V5 Dolphin touchpad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
        if (ec.bytes[1] == 0x02) {
            hw_init = &ApplePS2ALPSGlidePoint::alps_hw_init_dolphin_v2;
            IOLog("ALPS: Dolphin hardware rev. 2 detected. Support is experimental...\n");
        }
    } else if (ec.bytes[0] == 0x88 &&
               ((ec.bytes[1] & 0xf0) == 0xb0 || (ec.bytes[1] & 0xf0) == 0xc0)) {
        priv.proto_version = ALPS_PROTO_V7;
        setDefaults();
        IOLog("ALPS: Found a V7 touchpad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
    } else if (ec.bytes[0] == 0x88 && ec.bytes[1] == 0x08) {
        priv.proto_version = ALPS_PROTO_V3_RUSHMORE;
        setDefaults();
        IOLog("ALPS: Found a V3 Rushmore touchpad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
        
    } else if (ec.bytes[0] == 0x88 && ec.bytes[1] == 0x07 &&
               ec.bytes[2] >= 0x90 && ec.bytes[2] <= 0x9d) {
        priv.proto_version = ALPS_PROTO_V3;
        setDefaults();
        IOLog("ALPS: Found a V3 Pinnacle touchpad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
        
    } else {
        IOLog("ALPS: Touchpad id didn't match V1-V5: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n",
              e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
        return kIOReturnInvalid;
    }
    
    /* Save the Firmware version */
    memcpy(priv.fw_ver, ec.bytes, 3);
    return 0;
}
