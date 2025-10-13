#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;  // Cria objeto para o ADC de 16 bits

// ==== Calibração básica ====
// Ajuste estes valores conforme suas medições reais
const int valorBrutoSeco = 25000;     // Valor lido em solo completamente seco
const int valorBrutoMolhado = 12000;  // Valor lido em solo bem úmido

void setup() {
  Serial.begin(9600);
  Serial.println("=== Leitura de Umidade do Solo com ADS1115 (16 bits) ===");

  // Inicializa comunicação com o ADS1115
  if (!ads.begin()) {
    Serial.println("ERRO: ADS1115 não encontrado! Verifique conexões SDA(A4)/SCL(A5).");
    while (1);
  }

  // Configura ganho: GAIN_ONE = ±4.096 V (ideal para 0–3.3 V)
  ads.setGain(GAIN_ONE);
}

void loop() {
  // Lê o canal A0 do ADS1115 (onde está o YL-69)
  int16_t leitura = ads.readADC_SingleEnded(0);

  // Converte o valor em porcentagem de umidade
  float umidade = map(leitura, valorBrutoSeco, valorBrutoMolhado, 0, 100);
  umidade = constrain(umidade, 0, 100);

  // Exibe os resultados no Monitor Serial
  Serial.print("Leitura bruta (ADS1115): ");
  Serial.print(leitura);
  Serial.print(" | Umidade: ");
  Serial.print(umidade);
  Serial.println("%");

  delay(1000);  // Atualiza a cada 1 s
}
