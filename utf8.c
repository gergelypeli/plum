
int decode_utf8_raw(const char *bytes, int byte_length, unsigned short *characters) {
    const char *bytes_end = bytes + byte_length;
    const unsigned short *characters_start = characters;
    
    while (bytes < bytes_end) {
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
    
    return characters - characters_start;
}


int encode_utf8_raw(const unsigned short *characters, int character_length, char *bytes) {
    const unsigned short *characters_end = characters + character_length;
    const char *bytes_start = bytes;
    
    while (characters < characters_end) {
        if (characters[0] < 0x0080) {
            bytes[0] = characters[0];
            characters += 1;
            bytes += 1;
        }
        else if (characters[0] < 0x0800) {
            bytes[0] = 0xC0 | ((characters[0] & 0x07C0) >> 6);
            bytes[1] = 0x80 | (characters[0] & 0x003F);
            characters += 1;
            bytes += 2;
        }
        else {
            bytes[0] = 0xE0 | ((characters[0] & 0xF000) >> 12);
            bytes[1] = 0x80 | ((characters[0] & 0x0FC0) >> 6);
            bytes[2] = 0x80 | (characters[0] & 0x003F);
            characters += 1;
            bytes += 3;
        }
    }
    
    return bytes - bytes_start;
}
