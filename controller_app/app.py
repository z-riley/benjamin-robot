"""
BLE Controller Application for Benjamin the Robot
"""
import sys
import enum
import simplepyble
import numpy as np
from PyQt6.QtCore import Qt, QRectF, QTimer
from PyQt6.QtWidgets import QApplication, QWidget, QCheckBox, QVBoxLayout, QHBoxLayout, QGridLayout, QPushButton, QLabel
from PyQt6.QtGui import QPainter


WINDOW_WIDTH = 400
WINDOW_HEIGHT = 400

MAC_ADDRESS = "f7:6d:e3:5f:cb:f9"
BLE_SCAN_TIMEOUT_MS = 3000

MOVEMENT_SERVICE = "e9ea0001-e19b-482d-9293-c7907585fc48"
MOVEMENT_CHARACTERISTIC = "e9ea0003-e19b-482d-9293-c7907585fc48"
TRANSMIT_MOVE_COMMAND_PERIOD_MS = 80        # Transmits move command 

RADAR_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RADAR_CHARACTERISTIC = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
POLL_RADAR_NOTIFICATION_PERIOD_MS = 10      # Retreives radar notification data


DIRECTION_CONTROLS = {"forwards": Qt.Key.Key_W,
                      "left": Qt.Key.Key_A,
                      "backwards": Qt.Key.Key_S,
                      "right": Qt.Key.Key_D}


class RobotDir(enum.Enum):
    """ Directional commands to send to robot """
    e_none = 0
    e_north = 1
    e_northeast = 2
    e_east = 3
    e_southeast = 4
    e_south = 5
    e_southwest = 6
    e_west = 7
    e_northwest = 8


class BLEStatus(enum.Enum):
    """ BLE connection status """
    e_unknown = 0
    e_connecting = 1
    e_connected = 2
    e_disconnecting = 3
    e_disconnected = 4


class RadarGrid(QWidget):
    """ Radar grid related components """
    def __init__(self):
        super().__init__()
        self.columns = 20
        self.rows = 20
        self.column_depth = 0
        self.row_number = 0
        self.data = np.zeros((self.rows, self.columns), dtype=bool)
        
        self.layout = QVBoxLayout()
        self.data_box = QLabel("Column: -  Distance: -  Depth: -")
        self.layout.addWidget(self.data_box, stretch=1)
        self.layout.addWidget(self, stretch=10)

    def resizeEvent(self, event):
        # Compute the square size based on the aspect ratio, assuming that the
        # column and row numbers are fixed
        reference = self.width() * self.rows / self.columns
        if reference > self.height():
            # The window is larger than the aspect ratio
            # Use the height as a reference (minus 1 pixel)
            self.squareSize = (self.height() - 1) / self.rows
        else:
            # The opposite
            self.squareSize = (self.width() - 1) / self.columns
        
    def paintEvent(self, event):
        qp = QPainter(self)
        # Translate the painter by half a pixel to ensure correct line painting
        qp.translate(0.5, 0.5)
        qp.setRenderHints(qp.RenderHint.Antialiasing)

        width = self.squareSize * self.columns
        height = self.squareSize * self.rows
        # Center the grid
        left_pos = (self.width() - width) / 2
        top_pos = (self.height() - height) / 2
        y = top_pos
        # We need to add 1 to draw the topmost right/bottom lines too
        for row in range(self.rows + 1):
            qp.drawLine(int(left_pos), int(y), int(left_pos) + int(width), int(y))
            y += self.squareSize
        x = left_pos
        for column in range(self.columns + 1):
            qp.drawLine(int(x), int(top_pos), int(x), int(top_pos) + int(height))
            x += self.squareSize

        # Create a smaller rectangle
        objectSize = self.squareSize * 0.8
        margin = self.squareSize * 0.1
        objectRect = QRectF(margin, margin, objectSize, objectSize)
        qp.setBrush(Qt.GlobalColor.darkGreen)
        for index, value in np.ndenumerate(self.data):
            row = index[0]
            col = index[1]
            if value == True:
                qp.drawRect(objectRect.translated(
                    left_pos + col * self.squareSize, top_pos + row * self.squareSize))
                
    def write_grid_row(self, col_number, distance):
        """ Convert distance to squares and write to a row on the grid """
        depth = self.scale(distance, 0, 1000, 0, self.rows)
        depth = self.rows - depth
        self.data_box.setText(f"Column: {col_number}  Distance: {distance}  Depth: {depth}")
        self.data[:depth, col_number] = 1
        self.data[depth:, col_number] = 0
        self.update()

    def scale(self, val, in_min, in_max, out_min, out_max):
        """ Map a range of values onto another, keeping within bounds """
        if val < in_min:
            val = in_min
        if val > in_max:
            val = in_max
        return round((val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)
    

class DirIndicatorBox(QCheckBox):
    """ Components for direction indicators"""
    def __init__(self, bin_code_id):
        super().__init__()
        # Create check-box
        self.checkbox = QCheckBox(self)
        self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: lightgrey;}") 
        self.bin_code = 0b0000
    
    def button_pressed(self):
        self.set_bin_code()
        self.darken_box()

    def button_released(self):
        self.reset_bin_code()
        self.lighten_box()

    def set_bin_code(self):
        self.bin_code = self.bin_code_id 

    def reset_bin_code(self):
        self.bin_code = 0b000

    def darken_box(self):
        self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: grey}")
        
    def lighten_box(self):
        self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: lightgrey}")


class DirectionFinder(QWidget):
    """ Class containing all directional movement related components """
    def __init__(self):
        super().__init__()
        
        self.forwards_indicator = DirIndicatorBox(0b1000)
        self.left_indicator = DirIndicatorBox(0b0100)
        self.backwards_indicator = DirIndicatorBox(0b0010)
        self.right_indicator = DirIndicatorBox(0b0001)

        self.forwards_indicator.bin_code_id = 0b1000
        self.left_indicator.bin_code_id = 0b0100
        self.backwards_indicator.bin_code_id = 0b0010
        self.right_indicator.bin_code_id = 0b0001

        self.dir_label = QLabel(self)
        self.dir_label.setFixedWidth(70)       

        self.layout = QGridLayout()
        self.layout.addWidget(self.forwards_indicator, 0, 1)
        self.layout.addWidget(self.left_indicator, 1, 0)
        self.layout.addWidget(self, 1, 1)
        self.layout.addWidget(self.right_indicator, 1, 2)
        self.layout.addWidget(self.backwards_indicator, 2, 1)

    def calculate_dir(self):
        """ Return a direction based on all movement-key states """
        cmd_bin_code = self.forwards_indicator.bin_code | \
                       self.left_indicator.bin_code | \
                       self.backwards_indicator.bin_code | \
                       self.right_indicator.bin_code
                
        direction_lut = {0b0000: RobotDir.e_none,
                         0b0001: RobotDir.e_east,
                         0b0010: RobotDir.e_south,
                         0b0011: RobotDir.e_southeast,
                         0b0100: RobotDir.e_west,
                         0b0101: RobotDir.e_none,
                         0b0110: RobotDir.e_southwest,
                         0b0111: RobotDir.e_south,
                         0b1000: RobotDir.e_north,
                         0b1001: RobotDir.e_northeast,
                         0b1010: RobotDir.e_none,
                         0b1011: RobotDir.e_east,
                         0b1100: RobotDir.e_northwest,
                         0b1101: RobotDir.e_north,
                         0b1110: RobotDir.e_west,
                         0b1111: RobotDir.e_none}

        self.direction_code = direction_lut[cmd_bin_code]
        self.update_dir_label()
        return self.direction_code
    
    def update_dir_label(self):
        self.dir_label.setText(self.direction_code.name)


class BLETransceiver():
    """ GUI module for BLE related functionality """
    def __init__(self):
        super().__init__()
        self.direction = RobotDir.e_none
        self.status = BLEStatus.e_disconnected
        self.radar_position = 0
        self.radar_distance = 0
        self.grid_update_ready = False

        self.layout = QHBoxLayout()
        self.ble_label = QLabel("BLE Status: ")
        self.ble_status_label = QLabel(self.status.name)
        self.checkbox = QCheckBox()
        self.ble_button = QPushButton("Connect")
        self.ble_button.clicked.connect(self.on_button_press)
        self.layout.addWidget(self.ble_label)
        self.layout.addWidget(self.ble_status_label)
        self.layout.addWidget(self.checkbox)
        self.layout.addWidget(self.ble_button)
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
        if self.status == BLEStatus.e_connecting:
            self.ble_status_label.setText("Connecting")
            self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: yellow}")
        elif self.status == BLEStatus.e_connected:
            self.ble_status_label.setText("Connected")
            self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: lightgreen}")
        elif self.status == BLEStatus.e_disconnecting:
            self.ble_status_label.setText("Disconnecting")
            self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: yellow}")
        elif self.status == BLEStatus.e_disconnected:
            self.ble_status_label.setText("Disconnected")
            self.checkbox.setStyleSheet("QCheckBox::indicator{background-color: red}")

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
                self.ble_button.setText("Disconnect")
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
        self.ble_button.setText("Connect")

    def transmit(self):
        if self.status == BLEStatus.e_connected:
            self.peripheral.write_command(MOVEMENT_SERVICE, MOVEMENT_CHARACTERISTIC, str.encode(str(self.direction.value)))

    def process_notification(self):
        if self.status == BLEStatus.e_connected:
            self.peripheral.notify(RADAR_SERVICE, RADAR_CHARACTERISTIC, lambda data: self.notification_cb(data))

    def notification_cb(self, data):
        """ Clean radar notification data and prepare for GUI update """
        # Split around '&' delimeter
        position_slice = str(data).split("&", 1)[0]
        distance_slice = str(data).split("&", 1)[1]
        # Clear up extra characters around value
        self.radar_position = int(position_slice[2:])
        self.radar_distance = int(distance_slice.split("&", 1)[0])
        print(f"-> Raw data notification: {data} Position: {self.radar_position} Distance: {self.radar_distance}")
        # Set flag to update grid
        self.grid_update_ready = True


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()

        # Window settings
        self.setWindowTitle("Benjamin BLE Remote")
        self.setFixedWidth(400)
        self.setFixedHeight(400)

        # Radar grid
        self.grid = RadarGrid()

        # User input
        self.direction_finder = DirectionFinder()

        # Connectivity
        self.transceiver = BLETransceiver()
        self.movement_tx_timer = QTimer()
        self.movement_tx_timer.start(TRANSMIT_MOVE_COMMAND_PERIOD_MS)
        self.movement_tx_timer.timeout.connect(self.transceiver.transmit)
        self.radar_rx_timer = QTimer()
        self.radar_rx_timer.start(POLL_RADAR_NOTIFICATION_PERIOD_MS)
        self.radar_rx_timer.timeout.connect(self.update_grid)

        # Layout formation
        self.windowLayout = QVBoxLayout()
        self.toplayout = QHBoxLayout()
        self.middleLayout = QHBoxLayout()
        self.bottomLayout = QHBoxLayout()

        # TODO Add battery voltage to gui eventually

        # Layout assembly
        self.toplayout.addLayout(self.grid.layout)
        self.middleLayout.addLayout(self.direction_finder.layout)
        self.windowLayout.addLayout(self.toplayout)
        self.windowLayout.addLayout(self.middleLayout)
        self.windowLayout.addLayout(self.transceiver.layout)
        self.setLayout(self.windowLayout)
        self.show()
    
    def update_grid(self):
        if self.transceiver.status == BLEStatus.e_connected:
            # Retreive data from radar notification
            self.transceiver.process_notification()
            # Write to grid
            if self.transceiver.grid_update_ready:
                print("WRITING TO GRID")
                self.grid.write_grid_row(self.transceiver.radar_position, self.transceiver.radar_distance)
                self.transceiver.grid_update_ready = False

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            sys.exit()
        elif event.key() == DIRECTION_CONTROLS["forwards"]:
            self.direction_finder.forwards_indicator.button_pressed()
        elif event.key() == DIRECTION_CONTROLS["left"]:
            self.direction_finder.left_indicator.button_pressed()
        elif event.key() == DIRECTION_CONTROLS["backwards"]:
            self.direction_finder.backwards_indicator.button_pressed()
        elif event.key() == DIRECTION_CONTROLS["right"]:
            self.direction_finder.right_indicator.button_pressed()
        self.transceiver.direction = self.direction_finder.calculate_dir()

    def keyReleaseEvent(self, event):
        if event.key() == DIRECTION_CONTROLS["forwards"]:
            self.direction_finder.forwards_indicator.button_released()
        elif event.key() == DIRECTION_CONTROLS["left"]:
            self.direction_finder.left_indicator.button_released()
        elif event.key() == DIRECTION_CONTROLS["backwards"]:
            self.direction_finder.backwards_indicator.button_released()
        elif event.key() == DIRECTION_CONTROLS["right"]:
            self.direction_finder.right_indicator.button_released()
        self.transceiver.direction = self.direction_finder.calculate_dir()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
  
