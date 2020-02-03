
void updateCurrentTime() {
  if (ESP_MODE != 1) return;

  secs++;
  if (secs == 60) {
    secs = 0;
    mins++;
  }
  if (mins == 60) {
    mins = 0;
    hrs++;
    if (hrs == 24) {
      hrs = 0;
      days++;
      if (days > 6) days = 0;
    }
  }
}

void timeUpdate() {
  if (timeClient.update()) {
    hrs = timeClient.getHours();
    mins = timeClient.getMinutes();
    secs = timeClient.getSeconds();
    days = timeClient.getDay();
  }
}

String getTimeString(){

  String timeStr;
  timeStr = String(hrs);
  timeStr += ":";
  timeStr += (mins < 10) ? "0" : "";
  timeStr += String(mins);

  return timeStr;
}

void checkDawn() {

  #ifdef DEBUG
    Serial.println("Проверка рассвета.");
  #endif

  byte thisDay = days;
  if (thisDay == 0) thisDay = 7;  // воскресенье это 0
  thisDay--;
  int thisTime = hrs * 3600 + mins * 60 + secs;
  int alarmBeginTime = (alarm[thisDay].time - dawnOffsets[dawnMode]) * 60;

  #ifdef DEBUG
    Serial.print("День:");
    Serial.print(thisDay);
    Serial.print(" Текущее время:");
    Serial.print(thisTime);
    Serial.println(".");
  #endif

  // проверка рассвета
  if (alarm[thisDay].state &&        // день будильника
      thisTime >= alarmBeginTime &&  // позже начала
      thisTime < (alarm[thisDay].time + DAWN_TIMEOUT) * 60 ) {           // раньше конца + минута

    #ifdef DEBUG
        Serial.println("Время будильника:");
        Serial.println(alarm[thisDay].time);
        Serial.println(thisTime);
        Serial.println(alarmBeginTime);
      manualOff ? Serial.println("Рассвет остановлен.") : Serial.println("Рассвет запущен.");
    #endif

    if (!manualOff) {

      dawnFlag = true;
      timer.enable(alarmTimerID);
      
    }
  } else {
    if (dawnFlag) {
      dawnFlag = false;
      manualOff = false;
      ONflag = false;
      timer.disable(alarmTimerID);
      FastLED.setBrightness(modes[currentMode].brightness);
      FastLED.clear();
      FastLED.show();
    }
  }
}
 
void showAlarm(){

  if (!dawnFlag) return;

  byte thisDay = days;
  if (thisDay == 0) thisDay = 7;  // воскресенье это 0
  thisDay--;
  int thisTime = hrs * 3600 + mins * 60 + secs;
  int alarmBeginTime = (alarm[thisDay].time - dawnOffsets[dawnMode]) * 60;

  //позиция рассвета от 0 до 1000
  int dawnPosition = (float)1000 * (float)(thisTime - alarmBeginTime) / (float)(dawnOffsets[dawnMode] * 60);

  #ifdef DEBUG
    Serial.print("Позиция рассвета:");
    Serial.println(dawnPosition);
  #endif

  dawnPosition = constrain(dawnPosition, 0, 1000);
  CHSV dawnColor = CHSV(map(dawnPosition, 0, 1000, 10, 35),
          map(dawnPosition, 0, 1000, 255, 170),
          map(dawnPosition, 0, 1000, 10, DAWN_BRIGHT));

  CHSV textColor = CHSV(120, 120, map(dawnPosition, 0, 1000, 10, DAWN_BRIGHT / 2 ));

  fill_solid(leds, NUM_LEDS, dawnColor);   
  fillString(getTimeString(), textColor, false);     
  FastLED.setBrightness(255);

}


String getTimeStampString() {
   time_t rawtime = timeClient.getEpochTime();
   struct tm * ti;
   ti = localtime (&rawtime);

   uint16_t year = ti->tm_year + 1900;
   String yearStr = String(year);

   uint8_t month = ti->tm_mon + 1;
   String monthStr = month < 10 ? "0" + String(month) : String(month);

   uint8_t day = ti->tm_mday;
   String dayStr = day < 10 ? "0" + String(day) : String(day);

   uint8_t hours = ti->tm_hour;
   String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

   uint8_t minutes = ti->tm_min;
   String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

   uint8_t seconds = ti->tm_sec;
   String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

   return "Дата: "+ dayStr + "-" + monthStr + "-" + yearStr + ". " + "Время: " +
          hoursStr + ":" + minuteStr;
}
