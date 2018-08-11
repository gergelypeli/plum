
void decode_utf8_buffer(const char *bytes, int64 byte_length, unsigned16 *characters, int64 character_length, int64 *byte_count, int64 *character_count) {
    const char *bytes_start = bytes;
    const char *bytes_end = bytes + byte_length;
    
    const unsigned16 *characters_start = characters;
    const unsigned16 *characters_end = characters + character_length;
    
    while (bytes < bytes_end && characters < characters_end) {
        if ((bytes[0] & 0x80) == 0x00) {
            characters[0] = bytes[0];
            bytes += 1;
            characters += 1;
        }
        else if ((bytes[0] & 0xE0) == 0xC0) {
            if (bytes + 1 >= bytes_end)
                break;
                
            if ((bytes[1] & 0xC0) != 0x80)
                break;
                
            characters[0] = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
            
            if (characters[0] < 0x80)
                break;
            
            bytes += 2;
            characters += 1;
        }
        else if ((bytes[0] & 0xF0) == 0xE0) {
            if (bytes + 2 >= bytes_end)
                break;
                
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80)
                break;
                
            characters[0] = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
            
            if (characters[0] < 0x800)
                break;
            
            bytes += 3;
            characters += 1;
        }
        else if ((bytes[0] & 0xF8) == 0xF0) {
            if (bytes + 3 >= bytes_end)
                break;
                
            bytes += 4;
        }
        else
            break;
    }
    
    *byte_count = bytes - bytes_start;
    *character_count = characters - characters_start;
}


void encode_utf8_buffer(const unsigned16 *characters, int64 character_length, char *bytes, int64 byte_length, int64 *character_count, int64 *byte_count) {
    const unsigned16 *characters_start = characters;
    const unsigned16 *characters_end = characters + character_length;

    const char *bytes_start = bytes;
    const char *bytes_end = bytes + byte_length;
    
    while (characters < characters_end) {
        if (characters[0] < 0x0080) {
            if (bytes >= bytes_end)
                break;
                
            bytes[0] = characters[0];
            characters += 1;
            bytes += 1;
        }
        else if (characters[0] < 0x0800) {
            if (bytes + 1 >= bytes_end)
                break;

            bytes[0] = 0xC0 | ((characters[0] & 0x07C0) >> 6);
            bytes[1] = 0x80 | (characters[0] & 0x003F);
            characters += 1;
            bytes += 2;
        }
        else {
            if (bytes + 2 >= bytes_end)
                break;

            bytes[0] = 0xE0 | ((characters[0] & 0xF000) >> 12);
            bytes[1] = 0x80 | ((characters[0] & 0x0FC0) >> 6);
            bytes[2] = 0x80 | (characters[0] & 0x003F);
            characters += 1;
            bytes += 3;
        }
    }

    *character_count = characters - characters_start;
    *byte_count = bytes - bytes_start;
}
