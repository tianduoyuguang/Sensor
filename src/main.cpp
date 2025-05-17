#include <Arduino.h>
#define BLINKER_WIFI

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Blinker.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include "SGP30.h"

#define seaLevelPressure_Pa 101325 // 标准大气压

// blinker连接秘钥与wifi名称密码
char auth[] = "96cec1651255";   
char ssid[] = "zhangge";          //非本人wifi
char pswd[] = "zhangge007";

DHT dht(D5, DHT22);
SGP mySGP30;
Adafruit_BMP085 bmp; // scl D1  ,sda D2
WiFiClient wc;
PubSubClient pc(wc);
// 新建组件对象
BlinkerNumber HUMI("humi");
BlinkerNumber TEMP("temp");
BlinkerNumber PRES("pres");
BlinkerNumber ALTI("alti");
BlinkerNumber CO2("num-co2");
BlinkerNumber TVOC("num-tvoc");
BlinkerText TEXT1("tex-s");

uint32_t read_time = 0;
u16 CO2Data, TVOCData; // 定义CO2浓度变量与TVOC浓度变量
u32 sgp30_dat;

float humi_read, temp_read, pres_read, alti_cal, air_ad, sum_ad;
std::string text = "";

// 参数一为WIFI名称,参数二为WIFI密码,参数三为设定的最长等待时间
void connectWifi(const char *wifiName, const char *wifiPassword, uint8_t waitTime)
{
    WiFi.mode(WIFI_STA);    // 设置无线终端模式
    WiFi.disconnect();      // 清除配置缓存
    WiFi.begin(ssid, pswd); // 开始连接
    uint8_t count = 0;
    while (WiFi.status() != WL_CONNECTED)
    { // 没有连接成功之前等待
        delay(1000);
        Serial.printf("connect WIFI...%ds\r\n", ++count);
        if (count >= waitTime)
        { // 超过设定的等待时候后退出
            Serial.println("connect WIFI fail");
            return;
        }
    }
    // 连接成功,输出打印连接的WIFI名称以及本地IP
    Serial.printf("connect WIFI %s success,local IP is %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

uint8_t connectMQTT()
{
    if (WiFi.status() != WL_CONNECTED)
        return -1;                        // 如果网没连上,那么直接返回
    pc.setServer("broker.emqx.io", 1883); // 设置MQTT服务器IP地址以及端口(一般固定是1883)
    if (!pc.connect(WiFi.macAddress().c_str()))
    { // 以物理地址为ID去连接MQTT服务器
        Serial.println("connect MQTT fail");
        return -1;
    }

    Serial.println("connect MQTT success");
    return 0;
}

String rts() // 计算运行时间
{
    int rt = Blinker.runTime();
    int r, e, f, s;
    String fh;
    Blinker.delay(100);
    if (rt >= 86400) // 天数
    {
        r = rt / 86400;
        e = rt / 3600 - r * 24;
        f = rt / 60 - r * 1440 - e * 60;
        s = rt - r * 86400 - e * 3600 - f * 60;
    }
    else if (rt >= 3600)
    {
        r = 0;
        e = rt / 3600;
        f = rt / 60 - e * 60;
        s = rt - e * 3600 - f * 60;
    }
    else
    {
        r = 0;
        e = 0;
        f = rt / 60;
        s = rt - f * 60;
    }

    // BLINKER_LOG(r," ",e," ",f," ",s);//输出数据测试

    if (f == 0 & e == 0 & r == 0)
    {
        fh = String("") + s + "秒";
    }
    else if (r == 0 & e == 0)
    {
        fh = String("") + f + "分" + s + "秒";
    }
    else if (r == 0)
    {
        fh = String("") + e + "时" + f + "分" + s + "秒";
    }
    else
    {
        fh = String("") + r + "天" + e + "时" + f + "分" + s + "秒";
    }

    return (fh);
}

void dataRead(const String &data)
{
    BLINKER_LOG("Blinker readString: ", data);

    Blinker.vibrate();

    uint32_t BlinkerTime = millis();

    Blinker.print("millis", BlinkerTime);
}

void heartbeat() // 心跳
{
    HUMI.print(humi_read);
    TEMP.print(temp_read);
    PRES.print(pres_read);
    ALTI.print(alti_cal);
    CO2.print(CO2Data);
    TVOC.print(TVOCData);
    TEXT1.print(rts());
}

void setup()
{
    // 初始化串口，并开启调试信息
    Serial.begin(115200);
    // 初始化blinker
    Blinker.begin(auth, ssid, pswd);
    Blinker.attachData(dataRead);
    Blinker.attachHeartbeat(heartbeat); // 附加心跳
    connectWifi(ssid, pswd, 10);
    connectMQTT();

    dht.begin();
    bmp.begin();

    mySGP30.SGP30_Init();
    mySGP30.SGP30_Write(0x20, 0x08);
    sgp30_dat = mySGP30.SGP30_Read(); // 读取SGP30的值
    CO2Data = (sgp30_dat & 0xffff0000) >> 16;
    TVOCData = sgp30_dat & 0x0000ffff;
}

void loop()
{
    // put your main code here, to run repeatedly:
    Blinker.run();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float p = bmp.readPressure();
    float a = bmp.readAltitude(seaLevelPressure_Pa);

    mySGP30.SGP30_Write(0x20, 0x08);
    sgp30_dat = mySGP30.SGP30_Read();         // 读取SGP30的值
    CO2Data = (sgp30_dat & 0xffff0000) >> 16; // 取出CO2浓度值
    TVOCData = sgp30_dat & 0x0000ffff;        // 取出TVOC值
    Serial.print("CO2:");
    Serial.print(CO2Data, DEC);
    Serial.println("ppm");
    Serial.print("TVOC:");
    Serial.print(TVOCData, DEC);
    Serial.println("ppd");
    if (isnan(h) || isnan(t) || isnan(p) || isnan(a) /*|| isnan(CO2Data)*/)
    {
        BLINKER_LOG("Failed to read from sensor!");
        Serial.println("Failed to read from sensor!");
    }
    else
    {
        humi_read = h;
        temp_read = t;
        pres_read = p;
        if (a > 40)
            alti_cal = a;
        else
            alti_cal = 40;
        Serial.print("tempread");
        Serial.println(temp_read);
    }

    text = std::to_string(temp_read) + "," + std::to_string(humi_read) + "," + std::to_string(pres_read) + "," + std::to_string(alti_cal) + "," + std::to_string(CO2Data) + "," + std::to_string(TVOCData);
    Serial.print("altitude:");
    Serial.println(alti_cal);

    if (pc.connected())
    {                             // 如果还和MQTT服务器保持连接
        pc.loop();                // 发送心跳信息
        String topic = "tempsss"; // 给主题名为物理地址+"-send"的主题发送信息.
        pc.publish(topic.c_str(), text.c_str());
    }
    else
    {
        connectMQTT(); // 如果和MQTT服务器断开连接,那么重连
    }
    delay(3000);
}