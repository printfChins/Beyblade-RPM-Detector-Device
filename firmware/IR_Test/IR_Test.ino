#define LED_GPIO                       8
#define IR1_GPIO                       0
#define IR2_GPIO                       1
#define digitalToggle(x) digitalWrite(x,!digitalRead(x));

void setup() {
  pinMode(LED_GPIO, OUTPUT);
  pinMode(IR1_GPIO, INPUT);
  pinMode(IR2_GPIO, INPUT);

  digitalWrite(LED_GPIO, LOW);

}

int ls_IR1 = -1;
int ls_IR2 = -1;
void loop() {
  // put your main code here, to run repeatedly:
  int IR1 = digitalRead(IR1_GPIO);
  int IR2 = digitalRead(IR2_GPIO);
  if(IR1!=ls_IR1 || IR2!=ls_IR2){
    digitalToggle(LED_GPIO);
    ls_IR1 = IR1;
    ls_IR2 = IR2;
  }
  delay(10);
}
