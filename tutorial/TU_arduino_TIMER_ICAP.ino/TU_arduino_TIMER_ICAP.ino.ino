#define TIMEOUT 1000000UL

const int trigPin = 10;   // Trigger pin : PWM out
const int echoPin = 12;    // Echo pin : Interrupt in

unsigned long duration;
float distance;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);
}

void loop() {
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  delayMicroseconds(10);

  duration = pulseIn(echoPin, HIGH, TIMEOUT);
  distance = (float)duration / 58.0;

  Serial.printf("duration : %d Distance : ", duration);
  Serial.print(distance);
  Serial.println(" [cm]");
  delay(500);
}