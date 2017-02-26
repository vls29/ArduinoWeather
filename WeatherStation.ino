#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

const char pompeii[] = "192.168.0.16";
const int pompeiiPort = 80;

// the number determined to be "on" from the analogue pin
const unsigned int anemometerOnState = 200;

const double ANEMOMETER_CIRCUMFERENCE = 0.5655;
const double MPS_TO_MPH = 2.23694;

const int humiditySensorPin = A0;
const int temperatureSensorPin = A1;
const int pressureSensorPin = A2;
const int anemometerSensorPin = A3;

const bool trace = false;
const bool debug = true;

const bool SKIP_ETHERNET_INIT = true;

///////// CHANGEABLE VALUES ABOVE /////////

// calibrated to the exact voltage on the ethernet arduino I'm using
const double FIVE_VOLTS = 5.006;
const double ANALOG_PIN_RANGE = 1023.0;
const long READING_INCREMENT = 1L;
const long ZERO_LONG = 0L;
const double ZERO_DOUBLE = 0.0;
const int ZERO_INT = 0;

// pressure constants
const double PRESSURE_TEMPERATURE_ERROR_FACTOR_NORMAL = 1.0;
const double KPA_TO_MILLIBARS = 10.0;

long loopCounter = 0L;

double temperatureReadings = ZERO_DOUBLE;
double humidityReadings = ZERO_DOUBLE;
double pressureReadings = ZERO_DOUBLE;


EthernetClient pompeiiClient;
byte mac[] = {0x90, 0xA0, 0xDA, 0x0E, 0x9B, 0xE6};
char pompeiiService[] = "/pvoutput-post-weather.php";

unsigned int windCircuitOnCount = ZERO_INT;
unsigned int windCircuitOnCountGust = ZERO_INT;
unsigned int gustMaxCount = ZERO_INT;
unsigned int readCount = ZERO_INT;

boolean lastStatus = false;

// timings
const double millisecondsPerMinute = 60000.0;
const long millisecondsBetweenCalls = 60000L;
const double SECONDS_PER_MINUTE = 60.0;

unsigned long lastTimeUploaded = millis();
unsigned long previousTime = 0UL;

unsigned long lastTimeGustCalculated = millis();
unsigned long previousGustTime = 0UL;

unsigned long gustPeriod = 1000UL;

void setup() {
  Serial.begin(9600);
  if (!SKIP_ETHERNET_INIT) {
    connectToEthernet();
  }
}

void connectToEthernet() {
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
        while (true);
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
  readInstantTemperatureSensorValue();
  readInstantHumiditySensorValue();
  readInstantPressureSensorValue();

  readInstantAnemometerSensorValue();
  calculateGustSpeed();

  incrementLoopCounter();

  if (isTimeToUploadData()) {
    Serial.println("Uploading data");
    if (debug) {
      Serial.println("******************************************************");
    }
    double averageTemperature = calculateAverageTemperature();
    double averageTemperatureCompensatedHumidity = calculateAverageTemperatureCompensatedHumidity(averageTemperature);
    double averageTemperatureCompensatedPressure = calculateAverageTemperatureCompensatedPressure(averageTemperature);
    double windSpeed = getWindSpeedInMph();
    double gustSpeed = getGustSpeedInMph();
    if (debug) {
      Serial.println("******************************************************");
    }

    sendResultsToPompeii(averageTemperature, averageTemperatureCompensatedHumidity, averageTemperatureCompensatedPressure, windSpeed, gustSpeed);

    resetFlags();
  }
}

void resetFlags() {
  windCircuitOnCount = ZERO_INT;
  windCircuitOnCountGust = ZERO_INT;
  gustMaxCount = ZERO_INT;

  resetAverageTemperature();
  resetAverageHumidity();
  resetAveragePressure();

  resetLoopCounter();
}

boolean isTimeToUploadData() {
  unsigned long currentTime = millis();

  if (currentTime < previousTime)  {
    lastTimeUploaded = currentTime;
  }

  previousTime = currentTime;

  if ( (currentTime - lastTimeUploaded) >= millisecondsBetweenCalls) {
    Serial.println("Time to upload");
    lastTimeUploaded = currentTime;
    return true;
  }
  return false;
}


/*
 * Read from a 5V pin and multiply it up to it's true value.
 */
double read5VAnalogPin(const int pin) {
  double sensorVal = analogRead(pin);

  if (trace) {
    Serial.print("Raw pin '");
    Serial.print(pin);
    Serial.print("' sensor value: ");
    Serial.println(sensorVal);
  }

  sensorVal = sensorVal * (FIVE_VOLTS / ANALOG_PIN_RANGE);

  if (trace) {
    Serial.print("Pin '");
    Serial.print(pin);
    Serial.print("' true value: ");
    Serial.println(sensorVal);
  }

  return sensorVal;
}

void incrementLoopCounter() {
  loopCounter = loopCounter + READING_INCREMENT;
}

void resetLoopCounter() {
  if (debug) {
    Serial.print("Loops completed ");
    Serial.println(loopCounter);
  }

  loopCounter = ZERO_LONG;
}

///////////////////////////////////////////////////////
////////////////// POMPEII functions //////////////////
///////////////////////////////////////////////////////

String generatePostData(double averageTemperature, double averageTemperatureCompensatedHumidity, double averageTemperatureCompensatedPressure, double windSpeed, double gustSpeed) {
  return "temperature=" + String(averageTemperature) +
         "&humidity=" + String(averageTemperatureCompensatedHumidity) +
         "&pressure=" + String(averageTemperatureCompensatedPressure) +
         "&windSpeed=" + String(windSpeed) +
         "&gustSpeed=" + String(gustSpeed);
}

void sendResultsToPompeii(double averageTemperature, double averageTemperatureCompensatedHumidity, double averageTemperatureCompensatedPressure, double windSpeed, double gustSpeed) {
  Serial.println("sendResultsToPompeii");

  String postData = generatePostData(averageTemperature, averageTemperatureCompensatedHumidity, averageTemperatureCompensatedPressure, windSpeed, gustSpeed);
  Serial.println("post data: " + postData);

  if (!SKIP_ETHERNET_INIT) {
    if (pompeiiClient.connect(pompeii, pompeiiPort)) {
      Serial.println("connected to pompeii");
      // Make a HTTP request:
      pompeiiClient.print("POST ");
      pompeiiClient.print(pompeiiService);
      pompeiiClient.println(" HTTP/1.1");
      pompeiiClient.print("Host: ");
      pompeiiClient.print(pompeii);
      pompeiiClient.print(":");
      pompeiiClient.println(pompeiiPort);
      pompeiiClient.println("Accept: text/html");
      pompeiiClient.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
      pompeiiClient.print("Content-Length: ");
      pompeiiClient.println(postData.length());
      pompeiiClient.println("Pragma: no-cache");
      pompeiiClient.println("Cache-Control: no-cache");
      pompeiiClient.println("Connection: close");
      pompeiiClient.println("X-Data-Source: WEATHER");
      pompeiiClient.println();

      pompeiiClient.println(postData);
      pompeiiClient.println();

      pompeiiClient.stop();
      pompeiiClient.flush();
      Serial.println("Called pompeii");
    }
  }
}

///////////////////////////////////////////////////////
///////////////// Anemometer functions ////////////////
///////////////////////////////////////////////////////

void readInstantAnemometerSensorValue() {
  int sensorVal = analogRead(anemometerSensorPin);

  if ( sensorVal > anemometerOnState) {
    if ( !lastStatus ) {
      windCircuitOnCount++;
      lastStatus = true;
    }
  }
  else {
    lastStatus = false;
  }
}

void calculateGustSpeed() {
  unsigned long currentTime = millis();

  // deals with the clock starting from zero after overflowing
  if (currentTime < previousGustTime)  {
    lastTimeGustCalculated = currentTime;
  }

  previousGustTime = currentTime;

  if ( (currentTime - lastTimeGustCalculated) >= gustPeriod) {
    //Serial.println(currentTime - lastTimeGustCalculated);

    // calc the difference between the count on and the last value gust count was and if the count is greater than the last gust reading, store it.
    int countDifferenceSinceLastCalc = windCircuitOnCount - windCircuitOnCountGust;

    //Serial.print("Gust: ");
    //Serial.println(countDifferenceSinceLastCalc);
    //Serial.print("CurrentMax: ");
    //Serial.println(gustMaxCount);

    if ( countDifferenceSinceLastCalc > gustMaxCount ) {
      gustMaxCount = countDifferenceSinceLastCalc;
    }

    // store the current wind on count so that the calculation for the gust is using the last value that was previously checked against.
    windCircuitOnCountGust = windCircuitOnCount;

    lastTimeGustCalculated = currentTime;
  }
}

double getWindSpeedInMph() {
  if (windCircuitOnCount == ZERO_INT) {
    return ZERO_DOUBLE;
  }

  double mph = ((double)windCircuitOnCount / SECONDS_PER_MINUTE) * ANEMOMETER_CIRCUMFERENCE * MPS_TO_MPH;

  if (debug) {
    Serial.print("Wind mph: ");
    Serial.println(mph);
  }

  return mph;
}

double getGustSpeedInMph() {
  if (gustMaxCount == ZERO_INT) {
    return ZERO_DOUBLE;
  }

  double mph = ((double)gustMaxCount / SECONDS_PER_MINUTE) * ANEMOMETER_CIRCUMFERENCE * MPS_TO_MPH;

  if (debug) {
    Serial.print("Gust mph: ");
    Serial.println(mph);
  }

  return mph;
}

///////////////////////////////////////////////////////
////////////////// Humidity functions /////////////////
///////////////////////////////////////////////////////

/*
 * For efficiency reasons, the stored value is the relative uncompensated humidity.
 * Difference in number of readings is staggering if too many calculations done on the arduino.
 */
void readInstantHumiditySensorValue() {
  double humidity = read5VAnalogPin(humiditySensorPin);
  if (trace) {
    Serial.print("Humidity sensor value: ");
    Serial.println(humidity);
  }

  humidityReadings = humidityReadings + humidity;
}

/**
 * Calculates the average non-temperature compensated humidity first and then uses the value in the average temperature compensated humidity calculation.
 * All the calculations are done here for efficiency.
 */
double calculateAverageTemperatureCompensatedHumidity(double averageTemperature) {
  double averageNonCompensatedHumidity = (double)humidityReadings / (double)loopCounter;

  // From Honeywell: Vout = (Vsupply)(0.00636(Sensor RH) + 0.1515), typical at 25 deg. C
  // Or written to calculate Relative Humidity: Relative Humidity = ((Vout / Vsupply) - 0.1515) / 0.00636, typical at 25 deg. C
  double averageRelativeHumidity = ((averageNonCompensatedHumidity / FIVE_VOLTS) - 0.1515) / 0.00636;

  // Temperature Compensated Relative Humidity = (Relative Humidity)/(1.0546 - 0.00216T), T in deg. C
  double averageTemperatureCompensatedHumidity = (averageRelativeHumidity / (1.0546 - (0.00216 * averageTemperature)));

  if (debug) {
    Serial.print("Average humidity sensor value: ");
    Serial.println(averageNonCompensatedHumidity);
    Serial.print("Average non-compensated humidity: ");
    Serial.println(averageRelativeHumidity);
    Serial.print("Temperature compensated humidity: ");
    Serial.println(averageTemperatureCompensatedHumidity);
  }

  return averageTemperatureCompensatedHumidity;
}

void resetAverageHumidity() {
  humidityReadings = ZERO_DOUBLE;
}

///////////////////////////////////////////////////////
//////////////// Temperature functions ////////////////
///////////////////////////////////////////////////////

void readInstantTemperatureSensorValue() {
  double temperature = calculateInstantTemperature();

  if (trace) {
    Serial.print("temperature: ");
    Serial.println(temperature);
  }

  temperatureReadings = temperatureReadings + temperature;
}

double calculateInstantTemperature() {
  double voltage = read5VAnalogPin(temperatureSensorPin);
  double temperature = ((voltage - 0.5) * 100.0); // NEEDS CHECKING AS CANNOT FIND ANY DOCUMENTATION ON THIS FIGURE...

  return temperature;
}

double calculateAverageTemperature() {
  double averageTemperature = temperatureReadings / (double)loopCounter;

  if (debug) {
    Serial.print("Average Temperature: ");
    Serial.println(averageTemperature);
  }

  return averageTemperature;
}

void resetAverageTemperature() {
  temperatureReadings = ZERO_DOUBLE;
}


///////////////////////////////////////////////////////
///////////////// Pressure functions //////////////////
///////////////////////////////////////////////////////

void readInstantPressureSensorValue() {
  double uncompensatedPressure = read5VAnalogPin(pressureSensorPin);

  if (trace) {
    Serial.print("Uncompensated pressure: ");
    Serial.println(uncompensatedPressure);
  }

  pressureReadings = pressureReadings + uncompensatedPressure;
}

/**
 * From the pressure sensor document:
 * Vout = Vsupply * ((Pressure * 0.004) - 0.04) +- (Pressure Error * Temperature Factor * 0.004 x Vsupply)
 *
 * Calculate: Vout = Vsupply * ((Pressure * 0.004) - 0.04) using getErrorFactor(averageTemperature) for the error factor portion of the equation.
 *
 * NOT USING the error factor as it's too difficult to factor in as it can be positive or negative...
 */
double calculateAverageTemperatureCompensatedPressure(double averageTemperature) {
  double averagePressure = (double)pressureReadings / (double)loopCounter;

  double calculatedPressure = ((averagePressure / FIVE_VOLTS) + 0.04) / 0.004;
  //pressure = (pressure - getErrorFactor(instantTemperature)) * kpaToMillibarsMultiplier;
  calculatedPressure = calculatedPressure * KPA_TO_MILLIBARS;

  if (debug) {
    Serial.print("Average pressure sensor reading: ");
    Serial.println(averagePressure);
    Serial.print("Calculated pressure: ");
    Serial.println(calculatedPressure);
  }

  return calculatedPressure;
}

void resetAveragePressure() {
  pressureReadings = ZERO_DOUBLE;
}

/**
 * NOT IN USE
 *
 * Because the document is too vague on the +- values, cannot reliably use this error factor...
 *
 *
 * From the pressure sensor document:
 * Vout = Vsupply * ((Pressure * 0.004) - 0.04) +- (Pressure Error * Temperature Factor * 0.004 x Vsupply)
 *
 * This method calculates the Error Factor, which is: (Pressure Error * Temperature Factor * 0.004 x Vsupply).
 *
 * From the manual, the Pressure Error is a fixed +-3.45 kPa between 20 and 250kPa
 */
double getErrorFactor(double instantTemperature) {
  double temperatureCompensation = PRESSURE_TEMPERATURE_ERROR_FACTOR_NORMAL;
  if ( instantTemperature < 0.0 || instantTemperature > 85.0)
  {
    double difference = 0.0;
    if (instantTemperature > 0.0)  {
      difference = instantTemperature - 85.0;
    } else {
      // invert the negative number
      difference = instantTemperature - (instantTemperature * 2.0);
    }

    temperatureCompensation = (difference * (2.0 / 40.0)) + 1.0;
  }

  return (3.45 * temperatureCompensation * 0.004 * FIVE_VOLTS);
}


