void eeWriteInt(int pos, int val) {
  byte* p = (byte*) &val;
  EEPROM.write(pos, *p);
  EEPROM.write(pos + 1, *(p + 1));
  EEPROM.write(pos + 2, *(p + 2));
  EEPROM.write(pos + 3, *(p + 3));
  EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

bool modeNeedSave(ModeSettings Mode1, ModeSettings Mode2){

  if (Mode1.brightness != Mode2.brightness) return true;
  if (Mode1.scale != Mode2.scale) return true;
  if (Mode1.speed != Mode2.speed) return true;

  return false;

}

void readEEPROM(){

  for (byte i = 0; i < MODE_AMOUNT; i++) {
    EEPROM.get(3 * i + 40, modes[i]);
  }

  for (byte i = 0; i < 7; i++) {
    alarm[i].state = EEPROM.read(5 * i);
    alarm[i].time = eeGetInt(5 * i + 1);
  }

  ONflag = EEPROM.read(198);
  dawnMode = EEPROM.read(199);
  currentMode = (int8_t)EEPROM.read(200);
  FastLED.setBrightness(modes[currentMode].brightness);

}

void saveEEPROM() {
  
  // Флаги
  if (EEPROM.read(198) != ONflag) EEPROM.write(198, ONflag);
  if (EEPROM.read(199) != dawnMode) EEPROM.write(199, dawnMode);
  if (EEPROM.read(200) != currentMode) EEPROM.write(200, currentMode);
 
  // Режимы
  for (byte i = 0; i < MODE_AMOUNT; i++) {
 
    ModeSettings savedModeSettings;
    EEPROM.get(3 * i + 40, savedModeSettings);

    if(modeNeedSave(savedModeSettings, modes[i])) EEPROM.put(3 * i + 40, modes[i]);

  }

  // рассвет
  for (byte i = 0; i < 7; i++) {

    if(alarm[i].state != EEPROM.read(5 * i) || alarm[i].time != eeGetInt(5 * i + 1)){

      EEPROM.write(5 * i, alarm[i].state);   
      eeWriteInt(5 * i + 1, alarm[i].time);

    }

  }

  EEPROM.commit();

  #ifdef DEBUG    
  Serial.println("EEPROM saved");
  #endif

}

void initEEPROM() {

  if (EEPROM.read(197) != 20) {   // первый запуск
    #ifdef DEBUG    
    Serial.println("EEPROM init");
    #endif
    
    
    EEPROM.write(197, 20);
    //EEPROM.commit();

    //5-40 рассвет
    for (byte i = 0; i < 7; i++) {
      EEPROM.write(5 * i, alarm[i].state);   // 
      eeWriteInt(5 * i + 1, alarm[i].time);
      //EEPROM.commit();
    }

    // 43 - 94  Настройки режимов
    for (byte i = 0; i < MODE_AMOUNT; i++) {
      EEPROM.put(3 * i + 40, modes[i]);
      //EEPROM.commit();
    }

    EEPROM.write(198, 0);   // включено выключено
    EEPROM.write(199, 0);   // рассвет
    EEPROM.write(200, 0);   // текущий режим

    EEPROM.commit();
  }

  readEEPROM();
  
}

void eepromTick() {
  if (settChanged) {
    settChanged = false;
    saveEEPROM();
  }
}

