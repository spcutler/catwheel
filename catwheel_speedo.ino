#define ESP8266 1
//#define ESP32 1
#include <ESP8266WiFi.h>
#include "ESPTelnet.h"

#include <ezTime.h>
//#include <Fetch.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "catwheel.h"
#include "histogram.h"
#include "net.h"
#include "vars.h"

Timezone tz;
ESPTelnet telnet;

Led g_led;

int g_lastTimeUpdate = 0;
int g_lastDirUpdate = 0;
int g_lastSpeedUpdate = 0;

#define MAG0_PIN D6
#define MAG1_PIN D7

#define MM_PER_REV 3381
#define NUM_SEGMENTS 8
#define MM_PER_SEG (MM_PER_REV / NUM_SEGMENTS)

#define MMPS_IN_MPH 0.00223694f

#define SPEED_TIMEOUT 5000

void log(DynamicJsonDocument &json)
{
    post(g_googleAppsUrl, json);
}

void logStartup()
{
    DynamicJsonDocument json(1024);
    json["name"] = g_hostname;
    json["message"] = "startup";
    json["build"]["date"] = __DATE__;
    json["build"]["time"] = __TIME__;
    post(g_googleAppsUrl, json);
}

struct MagUpdate
{
    enum { COUNT = 16 };
    int lastSensor = 0;
    uint32_t ts[COUNT];
    uint32_t curIndex = 0;
    int totalHits = 0;
    int millisPerSeg = 0;
    int millisPerSegPeak = 1000000000;
    int segmentsSinceLastRun = 0;
    int millisSinceLastRun = 0;
    bool isClockwise = true;
    uint32_t lastUpdateTime = 0;

    enum 
    {
        HISTO_SLOT_WIDTH = 10, // in ms
        HISTO_COUNT = 64,
    };

    HistoSlot m_histo[HISTO_COUNT];

    void clearHisto()
    {
        memset(m_histo, 0, sizeof(m_histo));
    }

    void reset(uint32_t curTime)
    {
        millisPerSeg = 0;
        millisPerSegPeak = 1000000000;
        segmentsSinceLastRun = 0;
        millisSinceLastRun = 0;
        lastUpdateTime = curTime;
        clearHisto();
    }

    uint32_t getTS(int ago)
    {
        ago %= COUNT;
        int idx = (curIndex + COUNT - ago) % COUNT;
        return ts[idx];
    }

    void update(int s)
    {
        totalHits++;
        if (lastSensor != s)
        {
            curIndex = (curIndex + 1) % COUNT;
            lastSensor = s;
        }
        ts[curIndex] = millis();

        uint32_t m0 = getTS(0);
        uint32_t m1 = getTS(1);
        uint32_t m2 = getTS(2);

        uint32_t pct75 = millisGet75pctileOrdered(m0, m2);

        bool top25 = millisIsGreater(m1, pct75);

        if (top25)
        {
            millisPerSeg = (m0 - m2);

            Serial.println(millisPerSeg);
            if (millisPerSeg < SPEED_TIMEOUT)
            {
                isClockwise = lastSensor ? false : true;

                millisPerSegPeak = min(millisPerSegPeak, millisPerSeg);

                segmentsSinceLastRun++;
                millisSinceLastRun += millisPerSeg;

                uint32_t slotIndex = min(millisPerSeg / HISTO_SLOT_WIDTH, HISTO_COUNT-1);
                auto &histoSlot = m_histo[slotIndex];
                histoSlot.count++;
                histoSlot.value += millisPerSeg;

                lastUpdateTime = m0;
            }
        }
    }
};

MagUpdate g_mu;

ICACHE_RAM_ATTR void mag0ISR() { g_mu.update(0); }
ICACHE_RAM_ATTR void mag1ISR() { g_mu.update(1); }

void detachMagInterrupts()
{
    detachInterrupt(digitalPinToInterrupt(MAG0_PIN));
    detachInterrupt(digitalPinToInterrupt(MAG1_PIN));

}

void attachMagInterrupts()
{
    attachInterrupt(digitalPinToInterrupt(MAG0_PIN), mag0ISR, RISING);
    attachInterrupt(digitalPinToInterrupt(MAG1_PIN), mag1ISR, RISING);
}

void reboot() 
{
    wdt_disable();
    wdt_enable(WDTO_15MS);
    while (1) {}
}

void onTelnetConnect(String ip) 
{
    Serial.println("telnet connected from " + ip);
    telnet.println("welcome to " + String(g_hostname));
    telnet.println("commands:");
    telnet.println("    i: info");
    telnet.println("    r: reboot");
    telnet.println("    t: log test data");
    telnet.print("> ");
}

void onTelnetInputReceived(String str) 
{
    telnet.println("input received: '" + str + "'");
    if (str == "r")
    {
        telnet.println("rebooting");
        reboot();        
    }
    else if (str == "i")
    {
        telnet.println("milliseconds: " + String(millis()));
        telnet.println("current time: " + tz.dateTime());
    }
    else if (str == "t")
    {
        telnet.println("logging test data");

        DynamicJsonDocument json(1024);
        json["message"] = "test";
        json["misc"] = "local time is " + tz.dateTime();
        log(json);        
    }
    else
    {
        telnet.print("unknown command: ");
        telnet.println(str);
    }
    telnet.print("> ");
}

void setup() 
{
    Serial.begin(115200);
  
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.print("Connecting to ");
    Serial.println(g_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(g_hostname);
    Serial.printf("hostname: %s\n", WiFi.hostname().c_str());

    WiFi.begin(g_ssid, g_password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("#");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  
    Serial.print("ESP Board MAC Address:  ");
    Serial.println(WiFi.macAddress());
    WiFi.printDiag(Serial);

    ArduinoOTA.setHostname(g_hostname);
    ArduinoOTA.onStart([]() { Serial.println("Start");  });
    ArduinoOTA.onEnd  ([]() { Serial.println("\nEnd");  });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    
    waitForSync();
    if (!tz.setCache(0)) tz.setLocation("America/Los_Angeles");

    telnet.onConnect(onTelnetConnect);
    telnet.onReconnect(onTelnetConnect);
    telnet.onInputReceived(onTelnetInputReceived);
    telnet.begin(23);
  
    pinMode(MAG0_PIN, INPUT);
    pinMode(MAG1_PIN, INPUT);

    attachMagInterrupts();

    logStartup();
}

void loop() {
    events();
    ArduinoOTA.handle();
    telnet.loop();

    int curTime = millis();
    if (g_mu.segmentsSinceLastRun && ((curTime - g_mu.lastUpdateTime) > SPEED_TIMEOUT))
    {
        detachMagInterrupts();

        float mph = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / float(g_mu.millisPerSeg);
        float mphPeak = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / float(g_mu.millisPerSegPeak);
        float millisPerSegAve = float(g_mu.millisSinceLastRun) / float(g_mu.segmentsSinceLastRun);
        float mphAve = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / millisPerSegAve;

        const uint32_t numBins = 5;
        float histoBins[numBins];
        ComputeHistoBins(histoBins, numBins, g_mu.m_histo, MagUpdate::HISTO_COUNT);

        DynamicJsonDocument json(1024);
        json["message"] = "run";
        json["speed"]["peak_mph"] = mphPeak;
        json["speed"]["ave_mph"] = mphAve;
        json["dir"] = g_mu.isClockwise ? "CW" : "CCW";
        json["dur"] = 0.001f * g_mu.millisSinceLastRun;
        json["histo"]["80-100"] = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / histoBins[0];
        json["histo"]["60-80"]  = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / histoBins[1];
        json["histo"]["40-60"]  = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / histoBins[2];
        json["histo"]["20-40"]  = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / histoBins[3];
        json["histo"]["0-20"]   = MMPS_IN_MPH * MM_PER_SEG * 1000.0f / histoBins[4];
        log(json);

        g_mu.reset(curTime);
        attachMagInterrupts();
    }

    g_led.checkToggle();
}
