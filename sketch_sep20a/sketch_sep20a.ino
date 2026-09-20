#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* WIFI_NOMBRE = "canda-workshop";
const char* WIFI_CLAVE = "Handbook7";

constexpr int PIN_SERVO = 4;  // GPIO 4, no el cuarto pin físico
constexpr int ANGULO_MIN = 45;
constexpr int ANGULO_MAX = 135;
constexpr unsigned long INTERVALO_GIRO_MS = 1000;

WebServer servidor(80);
Servo servo;

bool enMovimiento = true;
int sentido = 1;
unsigned long ultimoPaso = 0;

void mostrarPagina() {
  servidor.send(200, "text/html; charset=utf-8", R"HTML(
    <!doctype html>
    <html lang="es">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <meta charset="utf-8">
      <title>Control del servo</title>
      <h1>Control del servo</h1>
      <button onclick="enviar('parar')">Parar</button>
      <button onclick="enviar('iniciar')">Reanudar</button>
      <p id="estado"></p>
      <script>
        async function enviar(orden) {
          try {
            const respuesta = await fetch('/' + orden, {
              method: 'POST'
            });
            document.getElementById('estado').textContent =
              await respuesta.text();
          } catch {
            document.getElementById('estado').textContent =
              'No se pudo contactar con la ESP32';
          }
        }
      </script>
    </html>
  )HTML");
}

void parar() {
  enMovimiento = false;
  servidor.send(200, "text/plain; charset=utf-8", "Servo pausado");
}

void iniciar() {
  enMovimiento = true;
  servidor.send(200, "text/plain; charset=utf-8", "Movimiento reanudado");
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);

  Serial.print("Conectando al Wi-Fi");

  // El servo todavía no está activado durante esta espera.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Abrí en tu navegador: http://");
  Serial.println(WiFi.localIP());

  servidor.on("/", HTTP_GET, mostrarPagina);
  servidor.on("/parar", HTTP_POST, parar);
  servidor.on("/iniciar", HTTP_POST, iniciar);
  servidor.begin();

  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO);
  servo.write(ANGULO_MAX);
  ultimoPaso = millis();
}

void loop() {
  servidor.handleClient();

  if (!enMovimiento) {
    return;
  }

  unsigned long ahora = millis();

  // Esperamos el intervalo de giro antes de mover el servo nuevamente.
  if (ahora - ultimoPaso < INTERVALO_GIRO_MS) {
    return;
  }

  Serial.println("Terminó la espera");

  ultimoPaso = ahora;
  if (sentido == 1) {
    sentido = -1;
    servo.write(ANGULO_MIN);
    Serial.println("Girando a " + String(ANGULO_MIN));
  } else {
    sentido = 1;
    servo.write(ANGULO_MAX);
    Serial.println("Girando a " + String(ANGULO_MAX));
  }
}