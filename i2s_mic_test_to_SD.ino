#include <driver/i2s.h>
#include <SD.h>
#include <FS.h>
#define SAMPLE_RATE (16000)
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_PORT I2S_NUM_0
#define bufferLen 256

// SD Card pins
#define SD_CS 5

int32_t sBuffer[bufferLen];
int16_t outputBuffer[bufferLen];


File wavFile;
const int sampleRate = 16000;
const int bitsPerSample = 32;
const int numChannels = 1;
unsigned long recordingDuration = 5000; // Recording duration in milliseconds (10 seconds)
unsigned long startTime;

void setup() {
  Serial.begin(115200);
  while(!Serial) { delay(100); } // Wait for serial to be ready
  Serial.println("\nSetup starting...");

  delay(1000);
  
  Serial.println("Installing I2S driver...");
  esp_err_t err = i2s_install();
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    return;
  }
  
  Serial.println("Setting I2S pins...");
  err = i2s_setpin();
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    return;
  }
  
  Serial.println("Starting I2S...");
  err = i2s_start(I2S_PORT);
  if (err != ESP_OK) {
    Serial.printf("Failed to start I2S: %d\n", err);
    return;
  }

  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");

  Serial.println("Creating WAV file...");
  wavFile = SD.open("/recording.wav", FILE_WRITE);
  if (!wavFile) {
    Serial.println("Failed to create file!");
    return;
  }
  Serial.println("WAV file created successfully.");

  Serial.println("Writing WAV header...");
  writeWavHeader(wavFile, sampleRate, 16, numChannels);

  startTime = millis();
  Serial.println("Recording started...");
}

void loop() {
  if (millis() - startTime < recordingDuration) {
    size_t bytesIn = 0;
    esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen * sizeof(int32_t), &bytesIn, portMAX_DELAY);
    if (result == ESP_OK) {
      // Serial.printf("Read %d bytes from I2S\n", bytesIn);
      
// Print first few samples before conversion
      Serial.println("Before conversion:");
      for (int i = 0; i < 5; i++) {
        Serial.printf("%d ", sBuffer[i]);
      }
      Serial.println();
      
      // Extract 24-bit samples from 32-bit variable and downscale to 16-bit
      for (int i = 0; i < bufferLen; i++) {
        int32_t temp = sBuffer[i];
        // Shift right by 11 bits. works for soft sounds, louder sounds may need more reduction
        temp = (temp >> 11);
        // Check the sign bit and extend it if necessary
        if (temp & 0x8000) {
          temp |= 0xFFFF0000;
        }
        
        outputBuffer[i] = (int16_t)temp;
      }

       // Print first few samples after conversion
      Serial.println("After conversion:");
      for (int i = 0; i < 5; i++) {
        Serial.printf("%d ", outputBuffer[i]);
      }
      Serial.println();
      
      size_t bytesWritten = wavFile.write((const byte*)outputBuffer, bufferLen * sizeof(int16_t));
      // Serial.printf("Wrote %d bytes to SD card\n", bytesWritten);
    } else {
      Serial.printf("Error reading from I2S: %d\n", result);
    }
  } else if (wavFile) {
    Serial.println("Recording finished. Updating WAV header...");
    unsigned long fileSize = wavFile.size();
    updateWavHeader(wavFile, fileSize);
    wavFile.close();
    Serial.println("WAV file closed. Recording complete.");
    while(1); // Stop the program
  }
}

esp_err_t i2s_install() {
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };

  return i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

esp_err_t i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  return i2s_set_pin(I2S_PORT, &pin_config);
}

void writeWavHeader(File file, int sampleRate, int bitsPerSample, int numChannels) {
  byte header[44];
  unsigned long sampleDataSize = 100000; // Placeholder size, will be updated later

  // RIFF chunk
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  unsigned long fileSize = sampleDataSize + 36;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';

  // fmt subchunk
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; // Subchunk1Size is 16
  header[20] = 1; header[21] = 0; // AudioFormat is 1 (PCM)
  header[22] = (byte)numChannels; header[23] = 0;
  header[24] = (byte)(sampleRate & 0xFF);
  header[25] = (byte)((sampleRate >> 8) & 0xFF);
  header[26] = (byte)((sampleRate >> 16) & 0xFF);
  header[27] = (byte)((sampleRate >> 24) & 0xFF);
  unsigned long byteRate = sampleRate * numChannels * bitsPerSample / 8;
  header[28] = (byte)(byteRate & 0xFF);
  header[29] = (byte)((byteRate >> 8) & 0xFF);
  header[30] = (byte)((byteRate >> 16) & 0xFF);
  header[31] = (byte)((byteRate >> 24) & 0xFF);
  header[32] = (byte)(numChannels * bitsPerSample / 8); header[33] = 0; // BlockAlign
  header[34] = (byte)bitsPerSample; header[35] = 0;

  // data subchunk
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  header[40] = (byte)(sampleDataSize & 0xFF);
  header[41] = (byte)((sampleDataSize >> 8) & 0xFF);
  header[42] = (byte)((sampleDataSize >> 16) & 0xFF);
  header[43] = (byte)((sampleDataSize >> 24) & 0xFF);

  file.write(header, 44);
}

void updateWavHeader(File file, unsigned long fileSize) {
  file.seek(4);
  unsigned long riffSize = fileSize - 8;
  file.write((byte)(riffSize & 0xFF));
  file.write((byte)((riffSize >> 8) & 0xFF));
  file.write((byte)((riffSize >> 16) & 0xFF));
  file.write((byte)((riffSize >> 24) & 0xFF));

  file.seek(40);
  unsigned long dataSize = fileSize - 44;
  file.write((byte)(dataSize & 0xFF));
  file.write((byte)((dataSize >> 8) & 0xFF));
  file.write((byte)((dataSize >> 16) & 0xFF));
  file.write((byte)((dataSize >> 24) & 0xFF));
}