#include "ump_types.h"
#include "ump_defs.h"
#include "esp_err.h"

esp_err_t ump_parser_parse_packet(const uint32_t *words, ump_packet_t *packet);