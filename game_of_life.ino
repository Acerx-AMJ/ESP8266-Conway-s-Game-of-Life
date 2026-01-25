#include <ESP8266WiFi.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266TrueRandom.h>

// Adafruit

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 14
#define OLED_SCL 12

Adafruit_SSD1306 *display;

// Wi-fi

const char *ssid = "XXXXXXXX";
const char *password = "XXXXXXXX";

WiFiServer server(80);
String header;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
unsigned long timeoutTime = 2000;
bool connected = false;

// Globals

bool map1[SCREEN_HEIGHT][SCREEN_WIDTH];
bool map2[SCREEN_HEIGHT][SCREEN_WIDTH];
bool paused = false;
int cursorX = 0;
int cursorY = 0;

// Setup

void setupMap() {
  for (int y = 1; y < SCREEN_HEIGHT - 1; ++y) {
    for (int x = 1; x < SCREEN_WIDTH - 1; ++x) {
      map1[y][x] = ESP8266TrueRandom.random(2); // Generate either 0 or 1
      map2[y][x] = 0;
    }
  }
}

void clearMap() {
  for (int y = 1; y < SCREEN_HEIGHT - 1; ++y) {
    for (int x = 1; x < SCREEN_WIDTH - 1; ++x) {
      map1[y][x] = map2[y][x] = 0;
    }
  }
}

void setup() {
  // Setup server
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  server.begin();

  // Setup game
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
  Wire.begin(OLED_SDA, OLED_SCL);
  display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  setupMap();

  // Serial is fucking with me here
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(WHITE);
  display->setCursor(0, 0);
  display->println("Connection made to");
  display->println(ssid);
  display->println("Web server is open at");
  display->println(WiFi.localIP());
  display->println("Open it to access thegame.");
  display->display();
}

// Loop

void loop() {
  // Update webserver logic
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    previousTime = currentTime = millis();
    connected = true;

    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();
      if (!client.available()) continue;

      char c = client.read();
      Serial.write(c);
      header += c;

      if (c == '\n') {
        if (currentLine.length() == 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          // Reset the game
          if (header.indexOf("GET /r") >= 0) {
            setupMap();
          }

          // Clear the game
          if (header.indexOf("GET /c") >= 0) {
            clearMap();
          }

          // Move the cursor
          if (header.indexOf("GET /a") >= 0) {
            cursorX = (cursorX == 0 ? SCREEN_WIDTH - 1 : cursorX - 1);
          } else if (header.indexOf("GET /d") >= 0) {
            cursorX = (cursorX + 1) % SCREEN_WIDTH;
          } else if (header.indexOf("GET /w") >= 0) {
            cursorY = (cursorY == 0 ? SCREEN_HEIGHT - 1 : cursorY - 1);
          } else if (header.indexOf("GET /s") >= 0) {
            cursorY = (cursorY + 1) % SCREEN_HEIGHT;
          }

          // Set/clear tiles
          if (header.indexOf("GET /q") >= 0) {
            map1[cursorY][cursorX] = 1;
          } else if (header.indexOf("GET /e") >= 0) {
            map1[cursorY][cursorX] = 0;
          }

          // Pause/unpause the game
          if (header.indexOf("GET /p/off") >= 0 || header.indexOf("GET /p/on") >= 0) {
            paused = !paused;
          }

          // Feed HTML content to the client
          client.println("<!DOCTYPE html><html><head></head><body><h1>Conway's Game of Life</h1><p>CURSOR X:");
          client.println(cursorX);
          client.println(" CURSOR Y:");
          client.println(cursorY);
          client.println("</p><a href=\"/r\">RESET</a><br><a href=\"/c\">CLEAR</a><br><a href=\"/a\">MOVE CURSOR LEFT</a><br><a href=\"/w\">MOVE CURSOR UP</a><br><a href=\"/s\">MOVE CURSOR DOWN</a><br><a href=\"/d\">MOVE CURSOR RIGHT</a><br><a href=\"/q\">SET TILE 1</a><br><a href=\"/e\">SET TILE 0</a>");

          if (paused) {
            client.println("<br><a href=\"/p/off\">UNPAUSE</a>");
          } else {
            client.println("<br><a href=\"/p/on\">PAUSE</a>");
          }

          client.println("</body></html>");
          client.println();
          break;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }

  header = "";
  client.stop();
  if (!connected) return;

  // Render differently if paused
  if (paused) {
    display->clearDisplay();
    for (int y = 1; y < SCREEN_HEIGHT - 1; ++y) {
      for (int x = 1; x < SCREEN_WIDTH - 1; ++x) {
        if (map1[y][x]) {
          display->drawPixel(x, y, SSD1306_WHITE);
        }
      }
    }
    display->display();
    delay(100);
    return;
  }

  // Update GOL logic
  for (int y = 1; y < SCREEN_HEIGHT - 1; ++y) {
    for (int x = 1; x < SCREEN_WIDTH - 1; ++x) {
      int neighbors = map1[y - 1][x - 1] + map1[y - 1][x] + map1[y - 1][x + 1]
                    + map1[y    ][x - 1] +                  map1[y    ][x + 1]
                    + map1[y + 1][x - 1] + map1[y + 1][x] + map1[y + 1][x + 1];

      map2[y][x] = (neighbors == 3 || (map1[y][x] && neighbors == 2));
    }
  }

  // Swap arrays and draw themp
  display->clearDisplay();
  for (int y = 1; y < SCREEN_HEIGHT - 1; ++y) {
    for (int x = 1; x < SCREEN_WIDTH - 1; ++x) {
      bool temp = map1[y][x];
      map1[y][x] = map2[y][x];
      map2[y][x] = temp;

      if (map1[y][x]) {
        display->drawPixel(x, y, SSD1306_WHITE);
      }
    }
  }
  display->display();
  
  // Delay 100ms
  delay(100);
}