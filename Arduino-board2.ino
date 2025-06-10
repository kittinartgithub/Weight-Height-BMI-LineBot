#include <Wire.h>
#include <RTClib.h>
#include <HX711_ADC.h>

// สร้างตัวแปรไว้ใช้กับโมดูล RTC (ตัวนับเวลาจริง)
RTC_DS3231 rtc;

// ขาเชื่อมต่อสำหรับโมดูลวัดน้ำหนัก HX711
const int HX711_dout = 4;  // ขาส่งข้อมูลจาก HX711
const int HX711_sck = 5;   // ขาสัญญาณนาฬิกาจาก HX711

// สร้างออบเจกต์เซ็นเซอร์ Load Cell (ใช้สำหรับวัดน้ำหนัก)
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// ตัวแปรสำหรับเก็บข้อมูลต่างๆ
float weightInKg = 0.0;  // เก็บน้ำหนักเป็นกิโลกรัม
String rtcData = "";  // เก็บข้อมูลเวลาจาก RTC

// ฟังก์ชันสำหรับตั้งค่าต่างๆ ตอนเริ่มทำงาน
void setup() {
  Serial.begin(115200);  // เปิด Serial เพื่อใช้สื่อสาร
  delay(10);  // หน่วงเวลานิดหน่อยให้ Serial ทำงานทัน

  // เริ่มต้นการทำงานของ RTC (ตัวจับเวลา)
  if (!rtc.begin()) {  // ถ้าเชื่อมต่อกับ RTC ไม่ได้
    Serial.println("not Connecting RTC");
    while (1);  // STOP Programs
  }

  // ถ้า RTC เคยหยุดทำงานจากการจ่ายไฟหลุด
  if (rtc.lostPower()) {
    Serial.println("RTC ถูกรีเซ็ต กำลังตั้งเวลาใหม่...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // ตั้งเวลาใหม่ให้ตรงกับคอมพิวเตอร์ตอนนี้
  }

  // เริ่มต้นการทำงานของ Load Cell (เซ็นเซอร์วัดน้ำหนัก)
  Serial.println("Loadcell Starting!");
  LoadCell.begin();  // เริ่มเซ็นเซอร์
  float calibrationFactor = 6.71;  // ตั้งค่าการคาลิเบรต (ตามน้ำหนักที่วัดจริง) ค่าอิงจากการต่อตัว Loadcell ทั้ง 4 มุม
  LoadCell.start(2000);  // 
  LoadCell.setCalFactor(calibrationFactor);  // ตั้งค่าคาลิเบรต
  LoadCell.tare();  // ตั้งค่าเริ่มต้น (ชั่งขณะไม่มีวัตถุ)

  Serial.println("Loadcell Ready!");
}

// ฟังก์ชันอ่านเวลาและวันที่จาก RTC
String readRTC() {
  DateTime now = rtc.now();  // อ่านเวลาปัจจุบันจาก RTC

  // สร้างสตริงข้อมูลเวลาในรูปแบบ "ปี/เดือน/วัน ชั่วโมง:นาที"
  String dateTime = "";
  dateTime += String(now.year(), DEC) + "/";

  if (now.month() < 10) {
    dateTime += "0";  
  }
  dateTime += String(now.month(), DEC) + "/";

  if (now.day() < 10) {
    dateTime += "0"; 
  }
  dateTime += String(now.day(), DEC) + " ";

  if (now.hour() < 10) {
    dateTime += "0";  
  }
  dateTime += String(now.hour(), DEC) + ":";

  if (now.minute() < 10) {
    dateTime += "0"; 
  }
  dateTime += String(now.minute(), DEC);

  return dateTime;
}

// ฟังก์ชันอ่านค่าน้ำหนักจาก Load Cell
float readLoadCell() {
  LoadCell.update();  // Update Load Cell
  float weight = LoadCell.getData();  // ดึงค่าน้ำหนักดิบที่ยังไม่จัดการค่า
  float weightInKg = weight / 1000.0;  // แปลง Weight (g) => Kg
  return weightInKg;  // Return Weight (Kg)
}

// Main loop()
void loop() {
  // อ่านข้อมูลเวลาจาก RTC และน้ำหนักจาก Load Cell
  rtcData = readRTC();
  weightInKg = readLoadCell();

  // สร้างข้อความที่จะส่งไปยังบอร์ดหลัก
  String sendData = rtcData + "," + String(weightInKg, 2);  // รูปแบบ "YYYY/MM/DD HH:MM,น้ำหนัก"

  // ส่งข้อมูลไปยัง Serial Monitor
  Serial.println(sendData);

  delay(185);  // Delay 1 second 
}
