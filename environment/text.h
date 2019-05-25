
bool decode_utf8_buffer(const char *bytes, int64 byte_length, unsigned16 *characters, int64 character_length, int64 *byte_count, int64 *character_count);
bool encode_utf8_buffer(const unsigned16 *characters, int64 character_length, char *bytes, int64 byte_length, int64 *character_count, int64 *byte_count);
bool parse_float(const unsigned16 *characters, int64 character_length, double *result, int64 *character_count);
bool parse_unteger(const unsigned16 *characters, int64 character_length, unsigned64 *result, int64 *character_count);
