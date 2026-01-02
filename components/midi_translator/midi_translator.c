#include "midi_defs.h"
#include "ump_packet.h"
#include "midi_translator.h"

/*      BIT SCALING FUNCTIONS      */

uint16_t midi_upscale_7to16(uint8_t value_7) 
{
    if (value_7 == 0) return 0x0000;
    if (value_7 == 0x40) return 0x8000;  // Center value
    if (value_7 == 0x7F) return 0xFFFF;
    
    // For values below center
    if (value_7 < 0x40) {
        return (uint16_t)((value_7 << 9) | (value_7 << 2) | (value_7 >> 5));
    }
    // For values above center
    else {
        return (uint16_t)(0x8000 + ((value_7 - 0x40) << 9) + 
                         ((value_7 - 0x40) << 2) + ((value_7 - 0x40) >> 5));
    }
}

// 7-bit to 32-bit upscaling
uint32_t midi_upscale_7to32(uint8_t value_7)
{
    if (value_7 == 0) return 0x00000000;
    if (value_7 == 0x40) return 0x80000000;  // Center value
    if (value_7 == 0x7F) return 0xFFFFFFFF;
    
    // For values below center
    if (value_7 < 0x40) {
        return ((uint32_t)value_7 << 25) | ((uint32_t)value_7 << 18) | 
               ((uint32_t)value_7 << 11) | ((uint32_t)value_7 << 4) | 
               ((uint32_t)value_7 >> 3);
    }
    // For values above center
    else {
        uint32_t offset = value_7 - 0x40;
        return 0x80000000 + (offset << 25) + (offset << 18) + 
               (offset << 11) + (offset << 4) + (offset >> 3);
    }
}

// MIDI 1.0 (14-bit) to 32-bit (MIDI 2.0)
uint32_t midi_upscale_14to32(uint16_t value_14)
{
    if (value_14 == 0) return 0x00000000;
    if (value_14 == 0x2000) return 0x80000000;  // Center value
    if (value_14 == 0x3FFF) return 0xFFFFFFFF;
    
    // For values below center
    if (value_14 < 0x2000) {
        return ((uint32_t)value_14 << 18) | ((uint32_t)value_14 << 4) | 
               ((uint32_t)value_14 >> 10);
    }
    // For values above center
    else {
        uint32_t offset = value_14 - 0x2000;
        return 0x80000000 + (offset << 18) + (offset << 4) + (offset >> 10);
    }
}

// Downscale 16-bit to 7-bit
uint8_t midi_downscale_16to7(uint16_t value_16)
{
    // Right shift by 9 bits (divide by 512)
    return (value_16 >> 9) & 0x7F;
}

// Downscale 32-bit to 7-bit
uint8_t midi_downscale_32to7(uint32_t value_32)
{
    // Right shift by 25 bits
    return (value_32 >> 25) & 0x7F;
}

// Downscale 32-bit to 14-bit (for pitch bend)
uint16_t midi_downscale_32to14(uint32_t value_32)
{
    // Right shift by 18 bits
    return (value_32 >> 18) & 0x3FFF;
}

/*          BUILDER FUNCTIONS          */

esp_err_t ump_build_midi2_note_on(uint8_t group, uint8_t channel, 
                                   uint8_t note, uint16_t velocity,
                                   uint8_t attr_type, uint16_t attr_data,
                                   ump_packet_t *packet)
{
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |     // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_NOTE_ON << 16) |    // Status: Note On
                       ((channel & 0x0F) << 16) |        // Channel
                       ((note & 0x7F) << 8) |            // Note number
                       (attr_type & 0xFF);               // Attribute type
    
    packet->words[1] = ((uint32_t)velocity << 16) |     // Velocity (16-bit)
                       (attr_data & 0xFFFF);             // Attribute data
    
    return ESP_OK;
}

esp_err_t ump_build_midi2_control_change(uint8_t group, uint8_t channel,
                                          uint8_t controller, uint32_t value,
                                          ump_packet_t *packet)
{
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |    // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_CONTROL_CHANGE << 16) |   // Status: CC
                       ((channel & 0x0F) << 16) |              // Channel
                       ((controller & 0x7F) << 8);           // Controller index
    
    packet->words[1] = value;                                // 32-bit value
    
    return ESP_OK;
}

esp_err_t ump_build_midi2_pitch_bend(uint8_t group, uint8_t channel,
                                      uint32_t value, ump_packet_t *packet)
{
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |    // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_PITCH_BEND << 16) |      // Status: Pitch Bend
                       ((channel & 0x0F) << 16);               // Channel
    
    packet->words[1] = value;                           // 32-bit pitch bend
    
    return ESP_OK;
}

esp_err_t ump_build_midi2_note_off(uint8_t group, uint8_t channel, uint8_t note,
                                    uint16_t velocity, uint8_t attr_type,
                                    uint16_t attr_data, ump_packet_t *packet)
{
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |    // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_NOTE_OFF << 16) |        // Status: Note Off
                       ((channel & 0x0F) << 16) |              // Channel
                       ((note & 0x7F) << 8) |                  // Note number
                       (attr_type & 0xFF);                     // Attribute type
    
    packet->words[1] = ((uint32_t)velocity << 16) |     // Velocity (16-bit)
                       (attr_data & 0xFFFF);             // Attribute data
    
    return ESP_OK;
}

esp_err_t ump_build_midi2_poly_pressure(uint8_t group, uint8_t channel,
                                         uint8_t note, uint32_t pressure,
                                         ump_packet_t *packet)
{
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |    // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_POLY_PRESSURE << 16) |   // Status: Poly Pressure
                       ((channel & 0x0F) << 16) |              // Channel
                       ((note & 0x7F) << 8);                   // Note number
                       // Byte 4 is reserved (0x00)
    
    packet->words[1] = pressure;                        // 32-bit pressure value
    
    return ESP_OK;
}

esp_err_t ump_build_midi2_program_change(uint8_t group, uint8_t channel,
                                          uint8_t program, bool bank_valid,
                                          uint8_t bank_msb, uint8_t bank_lsb,
                                          ump_packet_t *packet)
{
    uint8_t option_flags = bank_valid ? 0x01 : 0x00;
    
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |    // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_PROGRAM_CHANGE << 16) |  // Status: Program Change
                       ((channel & 0x0F) << 16) |              // Channel
                       (option_flags << 8) |                   // Option flags (bit 0 = bank valid)
                       (program & 0x7F);                       // Program number
    
    packet->words[1] = ((bank_msb & 0x7F) << 8) |      // Bank MSB
                       (bank_lsb & 0x7F);                // Bank LSB
                       // Upper bytes reserved (0x00)
    
    return ESP_OK;
}

esp_err_t ump_build_midi2_channel_pressure(uint8_t group, uint8_t channel,
                                            uint32_t pressure,
                                            ump_packet_t *packet)
{
    packet->words[0] = (UMP_MT_MIDI2_CHANNEL_VOICE << 28) |    // MT = 0x4
                       ((group & 0x0F) << 24) |                // Group
                       (MIDI1_STATUS_CHANNEL_PRESSURE << 16) |   // Status: Channel Pressure
                       ((channel & 0x0F) << 16);              // Channel
                       // Bytes 3-4 reserved (0x00)
    
    packet->words[1] = pressure;                        // 32-bit pressure value
    
    return ESP_OK;
}

esp_err_t midi_translate_system_messages_1to2(const midi1_message_t *msg, 
                                                ump_packet_t *packet)
{
    if (!msg || !packet) {
        return ESP_ERR_INVALID_ARG;
    }
    
    switch (msg->status_byte) {
        // System Common Messages (0xF1 - 0xF7)
        
        case MIDI1_STATUS_MTC_QUARTER_FRAME: // MIDI Time Code
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_MTC_QUARTER_FRAME << 16) | 
                               ((msg->data[0] & 0x7F) << 8);  // Time code value
            break;
            
        case MIDI1_STATUS_SONG_POSITION: // Song Position Pointer
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_SONG_POSITION << 16) |
                               ((msg->data[0] & 0x7F) << 8) |  // LSB
                               (msg->data[1] & 0x7F);          // MSB
            break;
            
        case MIDI1_STATUS_SONG_SELECT: // Song Select
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_SONG_SELECT << 16) |
                               ((msg->data[0] & 0x7F) << 8);
            break;
            
        case MIDI1_STATUS_TUNE_REQUEST: // Tune Request
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_TUNE_REQUEST << 16);
            break;
            
        // System Real-Time Messages (0xF8 - 0xFF)
        
        case MIDI1_STATUS_TIMING_CLOCK: // Timing Clock
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_TIMING_CLOCK << 16);
            break;
            
        case MIDI1_STATUS_START: // Start
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_START << 16);
            break;
            
        case MIDI1_STATUS_CONTINUE: // Continue
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_CONTINUE << 16);
            break;
            
        case MIDI1_STATUS_STOP: // Stop
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_STOP << 16);
            break;
            
        case MIDI1_STATUS_ACTIVE_SENSING: // Active Sensing
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_ACTIVE_SENSING << 16); 
            break;
            
        case MIDI1_STATUS_SYSTEM_RESET: // System Reset
            packet->words[0] = (UMP_MT_SYSTEM << 28) |
                               (0 << 24) |
                               (MIDI1_STATUS_SYSTEM_RESET << 16);
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ESP_OK;
}

esp_err_t midi_translate_1to2(const midi1_message_t *msg, ump_packet_t *packet) 
{
    if (!msg || !packet) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t status_type = MIDI1_MSG_GET_STATUS(msg);
    uint8_t channel = MIDI1_MSG_GET_CHANNEL(msg);
    
    if (msg->status_byte >= 0xF0) {
        return midi_translate_system_messages_1to2(msg, packet);
    }

    switch (status_type) {
        
        // Note Off (0x80)
        case MIDI1_STATUS_NOTE_OFF: {
            uint8_t note = msg->data[0] & 0x7F;
            uint16_t velocity = midi_upscale_7to16(msg->data[1] & 0x7F);
            return ump_build_midi2_note_off(0, channel, note, velocity, 0, 0, packet);
        }
        
        // Note On (0x90)
        case MIDI1_STATUS_NOTE_ON: {
            uint8_t note = msg->data[0] & 0x7F;
            uint8_t vel_7bit = msg->data[1] & 0x7F;
            
            // Special case: Note On with velocity 0 = Note Off
            if (vel_7bit == 0) {
                return ump_build_midi2_note_off(0, channel, note, 0x8000, 0, 0, packet);
            }
            
            // Upscale velocity: 0x01 → 0x0200, 0x7F → 0xFFFF
            uint16_t velocity = midi_upscale_7to16(vel_7bit);
            
            // Attribute Type = 0, Attribute Data = 0 (unless Profile specifies otherwise)
            return ump_build_midi2_note_on(0, channel, note, velocity, 0, 0, packet);
        }
        
        // Polyphonic Aftertouch / Poly Pressure (0xA0)
        case MIDI1_STATUS_POLY_PRESSURE: {
            uint8_t note = msg->data[0] & 0x7F;
            uint32_t pressure = midi_upscale_7to32(msg->data[1] & 0x7F);
            return ump_build_midi2_poly_pressure(0, channel, note, pressure, packet);
        }
        
        // Control Change (0xB0)
        case MIDI1_STATUS_CONTROL_CHANGE: {
            uint8_t controller = msg->data[0] & 0x7F;
            uint32_t value = midi_upscale_7to32(msg->data[1] & 0x7F);
            
            // Skip controllers used for RPN/NRPN compound messages in MIDI 1.0
            // These are handled separately (CC 6, 38, 98, 99, 100, 101)
            if (controller == 6 || controller == 38 || 
                controller == 98 || controller == 99 || 
                controller == 100 || controller == 101) {
                // These should be handled by RPN/NRPN state machine
                // Return NOT_SUPPORTED to indicate special handling needed
                return ESP_ERR_NOT_SUPPORTED;
            }
            
            // Skip Bank Select (handled in Program Change)
            if (controller == 0 || controller == 32) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            
            return ump_build_midi2_control_change(0, channel, controller, value, packet);
        }
        
        // Program Change (0xC0)
        case MIDI1_STATUS_PROGRAM_CHANGE: {
            uint8_t program = msg->data[0] & 0x7F;
            
            // Note: Bank Select should be tracked separately and included here
            // For simple translation without bank tracking:
            // Bank Valid = 0, Bank MSB = 0, Bank LSB = 0
            return ump_build_midi2_program_change(0, channel, program, 
                                                   false, 0, 0, packet);
        }
        
        // Channel Pressure / Aftertouch (0xD0)
        case MIDI1_STATUS_CHANNEL_PRESSURE: {
            uint32_t pressure = midi_upscale_7to32(msg->data[0] & 0x7F);
            return ump_build_midi2_channel_pressure(0, channel, pressure, packet);
        }
        
        // Pitch Bend (0xE0)
        case MIDI1_STATUS_PITCH_BEND: {
            // MIDI 1.0: LSB then MSB (little-endian)
            uint8_t lsb = msg->data[0] & 0x7F;
            uint8_t msb = msg->data[1] & 0x7F;
            uint16_t value_14bit = (msb << 7) | lsb;
            
            // Upscale 14-bit to 32-bit
            uint32_t pitch_bend = midi_upscale_14to32(value_14bit);
            
            return ump_build_midi2_pitch_bend(0, channel, pitch_bend, packet);
        }
        
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t midi_translate_2to1(const ump_packet_t *packet, midi1_message_t *msg)
{
    if (!packet || !msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mt = (packet->words[0] >> 28) & 0x0F;

    // Handle MIDI 1.0 Channel Voice Messages (MT 0x2)
    if (mt == UMP_MT_MIDI1_CHANNEL_VOICE) {
        uint32_t word0 = packet->words[0];
        
        uint8_t status_type = (word0 >> 16) & 0xF0;  // Upper 4 bits
        uint8_t channel = (word0 >> 16) & 0x0F; // Lower 4 bits
        uint8_t data1 = (word0 >> 8) & 0x7F;    // Bit 7 is reserved
        uint8_t data2 = word0 & 0x7F;           // Bit 7 is reserved
        
        msg->status_byte = status_type | channel;
        msg->data[0] = data1;
        msg->data[1] = data2;
        
        return ESP_OK;
    }
    
    // Handle MIDI 2.0 Channel Voice Messages (MT 0x4)
    if (mt == UMP_MT_MIDI2_CHANNEL_VOICE) {
        uint32_t word0 = packet->words[0];
        uint32_t word1 = packet->words[1];
        
        uint8_t status_type = (word0 >> 16) & 0xF0;
        uint8_t channel = (word0 >> 16) & 0x0F;
        
        switch (status_type) {
            case MIDI2_STATUS_NOTE_OFF: {
                uint8_t note = (word0 >> 8) & 0x7F;
                uint16_t velocity16 = (word1 >> 16) & 0xFFFF;
                uint8_t velocity7 = midi_downscale_16to7(velocity16);
                
                msg->status_byte = 0x80 | channel;
                msg->data[0] = note;
                msg->data[1] = velocity7;
                return ESP_OK;
            }
            case MIDI2_STATUS_NOTE_ON: {
                uint8_t note = (word0 >> 8) & 0x7F;
                uint16_t velocity16 = (word1 >> 16) & 0xFFFF;
                uint8_t velocity7 = midi_downscale_16to7(velocity16);
                
                // Special case: if downscaled velocity is 0, use 1 instead
                if (velocity7 == 0) {
                    velocity7 = 1;
                }
                
                msg->status_byte = 0x90 | channel;
                msg->data[0] = note;
                msg->data[1] = velocity7;
                return ESP_OK;
            }
            case MIDI2_STATUS_POLY_PRESSURE: {
                uint8_t note = (word0 >> 8) & 0x7F;
                uint32_t pressure32 = word1;
                uint8_t pressure7 = midi_downscale_32to7(pressure32);
                
                msg->status_byte = 0xA0 | channel;
                msg->data[0] = note;
                msg->data[1] = pressure7;
                return ESP_OK;
            }
            case MIDI2_STATUS_CONTROL_CHANGE: {
                uint8_t controller = (word0 >> 8) & 0x7F;
                uint32_t value32 = word1;
                uint8_t value7 = midi_downscale_32to7(value32);
                
                msg->status_byte = 0xB0 | channel;
                msg->data[0] = controller;
                msg->data[1] = value7;
                return ESP_OK;
            }
            case MIDI2_STATUS_RPN: {
                // Translate to MIDI 1.0 RPN sequence (3 messages)
                // This requires multiple MIDI 1.0 messages
                // Return NOT_SUPPORTED to indicate special handling needed
                return ESP_ERR_NOT_SUPPORTED;
            }
            case MIDI2_STATUS_NRPN: {
                // Translate to MIDI 1.0 NRPN sequence (3 messages)
                // This requires multiple MIDI 1.0 messages
                // Return NOT_SUPPORTED to indicate special handling needed
                return ESP_ERR_NOT_SUPPORTED;
            }
            case MIDI2_STATUS_PROGRAM_CHANGE: {
                uint8_t program = word0 & 0x7F;
                bool bank_valid = (word0 >> 8) & 0x01;
                
                if (!bank_valid) {
                    // Simple program change without bank
                    msg->status_byte = 0xC0 | channel;
                    msg->data[0] = program;
                    return ESP_OK;
                } else {
                    // Bank select included - requires multiple messages
                    return ESP_ERR_NOT_SUPPORTED;
                }
            }
            case MIDI2_STATUS_CHANNEL_PRESSURE: {
                uint32_t pressure32 = word1;
                uint8_t pressure7 = midi_downscale_32to7(pressure32);
                
                msg->status_byte = 0xD0 | channel;
                msg->data[0] = pressure7;
                return ESP_OK;
            }
            case MIDI2_STATUS_PITCH_BEND: {
                uint32_t pitch_bend32 = word1;
                uint16_t pitch_bend14 = midi_downscale_32to14(pitch_bend32);
                
                // MIDI 1.0 Pitch Bend is LSB first (little-endian)
                uint8_t lsb = pitch_bend14 & 0x7F;
                uint8_t msb = (pitch_bend14 >> 7) & 0x7F;
                
                msg->status_byte = 0xE0 | channel;
                msg->data[0] = lsb;
                msg->data[1] = msb;
                return ESP_OK;
            }
            
            // Per-Note Controllers and Management (cannot translate)
            case 0x00: // Registered Per-Note Controller
            case 0x10: // Assignable Per-Note Controller
            case 0x40: // Relative Registered Controller
            case 0x50: // Relative Assignable Controller
            case 0x60: // Per-Note Pitch Bend
            case 0xF0: // Per-Note Management
                return ESP_ERR_NOT_SUPPORTED;
            
            default:
                return ESP_ERR_NOT_SUPPORTED;
        }
    }
    
    // Handle System Messages (MT 0x1)
    else if (mt == UMP_MT_SYSTEM) {
        uint32_t word0 = packet->words[0];
        uint8_t status = (word0 >> 16) & 0xFF;
        
        msg->status_byte = status;
        
        switch (status) {
            case MIDI1_STATUS_MTC_QUARTER_FRAME: // MIDI Time Code
                msg->data[0] = (word0 >> 8) & 0x7F;
                return ESP_OK;
                
            case MIDI1_STATUS_SONG_POSITION: // Song Position Pointer
                msg->data[0] = (word0 >> 8) & 0x7F;  // LSB
                msg->data[1] = word0 & 0x7F;          // MSB
                return ESP_OK;
                
            case MIDI1_STATUS_SONG_SELECT: // Song Select
                msg->data[0] = (word0 >> 8) & 0x7F;
                return ESP_OK;
                
            case MIDI1_STATUS_TUNE_REQUEST: // Tune Request
            case MIDI1_STATUS_TIMING_CLOCK: // Timing Clock
            case MIDI1_STATUS_START: // Start
            case MIDI1_STATUS_CONTINUE: // Continue
            case MIDI1_STATUS_STOP: // Stop
            case MIDI1_STATUS_ACTIVE_SENSING: // Active Sensing
            case MIDI1_STATUS_SYSTEM_RESET: // System Reset
                // No data bytes
                return ESP_OK;
                
            default:
                return ESP_ERR_NOT_SUPPORTED;
        }
    }
    
    return ESP_ERR_NOT_SUPPORTED;
}

