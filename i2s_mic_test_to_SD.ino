
#include <driver/i2s.h>
#include <SD.h>
#include <FS.h>

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_PORT I2S_NUM_0
#define bufferLen 512

// SD Card pins
#define SD_CS 5

int16_t sBuffer[bufferLen];
File wavFile;
const int sampleRate = 16000;
const int bitsPerSample = 16;
const int numChannels = 1;
unsigned long recordingDuration = 10000; // Recording duration in milliseconds (10 seconds)
unsigned long startTime;

void setup() {
  Serial.begin(115200);
  Serial.println("Setup I2S ...");

  delay(1000);
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");

  // Create WAV file
  wavFile = SD.open("/recording.wav", FILE_WRITE);
  if (!wavFile) {
    Serial.println("Failed to create file!");
    return;
  }

  // Write WAV header
  writeWavHeader(wavFile, sampleRate, bitsPerSample, numChannels);

  startTime = millis();
  Serial.println("Recording started...");
}

void loop() {
  if (millis() - startTime < recordingDuration) {
    size_t bytesIn = 0;
    esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);
    if (result == ESP_OK) {
      wavFile.write((const byte*)sBuffer, bytesIn);
    }
  } else if (wavFile) {
    // Update WAV header with final size
    unsigned long fileSize = wavFile.size();
    updateWavHeader(wavFile, fileSize);
    wavFile.close();
    Serial.println("Recording finished.");
    while(1); // Stop the program
  }
}

void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = sampleRate,
    .bits_per_sample = i2s_bits_per_sample_t(bitsPerSample),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0, // default interrupt priority
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
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