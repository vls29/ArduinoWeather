#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

char pompeii[] = "192.168.0.16";
int pompeiiPort = 28080;
const int httpRequestDelay = 15;

// the number determined to be "on" from the analogue pin
unsigned int onState = 200;

double minutesBetweenCalls = 0.0167;
String anemometerCircumference = String("0.5655");
//double mpsToMphMultiplier = 2.2369;

long millisecondsBetweenCalls = 60000L;

///////// CHANGEABLE VALUES ABOVE /////////

const int sensorPin = A0;

EthernetClient pompeiiClient;
byte mac[] = {0x90, 0xA0, 0xDA, 0x0E, 0x9B, 0xE6};
char pompeiiService[] = "/weather";

unsigned int windCircuitOnCount = 0;
unsigned int windCircuitOnCountGust = 0;
unsigned int gustMaxCount = 0;
unsigned int readCount = 0;

boolean lastStatus = false;

unsigned long lastTimeUploaded = millis();
unsigned long previousTime = 0UL;

unsigned long lastTimeGustCalculated = millis();
unsigned long previousGustTime = 0UL;

unsigned long gustPeriod = 1000UL;

void setup()
{
  Serial.begin(9600);
  connectToEthernet();
}

void connectToEthernet()
{
  unsigned long millisecondsPerMinute = 60000L;
  // attempt to connect to Wifi network:
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP waiting 1 minute");
    delay(millisecondsPerMinute);

    if (Ethernet.begin(mac) == 0)
    {
      Serial.println("Failed to configure Ethernet using DHCP waiting 1 more minute");
      delay(millisecondsPerMinute);

      if (Ethernet.begin(mac) == 0) {
        Serial.println("Failed to configure Ethernet using DHCP stopping - will need reset");
        while(true);
      }
    }

  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");

  Serial.print("Connected to the network IP: ");
  Serial.println(Ethernet.localIP());
}

void loop()
{
  readAnemometer();
  calculateGustSpeed();

  if (isTimeToUploadData()) {
    Serial.println("Uploading data");
    sendResultsToPompeii();

    resetFlags();
  }

  delay(1);

  /*if (isTimeToOutputData())
   {
   Serial.print("windCircuitOnCount ");
   Serial.println(windCircuitOnCount);
   
   Serial.print("estimated mps ");
   double mps = ((double) windCircuitOnCount * anemometerCircumference);
   Serial.println(doubleToString(mps));
   
   Serial.print("estimated mph ");
   double mph = (mps * mpsToMphMultiplier);
   Serial.println(doubleToString(mph));
   
   // count needs to be higher than 200 otherwise we won't potentially capture enough readings to measure a really strong gust
   Serial.print("count per period: ");
   Serial.println(readCount);
   
   readCount = 0;
   windCircuitOnCount = 0;
   }*/
}

void resetFlags()
{
  windCircuitOnCount = 0;
  windCircuitOnCountGust = 0;
  gustMaxCount = 0;
  readCount = 0;
}

void calculateGustSpeed() {
  unsigned long currentTime = millis();

  if(currentTime < previousGustTime)  {
    lastTimeGustCalculated = currentTime;
  }

  previousGustTime = currentTime;

  if( (currentTime - lastTimeGustCalculated) >= gustPeriod) {
    //Serial.println(currentTime - lastTimeGustCalculated);
    
    // calc the difference between the count on and the last value gust count was and if the count is greater than the last gust reading, store it.
    int countDifferenceSinceLastCalc = windCircuitOnCount - windCircuitOnCountGust;
    
    //Serial.print("Gust: ");
    //Serial.println(countDifferenceSinceLastCalc);
    //Serial.print("CurrentMax: ");
    //Serial.println(gustMaxCount);
    
    if( countDifferenceSinceLastCalc > gustMaxCount ) {
      gustMaxCount = countDifferenceSinceLastCalc;
    }

    windCircuitOnCountGust = windCircuitOnCount;
    
    lastTimeGustCalculated = currentTime;
  }
}

boolean isTimeToUploadData() {
  unsigned long currentTime = millis();

  if(currentTime < previousTime)  {
    lastTimeUploaded = currentTime;
  }

  previousTime = currentTime;

  if( (currentTime - lastTimeUploaded) >= millisecondsBetweenCalls) {
    Serial.println("Time to upload");
    lastTimeUploaded = currentTime;
    return true;
  }
  return false;
}

void readAnemometer()
{
  readCount++;

  int sensorVal = analogRead(sensorPin);

  if( sensorVal > onState) {
    if( !lastStatus ) {
      windCircuitOnCount++;
      lastStatus = true;
    }
  } 
  else {
    lastStatus = false;
  }
}

String getPostData() {
  return "{\"circ\":" + anemometerCircumference + ",\"windOn\":" + String(windCircuitOnCount) + ",\"gustMax\":" + String(gustMaxCount) + "}";
}

void sendResultsToPompeii() {
  Serial.println("sendResultsToPompeii");

  String postData = getPostData();
  Serial.println(postData);

  if (pompeiiClient.connect(pompeii, pompeiiPort)) {
    Serial.println("connected to pompeii");
    // Make a HTTP request:
    pompeiiClient.println("POST " + String(pompeiiService) + " HTTP/1.1");
    pompeiiClient.println("Host: " + String(pompeii) + ":" + pompeiiPort);
    pompeiiClient.println("Content-Type: application/json");
    pompeiiClient.println("Content-Length: " + String(postData.length()));
    pompeiiClient.println("Pragma: no-cache");
    pompeiiClient.println("Cache-Control: no-cache");
    pompeiiClient.println("Connection: close");
    pompeiiClient.println();

    pompeiiClient.println(postData);
    pompeiiClient.println();

    delay(httpRequestDelay);
    pompeiiClient.stop();
    pompeiiClient.flush();
    Serial.println("Called pompeii");
  }
}

