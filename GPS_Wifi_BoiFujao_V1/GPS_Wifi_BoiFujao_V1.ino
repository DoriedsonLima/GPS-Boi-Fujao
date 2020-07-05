//  TCC - MONITORAMENTO DE BOVINOS POR GEOLOCALIZAÇÃO
//  16/06/2020
//
//  GY-GPS6MV2 --> NodeMcu8266 -> Servidor Helix
//
//  VCC   ------->  3.3V
//  GND   ------->  GND
//  TX    ------->  D1 (GPIO5) - RX
//  RX    ------->  D2 (GPIO4) - TX

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>

static const int RXPin = 5, TXPin = 4;
byte Second, Minute, Hour, Day, Month;
int Year, httpCode = 0;
String date_str, time_str, lat_str, lng_str, httpCode_str;
double latitude, longitude;

//********************************************************************************
const char * ssid = "NOME_SUA_REDE_WIFI";  //SSID da rede wifi
const char * senha = "SUA_SENHA_WIFI";    //senha da rede wifi
//********************************************************************************

//*************************** HELIX **********************************************
//
String helixAddress = "IP Helix Sandbox:porta/v2/entities/";

//Entidade no Helix
String IdDevice = "Id da entidade";

//Endereco completo para a entidade no Helix
String fullAddress = helixAddress + IdDevice + "/attrs?";

//********************************************************************************

TinyGPSPlus gps; // Cria objeto TinyGPS ++

SoftwareSerial ss(RXPin, TXPin); // Conexão serial ao dispositivo GPS

WiFiServer server(80);

//-------------------------------------SETUP-------------------------------
void setup() {
  Serial.begin(9600);      //Conexao serial a 9600 baund com monitor serial
  ss.begin(9600);          //Comunicação serial a 9600 baund com o GPS
  pinMode(LED_BUILTIN, OUTPUT);

  delay(1000);

  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, senha);             //Inicia conexão WiFi

  while (WiFi.status() != WL_CONNECTED) //Aguarda conexão wifi
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado !");

  server.begin();
  Serial.println ("Servidor para pagina iniciado !");
  Serial.println("");

  Serial.println(WiFi.localIP());     // Imprime o endereço IP obtido na rede
  delay(1000);
}

//-------------------------------------LOOP-------------------------------
void loop() {

  while (ss.available() > 0)
    if (gps.encode( ss.read() ))
    {
      if (gps.time.isValid())       // Obtem o time UTC 
      {
        time_str = "";
        Minute = gps.time.minute();
        Second = gps.time.second();
        Hour   = gps.time.hour();
        time_str += String(Hour) + ":";
        time_str += String(Minute) + ":";
        time_str += String(Second);

        Serial.println("------------------------------------------------------");
        Serial.print("Hora UTC: ");
        Serial.print(time_str);
        Serial.println("");
      }

      if (gps.date.isValid())     // Obtem a data
      {
        date_str = "";
        Day   = gps.date.day();
        Month = gps.date.month();
        Year  = gps.date.year();
        date_str += String(Day) + "/";
        date_str += String(Month) + "/";
        date_str += String(Year);

        Serial.print("Data: ");
        Serial.print(date_str);
        Serial.println("");
      }
      else
      {
        Serial.println("");
        Serial.println("Data invalida !");
      }

      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        lat_str = String(latitude, 5);

        longitude = gps.location.lng();
        lng_str = String(longitude, 5);

        EnviarCoords(latitude, longitude); // Enviar coordenadas para o Helix

      }
      else
      {
        lat_str = "Invalido !";
        lng_str = "Invalido !";
        Serial.println("");
        Serial.println("Coordenadas inválidas !");
        Serial.println("");
      }

    } //gps.encode

  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);


  // Verifica se um cliente conectou
  WiFiClient client = server.available();
  if (!client)
  {
    return;
  }

  // Prepara a resposta para o cliente - página web na memoria do NodeMCU8266
  //
  String s = "HTTP/ 1.1 200 OK \r\nContent-type:text/html\r\n\r\n<!DOCTYPE html><html><head><title>Boi Fujao</title><style>";
  s += "a:link {background-color: YELLOW; text-decoration: none;}";
  s += "table, th, td{border: 1px solid black;}</style></head><body><h1 style=";
  s += "font-size:200%;";
  s += " ALIGN=CENTER>GPS Boi Fujao</h1>";

  s += "<p ALIGN=CENTER style=""font-size:150%;""";
  s += "></p><table ALIGN=CENTER style=";
  s += "width:50%";

  s += "><tr><th>Latitude</th>";
  s += "<td ALIGN=CENTER>";
  s += lat_str;

  s += "</td></tr><tr><th> Longitude </th><td ALIGN=CENTER>";
  s += lng_str;

  s += "</td> </tr> <tr> <th> Data </th><td ALIGN=CENTER>";
  s += date_str;

  s += "</td></tr><tr><th> Hora UTC </th><td ALIGN=CENTER>";
  s += time_str;
  //s += "</td></tr></table>";

  s += "</td></tr><tr><th> status HTTP </th><td ALIGN=CENTER>";
  s += httpCode_str;
  s += "</td></tr></table>";

  if (gps.location.isValid())
  {
    s += "<p align=center><a style=""color:RED;"" href=""http://maps.google.com/maps?&z=19&mrt=yp&t=k&q=" + lat_str + "+" + lng_str + "&ll=" + lat_str + "+" + lng_str + """>Clique aqui</a> para ver no Google maps.</p>";
  }
  s += "</body></html>\n";
  client.print(s);

  delay(5000);  // A cada 5 segundos volta a retransmitir as coordenadas

}//void loop

//------------------------------------------------------------

void EnviarCoords(double latitude, double longitude) {

  if (WiFi.status() == WL_CONNECTED) {

    StaticJsonBuffer<200> JSONbuffer;                 // Declara static buffer com 200 bytes
    JsonObject& JSONroot = JSONbuffer.createObject(); // Cria a raiz do json

    JsonObject& location = JSONroot.createNestedObject("location");

    JsonObject& location_value = location.createNestedObject("value");
    location_value["type"] = "Point";

    JsonArray& location_value_coordinates = location_value.createNestedArray("coordinates");
    location_value_coordinates.add(latitude);   // Latitude
    location_value_coordinates.add(longitude);  // Longitude

    char JSONmessageBuffer[200];
    JSONroot.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    Serial.println(JSONmessageBuffer);          // Imprime no monitor serial o objeto json
    Serial.println("");

    HTTPClient http; //cria o objeto da classe HTTPClient
    
    http.begin(fullAddress);
    http.addHeader("Content-Type", "application/json");

    httpCode = http.POST(JSONmessageBuffer); // Envia o Json com os dados
    httpCode_str = String(httpCode);

    Serial.print("Retorno http: ");         // Imprime no monitor serial o retorno HTTP
    Serial.println(httpCode);
    Serial.println(fullAddress);
    //Serial.println();

    http.end();  //Fecha a conexão
  }
  else {
    Serial.println("Erro na conexão Wifi");
  }
}
