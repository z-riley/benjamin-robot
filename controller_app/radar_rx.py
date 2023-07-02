"""
Script for testing reception of radar data
"""

import enum
from time import sleep
import simplepyble

MAC_ADDRESS = "f7:6d:e3:5f:cb:f9"
MOVEMENT_SERVICE = "e9ea0001-e19b-482d-9293-c7907585fc48"
MOVEMENT_CHARACTERISTIC = "e9ea0003-e19b-482d-9293-c7907585fc48"  

RADAR_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RADAR_CHARACTERISTIC = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLE_SCAN_TIMEOUT_MS = 3000

class BLEStatus(enum.Enum):
    """ BLE connection status """
    e_unknown = 0
    e_connecting = 1
    e_connected = 2
    e_disconnecting = 3
    e_disconnected = 4

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

class BLETransceiver():
    """ Mmodule for BLE related functionality """
    def __init__(self):
        self.status = BLEStatus.e_disconnected
        self.update_connection_status(BLEStatus.e_disconnected)

    def on_button_press(self):
        """ Handle pressing of Connect button """
        if self.status is BLEStatus.e_disconnected:
            self.connect()
        elif self.status is BLEStatus.e_connected:
            self.update_connection_status(BLEStatus.e_disconnecting)
            self.disconnect()   

    def update_connection_status(self, new_status):
        """ Update BLE status label """
        self.status = new_status

    def connect(self):
        """ Attempt to connect to Benjamin """
        self.adapter = simplepyble.Adapter.get_adapters()[0]

        def scan_found_cb(self, peripheral):
            self.peripheral = peripheral                
            if self.peripheral.address() == MAC_ADDRESS:
                print("Found Benjamin")
                self.update_connection_status(BLEStatus.e_connecting)
                self.adapter.scan_stop()
                self.peripheral.connect()
                self.update_connection_status(BLEStatus.e_connected)
                print("Successfully connected, listing services...")
                services = peripheral.services()
                service_characteristic_pairs = []
                for service in services:
                    for characteristic in service.characteristics():
                        service_characteristic_pairs.append((service.uuid(), characteristic.uuid()))
                print(service_characteristic_pairs)
        self.adapter.set_callback_on_scan_found(lambda peripheral: scan_found_cb(self, peripheral))
        print("Starting scan")
        self.adapter.scan_for(BLE_SCAN_TIMEOUT_MS)
        if self.status == BLEStatus.e_disconnected:
            print("Connection failed")

    def disconnect(self):
        self.peripheral.disconnect()
        self.update_connection_status(BLEStatus.e_disconnected)

    def transmit(self, transmit_int):
        if self.status == BLEStatus.e_connected:
            self.peripheral.write_command(MOVEMENT_SERVICE, MOVEMENT_CHARACTERISTIC, str.encode(str(transmit_int)))

    
    
    def read(self):
        contents = self.peripheral.read(RADAR_SERVICE, RADAR_CHARACTERISTIC)
        return contents


def print_notif(data):
    # print(f"--Notification: {data}")
    print(f"{bcolors.OKCYAN}Notification: {data}{bcolors.ENDC}")


if __name__ == "__main__":

    ble = BLETransceiver()
    ble.connect()
    sleep(2)
    count = 0
    while True:
        #sleep(2)
        #ble.transmit_motors(1)
        contents = ble.peripheral.notify(RADAR_SERVICE, RADAR_CHARACTERISTIC, lambda data: print_notif(data))
        #contents = ble.read
        #print(f"readout[{count}] -- {contents}")
        count += 1

  