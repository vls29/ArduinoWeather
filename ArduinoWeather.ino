#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

char serverAddress[] = "home-monitoring.scaleys.co.uk";
int serverPort = 80;
const int httpRequestDelay = 15;

// the number determined to be "on" from the analogue pin
unsigned int onState = 200;

String anemometerCircumference = String("0.5655");

long millisecondsBetweenCalls = 60000L;

///////// CHANGEABLE VALUES ABOVE /////////

const int sensorPinAnemometer = A0;
const int sensorPinWindVane = A1;

const int sensorPinRainGauge = 2;

EthernetClient ethernetClient;
byte mac[] = {0x90, 0xA0, 0xDA, 0x0E, 0x9B, 0xE6};
char serviceEndpoint[] = "/weather";

unsigned int windCircuitOnCount = 0;
unsigned int windCircuitOnCountGust = 0;
unsigned int gustMaxCount = 0;

unsigned int readCountAnemometer = 0;

boolean anemometerLastStatus = false;

unsigned long lastTimeUploaded = millis();
unsigned long previousTime = 0UL;

unsigned long lastTimeGustCalculated = millis();
unsigned long previousGustTime = 0UL;

unsigned long gustPeriod = 1000UL;

unsigned int windDirection = 0;
volatile unsigned int rainCount = 0;

void setup()
{
  Serial.begin(9600);
  connectToEthernet();
  attachInterrupts();
  Serial.println("Setup complete");
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

void attachInterrupts()
{
  pinMode(sensorPinRainGauge, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPinRainGauge), rainGuageChange, RISING);
}

void loop()
{
  readAnemometer();
  calculateGustSpeed();

  if (isTimeToUploadData()) {
    readWindVane();
    Serial.println("Uploading data");
    sendResultsToServer();

    resetFlags();
  }
}

void resetFlags()
{
  windCircuitOnCount = 0;
  windCircuitOnCountGust = 0;
  gustMaxCount = 0;
  readCountAnemometer = 0;
  rainCount = 0;
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
  readCountAnemometer++;

  int sensorVal = analogRead(sensorPinAnemometer);

  if( sensorVal > onState) {
    if( !anemometerLastStatus ) {
      windCircuitOnCount++;
      anemometerLastStatus = true;
    }
  } 
  else {
    anemometerLastStatus = false;
  }
}

void readWindVane() {
  int sensorVal = analogRead(sensorPinWindVane);
  //Serial.print("Wind vane sensor value: ");
  //Serial.println(sensorVal);
  windDirection = sensorVal;
}

void rainGuageChange() {
  rainCount++;
  //Serial.println("Rain gauge change detected. Total count: " + String(rainCount));
}

String getPostData() {
  return "{\"circ\":" + anemometerCircumference + ",\"windOn\":" + String(windCircuitOnCount) + ",\"gustMax\":" + String(gustMaxCount) + ",\"direction\":" + String(windDirection) + ",\"rainCount\":" + String(rainCount) + "}";
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
