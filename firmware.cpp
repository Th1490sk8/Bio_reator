// -----------------------------------------------------
// 1. INCLUSÕES DE BIBLIOTECAS E DEFINIÇÕES
// -----------------------------------------------------

#include <WiFi.h> 
#include <PubSubClient.h> 
#include <OneWire.h> 
#include <DallasTemperature.h> 
#include <DHT.h> 
#include "HX711.h" 
#include <Adafruit_TCS34725.h> // Sensor de Cor
#include <Wire.h> 
#include <Adafruit_ADS1X15.h> // Para ADS1115

// Definições de Rede
const char* ssid = "SEU_WIFI_SSID";
const char* password = "SUA_SENHA_WIFI";
const char* mqtt_server = "IP_DO_RASPBERRY_PI"; 
const int MQTT_PORT = 1883;

// Tópicos MQTT
const char* MQTT_PUB_DATA = "compostech/data";
const char* MQTT_SUB_CMD = "compostech/cmd";

// Pinos de Controle (Atuadores)
#define PINO_BOMBA          25 // Motor/Bomba d'água
#define PINO_VENTOINHA      26 // Ventoinha
#define PINO_MOTOR_REVOLVER 27 // Motor/Servo para revolvimento
const long TEMPO_ACIONAMENTO_MS = 30000; // 30 segundos
long tempo_desligar_atuador = 0;

// Pinos dos Sensores Digitais e Outros
#define PINO_DS18B20 4      
#define PINO_DHT11 16       
#define PINO_TRIG 18        
#define PINO_ECHO 19        
#define PINO_DT 5           
#define PINO_SCK 17         

// Inicialização de Objetos de Sensores
Adafruit_ADS1115 ads; // Objeto ADS1115
OneWire oneWire(PINO_DS18B20);
DallasTemperature sensors_ds18b20(&oneWire);
DHT dht(PINO_DHT11, DHT11);
HX711 scale;
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_1X);

// Objetos Wi-Fi e MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// -----------------------------------------------------
// 2. FUNÇÕES AUXILIARES DE CONEXÃO E COMANDO
// -----------------------------------------------------

void setup_wifi() { /* Código de conexão Wi-Fi (igual ao exemplo anterior) */ }

void reconnect() { /* Código de reconexão MQTT (igual ao exemplo anterior) */ }

// Função de Callback para Comandos Remotos (MQTT)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Comando Recebido: ");
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  Serial.println(message);

  if (String(topic) == MQTT_SUB_CMD) {
    if (message == "REVOLVER_LIGAR") {
      digitalWrite(PINO_MOTOR_REVOLVER, HIGH);
      Serial.println("Motor de Revolução Ativado!");
      // Definir temporizador para desligar o motor
      tempo_desligar_atuador = millis() + 10000; // 10 segundos para revolver
    } else if (message == "BOMBA_LIGAR") {
      // Ativa a bomba e a ventoinha por um tempo
      digitalWrite(PINO_BOMBA, HIGH);
      digitalWrite(PINO_VENTOINHA, HIGH);
      tempo_desligar_atuador = millis() + TEMPO_ACIONAMENTO_MS;
    }
    // Implementar mais comandos de controle remoto via app/dashboard
  }
}


// -----------------------------------------------------
// 3. LEITURA DE SENSORES
// -----------------------------------------------------

struct SensorData {
  float umidade_solo; // ADS1115 A0
  float temp_composto; // DS18B20
  float temp_ar; // DHT11
  float umid_ar; // DHT11
  float ph; // ADS1115 A1
  float mq135_raw; // ADS1115 A2
  float ec_raw; // ADS1115 A3
  long celula_carga_g; // HX711
  float distancia_cm; // HC-SR04
  int color_r, color_g, color_b; // TCS34725
};

SensorData readAllSensors() {
  SensorData data;

  // 1, 4, 5, 6. Leitura do ADS1115 (16 bits de alta resolução)
  int16_t umidade_raw_ads = ads.readADC_SingleEnded(0); // Umidade A0
  int16_t ph_raw_ads = ads.readADC_SingleEnded(1);      // pH A1
  int16_t mq135_raw_ads = ads.readADC_SingleEnded(2);    // MQ135 A2
  int16_t ec_raw_ads = ads.readADC_SingleEnded(3);       // Condutividade A3
  
  // Mapeamento e Calibração dos Sensores Analógicos (Conceitual)
  data.umidade_solo = map(umidade_raw_ads, 20000, 10000, 0, 100); // Exemplo
  data.umidade_solo = constrain(data.umidade_solo, 0, 100);
  data.ph = 7.0 + ((ph_raw_ads - 16500) / 3300.0); // Exemplo de linearização
  data.mq135_raw = mq135_raw_ads; // Valor bruto para monitoramento
  data.ec_raw = ec_raw_ads; // Valor bruto para monitoramento

  // 2. DS18B20 (Temperatura do Composto)
  sensors_ds18b20.requestTemperatures(); 
  data.temp_composto = sensors_ds18b20.getTempCByIndex(0);

  // 3. DHT11 (Umidade e Temperatura do Ar)
  data.umid_ar = dht.readHumidity();
  data.temp_ar = dht.readTemperature();

  // 7. Sensor de Colorimetria/Espectroscopia
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);
  data.color_r = r; data.color_g = g; data.color_b = b;

  // 8. Célula de Carga
  data.celula_carga_g = (long)scale.get_units(5); 

  // 9. Sensor de Distância Ultrasônico
  digitalWrite(PINO_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);
  long duration = pulseIn(PINO_ECHO, HIGH);
  data.distancia_cm = duration * 0.0343 / 2; 

  return data;
}

// Envia os dados dos sensores via MQTT no formato JSON (Igual ao exemplo anterior)
void sendDataMQTT(SensorData data) { /* Implementação de envio JSON */ }

// -----------------------------------------------------
// 4. LÓGICA DE CONTROLE E AUTOMAÇÃO INTELIGENTE
// -----------------------------------------------------

void controlLogic(SensorData data) {
  // Lógica de Desligamento Temporizado
  if (millis() > tempo_desligar_atuador) {
    digitalWrite(PINO_BOMBA, LOW);
    digitalWrite(PINO_VENTOINHA, LOW);
    digitalWrite(PINO_MOTOR_REVOLVER, LOW);
  }

  // A. Lógica de Umidade do Solo (Aciona Bomba e Ventoinha)
  if (data.umidade_solo < 45.0) { 
    if (digitalRead(PINO_BOMBA) == LOW) { 
      Serial.println("Umidade baixa. Acionando bomba/ventoinha.");
      digitalWrite(PINO_BOMBA, HIGH);
      digitalWrite(PINO_VENTOINHA, HIGH);
      tempo_desligar_atuador = millis() + TEMPO_ACIONAMENTO_MS;
    }
  }

  // B. Lógica de Temperatura Crítica (Termofílica)
  if (data.temp_composto > 65.0) { 
    // Se muito quente, ativa o ventilador continuamente para resfriar
    digitalWrite(PINO_VENTOINHA, HIGH);
    Serial.println("Temperatura alta! Aeração máxima.");
    tempo_desligar_atuador = 0; // Mantém ligado até a temperatura cair

  } else if (data.temp_composto < 50.0 && data.temp_composto > 40.0) {
    // Se a temperatura está caindo, mas ainda não está pronta, pode ser falta de aeração/mistura
    if (data.mq135_raw > 10000) { // Alto índice de gases problemáticos (NH3)
      Serial.println("Risco de anaerobiose e mau cheiro! Revolver forçadamente.");
      if (digitalRead(PINO_MOTOR_REVOLVER) == LOW) {
        digitalWrite(PINO_MOTOR_REVOLVER, HIGH);
        tempo_desligar_atuador = millis() + 15000; // Revolver por 15s
      }
    }
  }

  // C. Lógica de Compactação e Volume (Ultrassônico vs. Célula de Carga)
  // Se o peso aumentou (> 100g desde a última leitura) E o volume (distância) não diminuiu,
  // ou a distância está muito baixa (alto nível de compactação), aciona o motor de revolvimento.
  if (data.distancia_cm < 10.0) { // Composteira muito cheia / compacto
    if (digitalRead(PINO_MOTOR_REVOLVER) == LOW) {
      Serial.println("Composteira cheia/compacta. Iniciando revolvimento.");
      digitalWrite(PINO_MOTOR_REVOLVER, HIGH);
      tempo_desligar_atuador = millis() + 20000;
    }
  }
}

// -----------------------------------------------------
// 5. SETUP E LOOP PRINCIPAIS
// -----------------------------------------------------

void setup() {
  Serial.begin(115200);

  // Configuração dos Pinos
  pinMode(PINO_BOMBA, OUTPUT);
  pinMode(PINO_VENTOINHA, OUTPUT);
  pinMode(PINO_MOTOR_REVOLVER, OUTPUT);
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  digitalWrite(PINO_BOMBA, LOW);
  digitalWrite(PINO_VENTOINHA, LOW);
  digitalWrite(PINO_MOTOR_REVOLVER, LOW);

  // Inicializa Comunicação
  setup_wifi();
  client.setServer(mqtt_server, MQTT_PORT); 
  client.setCallback(callback);

  // Inicializa Sensores I2C
  Wire.begin(); // Inicializa I2C
  ads.setGain(GAIN_ONE); // 4.096V range (ajustável)
  if (!ads.begin()) { Serial.println("Falha ao iniciar ADS1115!"); while (1); }
  if (!tcs.begin()) { Serial.println("Falha ao iniciar TCS34725!"); while (1); }

  // Inicializa Outros Sensores
  sensors_ds18b20.begin();
  dht.begin();
  scale.begin(PINO_DT, PINO_SCK);
}

long lastMsg = 0;
const int INTERVALO_LEITURA = 5000; // 5 segundos

void loop() {
  // Mantém a Conexão MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > INTERVALO_LEITURA) {
    lastMsg = now;

    // 1. LER TODOS OS SENSORES
    SensorData currentData = readAllSensors();

    // 2. EXECUTAR LÓGICA DE CONTROLE E ACIONAR ATUADORES
    controlLogic(currentData);

    // 3. ENVIAR DADOS PARA O RASPBERRY PI (MQTT)
    sendDataMQTT(currentData);
  }
}