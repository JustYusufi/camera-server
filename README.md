# camera-server

esp_camera.ino dosyasını ESP32-S3-DevKitC-1 N16R8 kartınıza COM portu üzerinden yükleyin.
Kod yüklendikten sonra kartınızı USB portu üzerinden takın, bilgisayarınızda yeni bir USB Bellek göreceksiniz, belleğin içerisine camera.exe dosyasını atın.
Kartı çıkartıp tekrar takın, bir kaç saniye sonra camera.exe'nin açıldığını ve cihazınızın kamreasından fotoğraf çektiğini göreceksiniz.
upload.php'yi başka bir cihazda çalıştırın, Cihazda php'nin yüklü olduğundan emin olun. php -S 0.0.0.0:8080 komudunu uçbirim üzerinden upload.php'nin bulunduğu dosya konumunda çalıştırın. Aynı dosya konumunda captures adında bir klasör oluşturun. Kart çalıştığında çekilen fotoğraflar sunucunuza gönderilecek ve captures dosyasına kaydedilecektir.
