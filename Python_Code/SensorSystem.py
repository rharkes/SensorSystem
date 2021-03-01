import sys

from PySide2.QtCore import Slot
from PySide2.QtWidgets import (QWidget, QAction, QApplication, QMainWindow, QPushButton, QGridLayout,
                               QLabel)
import serial.tools.list_ports


class SensorSystem(QWidget):
    serial_number = "DN041V5AA"  # identify sensorsystem by serialnumber instead of comport number

    def __init__(self):  # inherits all methods from QMainWindow
        # init GUI
        super().__init__()  # initialize QMainWindow
        self.button_update = QPushButton("Update")
        self.text_CO2 = QLabel("CO2 = ")
        self.text_ValveTime = QLabel("Valve open = ")
        self.text_RH = QLabel("Relative Humidity = ")
        self.text_Temp = QLabel("Temperature = ")
        self.text_AH = QLabel("Absolute Humidity = ")

        self.layout = QGridLayout()
        # add text in first column
        self.layout.addWidget(self.text_CO2, 0, 0)
        self.layout.addWidget(self.text_ValveTime, 1, 0)
        self.layout.addWidget(self.text_RH, 2, 0)
        self.layout.addWidget(self.text_Temp, 3, 0)
        self.layout.addWidget(self.text_AH, 4, 0)
        self.layout.addWidget(self.button_update, 5, 0)
        self.setLayout(self.layout)
        self.button_update.clicked.connect(self.update)

        # init Serial
        self.ser = None
        for comport in serial.tools.list_ports.comports():
            if comport.serial_number == SensorSystem.serial_number:
                print("Found sensor system. Connecting now.")
                self.ser = serial.Serial(port=comport.device, baudrate=9600, timeout=1)
                if self.ser.isOpen():
                    print("port was open, closing now")
                    self.ser.close()
                    self.ser.open()
                else:
                    self.ser.open()
                break
        if self.ser is None:
            raise ValueError("SensorSystem not found.")
        self.data = [0] * 5

    def identify(self):
        self.ser.reset_input_buffer()
        self.ser.write('*IDN?\r'.encode('utf-8'))
        self.ser.flush()
        print(self.ser.readline())

    def get_data(self):
        self.ser.reset_input_buffer()
        self.ser.write('?\r'.encode('utf-8'))
        self.ser.flush()
        raw_data = self.ser.read(12)
        self.data[0] = raw_data[0] + raw_data[1] * 2 ** 8 + raw_data[2] * 2 ** 16 + raw_data[3] * 2 ** 24  # co2
        self.data[1] = raw_data[4] + raw_data[5] * 2 ** 8  # temp
        self.data[2] = raw_data[6] + raw_data[7] * 2 ** 8  # rh
        self.data[3] = raw_data[8] + raw_data[9] * 2 ** 8  # ah
        self.data[4] = raw_data[10] + raw_data[11] * 2 ** 8  # valve

    def update(self):
        self.get_data()
        self.text_CO2.setText("CO2 = {0:.2f}%".format(self.data[0]/1E4))
        self.text_ValveTime.setText("Valve open = {0:.2f}%".format(self.data[4] / 655.35))
        self.text_RH.setText("Relative Humidity = {0:.2f}%".format(self.data[2] / 655.35))
        self.text_Temp.setText("Temperature = {0:.2f} °C".format((self.data[1] / 65535) * 175 - 45))
        self.text_AH.setText("Absolute Humidity = {0:.2f} g/m3".format(self.data[3] / 256))


class MicroscopeController(QMainWindow):
    def __init__(self, widget):
        QMainWindow.__init__(self)
        self.setWindowTitle("Microscope Controller")

        # Menu
        self.menu = self.menuBar()
        self.file_menu = self.menu.addMenu("File")

        # Exit QAction
        exit_action = QAction("Exit", self)
        exit_action.setShortcut("Ctrl+Q")
        exit_action.triggered.connect(self.exit_app)

        self.file_menu.addAction(exit_action)
        self.setCentralWidget(widget)

    @Slot()
    def exit_app(self, checked):
        QApplication.quit()


if __name__ == "__main__":
    # Qt Application
    app = QApplication(sys.argv)
    # QWidget
    ss = SensorSystem()
    # QMainWindow using QWidget as central widget
    window = MicroscopeController(ss)
    window.resize(400, 200)
    window.show()

    # Execute application
    sys.exit(app.exec_())
