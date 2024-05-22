#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Adafruit_BMP280.h>
#include "DHT.h"

#define ONBOARD_LED 2 // Altere para o pino do LED onboard do seu ESP32
#define SEALEVELPRESSURE_HPA (1013.25)

#define DHTPIN 4    // Pino do sensor DHT11
#define DHTTYPE DHT11   // Tipo do sensor DHT
#define LDRPIN 34   // Pino do sensor LDR
#define SENSORPIN 5 // Pino do sensor de Chuva

DHT dht(DHTPIN, DHTTYPE);   // Objeto para o sensor DHT11
Adafruit_BMP280 bmp; // I2C

const char* ntpServer = "pool.ntp.org";

int chuva;
int ldrValue = 0;
float umidade = 0;
float temperatura = 0;
float pressao = 0;
float altitude = 0;
float luminosidade = 0;
bool erro;

// Forward declarations
void handleRoot();
void handleForm();
void connectToWiFi(String ssidWifi, String passwordWifi);
void connectEeprom();
void sendDados(HTTPClient &http, float tensaoRede, float correnteRede, float tensaoPlaca, float correntePlaca, float bateria);
HTTPClient httpClient;

const char MAIN_page[] = R"=====(
<!DOCTYPE html>
<html lang="en">
    <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
    <title>Weather Wizard</title>
    <style>
        body, html {
            display: flex;
            justify-content: center;
            align-items: center;
            width: 100%;
            height: 100%;
            color: #434343;
            font-family: "Helvetica Neue",Helvetica,Arial,sans-serif;
            font-size: 14px;
            background-color: #eeeeee;
        }
        .container {
        margin: 0 auto;
        width: 70%;
        padding: 30px;
        box-shadow: 0 10px 20px rgba(0,0,0,0.19), 0 6px 6px rgba(0,0,0,0.23);
        background-color: #ffffff;
        border-radius: 10px;
        }
        h2 {
        text-align: center;
        margin-bottom: 20px;
        margin-top: 0px;
        color: #0ee6b1;
        font-size: 48px;
        }
        #titleGreen {
        color: #008cff;
        }
        #titleBlack {
        color: #000000;
        }
        h3 {
        text-align: center;
        margin-bottom: 40px;
        margin-top: 0px;
        color: #008cff;
        font-size: 44px;
        }
        form .field-group {
        box-sizing: border-box;
        clear: both;
        padding: 4px 0;
        position: relative;
        margin: 1px 0;
        width: 100%;
        }
        .text-field {
        font-size: 34px;
        margin-bottom: 4%;
        -webkit-appearance: none;
        display: block;
        background: #fafafa;
        color: #636363;
        width: 100%;
        padding: 15px 0px 15px 0px;
        text-indent: 10px;
        border-radius: 5px;
        border: 1px solid #e6e6e6;
        background-color: transparent;
        }
        .text-field:focus {
        border-color: #00bcd4;
        outline: 0;
        }
        .button-container {
        box-sizing: border-box;
        clear: both;
        margin: 1px 0 0;
        padding: 4px 0;
        position: relative;
        width: 100%;
        }
        .button {
        background: #008cff;
        border: none;
        border-radius: 5px;
        color: #ffffff;
        cursor: pointer;
        display: block;
        font-weight: bold;
        font-size: 34px;
        padding: 15px 0;
        text-align: center;
        text-transform: uppercase;
        width: 100%;
        -webkit-transition: background 250ms ease;
        -moz-transition: background 250ms ease;
        -o-transition: background 250ms ease;
        transition: background 250ms ease;
        }
        p {
        text-align: center;
        text-decoration: none;
        color: #008cff;
        font-size: 28px;
        }
        a {
        text-decoration: none;
        color: #ffffff;
        margin-top: 0%;
        }
        #status {
        text-align: center;
        text-decoration: none;
        color: #0ee6b1;;
        font-size: 28px;
        }
    </style>
    <script>
        function validateForm() {
        var ssid = document.forms["myForm"]["ssid"].value;
        var password = document.forms["myForm"]["password"].value;
        var status = document.getElementById("statusDiv");
        if (ssid == "" && password == "") {
            status.innerHTML = "<p id='status' style='color:red;'>Insira SSID e senha.</p>";
            return false;
        }
        else if (ssid == "") {
            status.innerHTML = "<p id='status' style='color:red;'>Insira SSID.</p>";
            return false;
        }
        else if (password == "") {
            status.innerHTML = "<p id='status' style='color:red;'>Insira senha.</p>";
            return false;
        }
        else {
            status.innerHTML = "<p id='status'>Conectando...</p>";
            return true;
        }
        }
    </script>
    </head>
    <body>
    <div class="container">
        <h2><span id="titleGreen">Weather </span><span id="titleBlack">Wizard</span></h2>
        <h3>Conectar à Rede</h3>
        <form name="myForm" action="/action_new_connection" onsubmit="return validateForm()" method="post">
        <div class="field-group">
            <select class='text-field' name='ssid'></select>
        </div>
        <br>
        <div class="field-group">
            <input class="text-field" type="password" name="password" length=64 placeholder="Password">
        </div>
        <br>
        <div id="statusDiv">
            <br><br>
        </div>
        <div class="button-container">
            <input class="button" type="submit" value="Conectar">
        </div>
        </form>
        <p>OU</p>
        <div class="button-container">
        <button class="button" type="button" onclick="window.location.href='/action_previous_connection'">Conectar à última rede utilizada</button>
        </div>
    </div>
    </body>
</html>
)=====";

const char *ssid = "Weather-AP"; // Nome da rede WiFi que será criada
const char *password = "weatherAP";   // Senha para se conectar nesta rede
WebServer server(80); // Server utiliza a porta 80


void setup() {
  pinMode(ONBOARD_LED, OUTPUT); // LED onboard
  pinMode(SENSORPIN, INPUT); // LED onboard
  Serial.begin(115200);


  WiFi.softAP(ssid, password);             
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" iniciado");

  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());         
  // Tratamento de rotas
  server.on("/", handleRoot);      
  server.on("/action_new_connection", handleForm);
  server.on("/action_previous_connection", connectEeprom); 
 
  server.begin();                 
  Serial.println("Servidor HTTP iniciado");

  dht.begin();

  Serial.println(F("BMP280 test"));
    unsigned status;
    //status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
    status = bmp.begin();
    if (!status) {
      Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                        "try a different address!"));
      Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
      Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
      Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
      Serial.print("        ID of 0x60 represents a BME 280.\n");
      Serial.print("        ID of 0x61 represents a BME 680.\n");
      while (1) delay(10);
    }

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    // Configurar o cliente NTP
    configTime(0, 0, ntpServer);
    // Esperar até que o tempo seja atualizado a partir do servidor NTP
    Serial.println("Aguardando sincronização de tempo...");
    while (!time(nullptr)) {
      delay(1000);
    }
}

void loop() {
  
  while (WiFi.status() != WL_CONNECTED) {
    server.handleClient(); // Trata requisições de clientes
    digitalWrite(ONBOARD_LED, HIGH); // Ativa o LED
    delay(500); // Aguarda 500 milissegundos antes de verificar novamente
  }

  if(WiFi.status() == WL_CONNECTED) {
     digitalWrite(ONBOARD_LED, LOW); // Ativa o LED
  }

  // Obter o tempo atual
  time_t now = time(nullptr);
  struct tm* timeinfo;
  timeinfo = localtime(&now);

  if (timeinfo->tm_min % 30 == 0 && timeinfo->tm_sec == 0) {
    umidade = dht.readHumidity();         // Umidade do sensor DHT11
    temperatura = bmp.readTemperature();
    pressao = bmp.readPressure(); 
    altitude = bmp.readAltitude(1013.25);
    ldrValue = analogRead(LDRPIN);
    luminosidade = map(ldrValue, 0, 4095, 0, 100);  // Convertendo para tensão (0-3.3V)
    if(digitalRead(SENSORPIN)){
      chuva = 1;
    }else{
      chuva = 0;
    }
    Serial.print(F("Umidade: "));
    Serial.println(umidade);
    Serial.print(F("Temperature = "));
    Serial.print(temperatura);
    Serial.println(" *C");
    Serial.print(F("Pressure = "));
    Serial.print(pressao);
    Serial.println(" Pa");
    Serial.print(F("Approx altitude = "));
    Serial.print(altitude); /* Adjusted to local forecast! */
    Serial.println(" m"); 
    Serial.print(F("Luminosidade "));
    Serial.println(luminosidade); 
    Serial.print(F("Chuva "));
    Serial.println(chuva); 
    sendDados(httpClient, umidade, temperatura, pressao, altitude, luminosidade, chuva);
  }

  if (timeinfo->tm_min % 1 == 0 && timeinfo->tm_sec == 0) {
    umidade = dht.readHumidity();         // Umidade do sensor DHT11
    temperatura = bmp.readTemperature();
    pressao = bmp.readPressure(); 
    altitude = bmp.readAltitude(1013.25);
    ldrValue = analogRead(LDRPIN);
    luminosidade = map(ldrValue, 0, 4095, 0, 100);  // Convertendo para tensão (0-3.3V)
    if(digitalRead(SENSORPIN)){
      chuva = 1;
    }else{
      chuva = 0;
    }
    Serial.print(F("Umidade: "));
    Serial.println(umidade);
    Serial.print(F("Temperature = "));
    Serial.print(temperatura);
    Serial.println(" *C");
    Serial.print(F("Pressure = "));
    Serial.print(pressao);
    Serial.println(" Pa");
    Serial.print(F("Approx altitude = "));
    Serial.print(altitude); /* Adjusted to local forecast! */
    Serial.println(" m"); 
    Serial.print(F("Luminosidade "));
    Serial.println(luminosidade); 
    Serial.print(F("Chuva "));
    Serial.println(chuva); 
    delay(1000);
    getDados(httpClient, umidade, temperatura, pressao, altitude, luminosidade, chuva);
  }
  error(erro);
  delay(1000);
}

void sendDados(HTTPClient &http, float umidade, float temperatura, float pressao, float altitude, float luminosidade, int chuva) {
  String serverEndpoint = "https://api-esp-edf6240880d2.herokuapp.com/postDadosEsp";

  // Usa a mesma instância httpClient para manter a conexão aberta
  http.begin(serverEndpoint);
  http.addHeader("Content-Type", "application/json");

  String jsonData = "{\"umidade\": " + String(umidade) +
                    ", \"temperatura\": " + String(temperatura) +
                    ", \"pressao\": " + String(pressao) +
                    ", \"altitude\": " + String(altitude) +
                    ", \"luminosidade\": " + String(luminosidade) +
                    ", \"chuva\": " + String(chuva) + "}";

  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode == 200) {
    Serial.print("Dados enviado com sucesso. Resposta do servidor: ");
    Serial.println(http.getString());
    erro = false;
    digitalWrite(ONBOARD_LED, LOW);
  } else {
    Serial.print("Erro ao enviar os Dados. Código de resposta HTTP: ");
    Serial.println(httpResponseCode);
    erro = true;
  }

  // Não chama http.end() aqui para manter a conexão aberta
}

void getDados(HTTPClient &http, float umidade, float temperatura, float pressao, float altitude, float luminosidade, int chuva) {
  String serverEndpoint = "https://api-esp-edf6240880d2.herokuapp.com/getDados";

  // Usa a mesma instância httpClient para manter a conexão aberta
  http.begin(serverEndpoint);
  http.addHeader("Content-Type", "application/json");

  String jsonData = "{\"umidade\": " + String(umidade) +
                    ", \"temperatura\": " + String(temperatura) +
                    ", \"pressao\": " + String(pressao) +
                    ", \"altitude\": " + String(altitude) +
                    ", \"luminosidade\": " + String(luminosidade) +
                    ", \"chuva\": " + String(chuva) + "}";

  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode == 200) {
    Serial.print("Dados enviado com sucesso. Resposta do servidor: ");
    Serial.println(http.getString());
    erro = false;
    digitalWrite(ONBOARD_LED, LOW);
  } else {
    Serial.print("Erro ao pegar os Dados. Código de resposta HTTP: ");
    Serial.println(httpResponseCode);
    erro = true;
  }

  // Não chama http.end() aqui para manter a conexão aberta
}

void handleRoot() {
  String index = listSSID(); // Leia o conteúdo HTML
  server.send(200, "text/html", index); // Enviar página Web
}

void handleForm() {
  String ssidWifi = server.arg("ssid");
  String passwordWifi = server.arg("password");

  Serial.printf("SSID: %s\n", ssidWifi.c_str());
  Serial.printf("Password: %s\n", passwordWifi.c_str());

  if(!ssidWifi.equals("") && !passwordWifi.equals("")) {
    connectToWiFi(ssidWifi, passwordWifi);
  }
}

void connectToWiFi(String ssidWifi, String passwordWifi) {
  int count = 0;
  WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());     // Conecta com seu roteador 
  Serial.println("");

  // Espera por uma conexão
  while ( count < 15 ) {
    delay(500);
    Serial.print(".");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      salvarEeprom(ssidWifi, passwordWifi);
      Serial.println("");
      // Se a conexão ocorrer com sucesso, mostre o endereço IP no monitor serial
      Serial.println("Conectado ao WiFi");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());  // Endereço IP do ESP32
      digitalWrite(ONBOARD_LED, LOW); // Acende o LED
      String responsePage = (const __FlashStringHelper*) MAIN_page; // Leia o conteúdo HTML
      responsePage.replace("<br><br>", "<p id='status'>Conectado!</p>");
      server.send(200, "text/html", responsePage);
      return;
    }
    else if (WiFi.status() == WL_CONNECT_FAILED) {
      String responsePage = (const __FlashStringHelper*) MAIN_page;
      responsePage.replace("<br><br>", "<p id='status' style='color:red;'>Falha na conexão.</p>");
      server.send(200, "text/html", responsePage);
    }
    count++;
  }
  Serial.println();
  Serial.println("Timed out.");
  String responsePage = (const __FlashStringHelper*) MAIN_page; // Leia o conteúdo HTML
  responsePage.replace("<br><br>", "<p id='status' style='color:red;'>Erro.</p>");
  server.send(200, "text/html", responsePage);
  return;
}

String listSSID() {
  String index = (const __FlashStringHelper*) MAIN_page; // Leia o conteúdo HTML
  String networks = "";
  int n = WiFi.scanNetworks();
  Serial.println("Scan done.");
  if (n == 0) {
    Serial.println("Nenhuma rede encontrada.");
    index.replace("<select class='text-field' name='ssid'></select>", "<select class='text-field' name='ssid'><option value='' disabled selected>Nenhuma rede encontrada</option></select>");
    index.replace("<br><br>", "<p id='status' style='color:red;'>Rede não encontrada.</p>");
    return index;
  }
  else {
    Serial.printf("%d networks found.\n", n);
    networks += "<select class='text-field' name='ssid'><option value='' disabled selected>SSID</option>";
    for (int i = 0; i < n; ++i)
    {
      // Imprime o SSID de cada rede encontrada
      networks += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
    }
    networks += "</select>";
  }
  index.replace("<select class='text-field' name='ssid'></select>", networks);
  return index;
}

void salvarEeprom(String ssidWifi, String passwordWifi) {
EEPROM.begin(98); // Tamanho da FLASH reservado para EEPROM. Pode ser de 4 a 4096 bytes

  if (!compareEeprom(ssidWifi, passwordWifi)) {
    Serial.println("Salvando:");
    EEPROM.write(0, ssidWifi.length());
    Serial.println(ssidWifi.length());

    for (int i = 2; i < 2 + ssidWifi.length(); i++) {
      Serial.print(ssidWifi.charAt(i - 2));
      EEPROM.write(i, ssidWifi.charAt(i - 2));
    }
    Serial.println("");

    Serial.println("Salvando:");
    EEPROM.write(1, passwordWifi.length());
    Serial.println(passwordWifi.length());

    for (int j = 2 + ssidWifi.length(); j < 2 + ssidWifi.length() + passwordWifi.length(); j++) {
      Serial.print(passwordWifi.charAt(j - 2 - ssidWifi.length()));
      EEPROM.write(j, passwordWifi.charAt(j - 2 - ssidWifi.length()));
    }
    Serial.println("");

    EEPROM.commit(); // Salva alterações na FLASH
  }
  EEPROM.end(); // Apaga a cópia da EEPROM salva na RAM
}

boolean compareEeprom(String ssidWifi, String passwordWifi) {
  int idLength = int(EEPROM.read(0)); // Tamanho do SSID armazenado (número de bytes)
  int passLength = int(EEPROM.read(1)); // Tamanho do Password armazenado (número de bytes)
  String id = "";
  String pass = "";

  Serial.println("Lendo SSID:");
  Serial.print("Tamanho:");
  Serial.println(idLength);
  for (int i = 2; i < 2 + idLength; i++) {
    Serial.print("Posição ");
    Serial.print(i);
    Serial.print(": ");
    id = id + char(EEPROM.read(i));
    Serial.println(id[i - 2]);
  }
  Serial.println("");

  Serial.println("Lendo senha:");
  Serial.print("Tamanho:");
  Serial.println(passLength);
  for (int j = 2 + idLength; j < 2 + idLength + passLength; j++) {
    Serial.print("Posição ");
    Serial.print(j);
    Serial.print(": ");
    pass = pass + char(EEPROM.read(j));
    Serial.println(pass[j - 2 - idLength]);
    Serial.println(pass);
  }
  Serial.println("");

  Serial.print("SSID é igual: ");
  Serial.println(id.equals(ssidWifi));

  Serial.print("Senha é igual: ");
  Serial.println(pass.equals(passwordWifi));

  if (id.equals(ssidWifi) && pass.equals(passwordWifi)) {
    Serial.println("Dados já presentes na memória.");
    return true;
  } else {
    return false;
  }
}

void connectEeprom() {
  EEPROM.begin(98); // Tamanho da FLASH reservado para EEPROM. Pode ser de 4 a 4096 bytes

  int ssidSize = (int)EEPROM.read(0); // Tamanho do SSID armazenado (número de bytes)
  int passwordSize = (int)EEPROM.read(1); // Tamanho do Password armazenado (número de bytes)
  String ssidWifi = "";
  String passwordWifi = "";

  Serial.println("Lendo:");
  for (int i = 2; i < 2 + ssidSize; i++) {
    Serial.print(char(EEPROM.read(i)));
    ssidWifi.concat(char(EEPROM.read(i)));
  }
  Serial.println("");

  Serial.println("Lendo:");
  for (int j = 2 + ssidSize; j < 2 + ssidSize + passwordSize; j++) {
    Serial.print(char(EEPROM.read(j)));
    passwordWifi.concat(char(EEPROM.read(j)));
  }
  Serial.println("");

  EEPROM.end(); // Apaga a cópia da EEPROM salva na RAM

  Serial.println("Leu:");
  Serial.println(ssidWifi);
  Serial.println(passwordWifi);

  connectToWiFi(ssidWifi, passwordWifi);
}
void error(bool erro){
  if(erro == true){
    digitalWrite(ONBOARD_LED, HIGH); // Ativa o LED
    delay(1000);
    digitalWrite(ONBOARD_LED, LOW); // Ativa o LED
    delay(1000);
  }
}