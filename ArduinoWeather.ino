#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

char serverAddress[] = "home-monitoring.scaleys.co.uk";
int serverPort = 80;
const int httpRequestDelay = 15;

// the number determined to be "on" from the analogue pin
unsigned int onState = 200;

double minutesBetweenCalls = 0.0167;
String anemometerCircumference = String("0.5655");
//double mpsToMphMultiplier = 2.2369;

long millisecondsBetweenCalls = 60000L;

///////// CHANGEABLE VALUES ABOVE /////////

const int sensorPin = A0;

EthernetClient ethernetClient;
byte mac[] = {0x90, 0xA0, 0xDA, 0x0E, 0x9B, 0xE6};
char serviceEndpoint[] = "/weather";

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
  // start the Ethernet connection:
  bool connectedToNetwork = false;
  while(!connectedToNetwork) {
    Serial.println("Attempting to connect to network...");

    if (Ethernet.begin(mac) == 0) {
        Serial.println("Failed to connect, trying again...");
    } else {
        Serial.println("Connected successfully");
        connectedToNetwork = true;
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
    sendResultsToServer();

    resetFlags();
  }

  delay(1);
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

void sendResultsToServer() {
  Serial.println("sendResultsToServer");

  String postData = getPostData();
  Serial.println(postData);

  if (ethernetClient.connect(serverAddress, serverPort)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    ethernetClient.println("POST " + String(serviceEndpoint) + " HTTP/1.1");
    ethernetClient.println("Host: " + String(serverAddress) + ":" + serverPort);
    ethernetClient.println("Content-Type: application/json");
    ethernetClient.println("Content-Length: " + String(postData.length()));
    ethernetClient.println("Pragma: no-cache");
    ethernetClient.println("Cache-Control: no-cache");
    ethernetClient.println("Connection: close");
    ethernetClient.println();

    ethernetClient.println(postData);
    ethernetClient.println();

    delay(httpRequestDelay);
    ethernetClient.stop();
    ethernetClient.flush();
    Serial.println("Called server");
  }
}
