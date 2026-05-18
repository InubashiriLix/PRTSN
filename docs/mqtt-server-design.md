# ESP32-C3 MQTT Server 高内聚设计方案

本文档给 PRTN 项目中的 `MqttServer` 提供一版更适合学习和实现的设计。核心原则是：

```text
不要把 MQTT packet 拆得到处都是。
不要让 server 里充满裸字节解析。
不要自己实现 WiFiClient / WiFiServer。
```

第一版目标是实现一个轻量 MQTT broker：

- ESP32-C3 开 WiFi AP。
- 外部 MQTT client 连接 `192.168.4.1:1883`。
- 支持 MQTT 3.1.1。
- 支持 `CONNECT / CONNACK / PUBLISH / SUBSCRIBE / SUBACK / PINGREQ / PINGRESP / DISCONNECT`。
- 第一版只支持 QoS 0。
- 第一版只支持精确 topic 匹配。

## 1. 分层关系

整体结构：

```text
手机 / 电脑 MQTT Client
        |
        | WiFi + TCP
        v
ESP32-C3
  Wifi
    负责 AP / STA / AP_STA

  WiFiServer / WiFiClient
    Arduino ESP32 已经提供
    负责 TCP 监听、连接、读写字节

  MqttServer
    负责 MQTT broker 逻辑
    管理连接池、订阅表、消息转发

  MqttPacket
    负责 MQTT packet 的语义表达
    包含 header、body、类型、解析辅助结构

  NodeController
    周期调用 Wifi 和 MqttServer
    处理业务 topic，例如 LED 控制
```

关键边界：

- `Wifi` 不知道 MQTT。
- `MqttServer` 不负责启动 WiFi AP。
- `MqttServer` 拥有 `WiFiServer`。
- `MqttServer` 拥有所有 MQTT client connection。
- `ClientConnection` 拥有一个 `WiFiClient`。
- `MqttPacket` 表达协议语义，尽量高内聚。

## 2. 不实现 WiFiClient / WiFiServer

ESP32 Arduino 已经提供 TCP 层：

```cpp
#include <WiFi.h>

WiFiServer server(1883);
WiFiClient client = server.available();
```

所以本项目要实现的是：

```text
基于 WiFiClient / WiFiServer 的 MQTT 协议层
```

不是：

```text
TCP server / TCP client 本身
```

`WiFiServer` 的职责：

```text
监听端口，接受新 TCP 连接。
```

`WiFiClient` 的职责：

```text
读写 TCP 字节流，判断连接是否仍然存在。
```

`MqttServer` 的职责：

```text
解析 TCP 字节流里的 MQTT packet。
维护 MQTT client session。
维护 topic subscription。
转发 publish message。
```

## 3. 文件组织

为了保持高内聚，第一版建议只用这两个文件：

```text
src/svc/inc/MqttServer.h
src/svc/src/MqttServer.cpp
```

`MqttPacket` 可以作为 `MqttServer.h` 里的独立结构体或嵌套结构体，不要一开始拆出 `MqttPacket.h/.cpp`。

等功能稳定之后，如果文件太大，再考虑拆。

## 4. Config 设计

`Config` 是 server 配置，不是 packet。

不要让它继承 `Packet`。

建议放在 `MqttServer` 内部：

```cpp
class MqttServer
{
public:
    struct Config
    {
        uint16_t port = 1883;

        uint8_t maxClients = 4;
        uint8_t maxSubscriptions = 16;

        uint16_t maxPacketSize = 256;
        uint8_t maxTopicLength = 64;
        uint8_t maxClientIdLength = 32;

        uint32_t defaultKeepAliveTimeoutMs = 90000;
    };
};
```

这样语义清楚：

```text
MqttServer::Config 是 broker 的运行配置。
MqttPacket 是协议数据。
二者没有继承关系。
```

## 5. MqttPacket 高内聚设计

不要写空的 `Packet` 基类，也不要写：

```cpp
Packet data;
```

这会导致对象切片，子类字段会丢失。

更适合嵌入式 C++ 的设计是：

```text
一个 MqttPacket 总结构
内部包含 Header
内部包含各类 Body struct
用 union 保存 body
用 bodyKind 表示当前 body 类型
```

建议结构：

```cpp
struct MqttPacket
{
    enum class Type : uint8_t
    {
        CONNECT    = 1,
        CONNACK    = 2,
        PUBLISH    = 3,
        SUBSCRIBE  = 8,
        SUBACK     = 9,
        PINGREQ    = 12,
        PINGRESP   = 13,
        DISCONNECT = 14,
        UNKNOWN    = 255,
    };

    enum class BodyKind : uint8_t
    {
        None,
        Connect,
        Publish,
        Subscribe,
    };

    struct Header
    {
        Type type = Type::UNKNOWN;
        uint8_t flags = 0;
        uint32_t remainingLength = 0;
    };

    struct Connect
    {
        char protocolName[8] = {};
        uint8_t protocolLevel = 0;
        uint8_t connectFlags = 0;
        uint16_t keepAliveSec = 0;
        char clientId[32] = {};
    };

    struct Publish
    {
        char topic[64] = {};
        const uint8_t* payload = nullptr;
        size_t payloadLen = 0;

        bool retain = false;
        bool dup = false;
        uint8_t qos = 0;
    };

    struct Subscribe
    {
        uint16_t packetId = 0;
        char topic[64] = {};
        uint8_t requestedQos = 0;
    };

    Header header;
    BodyKind bodyKind = BodyKind::None;

    union Body
    {
        Connect connect;
        Publish publish;
        Subscribe subscribe;

        Body() {}
        ~Body() {}
    } body;

    void clear() {
        header = {};
        bodyKind = BodyKind::None;
    }
};
```

这个设计的优点：

- MQTT packet 的所有语义集中在 `MqttPacket`。
- `MqttServer` 不需要知道一堆分散的 packet 类型。
- 没有虚函数。
- 没有动态分配。
- 适合 ESP32-C3。
- 调试时只看一个结构就知道 packet 内容。

## 6. Packet 生命周期

`MqttPacket::Publish::payload` 是指针，这里必须规定生命周期。

第一版建议这样设计：

```text
MqttServer 内部有一个 m_packetBuffer。
每次读取一个 MQTT packet 到 m_packetBuffer。
MqttPacket::Publish::payload 指向 m_packetBuffer 内部。
payload 指针只在当前 handlePacket() 调用期间有效。
handlePacket() 返回后，payload 指针不能继续保存。
```

示例：

```cpp
class MqttServer
{
private:
    uint8_t m_packetBuffer[256];
    MqttPacket m_packet;
};
```

如果业务层需要长期保存 payload，业务层必须自己复制。

这条规则很重要，否则后面会出现悬空指针问题。

## 7. ClientConnection 所有权

`ClientConnection` 表示一个 MQTT client session。

它应该由 `MqttServer` 分配和管理。

它内部持有一个 `WiFiClient`：

```cpp
struct ClientConnection
{
    WiFiClient tcp;

    bool used = false;
    bool mqttConnected = false;

    char clientId[32] = {};
    uint16_t keepAliveSec = 60;
    uint32_t lastSeenMs = 0;

    void attach(WiFiClient newTcp, uint32_t nowMs) {
        tcp = newTcp;
        used = true;
        mqttConnected = false;
        clientId[0] = '\0';
        keepAliveSec = 60;
        lastSeenMs = nowMs;
    }

    void close() {
        if (tcp) {
            tcp.stop();
        }

        used = false;
        mqttConnected = false;
        clientId[0] = '\0';
        keepAliveSec = 60;
        lastSeenMs = 0;
    }
};
```

初始化过程：

```text
1. MqttServer 构造 WiFiServer。
2. MqttServer::begin() 启动监听。
3. MqttServer::acceptClient() 调用 WiFiServer::available()。
4. 如果有新 WiFiClient，就从连接池找一个空 ClientConnection。
5. 调用 ClientConnection::attach(newTcp)。
6. 后续这个 WiFiClient 的生命周期由 ClientConnection 管理。
```

也就是说：

```text
WiFiServer 负责产生 WiFiClient。
MqttServer 负责接收 WiFiClient。
ClientConnection 负责持有 WiFiClient。
```

不要让 `NodeController` 持有 `WiFiClient`。

不要在业务层分配 `ClientConnection`。

## 8. MqttServer 内部结构

第一版可以这样：

```cpp
class MqttServer
{
public:
    struct Config;
    using MessageHandler = std::function<void(
        const char* topic,
        const uint8_t* payload,
        size_t payloadLen)>;

    explicit MqttServer(const Config& config);

    bool begin();
    void update();
    void stop();

    bool publish(const char* topic, const uint8_t* payload, size_t payloadLen);
    bool publish(const char* topic, const char* payload);

    void setMessageHandler(MessageHandler handler);

private:
    struct ClientConnection;
    struct Subscription;

    Config m_config;
    WiFiServer m_tcpServer;

    ClientConnection m_clients[4];
    Subscription m_subscriptions[16];

    uint8_t m_packetBuffer[256];
    MqttPacket m_packet;

    MessageHandler m_handler;
};
```

构造函数负责初始化 `WiFiServer` 的端口：

```cpp
MqttServer::MqttServer(const Config& config)
    : m_config(config),
      m_tcpServer(config.port)
{
}
```

`begin()` 负责开始监听：

```cpp
bool MqttServer::begin() {
    m_tcpServer.begin();
    return true;
}
```

## 9. Subscription 设计

第一版只支持精确 topic 匹配。

```cpp
struct Subscription
{
    bool used = false;
    uint8_t clientIndex = 0;
    char topic[64] = {};
};
```

添加订阅：

```text
找到空 slot
写入 clientIndex
写入 topic
```

关闭客户端时：

```text
删除所有 clientIndex 等于该客户端的 subscription
```

匹配：

```cpp
bool topicMatches(const char* subscribed, const char* published) {
    return strcmp(subscribed, published) == 0;
}
```

Wildcard 可以后面再加。

## 10. 主循环

`MqttServer::update()` 是非阻塞轮询：

```cpp
void MqttServer::update() {
    acceptClient();

    for (uint8_t i = 0; i < m_config.maxClients; ++i) {
        ClientConnection& conn = m_clients[i];

        if (!conn.used) {
            continue;
        }

        if (!conn.tcp.connected()) {
            closeClient(i);
            continue;
        }

        while (conn.tcp.available() > 0) {
            if (!readPacket(conn, m_packet)) {
                closeClient(i);
                break;
            }

            handlePacket(i, m_packet);
            m_packet.clear();
        }

        if (isTimedOut(conn)) {
            closeClient(i);
        }
    }
}
```

职责分解：

```text
acceptClient()
  接收新的 TCP client。

readPacket()
  从 WiFiClient 读取 MQTT fixed header 和 body。
  填充 MqttPacket。

handlePacket()
  根据 MqttPacket::Type 分发。

closeClient()
  停止 TCP，删除订阅，清理 slot。
```

## 11. Packet 读取流程

读取一个 MQTT packet 的流程：

```text
1. 读取 fixed header 第一个字节。
2. 解码 remaining length。
3. 检查 remaining length 是否超过 maxPacketSize。
4. 读取 remaining body 到 m_packetBuffer。
5. 根据 packet type 解析 body。
6. 填充 MqttPacket。
```

伪代码：

```cpp
bool MqttServer::readPacket(ClientConnection& conn, MqttPacket& packet) {
    uint8_t firstByte = 0;

    if (!readByte(conn.tcp, firstByte)) {
        return false;
    }

    packet.header.type = decodeType(firstByte >> 4);
    packet.header.flags = firstByte & 0x0F;

    if (!readRemainingLength(conn.tcp, packet.header.remainingLength)) {
        return false;
    }

    if (packet.header.remainingLength > m_config.maxPacketSize) {
        return false;
    }

    if (!readExact(conn.tcp, m_packetBuffer, packet.header.remainingLength)) {
        return false;
    }

    return parsePacketBody(packet, m_packetBuffer, packet.header.remainingLength);
}
```

`parsePacketBody()` 内部根据类型处理：

```cpp
bool MqttServer::parsePacketBody(
    MqttPacket& packet,
    const uint8_t* data,
    size_t len) {

    switch (packet.header.type) {
        case MqttPacket::Type::CONNECT:
            return parseConnect(packet, data, len);

        case MqttPacket::Type::PUBLISH:
            return parsePublish(packet, data, len);

        case MqttPacket::Type::SUBSCRIBE:
            return parseSubscribe(packet, data, len);

        case MqttPacket::Type::PINGREQ:
        case MqttPacket::Type::DISCONNECT:
            packet.bodyKind = MqttPacket::BodyKind::None;
            return len == 0;

        default:
            return false;
    }
}
```

这样 parser 没有散落在很多地方，仍然围绕 `MqttPacket` 和 `MqttServer`。

## 12. MQTT 字符串读取

MQTT 字符串格式：

```text
2 bytes length, big endian
N bytes content
```

辅助函数：

```cpp
bool readMqttString(
    const uint8_t* data,
    size_t len,
    size_t& offset,
    char* out,
    size_t outSize) {

    if (offset + 2 > len) {
        return false;
    }

    const uint16_t strLen =
        (static_cast<uint16_t>(data[offset]) << 8) |
        static_cast<uint16_t>(data[offset + 1]);

    offset += 2;

    if (offset + strLen > len) {
        return false;
    }

    if (strLen >= outSize) {
        return false;
    }

    memcpy(out, data + offset, strLen);
    out[strLen] = '\0';
    offset += strLen;

    return true;
}
```

这个函数会被 `parseConnect()`、`parsePublish()`、`parseSubscribe()` 复用。

## 13. CONNECT 处理

解析结果写入：

```cpp
packet.bodyKind = MqttPacket::BodyKind::Connect;
packet.body.connect = {};
```

需要验证：

```text
protocolName == "MQTT"
protocolLevel == 4
clientId 不为空
```

处理：

```cpp
void MqttServer::handleConnect(
    uint8_t clientIndex,
    const MqttPacket::Connect& connect) {

    ClientConnection& conn = m_clients[clientIndex];

    strncpy(conn.clientId, connect.clientId, sizeof(conn.clientId) - 1);
    conn.keepAliveSec = connect.keepAliveSec;
    conn.mqttConnected = true;
    conn.lastSeenMs = millis();

    sendConnAck(conn, true);
}
```

成功 CONNACK：

```text
20 02 00 00
```

## 14. PINGREQ 处理

收到 `PINGREQ`：

```cpp
void MqttServer::handlePingReq(uint8_t clientIndex) {
    ClientConnection& conn = m_clients[clientIndex];
    conn.lastSeenMs = millis();

    const uint8_t pingResp[] = {0xD0, 0x00};
    conn.tcp.write(pingResp, sizeof(pingResp));
}
```

## 15. SUBSCRIBE 处理

第一版只处理一个 topic。

解析结果：

```cpp
packet.bodyKind = MqttPacket::BodyKind::Subscribe;
packet.body.subscribe.packetId = packetId;
packet.body.subscribe.topic = topic;
packet.body.subscribe.requestedQos = requestedQos;
```

处理：

```cpp
void MqttServer::handleSubscribe(
    uint8_t clientIndex,
    const MqttPacket::Subscribe& subscribe) {

    addSubscription(clientIndex, subscribe.topic);
    sendSubAck(clientIndex, subscribe.packetId, 0);
}
```

SUBACK：

```text
90 03 [packet id msb] [packet id lsb] 00
```

最后的 `00` 表示授予 QoS 0。

## 16. PUBLISH 处理

解析结果：

```cpp
packet.bodyKind = MqttPacket::BodyKind::Publish;
packet.body.publish.topic = topic;
packet.body.publish.payload = data + payloadOffset;
packet.body.publish.payloadLen = payloadLen;
packet.body.publish.qos = 0;
packet.body.publish.retain = retain;
packet.body.publish.dup = dup;
```

处理：

```cpp
void MqttServer::handlePublish(
    uint8_t senderIndex,
    const MqttPacket::Publish& publish) {

    if (m_handler) {
        m_handler(publish.topic, publish.payload, publish.payloadLen);
    }

    forwardPublish(senderIndex, publish);
}
```

是否转发给发送者本身，可以作为策略决定。

简单起见，第一版建议：

```text
如果发送者也订阅了这个 topic，也转发给发送者。
```

这符合很多 broker 的行为，也方便测试。

## 17. 编码 PUBLISH

转发 PUBLISH 时需要重新编码 packet。

结构：

```text
fixed header:
  type = PUBLISH
  flags = 0

remaining length:
  2 + topic length + payload length

variable header:
  topic length MSB
  topic length LSB
  topic bytes

payload:
  payload bytes
```

辅助函数：

```cpp
bool writePublish(
    WiFiClient& client,
    const char* topic,
    const uint8_t* payload,
    size_t payloadLen);
```

`remaining length` 编码也要按 MQTT 标准写，不要只写一个字节。

## 18. handlePacket 分发

`handlePacket()` 应该只看语义，不直接解析裸字节：

```cpp
void MqttServer::handlePacket(uint8_t clientIndex, const MqttPacket& packet) {
    switch (packet.header.type) {
        case MqttPacket::Type::CONNECT:
            handleConnect(clientIndex, packet.body.connect);
            break;

        case MqttPacket::Type::PUBLISH:
            handlePublish(clientIndex, packet.body.publish);
            break;

        case MqttPacket::Type::SUBSCRIBE:
            handleSubscribe(clientIndex, packet.body.subscribe);
            break;

        case MqttPacket::Type::PINGREQ:
            handlePingReq(clientIndex);
            break;

        case MqttPacket::Type::DISCONNECT:
            closeClient(clientIndex);
            break;

        default:
            closeClient(clientIndex);
            break;
    }
}
```

这就是高内聚的关键：

```text
字节解析集中在 readPacket / parsePacketBody。
协议语义集中在 MqttPacket。
业务处理集中在 handleXXX。
连接生命周期集中在 ClientConnection。
```

## 19. 与 NodeController 集成

`NodeController` 应该持有 `MqttServer&`：

```cpp
class NodeController
{
private:
    Wifi& m_wifi;
    MqttServer& m_mqttServer;
};
```

setup：

```cpp
bool NodeController::setup() {
    if (!m_wifi.begin()) {
        return false;
    }

    if (!m_mqttServer.begin()) {
        return false;
    }

    m_mqttServer.setMessageHandler(
        [this](const char* topic, const uint8_t* payload, size_t len) {
            handleMqttMessage(topic, payload, len);
        });

    return createTask();
}
```

loop：

```cpp
void NodeController::ControlLoop() {
    TickType_t taskLastWakeTime = xTaskGetTickCount();

    for (;;) {
        m_wifi.updateByIntervalMs(400);

        if (m_wifi.apRunning() || m_wifi.staConnected()) {
            m_mqttServer.update();
        }

        m_ledAux.updateByIntervalMs(1000);

        vTaskDelayUntil(&taskLastWakeTime, m_period);
    }
}
```

## 20. 推荐实现顺序

按这个顺序做，比较稳：

```text
1. MqttServer 拥有 WiFiServer，能监听 1883。
2. ClientConnection 连接池，能接受和关闭 TCP client。
3. MqttPacket::Header 和 remaining length 解码。
4. CONNECT parser + CONNACK encoder。
5. PINGREQ + PINGRESP。
6. SUBSCRIBE parser + SUBACK encoder。
7. Subscription 表。
8. PUBLISH parser。
9. PUBLISH encoder + topic 转发。
10. MessageHandler 回调到 NodeController。
```

每一步都可以单独测。

## 21. 测试方式

ESP32 开 AP 后，电脑连接 AP。

假设 ESP32 AP IP 是：

```text
192.168.4.1
```

订阅：

```bash
mosquitto_sub -h 192.168.4.1 -p 1883 -t test/topic -d
```

发布：

```bash
mosquitto_pub -h 192.168.4.1 -p 1883 -t test/topic -m hello -d
```

期望：

```text
订阅端收到 hello。
串口能看到 CONNECT / SUBSCRIBE / PUBLISH 日志。
客户端长时间连接不会因为 PINGREQ 无响应而断开。
```

## 22. 第一版错误处理

第一版不用追求完整错误码。

建议策略：

```text
packet type 不支持: 断开客户端
remaining length 超限: 断开客户端
topic 太长: 断开客户端
client id 太长: 断开客户端
协议名不是 MQTT: 断开客户端
protocol level 不是 4: 断开客户端
QoS 不是 0: 断开客户端或拒绝
订阅表满: SUBACK failure 或断开客户端
```

对于学习和嵌入式稳定性来说，简单断开比半吊子恢复更好。

## 23. 后续扩展

基础版跑通后再加：

```text
1. topic wildcard: + 和 #
2. retained message
3. username / password
4. QoS 1: PUBACK
5. AP_STA 模式下桥接外部 broker
6. server 状态 topic
```

不要一开始做 TLS、QoS 2、持久 session。它们会把第一版复杂度拉得太高。

## 24. 最终设计评价

这版设计的核心是：

```text
MqttPacket 高内聚表达协议。
MqttServer 高内聚管理 broker。
ClientConnection 明确拥有 WiFiClient。
Config 是 server 配置，不伪装成 packet。
Packet payload 生命周期有明确规则。
```

这样的结构比“到处散落 Packet struct + Server 裸解析字节”更容易读，也更适合你后续边学 MQTT 边实现。
