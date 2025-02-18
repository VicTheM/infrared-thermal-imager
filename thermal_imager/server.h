#ifndef _THERMAL_CAMERA_SERVER_
#define _THERMAL_CAMERA_SERVER_

#include <Arduino.h>
#include "esp_partition.h"
#include <FS.h>
#include <LittleFS.h>

#define UNUSED __attribute__((unused))
#define TRACE(...) Serial.printf(__VA_ARGS__)
#define HOSTNAME "webserver"

extern fs::FS *fsys;
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

void startServer();
void stopServer();
void handleClient();


/**
 * @file builtinfiles.h
 * @brief This file is part of the WebServer example for the ESP8266WebServer.
 *
 * This file contains long, multiline text variables for  all builtin resources.
 */

// used for $upload.htm
static const char uploadContent[] PROGMEM =
  R"==(
<!DOCTYPE html>
<html>
<head>
<title>Image Library</title>
<style>
body {
  font-family: sans-serif;
  margin: 20px;
}
.file-container { /* Container for each file entry */
  display: flex; /* Use flexbox for layout */
  align-items: center; /* Vertically align items */
  margin-bottom: 5px; /* Space between file entries */
}
.file-button {
  padding: 10px 20px;
  margin: 5px;
  border: none;
  color: white;
  text-align: center;
  text-decoration: none;
  font-size: 16px;
  cursor: pointer;
  border-radius: 5px;
}
.download-button {
  background-color: #4CAF50; /* Green */
}
.download-button:hover {
  background-color: #45a049; /* Darker green */
}
.delete-button {
  background-color: #f44336; /* Red */
}
.delete-button:hover {
  background-color: #c82333; /* Darker red */
}
.file-icon {
    width: 20px;
    height: 20px;
    margin-right: 5px;
    vertical-align: middle;
}
#error-message {
  color: red;
  margin-top: 10px;
}
</style>
</head>
<body>

<h1>Files</h1>

<div id="file-list"></div>
<div id="error-message"></div>

<script>
  const fileListDiv = document.getElementById('file-list');
  const errorMessageDiv = document.getElementById('error-message');

  fetch('/api/list')
    .then(response => {
       if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.json();
    })
    .then(data => {
      if (!data || !Array.isArray(data)) {
        throw new Error("Invalid data format received from the server.");
      }
      if (data.length === 0) {
        fileListDiv.innerHTML = "<p>No files found.</p>";
        return;
      }
      data.forEach(file => {
        const fileContainer = document.createElement('div'); // Container for buttons and icon
        fileContainer.className = 'file-container';

        const fileIcon = document.createElement('img');
        fileIcon.src = "file_icon.png";
        fileIcon.alt = "File Icon";
        fileIcon.className = "file-icon";
        fileContainer.appendChild(fileIcon);

        const fileNameSpan = document.createElement('span'); // Add a span for the filename
        fileNameSpan.textContent = file.name;
        fileContainer.appendChild(fileNameSpan);


        const downloadButton = document.createElement('a');
        downloadButton.href = "/" + file.name;
        downloadButton.className = 'file-button download-button';
        downloadButton.textContent = 'Download';
        downloadButton.download = file.name; // Trigger download

        fileContainer.appendChild(downloadButton);

        const deleteButton = document.createElement('button');
        deleteButton.className = 'file-button delete-button';
        deleteButton.textContent = 'Delete';
        deleteButton.addEventListener('click', () => {
          fetch("/" + file.name, { method: 'DELETE' })
            .then(response => {
               if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
              }
              if (response.status === 200) {
                fileContainer.remove(); // Remove the file entry from the display
              } else {
                errorMessageDiv.textContent = `Error deleting ${file.name}: ${response.status}`;
              }
            })
            .catch(error => {
              console.error("Error deleting file:", error);
              errorMessageDiv.textContent = "Error deleting file: " + error.message;
            });
        });
        fileContainer.appendChild(deleteButton);
        fileListDiv.appendChild(fileContainer);
      });
    })
    .catch(error => {
      console.error("Error fetching file list:", error);
      errorMessageDiv.textContent = "Error loading files: " + error.message;
    });
</script>

</body>
</html>
)==";

// used for $upload.htm
static const char statsPage[] PROGMEM = R"==(
<!DOCTYPE html>
<html>
<head>
<title>System Info</title>
<style>
body {
  font-family: sans-serif;
}
.stats-table {
  width: 50%;
  border-collapse: collapse;
  margin-top: 20px;
}
.stats-table th, .stats-table td {
  border: 1px solid #ddd;
  padding: 8px;
  text-align: left;
}
.stats-table th {
  background-color: #f2f2f2;
}
</style>
</head>
<body>

<h1>ESP32 System Information</h1>

<table class="stats-table">
  <tr><th>Statistic</th><th>Value</th></tr>
  <tr><td>Chip Model</td><td id="chip-model">Loading...</td></tr>
  <tr><td>Chip Cores</td><td id="chip-cores">Loading...</td></tr>
  <tr><td>Chip Revision</td><td id="chip-revision">Loading...</td></tr>
  <tr><td>Flash Size</td><td id="flash-size">Loading...</td></tr>
  <tr><td>Free Heap</td><td id="free-heap">Loading...</td></tr>
  <tr><td>FS Total Bytes</td><td id="fs-total">Loading...</td></tr>
  <tr><td>FS Used Bytes</td><td id="fs-used">Loading...</td></tr>
  <tr><td>Number of Files</td><td id="file-count">Loading...</td></tr>
  <tr><td>Total File Size</td><td id="total-file-size">Loading...</td></tr>
</table>

<script>
  const chipModel = document.getElementById('chip-model');
  const chipCores = document.getElementById('chip-cores');
  const chipRevision = document.getElementById('chip-revision');
  const flashSize = document.getElementById('flash-size');
  const freeHeap = document.getElementById('free-heap');
  const fsTotal = document.getElementById('fs-total');
  const fsUsed = document.getElementById('fs-used');
  const fileCount = document.getElementById('file-count');
  const totalFileSize = document.getElementById('total-file-size');

  fetch('/api/sysinfo')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.json();
    })
    .then(data => {
      chipModel.textContent = data["Chip Model"];
      chipCores.textContent = data["Chip Cores"];
      chipRevision.textContent = data["Chip Revision"];
      flashSize.textContent = data["flashSize"];
      freeHeap.textContent = data["freeHeap"];
      fsTotal.textContent = data["fsTotalBytes"];
      fsUsed.textContent = data["fsUsedBytes"];
    })
    .catch(error => {
      console.error("Error fetching system info:", error);
      // Handle error, maybe display a message to the user
      chipModel.textContent = "Error loading";
      // ... set other fields to "Error loading" as well
    });


  fetch('/api/list')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.json();
    })
    .then(files => {
      if (!files || !Array.isArray(files)) {
        throw new Error("Invalid data format received from the server.");
      }
      fileCount.textContent = files.length;
      let totalSize = 0;
      files.forEach(file => {
        totalSize += file.size;
      });
      totalFileSize.textContent = (totalSize / 1024).toFixed(2) + " KB"; // Convert to KB
    })
    .catch(error => {
        console.error("Error fetching file list:", error);
        fileCount.textContent = "Error loading";
        totalFileSize.textContent = "Error loading";
    });

</script>

</body>
</html>
)==";

#endif // _THERMAL_CAMERA_SERVER_