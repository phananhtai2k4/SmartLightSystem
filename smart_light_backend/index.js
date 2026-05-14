require("dotenv").config();
const mqtt = require("mqtt");
const admin = require("firebase-admin");

// Khởi tạo Firebase Admin SDK
console.log("Đang cố gắng khởi tạo Firebase Admin SDK...");
try {
  admin.initializeApp({
    credential: admin.credential.cert({
      projectId: process.env.FIREBASE_PROJECT_ID,
      privateKey: process.env.FIREBASE_PRIVATE_KEY.replace(/\\n/g, "\n"),
      clientEmail: process.env.FIREBASE_CLIENT_EMAIL,
    }),
    databaseURL: process.env.FIREBASE_DB_URL,
  });
  console.log("Firebase Admin SDK đã khởi tạo thành công!");
  console.log("Kết nối đến RTDB URL:", process.env.FIREBASE_DB_URL);
} catch (error) {
  console.error("Lỗi khi khởi tạo Firebase Admin SDK:", error.message);
}

// Kết nối MQTT
const options = {
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD,
};
const client = mqtt.connect(process.env.MQTT_BROKER_URL, options);

console.log("Đang cố gắng kết nối đến:", process.env.MQTT_BROKER_URL);

client.on("connect", () => {
  console.log("Đã kết nối HiveMQ");
  client.subscribe(process.env.MQTT_TOPIC, (err) => {
    if (err) console.error("Lỗi khi subscribe:", err);
    else console.log("Subscribed đến topic:", process.env.MQTT_TOPIC);
  });
});

client.on("error", (err) => {
  console.error("Lỗi khi kết nối MQTT:", err.message);
});

client.on("message", (topic, message) => {
  const data = message.toString();
  console.log(`Nhận được từ ${topic}: ${data}`);
});
