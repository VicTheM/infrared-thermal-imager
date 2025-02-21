// Code adapted from the webserver example in Arduino-ESP

#include "thermal_imager_header.h"
#include <WebServer.h>
#include <WiFi.h>

const char *ssid = "Electrify";
const char *passPhrase = "Victory111";

// need a WebServer for http access on port 80.
WebServer server(80);
fs::FS *fsys = nullptr;
bool serverRunning = false;

// enable the CUSTOM_ETAG_CALC to enable calculation of ETags by a custom function
#define CUSTOM_ETAG_CALC

// ===== Simple functions used to answer simple GET requests =====
void handleRedirect() {
  TRACE("Redirect...\n");
  String url = "/index.htm";

  if (!fsys->exists(url)) {
    url = "/$upload.htm";
    TRACE("Uri Changed\n");
  }

  server.sendHeader("Location", url, true);
  server.send(302);
}



void handleListFiles() {
  File dir = fsys->open("/", "r");
  String result;

  result += "[\n";
  while (File entry = dir.openNextFile()) {
    if (result.length() > 4) {
      result += ",\n";
    }
    result += "  {";
    result += "\"type\": \"file\", ";
    result += "\"name\": \"" + String(entry.name()) + "\", ";
    result += "\"size\": " + String(entry.size()) + ", ";
    result += "\"time\": " + String(entry.getLastWrite());
    result += "}";
  }  // while

  result += "\n]";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
}  // handleListFiles()

// This function is called when the sysInfo service was requested.
void handleSysInfo() {
  String result;

  result += "{\n";
  result += "  \"Chip Model\": " + String(ESP.getChipModel()) + ",\n";
  result += "  \"Chip Cores\": " + String(ESP.getChipCores()) + ",\n";
  result += "  \"Chip Revision\": " + String(ESP.getChipRevision()) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"fsTotalBytes\": " + String(LittleFS.totalBytes()) + ",\n";
  result += "  \"fsUsedBytes\": " + String(LittleFS.usedBytes()) + ",\n";
  result += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
}  // handleSysInfo()

// ===== Request Handler class used to answer more complex requests =====

// The FileServerHandler is registered to the web server to support DELETE and UPLOAD of files into the filesystem.
class FileServerHandler : public RequestHandler {
public:
  // @brief Construct a new File Server Handler object
  // @param fs The file system to be used.
  // @param path Path to the root folder in the file system that is used for serving static data down and upload.
  // @param cache_header Cache Header to be used in replies.
  FileServerHandler() {
    TRACE("FileServerHandler is registered\n");
  }

  // @brief check incoming request. Can handle POST for uploads and DELETE.
  // @param requestMethod method of the http request line.
  // @param requestUri request resource from the http request line.
  // @return true when method can be handled.
  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &uri) override {
    return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE));
  }  // canHandle()

  bool canUpload(WebServer &server, const String &uri) override {
    // only allow upload on root fs level.
    return (uri == "/");
  }  // canUpload()

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    // ensure that filename starts with '/'
    String fName = requestUri;
    if (!fName.startsWith("/")) {
      fName = "/" + fName;
    }

    if (requestMethod == HTTP_POST) {
      // all done in upload. no other forms.

    } else if (requestMethod == HTTP_DELETE) {
      if (fsys->exists(fName)) {
        TRACE("DELETE %s\n", fName.c_str());
        fsys->remove(fName);
      }
    }  // if

    server.send(200);  // all done.
    return (true);
  }  // handle()

  // uploading process
  void upload(WebServer UNUSED &server, const String &requestUri, HTTPUpload &upload) override {
    ;
  }  // upload()

protected:
  File _fsUploadFile;
};

void startServer() {
    if (!serverRunning) {
      //Serial.println("Starting server...");
      if (strlen(ssid) == 0) {
        WiFi.begin();
      } else {
        WiFi.begin(ssid, passPhrase);
      }

      TRACE("Connect to WiFi...\n");
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        TRACE(".");
      }
      TRACE("connected.\n");
      configTzTime(TIMEZONE, "pool.ntp.org");

      TRACE("Register redirect...\n");

      // register a redirect handler when only domain name is given.
      server.on("/", HTTP_GET, handleRedirect);

      TRACE("Register service handlers...\n");

            // serve a built-in htm page
      server.on("/$upload.htm", []() {
        server.send(200, "text/html", FPSTR(uploadContent));
      });
      server.on("/stats/full", []() {
        server.send(200, "text/html", FPSTR(statsPage));
      });

          // register some REST services
      server.on("/api/list", HTTP_GET, handleListFiles);
      server.on("/api/sysinfo", HTTP_GET, handleSysInfo);

      TRACE("Register file system handlers...\n");

      // UPLOAD and DELETE of files in the file system using a request handler.
      server.addHandler(new FileServerHandler());

      // enable CORS header in webserver results
      server.enableCORS(true);
#if defined(CUSTOM_ETAG_CALC)
  // This is a fast custom eTag generator. It returns a value based on the time the file was updated like
  // ETag: 63bbceb5
  server.enableETag(true, [](FS &fs, const String &path) -> String {
    File f = fs.open(path, "r");
    String eTag = String(f.getLastWrite(), 16);  // use file modification timestamp to create ETag
    f.close();
    return (eTag);
  });

#else
  // enable standard ETAG calculation using md5 checksum of file content.
  server.enableETag(true);
#endif
      // serve all static files
      server.serveStatic("/", *fsys, "/");

      TRACE("Register default (not found) answer...\n");

      // handle cases when file is not found
      server.onNotFound([]() {
        // standard not found in browser.
        server.send(404, "text/html", FPSTR(statsPage));
      });

      server.begin();

      TRACE("open <http://%s> or <http://%s>\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());

      server.begin();
      serverRunning = true;
    }
}

void stopServer() {
  if (serverRunning) {
    //Serial.println("Stopping server... ");
    server.stop();
    serverRunning = false;
  }
}

void handleClient() {
  server.handleClient();
}