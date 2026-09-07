/*
  ===========================================================================
  مشروع التحكم بالذراع الآلية عبر الإنترنت باستخدام ESP32 وبروتوكول MQTT
  ===========================================================================
  المكونات المستهدفة:
  - ESP32 DevKit V1
  - PCA9685 PWM Servo Driver (I2C Address: 0x40)
  - محركات السيرفو (Base, ArmX [x2], ArmY, Wrist/Servo4, Gripper)
  
  التوصيلات الهاردويرية (I2C):
  - ESP32 Pin 21 -> PCA9685 SDA
  - ESP32 Pin 22 -> PCA9685 SCL
  - ESP32 GND    -> PCA9685 GND
  - ESP32 3.3V   -> PCA9685 VCC (تغذية الشريحة البرمجية)
  - مصدر تغذية خارجي (5V/6V) -> PCA9685 V+ & GND (تغذية المحركات)
  ===========================================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ===== 1. إعدادات الواي فاي والإنترنت =====
const char* ssid     = "ZM070_5G_EXT";     // اسم شبكة الواي فاي
const char* password = "MmM05055437";     // كلمة سر الواي فاي

// ===== 2. إعدادات خادم MQTT (HiveMQ Public Broker) =====
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;

// ===== 3. قناة التحكم (Topic) =====
// يجب أن يطابق تماماً المتغير topic في كود صفحة الـ HTML
const char* topic_control = "ZM070_5G_EXT/control";

// ===== 4. كائنات وإعدادات النبضات =====
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// حدود نبضات السيرفو القياسية (تُعدل حسب نوع السيرفو المستخدم)
#define SERVOMIN 150  // نبضة الزاوية 0 درجة
#define SERVOMAX 600  // نبضة الزاوية 180 درجة

// ===== 5. دالة تحويل الزوايا (0-180) إلى نبضات PWM =====
int angleToPulse(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

// ===== 6. دالة استقبال الرسائل القادمة عبر MQTT وتوجيهها للمحركات =====
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // طباعة النص الخام القادم من الويب على Serial Monitor
  Serial.print(" Raw Message Received: ");
  Serial.println(message);

  // تحليل صيغة الرسالة (مثال: "armX:120" أو "base:90")
  int colonIndex = message.indexOf(':');
  if (colonIndex != -1) {
    String name = message.substring(0, colonIndex);
    int val = message.substring(colonIndex + 1).toInt();
    int pulse = angleToPulse(val);

    // توجيه الأمر للمحرك المحدد وطباعة النتيجة
    if (name == "base") {
      pwm.setPWM(0, 0, pulse);
      Serial.printf(" [Base Servo]   -> Pin 0 | Angle: %d° | Pulse: %d\n", val, pulse);
    }
    else if (name == "armX") {
      pwm.setPWM(1, 0, pulse);
      pwm.setPWM(2, 0, pulse);
      Serial.printf(" [ArmX Servos] -> Pins 1 & 2 | Angle: %d° | Pulse: %d\n", val, pulse);
    }
    else if (name == "armY") {
      pwm.setPWM(3, 0, pulse);
      Serial.printf(" [ArmY Servo]  -> Pin 3 | Angle: %d° | Pulse: %d\n", val, pulse);
    }
    else if (name == "servo4") {
      pwm.setPWM(4, 0, pulse);
      Serial.printf(" [Wrist Servo] -> Pin 4 | Angle: %d° | Pulse: %d\n", val, pulse);
    }
    else if (name == "grip") {
      pwm.setPWM(5, 0, pulse);
      Serial.printf(" [Gripper]     -> Pin 5 | Angle: %d° | Pulse: %d\n", val, pulse);
    }
    else {
      Serial.println(" Unknown Servo Command!");
    }
    Serial.println("------------------------------------------------");
  }
}

// ===== 7. دالة الاتصال وإعادة الاتصال بخادم MQTT =====
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // إنشاء Client ID عشوائي لضمان استقرار الاتصال بالخادم المجاني
    String clientId = "ESP32ArmClient-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" Connected Successfully!");
      // الاشتراك في قناة التحكم لاستقبال الأوامر
      client.subscribe(topic_control);
      Serial.print(" Subscribed to Topic: ");
      Serial.println(topic_control);
      Serial.println("================================================");
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 3 seconds...");
      delay(3000);
    }
  }
}

// ===== 8. دالة الإعداد الإبتدائي (Setup) =====
void setup() {
  // فتح الشاشة التسلسلية لقرائتها برمجياً (Baud Rate: 115200)
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- Starting ESP32 Robot Arm Telecontrol ---");

  // تهيئة بروتوكول I2C لشريحة PCA9685 على الدبابيس (SDA 21, SCL 22)
  Wire.begin(21, 22, 400000);
  pwm.begin();
  pwm.setPWMFreq(50); // تردد تشغيل محركات السيرفو القياسية (50 Hz)

  // الاتصال بشبكة الواي فاي المنزلية
  Serial.print("Connecting to WiFi Network: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ضبط إعدادات خادم MQTT ودالة الاستجابة
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ===== 9. الحلقة الرئيسية (Loop) =====
void loop() {
  // التأكد المستمر من الاتصال بالخادم
  if (!client.connected()) {
    reconnect();
  }
  
  // المحافظة على معالجة واستقبال الرسائل لحظياً
  client.loop();
}
