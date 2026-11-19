"""Передача нот и переход в BLHeli через адаптер StableKite RP2040."""

import argparse
import json
from pathlib import Path
import struct
import sys
import time


def encode(command, frequency=0, duration=0):
    packet = struct.pack(">BBHB", 0xA6, command, frequency, duration // 10)
    crc = 0
    for value in packet:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 0x80 else 0)) & 255
    return packet + bytes((crc,))


def parse_note(text):
    frequency, duration, volume = map(int, text.split("/"))
    if (
        not (220 <= frequency <= 4000 or frequency == volume == 0)
        or not 10 <= duration <= 250
        or duration % 10
        or not 0 <= volume <= 5
    ):
        raise argparse.ArgumentTypeError("Нужны частота/мс/громкость, см. doc/dshot-melody.md")
    return frequency, duration, volume


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rp2040_tools", type=Path)
    parser.add_argument("notes", nargs="*", type=parse_note, metavar="ГЦ/МС/ГРОМКОСТЬ")
    parser.add_argument("--probe-blheli", action="store_true")
    args = parser.parse_args()
    if not (args.probe_blheli or args.notes) or sum(note[1] + 20 for note in args.notes) > 7500:
        parser.error("Укажите ноты общей длительностью до 7,5 с либо --probe-blheli")

    sys.path.insert(0, str(args.rp2040_tools.resolve(strict=True)))
    from adapter import Client, VendorTransport, encode_config, frame
    from blheli import Session
    from dshot import checksum, configuration

    def send_packet(transport, packet):
        data = bytearray()
        for byte in packet:
            for bit in range(7, -1, -1):
                value = ((30 + ((byte >> bit) & 1)) << 1) | 1
                data.extend(struct.pack(">BH", 0, (value << 4) | checksum(value)))
        transport.write(frame(1, data))

    profile_path = args.rp2040_tools.parent / "examples/kiss-winusb-dshot.json"
    profile = encode_config(json.loads(profile_path.read_text(encoding="utf-8")))
    def enter_melody(client, vendor):
        if vendor.status_read()[:4] != bytes((2, 5, 2, 1)) or vendor.config_read() != profile:
            raise IOError("На RP2040 должен быть активен профиль kiss-winusb-dshot")
        client.enter_init()
        if client.command(3, profile) != b"\0":
            raise IOError("Не удалось заново запустить DShot")
        time.sleep(0.15)
        vendor.write(frame(1, configuration(0, 600000, False)))
        stop = frame(1, b"\0\0\0")
        deadline = time.monotonic() + 2.6
        while time.monotonic() < deadline:
            vendor.write(stop)
            time.sleep(0.004)
        send_packet(vendor, encode(1))
        time.sleep(0.025)

    vendor = VendorTransport(identity="parallel", classes=3)
    try:
        client = Client(vendor)
        enter_melody(client, vendor)
        if args.probe_blheli:
            send_packet(vendor, encode(3))
            time.sleep(0.025)
            with Session(client, 1, 0, "arm") as bootloader:
                time.sleep(0.4)
                print(bootloader.connect())
            if args.notes:
                enter_melody(client, vendor)
        if args.notes:
            try:
                for frequency, duration, volume in args.notes:
                    send_packet(vendor, encode(0x10 | volume, frequency, duration))
                    time.sleep(duration / 1000 + 0.02)
            finally:
                send_packet(vendor, encode(2))
    finally:
        vendor.close()


if __name__ == "__main__":
    main()
