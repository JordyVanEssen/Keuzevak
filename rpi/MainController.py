import time

from smbus import SMBus
from influxdb import InfluxDBClient

class MainController():
    def __init__(self):
        pass

    def run(self):
        self.buttonStates = 0
        self.client = InfluxDBClient(host="localhost", port=8086)
        self.address = 0x8
        self.bus = SMBus(1)
        self.busBusy = False
        self.update = False

        self.getArduinoData()

    def intToByteArray(self, value):
        return [int(i) for i in "{0:08b}".format(value)]

    def getArduinoData(self):
        previousMillis = 0
        currentMillis = time.time()

        outs = [
            "cv pomp", 
            "houtkachel pomp", 
            "pelletketel", 
            "elektrische verwarmingselement", 
            "secundairePomp", 
            "mainPomp", 
            "driewegklep"
        ]

        tempSensors = [
            "boilertank", 
            "cv tank", 
            "houtkachel", 
            "zonneboiler", 
            "buiten"
        ]

        inputs = [
            "dompel thermostaat", 
            "kamer thermostaat", 
            "flowswitch"
        ]

        buttons = [
            "handmatigDriewegklep",
            "boilertankVerwarmen",
            "beidetanksVerwarmen",
            "ignorePelletfurnace",
            "elektrischelement",
            "zomerstand"
        ]

        values = {}
        previous = []

        while True:
            try:
                currentMillis = time.time()

                if currentMillis - previousMillis >= 10 or self.update:
                    previousMillis = currentMillis
                    self.update = False

                    self.busBusy = True
                    data = self.bus.read_i2c_block_data(self.address, 0, 16)
                    self.busBusy = False
                    filtered = [data[i] for i in range(0, len(data), 2)]
                    if 255 not in filtered:
                        for i in range(len(filtered)):
                            if previous == []:
                                previous = filtered
                            if (filtered[i] > previous[i] + 20 or filtered[i] < previous[i] - 20) and i > 2:
                                values[tempSensors[i]] = previous[i] 
                            else:
                                # outputs
                                if i == 0:
                                    b = self.intToByteArray(filtered[i])[1:]
                                    b.reverse()
                                    for v in range(len(b)):
                                        values[outs[v]] = b[v]
                                # buttons pressed
                                elif i == 1:
                                    self.buttonStates = filtered[i]
                                    b = self.intToByteArray(filtered[i])[2:]
                                    b.reverse()
                                    for v in range(len(b)):
                                        values[buttons[v]] = b[v]
                                # input sensors
                                elif i == 2:
                                    b = self.intToByteArray(filtered[i])[5:]
                                    b.reverse()
                                    for v in range(len(b)):
                                        values[inputs[v]] = b[v]
                                # temperatures
                                else:
                                    values[tempSensors[i - 3]] = filtered[i] - 255 if filtered[i] > 200 and filtered[i] < 255 else filtered[i]
                                    previous = filtered

                        temperatures = [
                            {
                                "measurement": "temperatures",
                                "tags": {
                                    "room": "Technischeruimte"
                                },
                                "fields": {
                                    "boilertank": values['boilertank'],
                                    "cvTank": values['cv tank'],
                                    "houtkachel": values['houtkachel'],
                                    "zonneboiler": values['zonneboiler'],
                                    "buiten": values['buiten']
                                }
                            }
                        ]

                        outValues = [
                            {
                                "measurement": "output",
                                "tags": {
                                    "room": "Technischeruimte"
                                },
                                "fields": {
                                    "cvPomp": values['cv pomp'],
                                    "houtkachelPomp": values['houtkachel pomp'],
                                    "pelletketel": values['pelletketel'],
                                    "elektrischeVerwarmingselement": values['elektrische verwarmingselement'],
                                    "secundairePomp": values['secundairePomp'],
                                    "mainPomp": values['mainPomp'],
                                    "driewegklep": values['driewegklep'],
                                    "dompelThermostaat": values['dompel thermostaat'],
                                    "kamerThermostaat": values['kamer thermostaat'],
                                    "flowswitch": values['flowswitch']
                                }
                            }
                        ]

                        inValues = [
                            {
                                "measurement": "input",
                                "tags": {
                                    "room": "Technischeruimte"
                                },
                                "fields": {
                                    "Handmatig driewegklep": values['handmatigDriewegklep'],
                                    "Boilertank verwarmen": values['boilertankVerwarmen'],
                                    "Beide tanks verwarmen": values['beidetanksVerwarmen'],
                                    "Pelletketel onderhoud": values['ignorePelletfurnace'],
                                    "Elektrischelement": values['elektrischelement'],
                                    "Zomerstand": values['zomerstand']
                                }
                            }
                        ]

                        self.client.write_points(inValues, database="Technischeruimte")
                        self.client.write_points(outValues, database="Technischeruimte")
                        self.client.write_points(temperatures, database="Technischeruimte")
            except Exception as e:
                print(e)

if __name__ == "__main__":
    controller = MainController()
    controller.run()