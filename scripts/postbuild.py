import binascii
from os.path import exists

INPUT = "build/GarageDoorBLE.bin"
OUTPUT = "release/GarageDoorBLE.bin"
MAGIC = 38269420

def CRC32_from_file(filename):
    buf = open(filename,'rb').read()
    buf = (binascii.crc32(buf) & 0xFFFFFFFF)
    buf = buf % MAGIC
    return buf.to_bytes(4,'little')

def main():
    # Check exists.
    if(not exists(INPUT)):
        print("Could not find file.")
        return
    # Get the crc 
    crc = CRC32_from_file(INPUT)
    print(crc)
    #Write file
    with open(OUTPUT, 'wb') as outfile:
        with open(INPUT, 'rb') as infile:
            outfile.truncate()
            outfile.write(infile.read())
            outfile.write(crc)

if __name__ == "__main__":
    main()