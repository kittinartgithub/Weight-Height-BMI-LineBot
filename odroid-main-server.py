from pymongo import MongoClient
import serial
import time
from flask import Flask, request, jsonify
from linebot import LineBotApi, WebhookHandler
from linebot.exceptions import InvalidSignatureError
from linebot.models import MessageEvent, TextMessage, TextSendMessage
import re
# ตั้งค่า Flask App
app = Flask(_name_)

# ตั้งค่าพอร์ตอนุกรมสำหรับการเชื่อมต่อ Arduino
arduino_port = '/dev/ttyS1'  # ตรวจสอบพอร์ตอนุกรมของ Arduino ในระบบของคุณ
baud_rate = 115200  # ตั้งค่า Baud Rate ให้ตรงกับ Arduino
ser = serial.Serial(arduino_port, baud_rate, timeout=1)

# เชื่อมต่อกับ MongoDB
client = MongoClient('mongodb://localhost:27017/')
db = client['user_database']
collection = db['user_data']

# ตั้ง Line Bot Access Token และ Channel Secret
line_bot_api = LineBotApi('/qJBmdLwS1iBRe2FVxdfOhVQB7mFeogNOuERE3xg0AjXGgncqRozSv2Bf1QxCPzcsa3+4rR/HAJzTRpfADSzVQ50nBLOM/rovJUJ55AKwLVZurD9TpO9OSNhXFIhDQgcVxz6Xza/Frd0qccAnZdkfQdB04t89/1O/w1cDnyilFU=')    #ใส่ Access Token
handler = WebhookHandler('e801a98ce752223b93c8dda035fb028c')  #ใส่  channel Secret 

# ฟังก์ชันสำหรับเพิ่มข้อมูล ID, Height, Weight
def insert_user_data(user_id, height, weight, bmi, date):
    user_document = {
        "user_id": user_id,
        "height": height,
        "weight": weight,
        "bmi": bmi,
        "date": date
    }
    collection.insert_one(user_document)
    print(f"Data inserted: {user_document}")

# ฟังก์ชันสำหรับรับข้อมูลจาก Arduino
def read_from_arduino():
    while True:
        if ser.in_waiting > 0:  # ตรวจสอบว่ามีข้อมูลจาก Arduino หรือไม่
            line = ser.readline().decode('utf-8', errors='ignore').strip()  # อ่านข้อมูลจาก Arduino
            if line:
                print(f"Received from Arduino: {line}")
                data = line.split(",")  # สมมติว่า Arduino ส่งข้อมูลเป็น 'ID123,170,65'
                if len(data) == 5:  # ตรวจสอบว่ามีข้อมูล 3 ส่วนหรือไม่
                    user_id = data[0]  # ID
                    height = data[1]  # Height
                    weight = data[2]  # Weight
                    bmi = data[3]  # BMI
                    date = data[4]
                    insert_user_data(user_id, height, weight, bmi, date)  # บันทึกข้อมูลลง MongoDB
        time.sleep(1)  # พักเล็กน้อยเพื่อป้องกันการใช้ CPU มากเกินไป
# ฟังก์ชันสำหรับค้นหาข้อมูลตาม ID
@app.route('/find_user', methods=['POST'])
def find_user():
    user_id = request.json.get('user_id')  #รับ User id จากคำขอ
    user_data = collection.find_one({"user_id": f"{user_id}"}) # ค้นหาข้อมูลใน MongoDB
    if user_data:
        response = jsonify({
                "user_id": user_data['user_id'],
                "height": user_data['height'],
                "weight": user_data['weight'],
                "bmi" : user_data['bmi'],
                "date": user_data['date']
        })
        response.headers['Content-Type'] = 'application/json; charset=utf-8'
        return response
    else: 
        response = jsonify({"message": "Not found ID"})
        response.headers['Content-Type'] = 'application/json; charset=utf-8'
        return response, 404

# ฟังก์ชัน Line Webhook
@app.route('/callback', methods=['POST'])
def callback():
    signature = request.headers['X-Line-Signature']
    body = request.get_data(as_text=True)
    
    try:
        handler.handle(body, signature)
       
    except InvalidSignatureError:
            return 'Invalid signature', 400
            
    return 'OK'


# ฟังก์ชันตอบกลับข้อความจากผู้ใช้งานผ่าน Line
@handler.add(MessageEvent, message=TextMessage)
def haddle_message(event):    
    user_id = event.message.text
    
    # ค้นหา user_id ที่ตรงกันทั้งหมดใน MongoDB
    user_data_list = list(collection.find({"user_id": f"{user_id}"}).limit(3))  # แปลง cursor เป็นลิสต์
    
    if len(user_data_list) > 0:  # ตรวจสอบว่าพบข้อมูลหรือไม่
        reply_message = ""
        for user_data in user_data_list:
            bmi_str = user_data['bmi']
            bmi_value = float(re.findall(r"\d+\.\d+", bmi_str)[0])

            if bmi_value < 18.50:
                bmi_status = "น้ำหนักน้อย"
            elif 18.50 <= bmi_value <= 22.99:
                bmi_status = "ปกติ (สุขภาพดี)"
            elif 23 <= bmi_value <= 24.99:
                bmi_status = "ท้วม"
            elif 25 <= bmi_value <= 29.99:
                bmi_status = "อ้วน"
            else:
                bmi_status = "อ้วนมาก"
            
            # สร้างข้อความตอบกลับรวม 3 รายการ
            reply_message += f"ID: {user_data['user_id']}\n{user_data['height']} cm\n{user_data['weight']} kg\n{user_data['bmi']} ({bmi_status})\n{user_data['date']}\n\n"
        
        # ตัดข้อความสุดท้ายที่เป็นบรรทัดว่างออก
        reply_message = reply_message.strip()
    else:
        reply_message = "ID not found"
    
    # ส่งข้อความตอบกลับ
    line_bot_api.reply_message(
        event.reply_token,
        TextSendMessage(text=reply_message)
    )
# รันฟังก์ชันอ่านข้อมูลจาก Arduino ในลูปตลอดเวลา และเปิด API
if __name__ == "__main__":
    print("Waiting for data from Arduino...")
    try:
        import threading
        arduino_thread = threading.Thread(target=read_from_arduino)
        arduino_thread.start()
        #read_from_arduino()  # รับข้อมูลจาก Arduino ตลอดเวลา
        
        # รัน Flask API
        app.run(debug=True, use_reloader=False) #เปิด API สำหรับค้นหาข้อมู,
    except KeyboardInterrupt:
        print("Stopped by user.")
