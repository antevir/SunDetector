#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <TimeLib.h>

#include "Log.h"
#include "server.h"

static ESP8266WebServer server(80);

static String getContentType(String filename)
{ // convert the file extension to the MIME type
    if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}

static bool sendFile(String path)
{ // send the right file to the client (if it exists)
    Log.info("handleFileRead: %s", path.c_str());
    if (path.endsWith("/"))
        path += "index.html";                  // If a folder is requested, send the index file
    String contentType = getContentType(path); // Get the MIME type
    String pathWithGz = path + ".gz";

    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
    {                                                       // If the file exists, either as a compressed archive, or normal
        if (SPIFFS.exists(pathWithGz))                      // If there's a compressed version available
            path += ".gz";                                  // Use the compressed verion
        File file = SPIFFS.open(path, "r");                 // Open the file
        size_t sent = server.streamFile(file, contentType); // Send it to the client
        file.close();                                       // Close the file again
        Log.info("Sent file: %s", path.c_str());
        return true;
    }
    Log.warn("File Not Found: %s", path.c_str()); // If the file doesn't exist, return false
    return false;
}

void server_init()
{
    SPIFFS.begin();

    server.onNotFound([]() {                                  // If the client requests any URI
        if (!sendFile(server.uri()))                          // send it if it exists
            server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    });

    server.on("/all", HTTP_GET, []() {
        String json = "{";
        json += "\"heap\":" + String(ESP.getFreeHeap());
        json += ", \"analog\":" + String(analogRead(A0));
        json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
        json += "}";
        server.send(200, "text/json", json);
        json = String();
    });

    server.on("/time", HTTP_GET, []() {
        String json = "{";
        json += "\"epoch\":" + String(now());
        json += "}";
        server.send(200, "text/json", json);
        json = String();
    });

    // Start the server
    server.begin();
    Log.info("Server started");
}

void server_handle()
{
    server.handleClient();
}
