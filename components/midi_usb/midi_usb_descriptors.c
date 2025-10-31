/**
 * @file midi_usb_descriptors.c
 * @brief USB MIDI Descriptors Implementation
 * 
 * Implements USB descriptors per USB MIDI 1.0 and 2.0 specifications
 */

#include "midi_usb_descriptors.h"
#include "sdkconfig.h"
#include <string.h>

// USB Class codes for Audio/MIDI
#define USB_CLASS_AUDIO                     0x01
#define USB_SUBCLASS_AUDIO_CONTROL          0x01
#define USB_SUBCLASS_MIDI_STREAMING         0x03

// Class-specific descriptor types
#define USB_CS_INTERFACE                    0x24
#define USB_CS_ENDPOINT                     0x25

// MIDI Streaming descriptor subtypes
#define USB_MS_HEADER                       0x01
#define USB_MS_MIDI_IN_JACK                 0x02
#define USB_MS_MIDI_OUT_JACK                0x03
#define USB_MS_ELEMENT                      0x04

// MIDI Jack types
#define USB_JACK_TYPE_EMBEDDED              0x01
#define USB_JACK_TYPE_EXTERNAL              0x02

// Endpoint descriptor subtypes
#define USB_MS_GENERAL                      0x01
#define USB_MS_GENERAL_2_0                  0x02

// Endpoint addresses
#define EPNUM_MIDI_OUT                      0x01
#define EPNUM_MIDI_IN                       0x81

// MIDI 2.0 specific descriptor subtypes
#define USB_MS_GENERAL_2_0                  0x02
#define USB_CS_GR_TRM_BLOCK                 0x26
#define USB_GR_TRM_BLOCK_HEADER             0x01
#define USB_GR_TRM_BLOCK                    0x02

// Group Terminal Block types[file:2]
#define USB_GTB_TYPE_BIDIRECTIONAL          0x00
#define USB_GTB_TYPE_INPUT_ONLY             0x01
#define USB_GTB_TYPE_OUTPUT_ONLY            0x02

// MIDI Protocol codes[file:2]
#define USB_MIDI_PROTO_UNKNOWN              0x00
#define USB_MIDI_PROTO_MIDI_1_0_64          0x01
#define USB_MIDI_PROTO_MIDI_1_0_64_JR       0x02
#define USB_MIDI_PROTO_MIDI_1_0_128         0x03
#define USB_MIDI_PROTO_MIDI_1_0_128_JR      0x04
#define USB_MIDI_PROTO_MIDI_2_0             0x11
#define USB_MIDI_PROTO_MIDI_2_0_JR          0x12

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0x00,    // Defined at interface level
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    
    .idVendor           = CONFIG_MIDI_USB_DEVICE_VID,
    .idProduct          = CONFIG_MIDI_USB_DEVICE_PID,
    .bcdDevice          = 0x0100,  // Device version 1.0
    
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    
    .bNumConfigurations = 0x01
};

/**
 * @brief Get device descriptor
 */
const uint8_t* midi_usb_get_device_descriptor(void) {
    return (const uint8_t*)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor - MIDI 1.0 Only (Alternate Setting 0)
//--------------------------------------------------------------------+

enum {
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + \
                           TUD_AUDIO_DESC_CLK_SRC_LEN + \
                           TUD_AUDIO_DESC_IAD_LEN + \
                           TUD_AUDIO_DESC_STD_AC_LEN + \
                           TUD_AUDIO_DESC_CS_AC_LEN + \
                           TUD_AUDIO_DESC_STD_AS_INT_LEN + \
                           TUD_AUDIO_DESC_CS_AS_INT_LEN + \
                           TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN + \
                           TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN + \
                           TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

/**
 * @brief Configuration descriptor with MIDI 1.0 support only
 */
static const uint8_t desc_configuration_midi1[] = {
    // Configuration Descriptor
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    
    // Interface Association Descriptor (IAD)
    // Required for composite USB-MIDI device
    0x08,                           // bLength
    TUSB_DESC_INTERFACE_ASSOCIATION,// bDescriptorType
    ITF_NUM_AUDIO_CONTROL,          // bFirstInterface
    0x02,                           // bInterfaceCount (AC + MS)
    USB_CLASS_AUDIO,                // bFunctionClass
    USB_SUBCLASS_AUDIO_CONTROL,     // bFunctionSubClass
    0x00,                           // bFunctionProtocol
    0x00,                           // iFunction
    
    // Standard AudioControl Interface Descriptor
    0x09,                           // bLength
    TUSB_DESC_INTERFACE,            // bDescriptorType
    ITF_NUM_AUDIO_CONTROL,          // bInterfaceNumber
    0x00,                           // bAlternateSetting
    0x00,                           // bNumEndpoints
    USB_CLASS_AUDIO,                // bInterfaceClass
    USB_SUBCLASS_AUDIO_CONTROL,     // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface
    
    // Class-Specific AudioControl Interface Descriptor
    0x09,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    0x01,                           // bDescriptorSubtype (HEADER)
    0x00, 0x01,                     // bcdADC (1.0)
    0x09, 0x00,                     // wTotalLength
    0x01,                           // bInCollection
    ITF_NUM_MIDI_STREAMING,         // baInterfaceNr(1)
    
    // Standard MIDI Streaming Interface Descriptor
    0x09,                           // bLength
    TUSB_DESC_INTERFACE,            // bDescriptorType
    ITF_NUM_MIDI_STREAMING,         // bInterfaceNumber
    0x00,                           // bAlternateSetting
    0x02,                           // bNumEndpoints (IN + OUT)
    USB_CLASS_AUDIO,                // bInterfaceClass
    USB_SUBCLASS_MIDI_STREAMING,    // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface
    
    // Class-Specific MS Interface Header Descriptor
    0x07,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_HEADER,                  // bDescriptorSubtype
    0x00, 0x01,                     // bcdMSC (1.0)
    0x41, 0x00,                     // wTotalLength (65 bytes)
    
    // MIDI IN Jack Descriptor (Embedded)
    0x06,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_IN_JACK,            // bDescriptorSubtype
    USB_JACK_TYPE_EMBEDDED,         // bJackType
    0x01,                           // bJackID
    0x00,                           // iJack
    
    // MIDI IN Jack Descriptor (External)
    0x06,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_IN_JACK,            // bDescriptorSubtype
    USB_JACK_TYPE_EXTERNAL,         // bJackType
    0x02,                           // bJackID
    0x00,                           // iJack
    
    // MIDI OUT Jack Descriptor (Embedded)
    0x09,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_OUT_JACK,           // bDescriptorSubtype
    USB_JACK_TYPE_EMBEDDED,         // bJackType
    0x03,                           // bJackID
    0x01,                           // bNrInputPins
    0x02,                           // baSourceID(1)
    0x01,                           // baSourcePin(1)
    0x00,                           // iJack
    
    // MIDI OUT Jack Descriptor (External)
    0x09,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_OUT_JACK,           // bDescriptorSubtype
    USB_JACK_TYPE_EXTERNAL,         // bJackType
    0x04,                           // bJackID
    0x01,                           // bNrInputPins
    0x01,                           // baSourceID(1)
    0x01,                           // baSourcePin(1)
    0x00,                           // iJack
    
    // Standard Bulk OUT Endpoint Descriptor
    0x09,                           // bLength
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    EPNUM_MIDI_OUT,                 // bEndpointAddress
    TUSB_XFER_BULK,                 // bmAttributes
    0x40, 0x00,                     // wMaxPacketSize (64)
    0x00,                           // bInterval
    0x00,                           // bRefresh
    0x00,                           // bSynchAddress
    
    // Class-Specific MS Bulk OUT Endpoint Descriptor
    0x05,                           // bLength
    USB_CS_ENDPOINT,                // bDescriptorType
    USB_MS_GENERAL,                 // bDescriptorSubtype
    0x01,                           // bNumEmbMIDIJack
    0x01,                           // baAssocJackID(1)
    
    // Standard Bulk IN Endpoint Descriptor
    0x09,                           // bLength
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    EPNUM_MIDI_IN,                  // bEndpointAddress
    TUSB_XFER_BULK,                 // bmAttributes
    0x40, 0x00,                     // wMaxPacketSize (64)
    0x00,                           // bInterval
    0x00,                           // bRefresh
    0x00,                           // bSynchAddress
    
    // Class-Specific MS Bulk IN Endpoint Descriptor
    0x05,                           // bLength
    USB_CS_ENDPOINT,                // bDescriptorType
    USB_MS_GENERAL,                 // bDescriptorSubtype
    0x01,                           // bNumEmbMIDIJack
    0x03,                           // baAssocJackID(1)
};

/**
 * @brief Configuration descriptor with MIDI 2.0 support (Alternate Setting 1)
 * 
 * Structure per USB MIDI 2.0 spec[file:2]:
 * - Alternate Setting 0: MIDI 1.0 compatible (legacy support)
 * - Alternate Setting 1: MIDI 2.0 with UMP
 * 
 * This provides backward compatibility with USB MIDI 1.0 hosts
 * while offering full MIDI 2.0 functionality to modern hosts[file:2]
 */
static const uint8_t desc_configuration_midi2[] = {
    //=========================================================================
    // Configuration Descriptor
    //=========================================================================
    0x09,                           // bLength
    TUSB_DESC_CONFIGURATION,        // bDescriptorType
    0xCF, 0x00,                     // wTotalLength (207 bytes total)
    0x02,                           // bNumInterfaces (AC + MS)
    0x01,                           // bConfigurationValue
    0x00,                           // iConfiguration
    0x80,                           // bmAttributes (bus powered)
    0x32,                           // bMaxPower (100mA)
    
    //=========================================================================
    // Interface Association Descriptor (IAD)[file:2]
    //=========================================================================
    0x08,                           // bLength
    TUSB_DESC_INTERFACE_ASSOCIATION,// bDescriptorType
    ITF_NUM_AUDIO_CONTROL,          // bFirstInterface
    0x02,                           // bInterfaceCount (AC + MS)
    USB_CLASS_AUDIO,                // bFunctionClass
    USB_SUBCLASS_AUDIO_CONTROL,     // bFunctionSubClass
    0x00,                           // bFunctionProtocol
    0x00,                           // iFunction
    
    //=========================================================================
    // Standard AudioControl Interface Descriptor[file:2]
    //=========================================================================
    0x09,                           // bLength
    TUSB_DESC_INTERFACE,            // bDescriptorType
    ITF_NUM_AUDIO_CONTROL,          // bInterfaceNumber
    0x00,                           // bAlternateSetting
    0x00,                           // bNumEndpoints
    USB_CLASS_AUDIO,                // bInterfaceClass
    USB_SUBCLASS_AUDIO_CONTROL,     // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface
    
    //=========================================================================
    // Class-Specific AudioControl Interface Descriptor[file:2]
    //=========================================================================
    0x09,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    0x01,                           // bDescriptorSubtype (HEADER)
    0x00, 0x01,                     // bcdADC (1.0)
    0x09, 0x00,                     // wTotalLength
    0x01,                           // bInCollection
    ITF_NUM_MIDI_STREAMING,         // baInterfaceNr(1)
    
    //=========================================================================
    // ALTERNATE SETTING 0: MIDI 1.0 (Backward Compatibility)[file:2]
    //=========================================================================
    
    // Standard MIDI Streaming Interface Descriptor (Alt Setting 0)
    0x09,                           // bLength
    TUSB_DESC_INTERFACE,            // bDescriptorType
    ITF_NUM_MIDI_STREAMING,         // bInterfaceNumber
    0x00,                           // bAlternateSetting (0 = MIDI 1.0)
    0x02,                           // bNumEndpoints
    USB_CLASS_AUDIO,                // bInterfaceClass
    USB_SUBCLASS_MIDI_STREAMING,    // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface
    
    // Class-Specific MS Interface Header (MIDI 1.0)
    0x07,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_HEADER,                  // bDescriptorSubtype
    0x00, 0x01,                     // bcdMSC (1.0)
    0x41, 0x00,                     // wTotalLength (65 bytes)
    
    // MIDI IN Jack (Embedded)
    0x06,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_IN_JACK,            // bDescriptorSubtype
    USB_JACK_TYPE_EMBEDDED,         // bJackType
    0x01,                           // bJackID
    0x00,                           // iJack
    
    // MIDI IN Jack (External)
    0x06,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_IN_JACK,            // bDescriptorSubtype
    USB_JACK_TYPE_EXTERNAL,         // bJackType
    0x02,                           // bJackID
    0x00,                           // iJack
    
    // MIDI OUT Jack (Embedded)
    0x09,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_OUT_JACK,           // bDescriptorSubtype
    USB_JACK_TYPE_EMBEDDED,         // bJackType
    0x03,                           // bJackID
    0x01,                           // bNrInputPins
    0x02,                           // baSourceID(1)
    0x01,                           // baSourcePin(1)
    0x00,                           // iJack
    
    // MIDI OUT Jack (External)
    0x09,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_MIDI_OUT_JACK,           // bDescriptorSubtype
    USB_JACK_TYPE_EXTERNAL,         // bJackType
    0x04,                           // bJackID
    0x01,                           // bNrInputPins
    0x01,                           // baSourceID(1)
    0x01,                           // baSourcePin(1)
    0x00,                           // iJack
    
    // Bulk OUT Endpoint (Alt 0)
    0x09,                           // bLength
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    EPNUM_MIDI_OUT,                 // bEndpointAddress
    TUSB_XFER_BULK,                 // bmAttributes
    0x40, 0x00,                     // wMaxPacketSize (64)
    0x00,                           // bInterval
    0x00,                           // bRefresh
    0x00,                           // bSynchAddress
    
    // Class-Specific MS Bulk OUT Endpoint (Alt 0)
    0x05,                           // bLength
    USB_CS_ENDPOINT,                // bDescriptorType
    USB_MS_GENERAL,                 // bDescriptorSubtype (0x01)
    0x01,                           // bNumEmbMIDIJack
    0x01,                           // baAssocJackID(1)
    
    // Bulk IN Endpoint (Alt 0)
    0x09,                           // bLength
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    EPNUM_MIDI_IN,                  // bEndpointAddress
    TUSB_XFER_BULK,                 // bmAttributes
    0x40, 0x00,                     // wMaxPacketSize (64)
    0x00,                           // bInterval
    0x00,                           // bRefresh
    0x00,                           // bSynchAddress
    
    // Class-Specific MS Bulk IN Endpoint (Alt 0)
    0x05,                           // bLength
    USB_CS_ENDPOINT,                // bDescriptorType
    USB_MS_GENERAL,                 // bDescriptorSubtype (0x01)
    0x01,                           // bNumEmbMIDIJack
    0x03,                           // baAssocJackID(1)
    
    //=========================================================================
    // ALTERNATE SETTING 1: MIDI 2.0 with UMP[file:2]
    //=========================================================================
    
    // Standard MIDI Streaming Interface Descriptor (Alt Setting 1)
    0x09,                           // bLength
    TUSB_DESC_INTERFACE,            // bDescriptorType
    ITF_NUM_MIDI_STREAMING,         // bInterfaceNumber
    0x01,                           // bAlternateSetting (1 = MIDI 2.0)
    0x02,                           // bNumEndpoints (Bulk OUT + Interrupt IN)
    USB_CLASS_AUDIO,                // bInterfaceClass
    USB_SUBCLASS_MIDI_STREAMING,    // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface
    
    // Class-Specific MS Interface Header (MIDI 2.0)[file:2]
    0x07,                           // bLength
    USB_CS_INTERFACE,               // bDescriptorType
    USB_MS_HEADER,                  // bDescriptorSubtype
    0x00, 0x02,                     // bcdMSC (2.0) - CRITICAL!
    0x07, 0x00,                     // wTotalLength (7 bytes)
    
    // Bulk OUT Endpoint (Alt 1 - UMP)
    0x07,                           // bLength (no bRefresh/bSynchAddress)
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    EPNUM_MIDI_OUT,                 // bEndpointAddress
    TUSB_XFER_BULK,                 // bmAttributes
    0x00, 0x02,                     // wMaxPacketSize (512 for HS)
    0x00,                           // bInterval
    
    // Class-Specific MS Bulk OUT Endpoint (MIDI 2.0)[file:2]
    0x05,                           // bLength
    USB_CS_ENDPOINT,                // bDescriptorType
    USB_MS_GENERAL_2_0,             // bDescriptorSubtype (0x02 = MIDI 2.0)
    0x01,                           // bNumGrpTrmBlock
    0x01,                           // baAssoGrpTrmBlkID(1)
    
    // Interrupt IN Endpoint (Alt 1 - UMP)
    0x07,                           // bLength
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    EPNUM_MIDI_IN,                  // bEndpointAddress
    TUSB_XFER_INTERRUPT,            // bmAttributes
    0x00, 0x02,                     // wMaxPacketSize (512 for HS)
    0x01,                           // bInterval (1ms)
    
    // Class-Specific MS Interrupt IN Endpoint (MIDI 2.0)[file:2]
    0x05,                           // bLength
    USB_CS_ENDPOINT,                // bDescriptorType
    USB_MS_GENERAL_2_0,             // bDescriptorSubtype (0x02 = MIDI 2.0)
    0x01,                           // bNumGrpTrmBlock
    0x01,                           // baAssoGrpTrmBlkID(1)
};

/**
 * @brief Group Terminal Block Descriptors (retrieved separately)[file:2]
 * 
 * These are NOT part of the configuration descriptor.
 * Retrieved via GET_DESCRIPTOR request with CS_GR_TRM_BLOCK type[file:2]
 */
static const uint8_t desc_group_terminal_blocks[] = {
    // Group Terminal Block Header Descriptor[file:2]
    0x05,                           // bLength
    USB_CS_GR_TRM_BLOCK,            // bDescriptorType (0x26)
    USB_GR_TRM_BLOCK_HEADER,        // bDescriptorSubtype (0x01)
    0x12, 0x00,                     // wTotalLength (18 bytes total)
    
    // Group Terminal Block Descriptor[file:2]
    0x0D,                           // bLength (13 bytes)
    USB_CS_GR_TRM_BLOCK,            // bDescriptorType (0x26)
    USB_GR_TRM_BLOCK,               // bDescriptorSubtype (0x02)
    0x01,                           // bGrpTrmBlkID
    USB_GTB_TYPE_BIDIRECTIONAL,     // bGrpTrmBlkType (0x00 = bidirectional)
    0x00,                           // nGroupTrm (Group 1, value 0x00)
    0x01,                           // nNumGroupTrm (1 group)
    0x04,                           // iBlockItem (String descriptor 4)
    USB_MIDI_PROTO_MIDI_2_0,        // bMIDIProtocol (0x11 = MIDI 2.0)
    0x00, 0x00,                     // wMaxInputBandwidth (0 = unknown)
    0x00, 0x00,                     // wMaxOutputBandwidth (0 = unknown)
};

/**
 * @brief Get configuration descriptor
 */
const uint8_t* midi_usb_get_configuration_descriptor(bool midi2_enabled) {
    if (midi2_enabled) {
        return desc_configuration_midi2;
    }
    return desc_configuration_midi1;
}

/**
 * @brief Get Group Terminal Block descriptors (MIDI 2.0 only)[file:2]
 * 
 * Called when host sends GET_DESCRIPTOR with CS_GR_TRM_BLOCK type
 */
const uint8_t* midi_usb_get_group_terminal_blocks(void) {
    return desc_group_terminal_blocks;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static const char* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: Language (English)
    CONFIG_MIDI_USB_MANUFACTURER,    // 1: Manufacturer
    CONFIG_MIDI_USB_PRODUCT,         // 2: Product
    CONFIG_MIDI_USB_SERIAL,          // 3: Serial Number
};

/**
 * @brief Get string descriptors
 */
const char** midi_usb_get_string_descriptors(void) {
    return string_desc_arr;
}

// TinyUSB Callbacks
uint8_t const* tud_descriptor_device_cb(void) {
    return midi_usb_get_device_descriptor();
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return midi_usb_get_configuration_descriptor(CONFIG_MIDI_USB_SUPPORT_MIDI2);
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    
    static uint16_t _desc_str[32];
    
    uint8_t chr_count;
    
    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        
        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }
    
    // First byte is length (in bytes), second byte is descriptor type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    
    return _desc_str;
}
