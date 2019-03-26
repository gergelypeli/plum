
bool decode_utf8_buffer(const char *bytes, int64 byte_length, unsigned16 *characters, int64 character_length, int64 *byte_count, int64 *character_count) {
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
                return false;
                
            characters[0] = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
            
            if (characters[0] < 0x80)
                return false;
            
            bytes += 2;
            characters += 1;
        }
        else if ((bytes[0] & 0xF0) == 0xE0) {
            if (bytes + 2 >= bytes_end)
                break;
                
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80)
                return false;
                
            characters[0] = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
            
            if (characters[0] < 0x800)
                return false;
            
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
    return true;
}


bool encode_utf8_buffer(const unsigned16 *characters, int64 character_length, char *bytes, int64 byte_length, int64 *character_count, int64 *byte_count) {
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
    return true;
}


bool parse_float(const unsigned16 *characters, int64 character_length, double *result, int64 *character_count) {
    double value = 0;
    int exponent = 0;
    bool seen_dot = false;
    unsigned i;
    
    for (i = 0; i < character_length; i++) {
        unsigned16 c = characters[i];
        
        if (c == '_')
            continue;
        else if (c == '.') {
            if (seen_dot) {
                return false;
            }
            
            seen_dot = true;
            continue;
        }
        else if (c >= '0' && c <= '9') {
            value = 10 * value + (c - '0');
            
            if (seen_dot)
                exponent -= 1;
                
            continue;
        }
        else if (c == 'e' || c == 'E') {
            i += 1;
            
            if (i == character_length) {
                return false;
            }
            
            bool is_negative;
            
            if (characters[i] == '-') {
                is_negative = true;
                i += 1;
            }
            else if (characters[i] == '+') {
                is_negative = false;
                i += 1;
            }
            else {
                is_negative = false;
            }
            
            if (i == character_length) {
                return false;
            }
            
            unsigned e = 0;
            
            while (i < character_length) {
                c = characters[i];
                
                if (c >= '0' && c <= '9')
                    e = e * 10 + c - '0';
                else
                    break;
                
                if (e > 308) {
                    return false;
                }
                
                i += 1;
            }
            
            if (is_negative)
                exponent -= e;
            else
                exponent += e;
        }
        else {
            break;
        }
    }
    
    *character_count = i;
    *result = value * pow(10.0, exponent);
    return true;
}


bool parse_unteger(const unsigned16 *characters, int64 character_length, unsigned64 *result, int64 *character_count) {
    unsigned base = 10;
    unsigned start = 0;
    
    if (character_length >= 2 && characters[0] == '0') {
        switch (characters[1]) {
        case 'x':
        case 'X':
            base = 16;
            break;
        case 'o':
        case 'O':
            base = 8;
            break;
        case 'b':
        case 'B':
            base = 2;
            break;
        default:
            return false;
        }
        
        start = 2;
    }
    
    const unsigned64 limit_value = (0UL - 1) / base;
    const unsigned64 limit_digit = (0UL - 1) % base;
    unsigned64 value = 0;
    unsigned i;
    
    for (i = start; i < character_length; i++) {
        unsigned16 c = characters[i];
        
        if (c == '_')
            continue;

        unsigned64 digit = (
            c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 :
            c >= 'A' && c <= 'F' ? c - 'A' + 10 :
            16
        );
        
        if (digit >= base) {
            break;
        }
        
        if (value > limit_value || (value == limit_value && digit > limit_digit)) {
            return false;
        }
            
        value = value * base + digit;
    }
    
    *character_count = i;
    *result = value;
    return true;
}
