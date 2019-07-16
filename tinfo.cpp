// **********************************************************************************
// Teleinfo management file for remora project
// **********************************************************************************
// Copyright (C) 2014 Thibault Ducret
// Licence MIT
//
// History : 15/01/2015 Charles-Henri Hallard (http://hallard.me)
//                      Intégration de version 1.2 de la carte electronique
//           13/04/2015 Theju
//                      Modification des variables cloud teleinfo
//                      (passage en 1 seul appel) et liberation de variables
//           15/09/2015 Charles-Henri Hallard Utilisation Librairie Teleinfo Universelle
// **********************************************************************************

#include "tinfo.h"

#ifdef MOD_TELEINFO
// Instanciation de l'objet Téléinfo
TInfo tinfo;
#endif

uint mypApp           = 0;
uint myiInst          = 0;
uint myindexHC        = 0;
uint myindexHP        = 0;
uint myimax           = 0;
uint myisousc         = ISOUSCRITE; // pour calculer la limite de délestage
char myPeriode[8]     = "";
char mytinfo[250]     = "";
char mycompteur[64]   = "";
float ratio_delestage = DELESTAGE_RATIO;
float ratio_relestage = RELESTAGE_RATIO;
float myDelestLimit   = 0.0;
float myRelestLimit   = 0.0;
int etatrelais        = 0; // Etat du relais
int fnctRelais        = 2; // Mode de fonctionnement du relais
int lastPtec          = PTEC_HP;

unsigned long tinfo_led_timer = 0; // Led blink timer
unsigned long tinfo_last_frame = 0; // dernière fois qu'on a recu une trame valide

const char FP_JSON_START[] PROGMEM = "{\r\n";
const char FP_JSON_END[] PROGMEM = "\r\n}\r\n";
const char FP_QCQ[] PROGMEM = "\":\"";
const char FP_QCNL[] PROGMEM = "\",\r\n\"";
const char FP_QCB[] PROGMEM = "\":";
const char FP_BNL[] PROGMEM = ",\r\n\"";
const char FP_NL[] PROGMEM = "\r\n";

ptec_e ptec; // Puissance tarifaire en cours

/* ======================================================================
Function: ADPSCallback
Purpose : called by library when we detected a ADPS on any phase
Input   : phase number
            0 for ADPS (monophase)
            1 for ADIR1 triphase
            2 for ADIR2 triphase
            3 for ADIR3 triphase
Output  : -
Comments: should have been initialised in the main sketch with a
          tinfo.attachADPSCallback(ADPSCallback())
====================================================================== */
void ADPSCallback(uint8_t phase)
{
  // Led Rouge
  LedRGBON(COLOR_RED);
  tinfo_led_timer = millis();

  // Monophasé
  if (phase == 0 ) {
    DebuglnF("ADPS");
  } else {
    DebugF("ADPS Phase ");
    Debugln('0' + phase);
  }

  // nous avons une téléinfo fonctionelle
  status |= STATUS_TINFO;
  tinfo_last_frame = millis();
}

/* ======================================================================
Function: DataCallback
Purpose : callback when we detected new or modified data received
Input   : linked list pointer on the concerned data
          value current state being TINFO_VALUE_ADDED/TINFO_VALUE_UPDATED
Output  : -
Comments: -
====================================================================== */
void DataCallback(ValueList * me, uint8_t flags)
{
  // Do whatever you want there
  Debug(me->name);
  Debug('=');
  Debug(me->value);

  //Debug(" Flags=0x");
  //DEBUG_SERIAL.print(flags, HEX);

  if ( flags & TINFO_FLAGS_NOTHING ) DebugF(" Nothing");
  if ( flags & TINFO_FLAGS_ADDED )   DebugF(" Added");
  if ( flags & TINFO_FLAGS_UPDATED ) DebugF(" Updated");
  if ( flags & TINFO_FLAGS_EXIST )   DebugF(" Exist");
  if ( flags & TINFO_FLAGS_ALERT )   DebugF(" Alert");
  Debugln();
  Debugflush();

  // Nous venons de recevoir la puissance tarifaire en cours
  // To DO : gérer les autres types de contrat
  if (!strcmp(me->name, "PTEC")) {
    // Récupération de la période tarifaire en cours
    strncpy(myPeriode, me->value, strlen(me->value));

    // Determination de la puissance tarifaire en cours
    // To DO : gérer les autres types de contrat
    if (!strcmp(me->value, "HP..")) ptec= PTEC_HP;
    if (!strcmp(me->value, "HC..")) ptec= PTEC_HC;
    if (!strcmp(me->value, "TH..")) ptec= PTEC_HP;

    //=============================================================
    //    Ajout de la gestion du relais aux heures creuses
    //=============================================================
    if (fnctRelais == FNCT_RELAIS_AUTO && lastPtec != ptec) {
      //Debug("PTEC: ");
      if (ptec == PTEC_HC) {
        //Debugln(" HC");
        relais("1");
      } else {
        //Debugln(" HP");
        relais("0");
      }
      lastPtec = (int)ptec;
    }
  }

  // Mise à jour des variables "cloud"
  if (!strcmp(me->name, "PAPP"))   mypApp    = atoi(me->value);
  if (!strcmp(me->name, "IINST"))  myiInst   = atoi(me->value);
  if (!strcmp(me->name, "HCHC"))   myindexHC = atol(me->value);
  if (!strcmp(me->name, "HCHP"))   myindexHP = atol(me->value);
  if (!strcmp(me->name, "IMAX"))   myimax    = atoi(me->value);
  if (!strcmp(me->name, "BASE"))   { myindexHP = atol(me->value); myindexHC = 0; }

  // Isousc permet de connaitre l'intensité max pour le delestage
  if (!strcmp(me->name, "ISOUSC")) {
    myisousc = atoi(me->value);
    myDelestLimit = ratio_delestage * myisousc;
    // Calcul de quand on déclenchera le relestage
    myRelestLimit = ratio_relestage * myisousc;

    // Maintenant on connait notre contrat, on peut commencer 
    // A traiter le delestage eventuel et si celui-ci 
    // n'a jamais été initialisé on le fait maintenant
    if ( timerDelestRelest == 0 )
      timerDelestRelest = millis();
  }

  Debugln();

  // nous avons une téléinfo fonctionelle
  status |= STATUS_TINFO;
}

/* ======================================================================
Function: NewFrame
Purpose : callback when we received a complete teleinfo frame
Input   : linked list pointer on the concerned data
Output  : -
Comments: -
====================================================================== */
void NewFrame(ValueList * me)
{
  char buff[32];

  // Light the RGB LED
  LedRGBON(COLOR_GREEN);
  tinfo_led_timer = millis();

  #if defined (ESP8266)
    //sprintf( buff, "New Frame (%ld Bytes free)", ESP.getFreeHeap() );
  #elif defined (SPARK)
    //sprintf( buff, "New Frame (%ld Bytes free)", System.freeMemory());
  #else
    //sprintf( buff, "New Frame");
  #endif
  //Debugln(buff);

  // Ok nous avons une téléinfo fonctionelle
  status |= STATUS_TINFO;
  tinfo_last_frame = millis();
}

/* ======================================================================
Function: NewFrame
Purpose : callback when we received a complete teleinfo frame
Input   : linked list pointer on the concerned data
Output  : -
Comments: it's called only if one data in the frame is different than
          the previous frame
====================================================================== */
void UpdatedFrame(ValueList * me)
{
  char buff[32];

  // Light the RGB LED (orange) and set timer
  LedRGBON(COLOR_ORANGE);
  tinfo_led_timer = millis();

  #if defined (ESP8266)
    //sprintf( buff, "Updated Frame (%ld Bytes free)", ESP.getFreeHeap() );
  #elif defined (SPARK)
    //sprintf( buff, "Updated Frame (%ld Bytes free)", System.freeMemory());
  #else
    //sprintf( buff, "Updated Frame");
  #endif
  //Debugln(buff);

  //On publie toutes les infos teleinfos dans un seul appel :
  sprintf(mytinfo,PSTR("{\"papp\":%u,\"iinst\":%u,\"isousc\":%u,\"ptec\":%u,\"indexHP\":%u,\"indexHC\":%u,\"imax\":%u,\"ADCO\":%u}"),
                    mypApp,myiInst,myisousc,ptec,myindexHP,myindexHC,myimax,mycompteur);
  // Posibilité de faire une pseudo serial avec la fonction suivante :
  //Spark.publish("Teleinfo",mytinfo);

  // nous avons une téléinfo fonctionelle
  status |= STATUS_TINFO;
  tinfo_last_frame = millis();
}

/* ======================================================================
Function: getTinfoListJson
Purpose : dump all teleinfo values in JSON
Input   : -
Output  : -
Comments: -
====================================================================== */
void  getTinfoListJson(String &response, bool with_uptime)
{ 
  ValueList * me = tinfo.getList();

  // Got at least one ?
  if (me) {
    char * p;
    long value;
    bool loop_first = true;

    // Json start
    response += FPSTR(FP_JSON_START);
    if (with_uptime) {
      response += F("\"_UPTIME\":");
      response += uptime;
      response += FPSTR(FP_NL) ;
      loop_first = false;
    }

    // Loop thru the node
    while (me->next) {
      // go to next node
      me = me->next;

      if (tinfo.calcChecksum(me->name,me->value) == me->checksum) {
        if (!loop_first) {
          response += F(",\"") ;
        }
        else {
          response += F("\"") ;
          loop_first = false;
        }
        response += me->name ;
        response += FPSTR(FP_QCB);

        // Check if value is a number
        value = strtol(me->value, &p, 10);

        // conversion failed, add "value"
        if (*p) {
          response += F("\"") ;
          response += me->value ;
          response += F("\"") ;

        // number, add "value"
        } else {
          response += value ;
        }
        //formatNumberJSON(response, me->value);
      } else {
        response = F(",\"_Error\":\"");
        response = me->name;
        response = "=";
        response = me->value;
        response = F(" CHK=");
        response = (char) me->checksum;
        response = "\"";
      }

      // Add new line to see more easier end of field
      response += FPSTR(FP_NL) ;

    }
    // Json end
    response += FPSTR(FP_JSON_END) ;
    //return response;
  }
  else {
    response = "-1";
    //return response;
    //return (String(-1, DEC));
  }
}

/* ======================================================================
Function: tinfo_setup
Purpose : prepare and init stuff, configuration, ..
Input   : indique si on doit bloquer jusqu'à reception ligne téléinfo
Output  : false si on devait attendre et time out expiré, true sinon
Comments: -
====================================================================== */
bool tinfo_setup(bool wait_data)
{
  bool ret = false;

  DebugF("Initializing Teleinfo...");
  Debugflush();

  #ifdef SPARK
    Serial1.begin(1200);  // Port série RX/TX on serial1 for Spark
  #endif

  // reset du timeout de detection de la teleinfo
  tinfo_last_frame = millis();

  // Init teleinfo
  tinfo.init();

  // Attacher les callback donc nous avons besoin
  #ifdef MOD_ADPS
    tinfo.attachADPS(ADPSCallback);
  #endif
  tinfo.attachData(DataCallback);
  tinfo.attachNewFrame(NewFrame);
  tinfo.attachUpdatedFrame(UpdatedFrame);

  // Doit-on attendre une ligne valide
  if (wait_data) {
    // ici on attend une trame complete ou le time out
    while ( !(status & STATUS_TINFO) && (millis()-tinfo_last_frame<TINFO_FRAME_TIMEOUT*1000)) {
      char c;
      // Envoyer le contenu de la serial au process teleinfo
      // les callback mettront le status à jour
      #ifdef SPARK
        if ( Serial1.available()) {
          c = Serial1.read();
          //Debug(c);
          //Debugflush();
          tinfo.process(c);
        }
      #else
        if (Serial.available()) {
          c = Serial.read();
          //Debug(c);
          //Debugflush();
          tinfo.process(c);
        }
      #endif
      
      _yield();
    }
  }

  ret = (status & STATUS_TINFO)?true:false;
  DebugF("Init Teleinfo ");
  Debugln(ret?"OK!":"Erreur!");

  return ret;
}

/* ======================================================================
Function: tinfo_loop
Purpose : gestion des trames reçues par la librairie teleinfo
Input   : -
Output  : -
Comments: -
====================================================================== */
void tinfo_loop(void)
{
#ifdef MOD_TELEINFO
  char c;
  uint8_t nb_char=0;
  // Evitons les conversions hasardeuses, parlons float
  float fiInst = myiInst; 

  // on a la téléinfo présente ?
  if ( status & STATUS_TINFO) {
    // est ce que cela fait un moment qu'on a pas recu de trame
    if ( millis()-tinfo_last_frame>TINFO_FRAME_TIMEOUT*1000) {
      // Indiquer qu'elle n'est pas présente
      status &= ~STATUS_TINFO;
      DebuglnF("Teleinfo absente/perdue!");
    }

  // Nous n'avions plus de téléinfo
  } else  {
    // est ce que cela fait un moment qu'on a pas recu de data
    if ( millis()-tinfo_last_frame>TINFO_DATA_TIMEOUT*1000) {
      // Light the RGB LED (RED) and set timer with
      // 500ms more than classic blink
      LedRGBON(COLOR_RED);
      tinfo_last_frame = millis();
      tinfo_led_timer = millis();
      DebuglnF("Teleinfo toujours absente!");
    }

  }

  // Caractère présent sur la sérial téléinfo ?
  // On prendra maximum 8 caractères par passage
  // les autres au prochain tour, çà evite les
  // long while bloquant pour les autres traitements
  #ifdef SPARK
    while (Serial1.available() && nb_char<8) {
      c = (Serial1.read());
      tinfo.process(c);
      nb_char++;
    }
  #else
    while (Serial.available() && nb_char<8) {
      c = (Serial.read());
      tinfo.process(c);
      nb_char++;
    }
  #endif

  // Faut-il enclencher le delestage ?
  #ifdef MOD_ADPS
  //On dépasse le courant max?
  if (fiInst > myDelestLimit) {
    if ((millis() - timerDelestRelest) > 5000L)  {
      //On ne passe pas dans la boucle si l'on a délesté ou relesté une zone il y a moins de 5s
      //On évite ainsi de délester d'autres zones avant que le délestage précédent ne fasse effet
      delester1zone();
      timerDelestRelest = millis();
    }
  } else {
    // Un délestage est en cours (nivDelest > 0)
    // Le délestage/relestage de la dernière zone date de plus de 3 minutes
    // On attend au moins ce délai pour relester ou décaler
    // pour éviter les délestage/relestage trop rapprochés
    if (nivDelest > 0 && (millis() - timerDelestRelest) > 180000L) {
      //Le courant est suffisamment bas pour relester
      if (fiInst < myRelestLimit) {
        relester1zone();
        timerDelestRelest = millis();
      } else {
        // On fait tourner le délestage
        // ex : AVANT = "DDCEEEE" => APRES = "CDDEEEE"
        decalerDelestage();
        timerDelestRelest = millis();
      }
    }
  }
  #endif //ADPS active

  // Do we have RGB led timer expiration ?
  if (tinfo_led_timer && (millis()-tinfo_led_timer >= TINFO_LED_BLINK_MS)) {
      LedRGBOFF(); // Light Off the LED
      tinfo_led_timer=0; // Stop virtual timer
  }
#endif
}
