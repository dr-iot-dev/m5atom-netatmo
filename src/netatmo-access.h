#include <Arduino.h>
#include <WiFiClientSecure.h>

extern String   getAccessToken(WiFiClientSecure *client);
extern String   getRefreshToken(WiFiClientSecure *client);
extern String   refreshAccessToken(WiFiClientSecure *client, String refreshToken);
extern int      getCO2value(WiFiClientSecure *client, String accessToken, String locationName);
extern void     cleanupClient(WiFiClientSecure *client);

extern String   composeMessage(String targetLocation, bool forced);