import serial
import json
import paho.mqtt.client as mqtt
import time

# Configurações
SERIAL_PORT = '/dev/ttyACM0'  # Verifique a porta do Arduino
BAUD_RATE = 9600
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_PUB_DATA = "compostech/data"
MQTT_SUB_CMD = "compostech/cmd"

ser = None  # Variável global para a serial

# --- Funções MQTT ---
def on_connect(client, userdata, flags, rc):
    print("Conectado ao Broker MQTT com código de resultado:", rc)
    client.subscribe(MQTT_SUB_CMD)

def on_message(client, userdata, msg):
    global ser
    cmd = msg.payload.decode()
    print(f"[MQTT] Comando recebido: {cmd}")
    if ser and ser.is_open:
        try:
            ser.write(f"{cmd}\n".encode())
            print(f"[SERIAL] Comando enviado ao Arduino: {cmd}")
        except Exception as e:
            print(f"[ERRO] Falha ao enviar comando Serial: {e}")

# --- Inicializa MQTT ---
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

# --- Loop Principal ---
while True:
    try:
        if ser is None or not ser.is_open:
            try:
                ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                print(f"[SERIAL] Conectado em {SERIAL_PORT} @ {BAUD_RATE}")
            except serial.SerialException as e:
                print(f"[ERRO] Porta serial indisponível: {e}")
                time.sleep(5)
                continue

        line = ser.readline().decode('utf-8').strip()
        if line:
            if line.startswith('{') and line.endswith('}'):
                try:
                    data = json.loads(line)
                    print(f"[DADOS] {data}")
                    client.publish(MQTT_PUB_DATA, json.dumps(data), qos=1, retain=True)
                except json.JSONDecodeError:
                    print(f"[ERRO] JSON inválido: {line}")
            else:
                print(f"[DEBUG] {line}")

        time.sleep(0.05)

    except KeyboardInterrupt:
        print("Encerrando...")
        break
    except Exception as e:
        print(f"[ERRO] Inesperado: {e}")
        time.sleep(2)

# --- Finalização ---
if ser and ser.is_open:
    ser.close()
client.loop_stop()
