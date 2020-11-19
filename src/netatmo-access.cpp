/**
  This file is based on BasicHTTPSClient.ino
  https://github.com/espressif/arduino-esp32/tree/master/libraries/HTTPClient/examples/BasicHttpsClient
*/

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>
#include <ArduinoLog.h>

#include "mysettings.h"

#define CO2_WARNING_LEVEL   800
//#define DISABLE_LOGGING
#define JSON_DOC_SIZE   4096


String serverURL  = "https://api.netatmo.com/oauth2/token";
String apiURL     = "https://api.netatmo.com/api";
String separator  = "&";

String getTokenMsg = "grant_type=password" + separator
  + "client_id=" + MY_NETATMO_CLIENT_ID + separator
  + "client_secret=" + MY_NETATMO_CLIENT_SECRET + separator
  + "username=" + MY_NETATMO_USERNAME + separator
  + "password=" + MY_NETATMO_PASSWORD + separator
  + "scope=read_station";

String refreshTokenMsg = "grant_type=refresh_token" + separator
  + "client_id=" + MY_NETATMO_CLIENT_ID + separator
  + "client_secret=" + MY_NETATMO_CLIENT_SECRET + separator;

String getStationDataURL = apiURL + "/getstationsdata?"
  + "device_id=" + MY_NETATMO_DEVICE_ID + "&"
  + "get_favorites=false";

String
postRequest(WiFiClientSecure *client, String serverURL, String message, String tag) {
  HTTPClient https;
  String value;
  
  Log.notice("postRequest(%X, %s, %s, %s)\n", client, serverURL.c_str(), message.c_str(), tag.c_str());
  if (https.begin(*client, serverURL)) {
    https.addHeader( "User-Agent", "ESP32/1.0");
    https.addHeader( "Connection", "close");
    https.addHeader( "Content-Type", "application/x-www-form-urlencoded;charset=UTF-8");

    // start post request
    int httpCode = https.POST(message);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Log.notice("httpCode: %d\n", httpCode);
      String response = https.getString();
      Log.notice("response=%s\n", response.c_str());
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        DynamicJsonDocument doc(JSON_DOC_SIZE);
        deserializeJson(doc, response);
        JsonObject obj = doc.as<JsonObject>();
        value = obj[tag].as<String>();
      }
    } else {
      Log.warning("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Log.warning("[HTTPS] https.begin failed...\n");
  }

  Log.notice("value=%s\n", value.c_str());
  return value;
}


String
getAccessToken(WiFiClientSecure *client) {
  String tag = "access_token";
  String accessToken = postRequest(client, serverURL, getTokenMsg, tag);
  return accessToken;
}

String
getRefreshToken(WiFiClientSecure *client) {
  String tag = "refresh_token";
  String refreshToken = postRequest(client, serverURL, getTokenMsg, tag);
  return refreshToken;
}

String
refreshAccessToken(WiFiClientSecure *client, String refreshToken) {
  String tag = "access_token";
  String refreshAccessTokenBody = refreshTokenMsg + "refresh_token=" + refreshToken;

  String accessToken = postRequest(client, serverURL, refreshAccessTokenBody, tag);
  return accessToken;
}

String
getRequest(WiFiClientSecure *client, String requestURL, String accessToken) {
  HTTPClient https;
  String response;

  Log.notice("getRequest(%X, %s, %s)\n", client, requestURL.c_str(), accessToken.c_str());
  if (https.begin(*client, requestURL)) {
    //https.addHeader( "Host", host);
    https.addHeader( "Accept", "application/json");
    https.addHeader( "Authorization", "Bearer " + accessToken);

    Log.notice("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Log.notice("[HTTPS] GET... code: %d\n", httpCode);

      response = https.getString();
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        Log.notice("httpCode=%d, response=%s\n", httpCode, response.c_str());
      } else {
        Log.warning("httpCode=%d, response=%s\n", httpCode, response.c_str());
      }
    } else {
      Log.warning("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.print("[HTTPS] https.begin failed...\n");
  }
  return response;
}

int
getCO2value(WiFiClientSecure *client, String accessToken, String locationName) {
  int co2value = -1;

  String response = getRequest(client, getStationDataURL, accessToken);
  if ( response != "" ) {
    DynamicJsonDocument doc(JSON_DOC_SIZE);
    deserializeJson(doc, response);
    JsonObject obj = doc.as<JsonObject>();

    JsonObject body = obj["body"];
    JsonArray devices = body["devices"];
    for ( auto device : devices ) {
      String station_name = device["station_name"];   
      String module_name = device["module_name"];
      Log.notice("station_name=%s, module_name=%s, locationName=%s ==> %d\n", station_name.c_str(), module_name.c_str(), locationName.c_str(), ( locationName == module_name ) );
      if ( locationName == module_name ) {
        co2value = device["dashboard_data"]["CO2"];
        Log.notice("co2value=%d\n", co2value);
      } else {
        JsonArray modules = device["modules"];
        for (auto module : modules ) {
          module_name = module["module_name"].as<String>();
          Log.notice("module_name=%s\n", module_name.c_str());
          if ( locationName == module_name ) {
            co2value = module["dashboard_data"]["CO2"];
            Log.notice("co2value=%d\n", co2value);
          }
        }
      }
    }
  }
  return co2value;
}

void cleanupClient(WiFiClientSecure *client) {
  if (client->connected()) client->stop();
  delete client;
}

String composeMessage(String targetLocation, bool forced) {
  Log.notice("composeMessage(%d)\n", forced);
  WiFiClientSecure *client = new WiFiClientSecure;
  if ( client == NULL ) {
    Log.error("Unable to create client\n");
    return "_fail";
  }
  
  // get access token 
  String accessToken = getAccessToken(client);
  if ( accessToken == "" ) {
    Log.error("getAccessToken failed.\n");
    cleanupClient(client);
    return "_fail";
  }

  // get CO2 value
  int co2value = getCO2value(client, accessToken, targetLocation);
  if ( co2value < 0 ) {
    Log.error("getCO2value failed.\n");
    cleanupClient(client);
    return "_fail";
  }

  // clean up client
  cleanupClient(client);

  String message;
  if ( co2value >= CO2_WARNING_LEVEL || forced ) {
    message = targetLocation + "のCO2濃度は、" + co2value + " ppmです。";
  }
  if ( co2value >= CO2_WARNING_LEVEL ) {
    message = message + "窓を開けるなどして、換気してください。";
  }
  Log.notice("message='%s'\n", message.c_str());

  return message;
}