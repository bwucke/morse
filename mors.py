# 80% of credit goes to Phind, the AI assistant,
# therefore License = Public Domain.
# Produces an ascii->morse code table in C syntax.

MORSE_CODE_DICT = { 'A':'.-', 'B':'-...',
                    'C':'-.-.', 'D':'-..', 'E':'.',
                    'F':'..-.', 'G':'--.', 'H':'....',
                    'I':'..', 'J':'.---', 'K':'-.-',
                    'L':'.-..', 'M':'--', 'N':'-.',
                    'O':'---', 'P':'.--.', 'Q':'--.-',
                    'R':'.-.', 'S':'...', 'T':'-',
                    'U':'..-', 'V':'...-', 'W':'.--',
                    'X':'-..-', 'Y':'-.--', 'Z':'--..',
                    '1':'.----', '2':'..---', '3':'...--',
                    '4':'....-', '5':'.....', '6':'-....',
                    '7':'--...', '8':'---..', '9':'----.',
                    '0':'-----', ',':'--..--', '.':'.-.-.-',
                    '?':'..--..', '/':'-..-.', '-':'-....-',
                    '(':'-.--.', ')':'-.--.-', ';':'-.-.-.',
                    '&':'.-...', '\'':'.----.', '!':'-.-.--',
                    ':':'---...','=':'-...-', '+':'.-.-.',
                    '_':'..--.-','"':'.-..-.', '$':'...-..-',
                    '@':'.--.-.','\n':'.-.-', '\b':'......',
                    ' ':'/' 
                    }


# Dictionary mapping ASCII values to human-readable names for non-printable characters
NON_PRINTABLE_CHARS = {
    0: 'NUL',  1: 'SOH',  2: 'STX',  3: 'ETX',  4: 'EOT',  5: 'ENQ',  6: 'ACK',  7: 'BEL',
    8: 'BS',  9: 'HT',  10: 'LF',  11: 'VT',  12: 'FF',  13: 'CR',  14: 'SO',  15: 'SI',
    16: 'DLE',  17: 'DC1',  18: 'DC2',  19: 'DC3',  20: 'DC4',  21: 'NAK',  22: 'SYN',  23: 'ETB',
    24: 'CAN',  25: 'EM',  26: 'SUB',  27: 'ESC',  28: 'FS',  29: 'GS',  30: 'RS',  31: 'US', 127:'DEL'
}

# Function to convert ASCII value to char or its name for non-printable characters
def ascii_to_char(ascii_val, printable):
    if ascii_val in NON_PRINTABLE_CHARS.keys():
        if not printable and (chr(ascii_val) in MORSE_CODE_DICT):
            return chr(ascii_val)
        return NON_PRINTABLE_CHARS.get(ascii_val, f"UNKNOWN_{ascii_val}")
    elif  32 <= ascii_val <  128:
        return chr(ascii_val)
    else:
        return ''

# Print the C array
print("const char* morse_code[] = {")
for ascii_val in range(128):  # Iterate over ASCII values  0-127
    ascii_char = ascii_to_char(ascii_val, False).upper()
    morse_val = '"' + MORSE_CODE_DICT.get(ascii_char, "")+'",'  # Get Morse code or default to ""
    ascii_char = ascii_to_char(ascii_val, True)
    print(f"    {morse_val: <9}/* {ascii_char} */")  # Format the C array entry
print("};")

