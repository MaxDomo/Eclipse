/////////////////////////////////
//       Projet ECLIPSE        //
// Ecrit par Maxime MAUCOURANT //
// MMAUCOURANT AT FREE DOT FR  //
/////////////////////////////////
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "ESP8266httpUpdate.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "PubSubClient.h"
#include "EspAlexa.h"
#include "strings.h"

#define USEOTA
#ifdef USEOTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif

#define HTTP_OTA_URL "http://firmwares.domotique-passion.fr/eclipse/"
#define FIRMWARE_NAME "eclipse-v1.70-BETA"

#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

#define pinSpeed 12
#define pinA 5
#define pinB 4

#define pinRelay 13 // Revoir l'affectation de la pin

#define pinSwitch 10

void lightDeviceChanged(EspalexaDevice *dev);
void lampShadeDeviceChanged(EspalexaDevice *dev);

// Déclaration du client Wifi
WiFiClient wifiClient;

// Déclaration du serveur web
ESP8266WebServer server(80);

// Déclaration du client MQTT
PubSubClient mqttClient(wifiClient);

DynamicJsonDocument config(1024);

Espalexa espalexa;
EspalexaDevice *lightDevice;
EspalexaDevice *lampShadeDevice;

int delayedPeriod = 1000;
unsigned long delayedTime;
void (*delayedFunction)();

// Init délai de tempo moteur
long motionDuration = 0;

// Init vitesse moteur
int motorSpeed = 100;

bool isRunning = false;
bool isClosed = false;

// Init délais d'ouverture/fermeture
int openingTime;
int closingTime;

// Init actions du moteur
int motorAction = -1;

// Init restauration états après coupure
bool restoreLastState = false;

// Variable pour la connexion à un nouveau réseau wifi
String newSSID;
String newPass;

// Etat de la dernière mise à jour
String lastUpdateStatus;

const char *hueLightName = "Eclipse light";
const char *hueLampshadeName = "Eclipse lampshade";

int mqttReconnectTime = 0;

int motorMotionTime = 0;

// Etat de la lumière
String lightStatus;

// Etat de l'abat jour
String lampshadeStatus;

int statusTime;

bool lastWifiConnected = false;

// Paramètre IP de station
IPAddress localIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 249);
IPAddress subnet(255, 255, 255, 0);

bool hueEnabled = false;
bool mqttEnabled = false;

void restart()
{
  if (WiFi.isConnected())
  {
    WiFi.disconnect();

    delay(1000);
  }

  ESP.restart();
}

IPAddress str2IP(String str)
{

  IPAddress ret(getIpBlock(0, str), getIpBlock(1, str), getIpBlock(2, str), getIpBlock(3, str));
  return ret;
}

int getIpBlock(int index, String str)
{
  char separator = '.';
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = str.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (str.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? str.substring(strIndex[0], strIndex[1]).toInt() : 0;
}

String JSONStringify(DynamicJsonDocument json)
{
  String buf;
  serializeJson(json, buf);
  return buf;
}

String getHostname()
{
  String macAddr = WiFi.macAddress();
  macAddr = macAddr.substring(10, macAddr.length());
  macAddr.replace(":", "");

  String hostname = "eclipse";
  hostname += "-";
  hostname += macAddr;

  return hostname;
}

// Fonction permettant d'obtenir la tension en milliVolt dans le circuit
unsigned int getVcc()
{
  // Calcul ADC
  // R1 = 100K
  // R2 = 22K
  // Vout = Vin * R2 / (R1 + R2)
  // 5 * 22 / (100 + 22) = 0.9016393443
  // Ratio pont diviseur = 5 / 0.9016393443 = 5.54
  // LSB = (0.901639 / 1017) * 5.54 = 0.004911583

  float LSB = 0.004911583;
  int ADC = analogRead(A0);

  return (ADC * LSB) * 1000; // mV
}

void lightBlink(int count = 1, int wait = 500)
{
  // Eteint la lumière
  digitalWrite(pinRelay, LOW);

  // On fait clignoter 3 fois la lumière
  for (int i = 0; i < count; i++)
  {
    delay(wait);
    digitalWrite(pinRelay, HIGH);
    delay(wait);
    digitalWrite(pinRelay, LOW);
  }
}

// Fonction pour construire le topic MQTT
String mqttTopicBuilder(String prefix, String topic)
{
  String fullTopic = config["mqtt"]["full_topic"].as<String>();
  fullTopic.replace("{prefix}", prefix);
  fullTopic.replace("{topic}", config["mqtt"]["topic"].as<String>());
  fullTopic += "/" + topic;

  return fullTopic;
}

// Fonction permettant de publier des messages sur un topic MQTT
void mqttPublish(String prefix, String topic, String payload, bool retain = false)
{
  String fullTopic = mqttTopicBuilder(prefix, topic);

  mqttClient.publish(fullTopic.c_str(), payload.c_str(), retain);
}

// Fonction de connexion au serveur MQTT
void mqttConnect(String client, String user, String pass, String inTopic)
{
  if (pass == "")
  {
    mqttClient.connect(client.c_str());
  }
  else
  {
    mqttClient.connect(client.c_str(), user.c_str(), pass.c_str());
  }

  // Construction des topic lampe et abat jour
  String lightTopic = mqttTopicBuilder("cmnd", "LIGHT");
  String lampshadeTopic = mqttTopicBuilder("cmnd", "LAMPSHADE");

  // Souscription aux topics pour les receptions de messages
  mqttClient.subscribe(lightTopic.c_str());
  mqttClient.subscribe(lampshadeTopic.c_str());
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String topicStr = topic;
  payload[length] = 0;
  String cmd = String((char *)payload);

  cmd.toLowerCase();

  if (topicStr == mqttTopicBuilder("cmnd", "LIGHT") || topicStr == mqttTopicBuilder("cmnd", "LAMPSHADE"))
  {
    sendCommand(cmd);
  }
}

#ifdef USEOTA
// Fonction permettant d'activer les mises à jour à travers le WiFi (OTA - Over The Air)
void beginOTA(const char *hostname)
{
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();
}
#endif

// Fonction de chargement du fichier de configuration JSON
void loadConfig()
{
  if (LittleFS.begin())
  {
    if (LittleFS.exists("/config.json"))
    {
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        deserializeJson(config, configFile);

        configFile.close();
      }
    }
  }
}

// Fonction de sauvegarde de configuration dans un fichier JSON
void saveConfig()
{
  File configFile = LittleFS.open("/config.json", "w");
  if (configFile)
  {
    serializeJson(config, configFile);

    configFile.close();
  }
}

// Fonction de réinitialisation de l'ESP
void reset()
{
  if (LittleFS.exists("/config.json"))
  {
    LittleFS.remove("/config.json");
  }

  WiFi.disconnect();
  startSoftAP(getHostname());
  config.clear();
}

// Fonction appelée en cas de page non trouvée (404)
void handleNotFound()
{
  if (hueEnabled && espalexa.handleAlexaApiCall(server.uri(), server.arg(0)))
  {
  }
  else
  {
    server.send(404, "text/plain", "Not found");
  }
}

// Fonction appelée pour le chargement d'un script
void handleJSLib()
{
  server.send(200, "text/plain", FPSTR(JS_lib));
}

// Fonction pour générer la page de config système
void handleConfig()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(CONFIG_PAGE);
  page += FPSTR(HTTP_END);

  page.replace("{title}", "Configuration");

  server.send(200, "text/html", page);
}

// Fonction pour générer la page principal
void handleRoot()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page.replace("{title}", "Main");
  page += "<script src='script.js'></script>";
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(MAIN_PAGE);
  page += FPSTR(HTTP_END);

  server.send(200, "text/html", page);
}

// Fonction permettant de générer la page de mise à jour du firmware
void handleUpdate()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_REFRESH);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);

  page.replace("{title}", "Firmware Update");

  if (!server.hasArg("finished"))
  {
    page += FPSTR(HTTP_UPDATE_START);

    page.replace("{time}", "30");
    page.replace("{url}", "/update?finished");

    // Lancement de la méthode de vérification d'une mise à jour en mode asynchrone
    delayedPeriod = 2000;
    delayedTime = millis();
    delayedFunction = &checkOTAUpdate;
  }
  else
  {
    page += FPSTR(HTTP_UPDATE_RESULT);

    if (lastUpdateStatus == "")
    {
      lastUpdateStatus = "Update successful";
    }

    page.replace("{updateStatus}", lastUpdateStatus);
    page.replace("{time}", "10");
    page.replace("{url}", "/");
  }

  page += FPSTR(HTTP_END);

  server.send(200, "text/html", page);
}

// Fonction permettant de générer la page de config système
void handleSystem()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(SYSTEM_PAGE);
  page += FPSTR(HTTP_END);

  // Remplacement des valeurs
  page.replace("{title}", "System Configuration");
  page.replace("{opentime}", (String)(config["open_time"].as<int>() | 0));
  page.replace("{closetime}", (String)(config["close_time"].as<int>() | 0));
  page.replace("{currentvoltage}", (String)getVcc());
  page.replace("{maxdropvoltage}", (String)(config["maxdropvoltage"].as<int>() | 150));
  page.replace("{maxdetectiontime}", (String)(config["maxdetectiontime"].as<int>() | 30));
  page.replace("{synclight}", (config["sync_light"] ? "checked" : ""));

  server.send(200, "text/html", page);
}

// Fonction appelée lors de la sauvegarde de la config système
void handleSystemSave()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_REFRESH);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);

  page.replace("{title}", "System Configuration");

  // Récupération des paramètres POST
  bool syncLight = server.hasArg("synclight");
  int openTime = server.arg("opentime").toInt();
  int closeTime = server.arg("closetime").toInt();
  int maxDropVoltage = server.arg("maxdropvoltage").toInt();
  int maxDetectionTime = server.arg("maxdetectiontime").toInt();

  if (openTime <= 0 || openTime > 30)
  {
    page += FPSTR("Closing time must be between 0 and 30 seconds");
  }
  else if (closeTime <= 0 || closeTime > 30)
  {
    page += FPSTR("Opening time must between 0 and 30 secondes");
  }
  else if (maxDropVoltage <= 0 || maxDropVoltage > 600)
  {
    page += FPSTR("Maximum drop voltage must between 0 and 600 secondes");
  }
  else if (maxDetectionTime <= 0 || maxDetectionTime > 50)
  {
    page += FPSTR("Maximum detection time must between 0 and 50 secondes");
  }
  else
  {
    page += FPSTR(HTTP_SYSTEM_SUCCESS);

    // Sauvegarde des paramètres
    config["sync_light"] = syncLight;
    config["open_time"] = openTime;
    config["close_time"] = closeTime;
    config["max_drop_voltage"] = maxDropVoltage;
    config["max_detection_time"] = maxDetectionTime;

    openingTime = openTime;
    closingTime = closeTime;

    saveConfig();
  }

  page += FPSTR(HTTP_END);

  page.replace("{time}", "5");
  page.replace("{url}", "/system");

  server.send(200, "text/html", page);
}

// Fonction permettant de générer la page de configuration MQTT
void handleMQTT()
{
  String page;

  // Récupération de nom d'hôte
  String hostname = getHostname();

  hostname.replace('-', '_');
  hostname.toUpperCase();

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(MQTT_PAGE);
  page += FPSTR(HTTP_END);

  // Remplacement des valeurs avec des présentes la configuration
  page.replace("{title}", "MQTT Configuration");
  page.replace("{checked}", (config["mqtt"]["enabled"] ? "checked" : ""));
  page.replace("{host}", config["mqtt"]["host"] | "");
  page.replace("{port}", config["mqtt"]["port"] | "1883");
  page.replace("{port}", config["mqtt"]["port"] | "1883");
  page.replace("{client}", config["mqtt"]["client"] | "eclipse");
  page.replace("{user}", config["mqtt"]["user"] | "");
  page.replace("{pass}", config["mqtt"]["pass"] | "");
  page.replace("{topic}", config["mqtt"]["topic"] | hostname);
  page.replace("{fulltopic}", config["mqtt"]["full_topic"] | "{prefix}/{topic}");

  server.send(200, "text/html", page);
}

// Fontion appelée lors de la sauvegarde de la page de config MQTT
void handleMQTTSave()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page.replace("{title}", "MQTT Configuration saved");
  page += FPSTR(HTTP_REFRESH);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);

  // Récupération des paramètres (POST)
  mqttEnabled = server.hasArg("enable");

  String host = server.arg("host");
  String port = server.arg("port");
  String clientName = server.arg("client");
  String user = server.arg("user");
  String pass = server.arg("pass");
  String topic = server.arg("topic");
  String fullTopic = server.arg("fulltopic");

  if (mqttEnabled && (host == "" || port == ""))
  {
    page += FPSTR("MQTT host or port cannot be empty");
    page.replace("{time}", "5");
  }
  else if (mqttEnabled && user == "")
  {
    page += FPSTR("MQTT user cannot be empty");
    page.replace("{time}", "5");
  }
  else if (mqttEnabled && (topic == "" || fullTopic == ""))
  {
    page += FPSTR("MQTT topic or fullTopic cannot be empty");
    page.replace("{time}", "5");
  }
  else
  {
    page += FPSTR(HTTP_MQTT_SUCCESS);

    // Sauvegarde des paramètres
    config["mqtt"]["enabled"] = mqttEnabled;
    config["mqtt"]["host"] = host;
    config["mqtt"]["port"] = port;
    config["mqtt"]["client"] = clientName;
    config["mqtt"]["user"] = user;
    config["mqtt"]["pass"] = pass;
    config["mqtt"]["topic"] = topic;
    config["mqtt"]["full_topic"] = fullTopic;

    saveConfig();

    page.replace("{time}", "30");

    // Rédemarrage de l'ESP
    delayedPeriod = 5000;
    delayedTime = millis();
    delayedFunction = &restart;
  }

  page += FPSTR(HTTP_END);

  page.replace("{url}", "/mqtt");

  server.send(200, "text/html", page);
}

// Fontion appelée pour construire la page de configuration HUE
void handleHUE()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HUE_PAGE);
  page += FPSTR(HTTP_END);

  // Remplacement des valeurs
  page.replace("{title}", "HUE Emulator Configuration");
  page.replace("{checked}", (config["hue"]["enabled"] ? "checked" : ""));
  page.replace("{lightname}", config["hue"]["light_name"] | hueLightName);
  page.replace("{lampshadename}", config["hue"]["lampshade_name"] | hueLampshadeName);

  server.send(200, "text/html", page);
}

// Fontion appelée lors de la sauvegarde des paramètres HUE
void handleHUESave()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_REFRESH);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);

  page.replace("{title}", "HUE Emulation Configuration saved");

  hueEnabled = server.hasArg("enable");

  String lightName = server.arg("light-name");
  String lampshadeName = server.arg("lampshade-name");

  if (hueEnabled && (lightName == "" || lampshadeName == ""))
  {
    page += FPSTR("Light or lampshade name cannot be empty !");
    page.replace("{time}", "5");
  }
  else
  {
    page += FPSTR(HTTP_HUE_SUCCESS);

    // Sauvegarde des paramètres
    config["hue"]["enabled"] = hueEnabled;
    config["hue"]["lignt_name"] = lightName;
    config["hue"]["lampshade_name"] = lampshadeName;

    saveConfig();

    // Redémarrage de l'ESP
    delayedPeriod = 5000;
    delayedTime = millis();
    delayedFunction = &restart;
  }

  page += FPSTR(HTTP_END);

  page.replace("{url}", "/hue");

  server.send(200, "text/html", page);
}

// Fonction appelée pour la construction de la page de séléction des réseaux WiFi
void handleWifi()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_HEAD_END);
  page += getNetworks();

  page += FPSTR(HTTP_END);

  page.replace("{title}", "Wifi Network");

  server.send(200, "text/html", page);
}

// Fontion appelée lors de la sauvegarde des paramètres WiFi
void handleWifiSave()
{
  String ssid = server.arg("ssid");
  String pass = server.arg("password");

  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_REFRESH);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);

  page.replace("{title}", "Wifi");

  if (server.hasArg("ip"))
  {
    if (server.hasArg("static"))
    {
      config["wifi"]["static"] = true;
      config["wifi"]["ip"] = (server.arg("ip") != "") ? server.arg("ip") : WiFi.localIP().toString();
      config["wifi"]["subnet"] = (server.arg("subnet") != "") ? server.arg("subnet") : WiFi.subnetMask().toString();
      config["wifi"]["gateway"] = (server.arg("gateway") != "") ? server.arg("gateway") : WiFi.gatewayIP().toString();
      config["wifi"]["dns"] = (server.arg("dns") != "") ? server.arg("dns") : WiFi.dnsIP().toString();

      String url = "http://";
      url += config["wifi"]["ip"].as<String>();
      url += "/";

      page.replace("{url}", url);
    }
    else
    {
      config["wifi"]["static"] = false;
      config["wifi"]["ip"] = "";
      config["wifi"]["subnet"] = "";
      config["wifi"]["gateway"] = "";
      config["wifi"]["dns"] = "";

      String url = "http://";
      url += getHostname();
      url += ".local/";

      page.replace("{url}", url);
    }

    saveConfig();

    delayedPeriod = 10000;
    delayedTime = millis();
    delayedFunction = &restart;

    page += FPSTR(HTTP_WIFI_PARAMS_SUCCESS);
    page.replace("{time}", "30");
  }

  if (server.hasArg("ssid"))
  {
    if (lastWifiConnected)
    {
      page += FPSTR(HTTP_WIFI_CONNECT);
      page.replace("{ssid}", ssid);

      newSSID = ssid;
      newPass = pass;

      delayedPeriod = 5000;
      delayedTime = millis();
      delayedFunction = &connectNewWifi;
    }
    else
    {
      if (wifiConnect(ssid, pass))
      {
        config["wifi"]["ssid"] = ssid;
        config["wifi"]["pass"] = pass;

        // Sauvegarde des identifiants
        saveConfig();

        String newUrl = "http://";
        newUrl += WiFi.localIP().toString();
        newUrl += "/";

        page += FPSTR(HTTP_WIFI_CONNECT_SUCCESS);

        page.replace("{time}", "30");
        page.replace("{url}", newUrl);

        // Redémarrage de l'ESP
        delayedPeriod = 10000;
        delayedTime = millis();
        delayedFunction = &restart;
      }
      else
      {

        String url = "http://";
        url += WiFi.softAPIP().toString();
        url += "/wifi";

        page += FPSTR(HTTP_WIFI_CONNECT_FAIL);

        page.replace("{time}", "30");
        page.replace("{url}", url);
      }
    }
  }

  page += FPSTR(HTTP_END);

  server.send(200, "text/html", page);
}

// Fonction de récupération des commandes HTTP
void handleCommand()
{
  DynamicJsonDocument result(512);

  // Commande de la lampe et abat jour
  if (server.hasArg("light"))
  {
    sendCommand(server.arg("light"));
    result["success"] = true;
    server.send(200, "application/json", JSONStringify(result));
  }
  else if (server.arg("restore") == "on")
  {
    restoreLastState = true;
    config["restore_state"] = restoreLastState;
    saveConfig();
    result["success"] = true;
    server.send(200, "application/json", JSONStringify(result));
  }
  else if (server.arg("restore") == "off")
  {
    restoreLastState = false;
    config["restore_state"] = restoreLastState;
    saveConfig();
    result["success"] = true;
    server.send(200, "application/json", JSONStringify(result));
  }
  else if (server.hasArg("status"))
  {
    result["status"]["light"] = lightStatus;
    result["status"]["lampshade"] = lampshadeStatus;
    result["success"] = true;

    server.send(200, "application/json", JSONStringify(result));
  }
  // Commande de lancement de mode d'apprentissage
  else if (server.hasArg("learning"))
  {
    sendCommand("learn");
    result["success"] = true;
    server.send(200, "application/json", JSONStringify(result));
  }
  // Commande de réinitialisation
  else if (server.hasArg("reset"))
  {
    result["success"] = true;
    server.send(200, "application/json", JSONStringify(result));

    delayedPeriod = 2000;
    delayedTime = millis();
    delayedFunction = &reset;
  }
  else
  {
    result["success"] = false;
    result["message"] = "Unknown command";
    server.send(401, "application/json", JSONStringify(result));
  }
}

// Fonction permettant de construire la page pour l'affichage des informations système
void handleInfos()
{
  String page;

  // Construction des éléments du DOM
  page += FPSTR(HTTP_HEAD_START);
  page += FPSTR(HTTP_REFRESH);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(INFOS_PAGE);

  // Récupération de la tension dans le circuit
  unsigned int vcc = getVcc();

  // Remplacement des valeurs
  page.replace("{title}", "Informations");
  page.replace("{time}", "5");
  page.replace("{url}", "/infos");

  page.replace("{firmware}", FIRMWARE_NAME);
  page.replace("{uptime_mins}", (String)(millis() / 1000 / 60));
  page.replace("{uptime_secs}", (String)((millis() / 1000) % 60));
  page.replace("{vcc}", (String)vcc);

  page.replace("{opentime}", (String)openingTime);
  page.replace("{closetime}", (String)closingTime);

  page.replace("{fchipid}", (String)ESP.getFlashChipId());
  page.replace("{idesize}", (String)ESP.getFlashChipSize());
  page.replace("{flashsize}", (String)ESP.getFlashChipRealSize());
  page.replace("{sdkver}", (String)system_get_sdk_version());
  page.replace("{corever}", (String)ESP.getCoreVersion());
  page.replace("{bootver}", (String)system_get_boot_version());
  page.replace("{cpufreq}", (String)ESP.getCpuFreqMHz());

  page.replace("{mqttenable}", (mqttEnabled ? "Enabled" : "Disabled"));
  page.replace("{hueenable}", (hueEnabled ? "Enabled" : "Disabled"));

  page.replace("{freeheap}", (String)ESP.getFreeHeap());
  page.replace("{memsketch_current}", (String)(ESP.getSketchSize()));
  page.replace("{memsketch_max}", (String)(ESP.getSketchSize() + ESP.getFreeSketchSpace()));
  page.replace("{memsmeter_current}", (String)(ESP.getSketchSize()));
  page.replace("{memsmeter_max}", (String)(ESP.getSketchSize() + ESP.getFreeSketchSpace()));
  page.replace("{restartreason}", (String)ESP.getResetReason());
  page.replace("{ssid}", htmlEntities(WiFi.SSID()));

  page.replace("{ip}", WiFi.localIP().toString());
  page.replace("{mac}", WiFi.macAddress());
  page.replace("{gw}", WiFi.gatewayIP().toString());
  page.replace("{mask}", WiFi.subnetMask().toString());
  page.replace("{dns}", WiFi.dnsIP().toString());

  page.replace("{host}", WiFi.hostname());
  page.replace("{isconnected}", WiFi.isConnected() ? "Yes" : "No");
  page.replace("{autocon}", WiFi.getAutoConnect() ? "Enabled" : "Disabled");

  page += FPSTR(HTTP_END);

  server.send(200, "text/html", page);
}

// Fonction callback pour la lampe utilisée lors d'une changement via Amazon Alexa
void lightDeviceChanged(EspalexaDevice *d)
{
  if (d == nullptr)
    return; // this is good practice, but not required

  if (d->getValue())
  {
    sendCommand("on");
  }
  else
  {
    sendCommand("off");
  }
}

// Fonction callback pour l'abat jour utilisée lors d'une changement via Amazon Alexa
void lampShadeDeviceChanged(EspalexaDevice *d)
{
  if (d == nullptr)
    return; // this is good practice, but not required

  if (d->getValue())
  {
    sendCommand("open");
  }
  else
  {
    sendCommand("close");
  }
}

// Fonction de connexion au serveur MQTT
void MQTTConnect()
{
  // Paramètres de connexion au serveur MQTT
  mqttClient.setServer(config["mqtt"]["server"].as<char *>(), (int)config["mqtt"]["port"]);

  // Reception des messages
  mqttClient.setCallback(mqttCallback);

  // Connexion au serveur MQTT
  if (mqttClient.connect("Eclipse", config["mqtt"]["user"], config["mqtt"]["pass"]))
  {
    /*String topic = config["mqtt"]["topic"];

    topic.replace("%TOPIC%", "test");

    mqttClient.subscribe(topic.c_str());

    mqttClient.publish("eclipse/results", "OK");*/
  }
}

// Fonction permettant de vérifier la disponibilité de nouvelles mises à jour
void checkOTAUpdate()
{
  WiFiClient client;

  switch (ESPhttpUpdate.update(client, HTTP_OTA_URL))
  {
  case HTTP_UPDATE_FAILED:
    lastUpdateStatus = ESPhttpUpdate.getLastErrorString();
    break;

  case HTTP_UPDATE_NO_UPDATES:
    lastUpdateStatus = "No update available";
    break;

  case HTTP_UPDATE_OK:
    lastUpdateStatus = "Update successful";
    break;
  }
}

// Fonction de convertion de puissance du signal WiFi
int getRSSI(int RSSI)
{
  int quality = 0;

  if (RSSI <= -100)
  {
    quality = 0;
  }
  else if (RSSI >= -50)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

String htmlEntities(String str)
{
  str.replace("&", "&amp;");
  str.replace("<", "&lt;");
  str.replace(">", "&gt;");
  str.replace("'", "&#39;");
  return str;
}

// Fonction permattant de mettre l'ESP en mode point d'accès
void startSoftAP(String apName)
{
  // WiFi.mode(WIFI_AP);
  WiFi.persistent(false);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP(apName);
}

// Fonction de récupération des réseaux WiFi disponibles
String getNetworks()
{
  String page;

  int minimumQuality = -1;
  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0)
  {
    page += FPSTR(NO_NETWORKS); // @token nonetworks
    page += "<br/><br/>";
  }
  else
  {
    // sort networks
    int signals[numNetworks];
    for (int i = 0; i < numNetworks; i++)
    {
      signals[i] = i;
    }

    // RSSI SORT
    for (int i = 0; i < numNetworks; i++)
    {
      for (int j = i + 1; j < numNetworks; j++)
      {
        if (WiFi.RSSI(signals[j]) > WiFi.RSSI(signals[i]))
        {
          std::swap(signals[i], signals[j]);
        }
      }
    }

    // remove duplicates ( must be RSSI sorted )
    String cssid;
    for (int i = 0; i < numNetworks; i++)
    {
      if (signals[i] == -1)
        continue;

      cssid = WiFi.SSID(signals[i]);
      for (int j = i + 1; j < numNetworks; j++)
      {
        if (cssid == WiFi.SSID(signals[j]))
        {
          signals[j] = -1; // set dup aps to index -1
        }
      }
    }

    // token precheck, to speed up replacements on large ap lists
    String HTTP_ITEM_STR = FPSTR(HTTP_ITEM);

    // toggle icons with percentage
    HTTP_ITEM_STR.replace("{qp}", FPSTR(HTTP_ITEM_QP));
    HTTP_ITEM_STR.replace("{qi}", FPSTR(HTTP_ITEM_QI));

    // display networks in page
    for (int i = 0; i < numNetworks; i++)
    {
      if (signals[i] == -1)
        continue; // skip dups

      int rssiPerc = getRSSI(WiFi.RSSI(signals[i]));
      int encrypType = WiFi.encryptionType(signals[i]);

      if (minimumQuality == -1 || minimumQuality < rssiPerc)
      {
        String item = HTTP_ITEM_STR;
        item.replace("{ssid}", htmlEntities(WiFi.SSID(signals[i])));               // ssid no encoding
        item.replace("{power}", (String)rssiPerc);                                 // rssi percentage 0-100
        item.replace("{level}", (String) int(round(map(rssiPerc, 0, 100, 1, 4)))); // quality icon 1-4

        if (encrypType != ENC_TYPE_NONE)
        {
          item.replace("{lock}", "lock");
        }
        else
        {
          item.replace("{lock}", "");
        }
        page += item;
      }
    }
    page += "<br/>";
  }

  String pitem = FPSTR(HTTP_FORM_WIFI);

  pitem.replace("{ssid}", config["wifi"]["ssid"] | "");
  pitem.replace("{password}", config["wifi"]["pass"] | "");

  page += pitem;

  if (WiFi.isConnected())
  {
    page += FPSTR(HTTP_FORM_WIFI_PARAMS);
    page.replace("{checked}", (config["wifi"]["static"] ? "checked" : ""));

    page.replace("{ip}", config["wifi"]["ip"] | "");
    page.replace("{subnet}", config["wifi"]["subnet"] | "");
    page.replace("{gateway}", config["wifi"]["gateway"] | "");
    page.replace("{dns}", config["wifi"]["dns"] | "");

    page.replace("{pip}", WiFi.localIP().toString());
    page.replace("{psubnet}", WiFi.subnetMask().toString());
    page.replace("{pgateway}", WiFi.gatewayIP().toString());
    page.replace("{pdns}", WiFi.dnsIP().toString());
  }

  page += FPSTR(HTTP_FORM_WIFI_BACK);

  return page;
}

// Fonction permettant de lancer des méthode après un délai défini
void checkDelayed()
{
  if (delayedFunction != NULL)
  {
    if (millis() - delayedTime >= delayedPeriod)
    {
      (*delayedFunction)();
      delayedFunction = NULL;
      return;
    }
  }
}

// Fonction de connexion à un nouveau réseau
void connectNewWifi()
{
  bool isConnected = wifiConnect(newSSID, newPass);

  // Si les identifiants ne sont pas corrects
  if (!isConnected)
  {
    wifiConnect(config["wifi"]["ssid"], config["wifi"]["pass"]);
  }
  else
  {
    // Sauvegarde des nouveaux identifiants
    config["wifi"]["ssid"] = newSSID;
    config["wifi"]["pass"] = newPass;

    saveConfig();
  }
}

// Fonction d'arrêt du mode point d'accès
void endSoftAP()
{
  WiFi.softAPdisconnect(true);
}

// Fonction de connexion à un réseau WiFi
bool wifiConnect(String ssid, String password)
{
  int retry = 0;

  WiFi.persistent(false);

  if (config["wifi"] && config["wifi"]["static"])
  {
    WiFi.config(str2IP(config["wifi"]["ip"]), str2IP(config["wifi"]["gateway"]), str2IP(config["wifi"]["subnet"]), str2IP(config["wifi"]["dns"]));
  }

  WiFi.begin(ssid, password);

  while (!WiFi.isConnected())
  {
    if (retry > 5)
    {
      return false;
    }

    delay(1000);
    retry++;
  }

  // Activation de la reconnexion en cas de déconnexion
  WiFi.setAutoReconnect(true);

  lastWifiConnected = true;

  return true;
}

void checkStatus()
{
  if (millis() - statusTime >= 1000)
  {
    // Recupération de l'état de la lampe
    lightStatus = (digitalRead(pinRelay) == HIGH) ? "on" : "off";

    if (mqttEnabled)
    {
      // Reconnection du client MQTT
      if (!mqttClient.connected())
      {
        // Tentative de reconnexion toutes les 3 secondes
        if (millis() - mqttReconnectTime >= 3000)
        {
          mqttConnect(config["mqtt"]["client_name"], config["mqtt"]["user"], config["mqtt"]["pass"], config["mqtt"]["topic"]);
          mqttReconnectTime = millis();
        }
      }
    }

    statusTime = millis();
  }
}

// Fonction d'apprentissage pour obtenir les délais d'ouverture et de fermeture de l'abat-jour
void startLearning()
{
  int maxDropVoltage = config["learn"]["max_drop_voltage"].as<int>() | 150;
  int maxDetectionTime = config["learn"]["max_detection_time"].as<int>() | 30;

  // Init délais
  int closeTime = 0;
  int openTime = 0;

  // Arrêt du moteur
  motorStop();

  // Obtention de la tension de référence dans le circuit
  long referenceTotal = 0;

  // Eteint la lumière
  digitalWrite(pinRelay, LOW);

  delay(3000);

  // On effectue 10 mesures de tension toutes les 500 ms
  for (int i = 0; i < 10; i++)
  {
    referenceTotal = referenceTotal + getVcc();
    delay(500);
  }

  // Voltage moyen dans le circuit
  int referenceAverage = referenceTotal / 10;

  delay(1000);

  // Démarrage moteur -> Sens fermeture
  digitalWrite(pinA, HIGH);
  digitalWrite(pinB, LOW);

  bool isDown = false;
  bool isUp = false;

  int i = 0;

  // Boucle prise de mesure chute de tension -> fermeture
  while (i < maxDetectionTime)
  {
    // Si la valeur de chute de tension et supérieure à celle définie
    if ((referenceAverage - getVcc()) > maxDropVoltage)
    {
      isDown = true;
      // Arrêt du moteur
      motorStop();

      delay(500);

      referenceAverage = getVcc();
      break;
    }

    delay(1000);
    i++;
  }

  // Si fermé
  if (isDown)
  {
    delay(1000);

    // Démarrage moteur -> Sens ouverture
    digitalWrite(pinA, LOW);
    digitalWrite(pinB, HIGH);

    i = 0;

    // Boucle prise de mesure chute de tension -> ouverture
    while (i < maxDetectionTime)
    {
      // Si la valeur de chute de tension et supérieure à celle définie
      if ((referenceAverage - getVcc()) > maxDropVoltage)
      {
        // Délai d'ouverture en seconde
        openTime = i;
        isUp = true;

        // Arrêt du moteur
        motorStop();

        delay(500);

        referenceAverage = getVcc();
        break;
      }

      delay(1000);
      i++;
    }
  }
  else
  {
    Serial.println("Opening time not detected");
    motorStop();
    return;
  }

  // Si ouvert
  if (isUp)
  {

    delay(1000);

    // Démarrage moteur -> Sens fermeture
    digitalWrite(pinA, HIGH);
    digitalWrite(pinB, LOW);

    i = 0;

    // Boucle prise de mesure chute de tension -> fermeture
    while (i < maxDetectionTime)
    {
      // Si la valeur de chute de tension et supérieure à celle définie
      if ((referenceAverage - getVcc()) > maxDropVoltage)
      {
        // Délai de fermeture
        closeTime = i;

        motorAction = 0;

        // Arrêt du moteur
        motorStop();

        delay(500);

        referenceAverage = getVcc();
        break;
      }

      delay(1000);
      i++;
    }
  }
  else
  {
    Serial.println("Closing time not detected");
    motorStop();
    return;
  }

  // Arrêt du moteur
  motorStop();

  // On sauvegarde les délais dans la config
  config["open_time"] = openTime;
  config["close_time"] = closeTime;

  saveConfig();

  // Défininition des délais d'ouverture/fermeture
  openingTime = openTime;
  closingTime = closeTime;

  // On fait clignoter 3 fois la lumière
  // lightBlink(3, 700);

  // Affichage des délais
  Serial.print("opening time ");
  Serial.print(openTime);
  Serial.println(" seconds");

  Serial.print("closing time ");
  Serial.print(closeTime);
  Serial.println(" seconds");
}

// Fonction permettant d'arrêter le moteur
void motorStop()
{
  // Init
  isRunning = false;

  // Si action d'ouverture
  if (motorAction == 0)
  {
    isClosed = true;
  }

  // Si action de d'ouverture
  else if (motorAction == 1)
  {
    isClosed = false;
  }

  // Sauvegarde dans la config
  config["status"]["lampshade"] = isClosed;

  lampshadeStatus = isClosed ? "closed" : "opened";

  if (mqttEnabled)
  {
    mqttPublish("stat", "RESULT", "{\"LIGHT\":\"" + lightStatus + "\",\"LAMPSHADE\":\"" + lampshadeStatus + "\"}");
    mqttPublish("stat", "LAMPSHADE", lampshadeStatus);
  }

  // Init action du motion
  motorAction = -1;
  // Arrêt pins L293D
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  Serial.println("Motor is stopped");

  saveConfig();
}

// Fonction permettant de définir la vitesse du moteur
void setSpeed(int val)
{
  val = map(val, 0, 100, 0, 255);
  analogWrite(pinSpeed, val);
}

// Fonction permettant de démarrer le moteur lorsqu'une action est demandée
void checkMotor()
{
  bool syncLight = config["sync_light"].as<bool>() | false;

  if (isRunning)
  {
    if (millis() - motorMotionTime >= motionDuration)
    {
      if (motorAction == 0) // Fermeture
      {
        Serial.println("Lampshade is closed");
        lampshadeStatus = "closed";

        if (syncLight)
        {
          sendCommand("off");
        }
      }
      else if (motorAction == 1) // Ouverture
      {
        Serial.println("Lampshade is opened");
        lampshadeStatus = "opened";
      }

      // Arrêt du moteur
      motorStop();
    }

    return;
  }

  // Si changement d'action
  if (!isRunning && motorAction > -1)
  {
    isRunning = true;
    // Réinitilisation de la tempo
    motorMotionTime = millis();

    // Vitesse du moteur
    setSpeed(motorSpeed);

    if (motorAction == 0) // Fermeture de l'abat jour
    {
      // Activation/désactivation des pins L293D SENS A
      digitalWrite(pinA, HIGH);
      digitalWrite(pinB, LOW);
      Serial.println("Lampshade is closing");
      lampshadeStatus = "closing";
    }
    else if (motorAction == 1) // Ouverture de l'abat jour
    {
      // Activation/désactivation des pins L293D SENS B
      digitalWrite(pinA, LOW);
      digitalWrite(pinB, HIGH);
      Serial.println("Lampshade is opening");
      lampshadeStatus = "opening";

      if (syncLight)
      {
        sendCommand("on");
      }
    }

    if (mqttEnabled)
    {
      mqttPublish("stat", "RESULT", "{\"LIGHT\":\"" + lightStatus + "\",\"LAMPSHADE\":\"" + lampshadeStatus + "\"}");
      mqttPublish("stat", "LAMPSHADE", lampshadeStatus);
    }
  }
}

void sendCommand(String cmd)
{
  if (cmd == "on")
  {
    digitalWrite(pinRelay, HIGH);
  }
  else if (cmd == "off")
  {
    digitalWrite(pinRelay, LOW);
  }
  else if (cmd == "toggle")
  {
    digitalWrite(pinRelay, !digitalRead(pinRelay));
  }
  else if (cmd == "open" && openingTime > 0 && isClosed)
  {
    motorAction = 1;
    motionDuration = openingTime * 1000;
  }
  else if (cmd == "close" && closingTime > 0 && !isClosed)
  {
    if (closingTime > 0)
    {
      motorAction = 0;
      motionDuration = closingTime * 1000;
    }
  }
  else if (cmd == "learn")
  {

    delayedPeriod = 2000;
    delayedTime = millis();
    delayedFunction = &startLearning;
  }

  if (cmd == "on" || cmd == "off" || cmd == "toggle")
  {

    lightStatus = (digitalRead(pinRelay) == HIGH) ? "on" : "off";

    if (mqttEnabled)
    {
      mqttPublish("stat", "RESULT", "{\"LIGHT\":\"" + lightStatus + "\",\"LAMPSHADE\":\"" + lampshadeStatus + "\"}");
      mqttPublish("stat", "LIGHT", lightStatus);
    }

    if (restoreLastState)
    {

      config["status"]["light"] = digitalRead(pinRelay);
      saveConfig();
    }
  }
}

void runExtra()
{
  String hostname = getHostname();
  // Si connecté au réseau Wifi
  if (WiFi.status() == WL_CONNECTED)
  {

    if (config.containsKey("hue") && config["hue"].containsKey("enabled") && config["hue"]["enabled"].as<bool>() == true)
    {
      lightDevice = new EspalexaDevice(config["hue"]["light_name"] | hueLightName, lightDeviceChanged, EspalexaDeviceType::onoff);
      espalexa.addDevice(lightDevice);

      lampShadeDevice = new EspalexaDevice(config["hue"]["lampshade_name"] | hueLampshadeName, lampShadeDeviceChanged, EspalexaDeviceType::onoff);
      espalexa.addDevice(lampShadeDevice);

      espalexa.begin(&server);

      hueEnabled = true;
    }

    if (config.containsKey("mqtt") && config["mqtt"].containsKey("enabled") && config["mqtt"]["enabled"].as<bool>() == true)
    {
      mqttClient.setServer(config["mqtt"]["host"].as<char *>(), config["mqtt"]["port"].as<int>());
      mqttClient.setCallback(mqttCallback);

      mqttConnect(config["mqtt"]["client"] | "", config["mqtt"]["user"] | "", config["mqtt"]["pass"] | "", config["mqtt"]["in_topic"] | "");

      mqttPublish("tele", "LWT", "online", true);
      mqttPublish("stat", "RESULT", "{\"LIGHT\":\"" + lightStatus + "\",\"LAMPSHADE\":\"" + lampshadeStatus + "\"}");

      mqttEnabled = true;
    }

    statusTime = millis();

#ifdef USEOTA
    beginOTA(hostname.c_str());
#endif
  }
}

void setup()
{
  WiFi.mode(WIFI_AP_STA);
  Serial.begin(115200);

  // Configuration des entrées/sorties du contrôleur ATMEGA328P
  pinMode(pinA, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinSpeed, OUTPUT);
  pinMode(pinRelay, OUTPUT);
  pinMode(pinSwitch, INPUT_PULLUP);

  setSpeed(100);

  // Changement de la configuration
  loadConfig();

  // Routes
  server.on("/cmd", handleCommand);
  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/wifi", handleWifi);
  server.on("/wifisave", HTTP_POST, handleWifiSave);
  server.on("/system", handleSystem);
  server.on("/systemsave", HTTP_POST, handleSystemSave);
  server.on("/mqtt", handleMQTT);
  server.on("/mqttsave", HTTP_POST, handleMQTTSave);
  server.on("/hue", handleHUE);
  server.on("/huesave", HTTP_POST, handleHUESave);
  server.on("/update", handleUpdate);
  server.on("/infos", handleInfos);
  server.on("/script.js", handleJSLib);
  server.onNotFound(handleNotFound);

  // Démarrage du serveur web
  server.begin();

  String hostname = getHostname();

  closingTime = config["close_time"].as<int>() | 0;
  openingTime = config["open_time"].as<int>() | 0;

  restoreLastState = config["restore_state"].as<bool>() | false;

  config["sync_light"] = true;

  if (restoreLastState)
  {
    digitalWrite(pinRelay, config["status"]["light"]);
  }

  MDNS.begin(hostname);

  lightStatus = (digitalRead(pinRelay) == HIGH) ? "on" : "off";
  lampshadeStatus = config["status"]["lampshade"] ? "closed" : "opened";

  isClosed = (lampshadeStatus == "closed");

  // Si une configuration wifi est présente
  if (config.containsKey("wifi") && config["wifi"].containsKey("ssid"))
  {
    WiFi.hostname(hostname);
    wifiConnect(config["wifi"]["ssid"], config["wifi"]["pass"] | "");
  }
  else
  {
    startSoftAP(hostname);
  }


  delayedPeriod = 5000;
  delayedTime = millis();
  delayedFunction = &runExtra;
}

void loop()
{
#ifdef USEOTA
  ArduinoOTA.handle();
#endif

  MDNS.update();

  if (digitalRead(pinSwitch) == HIGH)
  {
    motorAction = 0;
    motionDuration = 10000;
  }

  server.handleClient();

  checkDelayed();

  checkStatus();

  // Vérification des ordres moteur
  checkMotor();

  if (WiFi.isConnected())
  {
    if (mqttEnabled)
    {
      mqttClient.loop();
    }

    if (hueEnabled)
    {
      espalexa.loop();
    }
  }
}