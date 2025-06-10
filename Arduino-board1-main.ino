#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_VL53L0X.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <SoftwareSerial.h>

#define buzzer 3
#define lcdAddress 0x27  // กำหนดขาต่อจอ LCD
#define I2CADDR 0x21

SoftwareSerial ser(rxPin, txPin);  // การใช้งานการสื่อสาร Serial ผ่านพอร์ตเสมือน

const byte ROWS = 4;  // จำนวน Rows ของแป้นกด
const byte COLS = 4;  // จำนวน Columns ของแป้นกด

// แผนผังปุ่มกด 4x4
char keys[COLS][ROWS] = {
  { 'D', 'C', 'B', 'A' },
  { '#', '9', '6', '3' },
  { '0', '8', '5', '2' },
  { '*', '7', '4', '1' }
};

// กำหนดขาของแถวและคอลัมน์ของแป้นกด
byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6, 7 };

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR, PCF8574);  // การเชื่อมต่อแป้นกดผ่าน I2C
LiquidCrystal_I2C lcd(lcdAddress, 20, 4);  // การเชื่อมต่อจอ LCD ผ่าน I2C

// ตัวแปรสำหรับเก็บข้อมูลที่ใช้
int heightValue = 0;
float weightValue;
float bmiValue = 0;
String ReceiveDate = "";
String idStr = "";
bool inputComplete = false;
bool enteringId = true;

float ReceiveLoadcellFromBoard_2;

Adafruit_VL53L0X lox = Adafruit_VL53L0X();  // เซ็นเซอร์ TOF

// กำหนด pins สำหรับเซ็นเซอร์น้ำหนัก
const int HX711_dout = A1;
const int HX711_sck = A0;

void setup() {
  Wire.begin();  // เริ่มการสื่อสาร I2C
  Serial.begin(115200);  // เริ่มต้น Serial Monitor
  keypad.begin(makeKeymap(keys));  // เริ่มต้นแป้นกด

  lcd.begin(20, 4);  // เริ่มต้นจอ LCD ขนาด 20x4
  lcd.backlight();
  lcd.clear();

  pinMode(buzzer, OUTPUT);  // กำหนด pin ของ buzzer
  pinMode(rxPin, INPUT);  // Pin รับข้อมูล
  pinMode(txPin, OUTPUT);  // Pin ส่งข้อมูล
  ser.begin(115200);  // เริ่มการสื่อสารผ่าน SoftwareSerial

  // เริ่มต้นการทำงานของเซ็นเซอร์วัดระยะทาง (TOF)
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    lcd.print("Dist Sensor Failed!");
    while (1);  // หยุดการทำงานถ้าเซ็นเซอร์ไม่พร้อม
  }

  Serial.println(F("VL53L0X Ready!"));
  lcd.clear();

  Serial.println("Receiver Ready");
  displayData();  // แสดงข้อมูลเบื้องต้นบนจอ LCD
}

// ฟังก์ชันวัดระยะทางจากเซ็นเซอร์ TOF
int DistanceMeasure() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);  // วัดระยะทางโดยไม่ใช้ high-speed mode
  float maxHigh = 200;  // กำหนดค่าความสูงสุดที่วัดได้ 200 cm
  if (measure.RangeStatus != 4) {  // ถ้าไม่มีการตอบสนองจากเซ็นเซอร์
    float distanceCm = measure.RangeMilliMeter / 10.0;  // แปลงหน่วยมิลลิเมตรเป็นเซนติเมตร
    float f_high = maxHigh - distanceCm;  // คำนวณความสูงของวัตถุ
    return f_high;
  } else {
    return 0;  // กรณีไม่สามารถวัดระยะได้
  }
}

// ฟังก์ชันคำนวณ BMI
float calculateBMI(float weight, float height) {
  float heightInMeters = height / 100.0;  // แปลงความสูงเป็นเมตร
  float bmi = weight / (heightInMeters * heightInMeters);  // สูตรคำนวณ BMI
  return bmi;
}

// ฟังก์ชันสำหรับการทำงานของ buzzer
void playBuzzer(char key) {
  switch (key) {
    case '0': tone(buzzer, 1500); break;  // ความถี่ 1500 Hz สำหรับปุ่มตัวเลข
    case 'A': 
      for (int i = 0; i < 2 ; i++){
        tone(buzzer, 2000);  // ความถี่ 2000 Hz สำหรับปุ่ม A
        delay(100);
        noTone(buzzer);
        delay(100);
      } break;
    case 'C': tone(buzzer, 2100); break;  // ความถี่ 2100 Hz สำหรับปุ่ม C
  }
  delay(100);  // ดีเลย์การทำงานของ buzzer
  noTone(buzzer);  // ปิด buzzer
}

void loop() {
  char key = keypad.getKey();  // ตรวจจับการกดปุ่ม

  if (Serial.available() > 0) {  // ตรวจสอบข้อมูลจากบอร์ดอื่น
    String receivedData = Serial.readStringUntil('\n');  // อ่านข้อมูลจนเจอตัว '\n'
    int commaIndex = receivedData.indexOf(',');  // แยกข้อมูลด้วย comma

    if (commaIndex > 0) {
      ReceiveDate = receivedData.substring(0, commaIndex);  // แยกวันที่
      String weightStr = receivedData.substring(commaIndex + 1);  // แยกน้ำหนัก
      ReceiveLoadcellFromBoard_2 = weightStr.toFloat();  // แปลงค่าน้ำหนักเป็น float
    } else {
      Serial.println("Invalid data format");  // กรณีข้อมูลไม่ถูกต้อง
    }
  }
    
  if (key != NO_KEY) {  // ถ้ามีการกดปุ่ม
    playBuzzer(key);  // เรียกใช้ฟังก์ชัน buzzer

    if (!inputComplete) {
      switch (key) {
        case 'A':  // กดปุ่ม A เพื่อบันทึกค่า
          if (idStr.length() == 10) {  // ตรวจสอบ ID ครบ 10 หลัก
            weightValue = ReceiveLoadcellFromBoard_2;  // รับค่าน้ำหนักจากบอร์ดอื่น
            heightValue = DistanceMeasure();  // วัดค่าความสูง
            bmiValue = calculateBMI(weightValue, heightValue);  // คำนวณค่า BMI
            saveData();  // บันทึกข้อมูล
            inputComplete = true;
            delay(3000);

            // รีเซ็ตตัวแปรหลังบันทึกข้อมูลเสร็จ
            idStr = "";
            inputComplete = false;
            enteringId = true;
            lcd.clear();
            displayData();  // แสดงข้อมูลบนจอ
          } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("ID must be 10 digits");  // แจ้งเตือนกรณี ID ไม่ครบ
            delay(2000);
            lcd.clear();
            displayData();
          }
          break;

        case 'C':  // รีเซ็ตค่าเมื่อกดปุ่ม C
          idStr = "";
          inputComplete = false;
          enteringId = true;
          lcd.clear();
          displayData();
          break;

        default:  // กรอก ID ผ่านแป้นกด
          if (isdigit(key) && idStr.length() < 10) {  // ตรวจสอบว่าปุ่มที่กดเป็นตัวเลขและ ID ยังไม่ครบ 10 หลัก
            idStr += key;
            displayData();  // แสดงข้อมูลที่กรอกบนจอ LCD
          }
          break;
      }
    }
  }
}

// ฟังก์ชันแสดงข้อมูลบนจอ LCD
void displayData() {
  lcd.setCursor(0, 0);
  lcd.print(ReceiveDate);  // แสดงวันที่

  lcd.setCursor(0, 2);
  lcd.print("ID: ");
  lcd.print(idStr);  // แสดง ID ที่กรอก

  lcd.setCursor(6, 3);
  lcd.print("A=Save");
  lcd.setCursor(13, 3);
  lcd.print("C=Reset");  // แสดงคำสั่งบนจอ LCD
}

// ฟังก์ชันบันทึกข้อมูลและแสดงผล
void saveData() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID: ");
  lcd.print(idStr);  // แสดง ID

  lcd.setCursor(0, 1);
  lcd.print("Height: ");
  lcd.print(heightValue);
  lcd.print(" cm");  // แสดงความสูง

  lcd.setCursor(0, 2);
  lcd.print("Weight: ");
  lcd.print(weightValue);
  lcd.print(" Kg");  // แสดงน้ำหนัก

  lcd.setCursor(0, 3);
  lcd.print("BMI: ");
  lcd.print(bmiValue);  // แสดงค่า BMI

  saveToStorage(idStr, heightValue, weightValue, bmiValue, ReceiveDate);  // ส่งข้อมูลไปยัง Serial Monitor
}

// ฟังก์ชันแสดงผลใน Serial Monitor
void saveToStorage(String idStr, float f_high, float weight, float bmi, String ReceiveDate) {
  // แสดงข้อมูลบน Serial Monitor
  Serial.print(idStr);
  Serial.print(",Height: ");
  Serial.print(f_high);
  Serial.print(",Weight: ");
  Serial.print(weight);
  Serial.print(",BMI: ");
  Serial.print(bmi);
  Serial.print(",Date: ");
  Serial.println(ReceiveDate);
}
