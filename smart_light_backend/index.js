require("dotenv").config();
const mqtt = require("mqtt");
const admin = require("firebase-admin");

// Khởi tạo Firebase Admin SDK
console.log("Đang cố gắng khởi tạo Firebase Admin SDK...");
try {
  admin.initializeApp({
    credential: admin.credential.cert({
      projectId: process.env.FIREBASE_PROJECT_ID,
      privateKey: process.env.FIREBASE_PRIVATE_KEY.replace(
        /\\n/g,
        "\n",
      ).replace(/^"|"$/g, ""),
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

const db = admin.firestore();
const rtdb = admin.database();

async function findNodeByAddress(gatewayMac, meshAddress) {
  const nodesRef = db.collection("nodes");
  const snapshot = await nodesRef
    .where("gatewayMac", "==", String(gatewayMac))
    .where("meshAddress", "==", Number(meshAddress))
    .get();

  if (snapshot.empty) {
    return null;
  }
  return snapshot.docs[0].id;
}

const nodeDataBuffer = {};

client.on("message", async (topic, message) => {
  const data = message.toString();
  console.log(`Nhận được từ ${topic}: ${data}`);

  try {
    const payload = JSON.parse(data);
    const { name, devExtAddr } = payload;

    if (name === "CmdAlertOnline") {
      const { online } = payload;
      const gatewaysRef = db.collection("gateways").doc(String(devExtAddr));
      // Cập nhật trạng thái gateway trên Firestore
      await gatewaysRef
        .update({
          online: online === 1,
        })
        .catch((err) =>
          console.log(
            `Cập nhật gateway online thất bại (Có thể do doc chưa tồn tại):`,
            err.message,
          ),
        );
      console.log(
        `Đã cập nhật trạng thái Gateway ${devExtAddr} online: ${online === 1}`,
      );
    } else if (name === "CmdAlertData") {
      const { timestamp, address, light_lux, current_ma } = payload;
      const nodeMac = await findNodeByAddress(devExtAddr, address);

      if (nodeMac) {
        // 1. Cập nhật RTDB
        await rtdb.ref(`nodes/${nodeMac}/sensor`).set({
          light: light_lux,
          current: current_ma,
          updatedAt: timestamp,
        });

        // 2. Gom nhóm Cập nhật Firestore measurements
        const date = new Date(timestamp * 1000);
        // Reset giây và mili giây để tạo khoá phút
        const minuteDate = new Date(date);
        minuteDate.setSeconds(0, 0);

        const year = minuteDate.getFullYear();
        const month = String(minuteDate.getMonth() + 1).padStart(2, "0");
        const day = String(minuteDate.getDate()).padStart(2, "0");
        const hours = String(minuteDate.getHours()).padStart(2, "0");
        const minutes = String(minuteDate.getMinutes()).padStart(2, "0");
        const minuteId = `${year}-${month}-${day}T${hours}:${minutes}`;

        const newData = {
          t: date.getSeconds(),
          light: light_lux,
          current: current_ma,
        };

        if (!nodeDataBuffer[nodeMac]) {
          // Chưa có buffer cho node này thì khởi tạo
          nodeDataBuffer[nodeMac] = {
            minuteId: minuteId,
            minuteDate: minuteDate,
            data: [newData],
          };
        } else {
          if (nodeDataBuffer[nodeMac].minuteId !== minuteId) {
            // Cùng node nhưng sang phút mới -> Đẩy toàn bộ dữ liệu của phút trước lên Firestore
            const prevData = nodeDataBuffer[nodeMac];
            const docRef = db
              .collection("nodes")
              .doc(nodeMac)
              .collection("measurements")
              .doc(prevData.minuteId);

            const firestoreMinuteDate = prevData.minuteDate;
            const firestoreData = prevData.data;

            // Xử lý ghi Firestore không chặn luồng chính
            db.runTransaction(async (transaction) => {
              const docSnap = await transaction.get(docRef);
              if (!docSnap.exists) {
                transaction.set(docRef, {
                  minute:
                    admin.firestore.Timestamp.fromDate(firestoreMinuteDate),
                  data: firestoreData,
                });
              } else {
                transaction.update(docRef, {
                  data: admin.firestore.FieldValue.arrayUnion(...firestoreData),
                });
              }
            })
              .then(() => {
                console.log(
                  `[Firestore] Đã ghi gộp ${firestoreData.length} bản tin của Node ${nodeMac} (phút ${prevData.minuteId})`,
                );
              })
              .catch((err) => {
                console.error(`[Firestore] Lỗi ghi gộp Node ${nodeMac}:`, err);
              });

            // Khởi tạo lại buffer cho phút hiện tại
            nodeDataBuffer[nodeMac] = {
              minuteId: minuteId,
              minuteDate: minuteDate,
              data: [newData],
            };
          } else {
            // Cùng phút -> Nạp tiếp vào buffer
            nodeDataBuffer[nodeMac].data.push(newData);
          }
        }
        console.log(
          `Đã nạp data sensor vào buffer của Node ${nodeMac} (phút ${minuteId})`,
        );
      } else {
        console.log(
          `Không tìm thấy node với Address ${address} thuộc Gateway ${devExtAddr}`,
        );
      }
    } else if (name === "CmdAlertHistory") {
      const { timestamp, address, history_type, onoff } = payload;
      const nodeMac = await findNodeByAddress(devExtAddr, address);

      if (nodeMac) {
        const isOn = onoff === 1;

        // 1. Cập nhật thuộc tính nodes trên RTDB
        await rtdb.ref(`nodes/${nodeMac}`).update({
          online: true,
          state_on: isOn,
        });

        // 2. Cập nhật state và online trên Firestore
        await db.collection("nodes").doc(nodeMac).update({
          online: true,
          state_on: isOn,
        });

        // 3. Lưu mốc lịch sử event
        await db
          .collection("nodes")
          .doc(nodeMac)
          .collection("events")
          .add({
            timestamp: admin.firestore.Timestamp.fromMillis(timestamp * 1000),
            type: history_type,
            onoff: isOn,
          });
        console.log(`Đã lưu history của Node ${nodeMac}`);
      } else {
        console.log(
          `Không tìm thấy node với Address ${address} thuộc Gateway ${devExtAddr}`,
        );
      }
    }
  } catch (error) {
    if (error instanceof SyntaxError) {
      console.log("Bỏ qua message không phải là JSON format");
    } else {
      console.error("Lỗi khi xử lý MQTT message:", error);
    }
  }
});
