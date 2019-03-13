// This #include statement was automatically added by the Particle IDE.
#include "application.h" //Notwendig damit die externe RGB-LED schon beim Booten leuchtet!
#include "HX711_ADC.h"
#include "Particle.h"
#include "PietteTech_DHT.h" //Include PietteTech_DHT
#include "cellular_hal.h"
#include "JsonParserGeneratorRK.h"

PRODUCT_ID(8351); // replace by your product ID
PRODUCT_VERSION(1); // increment each time you upload to the console

// Using SEMI_AUTOMATIC mode to get the lowest possible data usage by
// registering functions and variables BEFORE connecting to the cloud.
SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

//Hologram APN
//STARTUP(cellular_credentials_set("hologram", "", "", NULL));


//********************************************************************
// Automatically mirror the onboard RGB LED to an external RGB LED
// No additional code needed in setup() or loop()
//Since 0.6.1 Allows a set of PWM pins to mirror the functionality of the on-board RGB LED.
STARTUP(RGB.mirrorTo(D3, D3, D3));
//********************************************************************

// DHT humidity/temperature sensors
#define DHTPIN3 D4     // what pin we're connected to
#define DHTPIN4 D5

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11		// DHT 11
#define DHTTYPE3 DHT22		// DHT 22 (AM2302)
#define DHTTYPE4 DHT22		// DHT 22 (AM2302)
//#define DHTTYPE DHT21		// DHT 21 (AM2301)

PietteTech_DHT dht_pin3(DHTPIN3, DHTTYPE3);
PietteTech_DHT dht_pin4(DHTPIN4, DHTTYPE4);

FuelGauge fuel; // Initalize the Fuel Gauge so we can call the fuel gauge functions below.

//HX711 Wägezellenverstärker
#define DOUT  A0
#define CLK  A1

//HX711 constructor (dout pin, sck pin)
HX711_ADC LoadCell(DOUT, CLK);

String strCalibration = "";
String strScalefactor="";
String strOffset="";

float offset = 0;
float scalefactor = 1;

float floatGewicht = 0;
String stringGewicht = "";

float floatHumidity3 = 0;
String stringHumidity3 = "";

float floatTemperature3 = 0;
String stringTemperature3 ="";

float floatHumidity4 = 0;
String stringHumidity4 = "";

float floatTemperature4 = 0;
String stringTemperature4 ="";

float floatRSSI = 0;
String stringRSSI = "";

double soc = 0; // Variable to keep track of LiPo state-of-charge (SOC)
String stringSOC = "";

boolean scale_conf = false;

// Global parser that supports up to 256 bytes of data and 20 tokens
JsonParserStatic<256, 20> parser1;

long t;


void setup() {

  // put your setup code here, to run once:
  delay(5000);
  // Begin serial communication
  Serial.begin(9600);

    Serial.println("Start Setup...");
    LoadCell.begin();
    long stabilisingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilising time
    LoadCell.start(stabilisingtime);

    PMIC pmic; //Initalize the PMIC class so you can call the Power Management functions below.
    pmic.setChargeCurrent(0,0,1,0,0,0); //Set charging current to 1024mA (512 + 512 offset)
    pmic.setInputVoltageLimit(4840);   //Set the lowest input voltage to 4.84 volts. This keeps my 5v solar panel from operating below 4.84 volts.

}

void loop() {

    Serial.println("Start Loop...");

    if(fuel.getSoC() > 20) // If the battery SOC is above 20% then we will turn on the modem and then send the sensor data.
    {
      //Needed in SEMI_AUTOMATIC Mode
      Cellular.connect();  // This command turns on the Cellular Modem and tells it to connect to the cellular network.

      Serial.println("Connecting to Cellular...");
      delay(500);

      if (!waitFor(Cellular.ready, 600000)) { //If the cellular modem does not successfuly connect to the cellular network in 10 mins then go back to sleep via the sleep command below.
          delay(5000);
          System.sleep(SLEEP_MODE_DEEP, 3580);
      }

      Serial.println("Connecting to Particle Cloud...");

      Particle.connect();
      Particle.process(); // explicitly trigger the background task

      // Listen for the webhook response, and call calibrationResponse
      Particle.subscribe("hook-response/calibration", calibrationResponse, MY_DEVICES);

      // Give ourselves 10 seconds before we actually start the
      // program.  This will open the serial monitor before
      // the program sends the request
      for(int i=0;i<10;i++) {
          Serial.println("waiting " + String(10-i) + " seconds before we publish");
          delay(1000);
      }


      if (Particle.connected()) {

          // publish the event that will trigger our Webhook
          Particle.publish("calibration");
          delay(5000); //Wichtig: Webhook gnügend Zeit geben, um die Daten zu Empfangen.
      }

      Serial.println("Calibration: " + strCalibration);
      scalefactor = strScalefactor.toFloat();
      Serial.println("Scalefactor: " + strScalefactor);
      offset = strOffset.toFloat();
      Serial.println("Offset: " + strOffset);

      delay(5000); //Wichtig: Webhook gnügend Zeit geben, um die Daten zu Empfangen.

      LoadCell.setCalFactor(scalefactor); // user set calibration factor (float)


      CellularSignal sig = Cellular.RSSI();
      Serial.print("RSSI: ");
      Serial.println(sig.rssi);
      Serial.print("Quality: ");
      Serial.println(sig.qual);

      CellularBand band_avail;
      if (Cellular.getBandSelect(band_avail)) {
        Serial.print("Available bands: ");
        for (int x=0; x<band_avail.count; x++) {
            Serial.printf("%d", band_avail.band[x]);
            if (x+1 < band_avail.count) Serial.printf(",");
          }
          Serial.println();
        }
        else {
          Serial.printlnf("Bands available not retrieved from the modem!");
        }

       //CellularSignal sig = Cellular.RSSI();
       floatRSSI = sig.rssi;
       stringRSSI = String(floatRSSI, 2);


        if (scalefactor != 0) {
          scale_conf = true;
        }

         //Begin DHT communication
         int result3 = dht_pin3.acquireAndWait(1000); // wait up to 1 sec (default indefinitely)
         int result4 = dht_pin4.acquireAndWait(1000);

         //update() should be called at least as often as HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS
         //longer delay in scetch will reduce effective sample rate (be carefull with delay() in loop)
         LoadCell.update();

         delay(5000);

         //get smoothed value from data set + current calibration factor
         if (millis() > t + 250) {
             float floatGewicht = LoadCell.getData();
             stringGewicht =  String(floatGewicht, 2);
             Serial.print("Load_cell output val: ");
             Serial.println(floatGewicht);
             t = millis();
         }

        // Reading temperature or humidity takes about 250 milliseconds!
        // Sensor readings may also be up to 2 seconds 'old' (its a
        // very slow sensor)
        	floatHumidity3 = dht_pin3.getHumidity();
          stringHumidity3 = String(floatHumidity3, 2),
        // Read temperature as Celsius
        	floatTemperature3 = dht_pin3.getCelsius();
          stringTemperature3 = String(floatTemperature3, 2);

          floatHumidity4 = dht_pin4.getHumidity();
          stringHumidity4 = String(floatHumidity4, 2),
        // Read temperature as Celsius
        	floatTemperature4 = dht_pin4.getCelsius();
          stringTemperature4 = String(floatTemperature4, 2);

        // Use the on-board Fuel Gauge
          soc = fuel.getSoC();
          stringSOC = String(soc);
          delay(1000);

          if (!scale_conf ){
            // Turn off microcontroller and cellular.
            // Reset after seconds.
            // Ultra low power usage.
            System.sleep(SLEEP_MODE_DEEP, 3580);

            } else {
              Particle.publish("cloud4bees", JSON(), PRIVATE); // Send JSON Particle Cloud
              delay(1000);
              System.sleep(SLEEP_MODE_DEEP, 3580);
          }
    }
}

void calibrationResponse(const char *name, const char *data) {
    strCalibration = String(data);
    // Clear the parser state, add the string test2, and parse it
  	parser1.clear();
  	parser1.addString(strCalibration);
  	if (!parser1.parse()) {
	  	  Serial.println("parsing failed strCalibration");
	}

	  Serial.println("parsing successful strCalibration");
	  parser1.getOuterValueByKey("scalefactor", strScalefactor);
    parser1.getOuterValueByKey("offset", strOffset);
	}

String JSON() {
 String ret = "&field1=";
  ret.concat(stringGewicht);
  ret.concat("&field2=");
  ret.concat(stringTemperature3);
  ret.concat("&field3=");
  ret.concat(stringHumidity3);
  ret.concat("&field4=");
  ret.concat(stringTemperature4);
  ret.concat("&field5=");
  ret.concat(stringRSSI);
  ret.concat("&field6=");
  ret.concat(stringSOC);
  return ret;
}
