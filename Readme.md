# TITO

## Escritorio

Implementación para el controlador de las luces del escritorio usando
[tito](https://github.com/grilix/tito) y platformio.

## Componentes

- ESP8266 nodemcu v2
- BME280 conectado por I2C
- Relay en pin D5
- Relay en pin D6

## Luz de estado

Estado|Descripción
------|-----------
Fija|Iniciando
Parpadeo lento (500ms)|Conectando a la WiFi/MQTT
Parpadeo rápido (200ms)|Esperando conexión a la placa BME
Apagada|Funcionamiento normal

## Instalación

Tener en cuenta:

- Copiar/editar src/network.h.example -> src/network.h
- Clonar submódulo `git submodule update`

```
pio run -t upload
```
