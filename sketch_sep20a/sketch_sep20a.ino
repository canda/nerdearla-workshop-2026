#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* WIFI_NOMBRE = "canda-workshop";
const char* WIFI_CLAVE = "Handbook7";

// Placa: ESP32-C3 Super Mini (Arduino IDE: ESP32C3 Dev Module).
// GPIO, no posiciones físicas del conector. Servo 1 conserva GPIO 4.
constexpr int PINES_SERVOS[] = {4, 5, 6, 7};
constexpr int CANTIDAD_SERVOS = sizeof(PINES_SERVOS) / sizeof(PINES_SERVOS[0]);
constexpr int ANGULO_MIN = 0;
constexpr int ANGULO_MAX = 180;
constexpr unsigned long ESPERA_INICIAL_MS = 1000;

WebServer servidor(80);
Servo servos[CANTIDAD_SERVOS];

void moverTodos(int angulo) {
  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    servos[i].write(angulo);
  }
}

bool leerAngulo(const String& texto, int& angulo) {
  if (texto.length() == 0 || texto.length() > 3) {
    return false;
  }
  int valor = 0;
  for (unsigned int i = 0; i < texto.length(); i++) {
    if (texto[i] < '0' || texto[i] > '9') {
      return false;
    }
    valor = valor * 10 + (texto[i] - '0');
  }
  if (valor < ANGULO_MIN || valor > ANGULO_MAX) {
    return false;
  }
  angulo = valor;
  return true;
}

void recibirAngulos() {
  int angulos[CANTIDAD_SERVOS];
  // Validar el pedido entero antes de mover cualquiera de los servos.
  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    String parametro = String("angulo") + String(i + 1);
    if (!servidor.hasArg(parametro) ||
        !leerAngulo(servidor.arg(parametro), angulos[i])) {
      servidor.send(400, "text/plain; charset=utf-8",
                    String("Parametro ") + parametro +
                    " requerido: entero entre 0 y 180. No se movio ningun servo.");
      return;
    }
  }

  String respuesta = "{\"angulos\":[";
  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    servos[i].write(angulos[i]);
    if (i > 0) respuesta += ",";
    respuesta += String(angulos[i]);
  }
  respuesta += "]}";
  servidor.send(200, "application/json", respuesta);
}

void mostrarPagina() {
  servidor.send(200, "text/html; charset=utf-8", R"HTML(
    <!doctype html>
    <html lang="es">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <meta charset="utf-8">
      <title>Control de los 4 servos</title>
      <h1>Control de los 4 servos</h1>
      <p>Ingresá los cuatro ángulos, entre 0° y 180°.</p>
      <form id="control">
        <p><label>Servo 1 (GPIO 4): <input name="angulo1" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <p><label>Servo 2 (GPIO 5): <input name="angulo2" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <p><label>Servo 3 (GPIO 6): <input name="angulo3" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <p><label>Servo 4 (GPIO 7): <input name="angulo4" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <button type="submit">Mover los cuatro servos</button>
      </form>
      <p id="estado" role="status"></p>
      <script>
        document.getElementById('control').addEventListener('submit', async (evento) => {
          evento.preventDefault();
          const parametros = new URLSearchParams(new FormData(evento.currentTarget));
          try {
            const respuesta = await fetch('/servos?' + parametros, { method: 'POST' });
            document.getElementById('estado').textContent = await respuesta.text();
          } catch {
            document.getElementById('estado').textContent = 'No se pudo contactar con la ESP32';
          }
        });
      </script>
    </html>
  )HTML");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    servos[i].setPeriodHertz(50);
    // Conservar el rango de pulsos predeterminado de ESP32Servo.
    servos[i].attach(PINES_SERVOS[i]);
  }
  moverTodos(ANGULO_MIN);
  delay(ESPERA_INICIAL_MS);
  moverTodos(ANGULO_MAX);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  Serial.print("Conectando al Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Permitir que el HTML local lea las respuestas, incluidos errores HTTP.
  servidor.enableCORS(true);
  servidor.on("/servos", HTTP_OPTIONS, []() {
    // Preflight CORS: no ejecutar movimientos.
    servidor.send(204);
  });
  servidor.on("/", HTTP_GET, mostrarPagina);
  // GET permite probar desde la barra del navegador; POST desde clientes HTTP.
  servidor.on("/servos", HTTP_GET, recibirAngulos);
  servidor.on("/servos", HTTP_POST, recibirAngulos);
  servidor.begin();

  Serial.println();
  Serial.print("Abrí en tu navegador: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  servidor.handleClient();
}
