
import sys
import asyncio
import logging
log = logging.getLogger(__name__)

from bleak import BleakClient

ADDRESS = "F7:6D:E3:5F:CB:F9"
ROBOT_CHARACTERISTIC = "e9ea0003-e19b-482d-9293-c7907585fc48"  

# NOTE: SUCCESSFULLY CHANGES MOTOR VALUES 
async def main(address):
    async with BleakClient(address) as client:
        print(f"Connected: {client.is_connected}")

        paired = await client.pair(protection_level=2)
        print(f"Paired: {paired}")

        for service in client.services:
            print(f"[Service] {service}")
            for characteristic in service.characteristics:
                print(f"[Characteristic] {characteristic}")

        print("Writing something...")
        await client.write_gatt_char(ROBOT_CHARACTERISTIC, b"1")
        await asyncio.sleep(2.0)

        print("Writing something else...")
        await client.write_gatt_char(ROBOT_CHARACTERISTIC, b"2")
        await asyncio.sleep(2.0)

        unpaired = await client.unpair()
        print(f"Unpaired: {unpaired}")

async def other():
    print("EEEEEE")
    await asyncio.sleep(1.5)
    print("OOOOO")
    

if __name__ == "__main__":
    asyncio.run(main(ADDRESS))
    asyncio.run(other())
