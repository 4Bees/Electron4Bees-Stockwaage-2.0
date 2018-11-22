// This #include statement was automatically added by the Particle IDE.
#include "application.h" //Notwendig damit die externe RGB-LED schon beim Booten leuchtet!
#include "HX711.h"
#include "Particle.h"
#include "PietteTech_DHT.h" //Include PietteTech_DHT
#include "cellular_hal.h"

PRODUCT_ID(7251); // replace by your product ID
PRODUCT_VERSION(1); // increment each time you upload to the console

// Using SEMI_AUTOMATIC mode to get the lowest possible data usage by
// registering functions and variables BEFORE connecting to the cloud.
//SYSTEM_MODE(SEMI_AUTOMATIC);

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

//HX711 Wägezellenverstärker
#define DOUT  A0
#define CLK  A1

HX711 scale(DOUT, CLK);

String str_scalefactor = "";
String str_offset = "";

float offset = 0;
float scalefactor = 0;

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



void setup() {

  // put your setup code here, to run once:

  //Needed in SEMI_AUTOMATIC Mode
  //Particle.connect();

  // Begin serial communication
  Serial.begin(9600);

    // Listen for the webhook response, and call gotScalefactor() and gotOffset()
    Particle.subscribe("hook-response/get_offset", gotOffset, MY_DEVICES);
    Particle.subscribe("hook-response/get_scalefactor", gotScalefactor, MY_DEVICES);

    // Give ourselves 10 seconds before we actually start the
    // program.  This will open the serial monitor before
    // the program sends the request
    for(int i=0;i<10;i++) {
        Serial.println("waiting " + String(10-i) + " seconds before we publish");
        delay(1000);
    }



    // publish the event that will trigger our Webhook
    Particle.publish("get_offset");
    delay(5000);
    Particle.publish("get_scalefactor");
    delay(5000);

    offset = str_offset.toFloat();
    scalefactor = str_scalefactor.toFloat();

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

    Serial.print("Offset: ");
    Serial.println(offset);
    Serial.print("Scalefactor: ");
    Serial.println(scalefactor);

    if (scalefactor != 0) {
      scale_conf = true;
    }


    scale.set_scale(scalefactor);                      //this value is obtained by calibrating the scale with known weights;
                                                 /*   How to Calibrate Your Scale
                                                      1.Call set_scale() with no parameter.
                                                      2.Call set_tare() with no parameter.
                                                      3.Place a known weight on the scale and call get_units(10).
                                                      4.Divide the result in step 3 to your known weight. You should get about the parameter you need to pass to set_scale.
                                                      5.Adjust the parameter in step 4 until you get an accurate reading.
                                                  */


}

void loop() {

      scale.power_up();


    //Begin DHT communication
     int result3 = dht_pin3.acquireAndWait(1000); // wait up to 1 sec (default indefinitely)
     int result4 = dht_pin4.acquireAndWait(1000);


      delay(5000);

    //scale.get_units(10) returns the medium of 10 measures
      floatGewicht = (scale.get_units(10) - offset);
      stringGewicht =  String(floatGewicht, 2);

      Serial.println(floatGewicht);
      Serial.println(stringGewicht);

      scale.power_down();

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

      CellularSignal sig = Cellular.RSSI();
      floatRSSI = sig.rssi;
      stringRSSI = String(floatRSSI, 2);

    // Use the on-board Fuel Gauge
      FuelGauge fuel;
      soc = fuel.getSoC();
      stringSOC = String(soc);
      delay(1000);

      if (!scale_conf ){
        System.sleep(SLEEP_MODE_DEEP, 3580);

      } else {

      //if (floatGewicht < 100000) {
        Particle.publish("cloud4bees", JSON(), PRIVATE); // Send JSON Particle Cloud
      //}

      delay(1000);

      System.sleep(SLEEP_MODE_DEEP, 3580);
    }
}



// This function will get called when offset comes in
void gotOffset(const char *name, const char *data) {
    str_offset = String(data);
}

// This function will get called when scalefactor comes in
void gotScalefactor(const char *name, const char *data) {
    str_scalefactor = String(data);
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
