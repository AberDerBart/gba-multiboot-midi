import serial
import argparse
import struct


def upload(ser, filename):

    f = open(filename, 'rb')

    data = f.read()

    f.close()

    ser.write(b"\nupload\n")

    ser.write(struct.pack("<L", len(data)))
    ser.write(data)


def boot(ser):
    ser.write(b"\nboot\n")


def main():
    parser = argparse.ArgumentParser(
        prog="Upload",
        description="Uploads a GBA ROM to the Pico",
    )

    parser.add_argument('-p', '--port', required=True)

    subParsers = parser.add_subparsers(required=True, dest="cmd")

    uploadParser = subParsers.add_parser("upload")
    uploadParser.add_argument("filename")

    subParsers.add_parser("boot")

    args = parser.parse_args()

    ser = serial.Serial(args.port, 115200, timeout=1)

    if args.cmd == "upload":
        upload(ser, args.filename)
    if args.cmd == "boot":
        boot(ser)

    ser.flush()
    ser.close()


if __name__ == '__main__':
    main()
