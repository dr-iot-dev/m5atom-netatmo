#include <Arduino.h>

extern int speak(String text);
extern int speakMessage(String speaker, String message);
extern String composeMessage(bool forced);
extern void decrementFuse();
extern void restoreFuse();

extern String getGoogleSpeechUrl(String text);
int outputUrlToGoogleHome(String url, String googleHomeName);
int outputUrlToSpeaker(String url);