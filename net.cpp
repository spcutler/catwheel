#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecureBearSSL.h>
#include "net.h"

BearSSL::WiFiClientSecure *client = nullptr;

void post(const char *url, DynamicJsonDocument &json)
{
    if (!client)
    {
        client = new BearSSL::WiFiClientSecure;
    }

    client->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, url)) 
    {
        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Content-Type", "application/json");

        String body;
        serializeJson(json, body);    

        int httpCode = https.POST(body);  

        // httpCode will be negative on error
        if (httpCode > 0) 
        {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

            // file found at server
            if ((httpCode == HTTP_CODE_OK) || (httpCode == HTTP_CODE_MOVED_PERMANENTLY) || (httpCode == HTTP_CODE_FOUND)) 
            {
                String payload = https.getString();
                Serial.println(payload);
            }
        } else {
            Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
            //String payload = https.getString();
            //Serial.println(payload);
        }

        https.end();
    }
}