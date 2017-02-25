

///////// CHANGEABLE VALUES /////////

const int humiditySensorPin = A0;
const double humidityVoltageSupply = 5.0;

const int temperatureSensorPin = A1;
const double baselineTemperature = 19.0;

const int pressureSensorPin = A2;
const double pressureVoltageSupply = 5.0;

const bool debug = false;

///////// CHANGEABLE VALUES ABOVE /////////

// humidity constants
const double sensorMultiplier = 0.00636;
const double sensorConstant = 0.1515;

// temperature constants
const double temperatureCompensationConstant = 1.0546;
const double temperatureCompensationMultiplier = 0.00216;

// pressure constants
const double pressureTempErrorNormal = 1.0;
const double pressureError = 3.45;

const double pressureCompensation1 = 0.004;
const double pressureCompensation2 = 0.04;

const double kpaToMillibarsMultiplier = 10.0;

// counters
long temperatureCounter = 0L;
double temperatureReadings = 0.0;

long humidityCounter = 0L;
double humidityReadings = 0.0;

long pressureCounter = 0L;
double pressureReadings = 0.0;

// timings
double minutesBetweenCalls = 0.5;

unsigned long millisecondsPerMinute = 60000;
unsigned long minutesInHour = 60;
unsigned long timeBetweenCalls = minutesBetweenCalls * millisecondsPerMinute;

unsigned long lastTimeUploaded = millis();

void setup() {
  Serial.begin(9600);
}

void loop() {
  double instantTemperature = getInstantTemperature();
  getInstantHumidity(instantTemperature);
  getInstantPressure(instantTemperature);

  if (isTimeToUpload()) {
     Serial.println("******************************************************");
     getAverageTemperature();
     getAverageHumidity();
     getAveragePressure();
     Serial.println("******************************************************");
     
     resetAverageTemperature();
     resetAverageHumidity();
     resetAveragePressure();
  }

  //delay(1000);
}

boolean isTimeToUpload() {
  unsigned long time = millis();

  if( (time - lastTimeUploaded) >= timeBetweenCalls) {
    Serial.println("Time to upload");
    lastTimeUploaded = time;
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////
////////////////// Humidity functions /////////////////
///////////////////////////////////////////////////////

double getInstantHumidity(double instantTemperature) {
  double sensorCalculation = humiditySensorCalculation();
  if(debug) {
    Serial.print("Humidity reading at 25c (not compensated): ");
    Serial.println(sensorCalculation);
  }

  double temperatureCompensatedCaculation = humidityTemperatureCompensated(sensorCalculation, instantTemperature);
  if(debug) {
    Serial.print("Temperature compensated humidity: ");
    Serial.println(temperatureCompensatedCaculation);
  }

  humidityReadings = humidityReadings + temperatureCompensatedCaculation;
  humidityCounter = humidityCounter + 1L;

  return temperatureCompensatedCaculation;
}

double getAverageHumidity() {
  double averageValue = (double)humidityReadings/(double)humidityCounter;

  Serial.print("Average Humidity: ");
  Serial.println(averageValue);

  return averageValue;
}

void resetAverageHumidity() {
  humidityCounter = 0L;
  humidityReadings = 0.0;
}

/**
 * From Honeywell: Vout = (Vsupply)(0.00636(Sensor RH) + 0.1515), typical at 25 deg. C
 */
double humiditySensorCalculation() {
  //return (humidityVoltageSupply * ((sensorMultiplier * readHumiditySensor()) + sensorConstant));
  //return (humidityVoltageSupply * (sensorMultiplier * (readHumiditySensor() + sensorConstant)));
  double sensorVoltage = readHumiditySensor() * (humidityVoltageSupply / 1023.0);

  if(debug) {
    Serial.print("humidity sensor voltage: ");
    Serial.println(sensorVoltage);
  }

  return ((sensorVoltage / humidityVoltageSupply) - sensorConstant) / sensorMultiplier;
}

double readHumiditySensor() {
  double sensorVal = analogRead(humiditySensorPin);

  if(debug) {
    Serial.print("humidity sensor value: ");
    Serial.println(sensorVal);
  }

  return sensorVal;
}

/**
 * From Honeywell: True RH = (Sensor RH)/(1.0546 - 0.00216T), T in deg. C
 */
double humidityTemperatureCompensated(double sensorCalculation, double instantTemperature) {
  //return (sensorCalculation / (temperatureCompensationConstant - (temperatureCompensationMultiplier * getAverageTemperature())));
  return (sensorCalculation / (temperatureCompensationConstant - (temperatureCompensationMultiplier * instantTemperature)));
}

///////////////////////////////////////////////////////
//////////////// Temperature functions ////////////////
///////////////////////////////////////////////////////

double readTemperatureSensor() {
  double voltage = ((double)analogRead(temperatureSensorPin) * (5.0 / 1023.0));
  double temperature = ((voltage - 0.5) * 100.0);

  if(debug) {
    Serial.print("temperature: ");
    Serial.println(temperature);
  }

  return temperature;
}

double getInstantTemperature() {
  double sensorVal = readTemperatureSensor();
  temperatureReadings = temperatureReadings + sensorVal;
  temperatureCounter = temperatureCounter + 1L;
  return sensorVal;
}

double getAverageTemperature() {
  double averageValue = temperatureReadings/(double)temperatureCounter;

  Serial.print("Average Temperature: ");
  Serial.println(averageValue);

  return averageValue;
}

void resetAverageTemperature() {
  temperatureCounter = 0L;
  temperatureReadings = 0.0;
}


///////////////////////////////////////////////////////
///////////////// Pressure functions //////////////////
///////////////////////////////////////////////////////

double getInstantPressure(double instantTemperature) {
  double sensorVal = readPressureSensor(instantTemperature);
  pressureReadings = pressureReadings + sensorVal;
  pressureCounter = pressureCounter + 1L;
  return sensorVal;
}

void resetAveragePressure() {
  pressureCounter = 0L;
  pressureReadings = 0.0;
}

double getAveragePressure() {
  double averageValue = (double)pressureReadings/(double)pressureCounter;

  Serial.print("Average Pressure: ");
  Serial.println(averageValue);

  return averageValue;
}

double readPressureSensor(double instantTemperature) {
  double voltage = ((double)analogRead(pressureSensorPin) * (pressureVoltageSupply / 1023.0));

  double pressure = ((voltage / pressureVoltageSupply) + pressureCompensation2) / pressureCompensation1;
  pressure = (pressure - getErrorFactor(instantTemperature)) * kpaToMillibarsMultiplier;

  if(debug) {
    Serial.print("pressure: ");
    Serial.println(pressure);
  }

  return pressure;
}

double getErrorFactor(double instantTemperature) {
    double temperatureCompensation = pressureTempErrorNormal;
    if( instantTemperature < 0.0 || instantTemperature > 85.0)
    {
      if(instantTemperature < 0.0)
      {
          double difference = instantTemperature - (instantTemperature*2);
          temperatureCompensation = (difference * (2.0/40.0)) + 1.0;
      } 
      else {
          double difference = instantTemperature - 85.0;
          temperatureCompensation = (difference * (2.0/40.0)) + 1.0;
      }
    }
    double errorFactor = (pressureError * temperatureCompensation * pressureCompensation1 * pressureVoltageSupply);
    return errorFactor;
}
