import serial
import tkinter as tk
import paho.mqtt.client as mqtt
from serial.serialutil import SerialException

# ==========================================
# 1. AYARLAR (KENDİNE GÖRE GÜNCELLE)
# ==========================================
port_name = 'COM11'  # Kendi ATmega portunu yaz
baudrate = 19200
slave_address = 2
register_address = 100

# MQTT Ayarları (HiveMQ Public Broker)
broker_address = "broker.hivemq.com"
mqtt_port = 1883
# Burası bizim internetteki özel odamız (Topic):
mqtt_topic = "iyte/ee446/berke/sicaklik" 

# ==========================================
# 2. MQTT İSTEMCİSİ KURULUMU
# ==========================================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, "Berke_EE446_Master")
try:
    client.connect(broker_address, mqtt_port, 60)
    client.loop_start()  # Arka planda HiveMQ ile iletişimi canlı tutar
    print("HiveMQ Sunucusuna Başarıyla Bağlanıldı!")
except Exception as e:
    print(f"MQTT Bağlantı Hatası: {e}")

# ==========================================
# 3. MODBUS HABERLEŞME FONKSİYONLARI
# ==========================================
def calculate_crc(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc.to_bytes(2, byteorder='little')

def configure_serial_connection():
    try:
        ser = serial.Serial(port=port_name, baudrate=baudrate, timeout=1)
        return ser
    except SerialException:
        status_label.config(text="HATA: COM Port Kapalı!", fg="red")
        return None

# ==========================================
# 4. PERİYODİK OKUMA VE PUBLISH (YAYINLAMA)
# ==========================================
def update_temperature():
    ser = configure_serial_connection()
    if ser is not None:
        try:
            # 0x03 Okuma İsteği Paketi Hazırlama
            read_request = bytearray([slave_address, 0x03, (register_address >> 8) & 0xFF, register_address & 0xFF, 0x00, 0x01])
            read_request += calculate_crc(read_request)
            
            # Paketi Yolla ve 7 Baytlık Cevap Bekle
            ser.write(read_request)
            response = ser.read(7)
            
            # Cevap doğru geldiyse parçala
            if len(response) == 7 and response[0] == slave_address and response[1] == 0x03:
                # Gelen sıcaklık değerini birleştir (High ve Low Byte)
                temperature_val = int.from_bytes(response[3:5], byteorder='big')
                
                # 1. Ekrana Yazdır
                temp_label.config(text=f"{temperature_val} °C", fg="green")
                status_label.config(text="Sistem Aktif: Veri Okunuyor & Gönderiliyor", fg="blue")
                
                # 2. MQTT ile İnternete (HiveMQ) Fırlat!
                client.publish(mqtt_topic, str(temperature_val))
                print(f"Sicaklik: {temperature_val}°C -> HiveMQ'ya gönderildi!")
                
            else:
                status_label.config(text="HATA: Modbus Yanıtı Geçersiz", fg="red")
        except Exception as e:
            status_label.config(text="HATA: Okuma Başarısız", fg="red")
        finally:
            ser.close()

    # GUI donmasın diye 2000 ms (2 saniye) sonra bu fonksiyonu tekrar çağır (Sonsuz Döngü)
    root.after(2000, update_temperature)

# ==========================================
# 5. GUI (ARAYÜZ) TASARIMI
# ==========================================
root = tk.Tk()
root.title("IoT Modbus Sıcaklık Monitörü")
root.geometry("400x300")
root.configure(bg="#2C3E50")

title_label = tk.Label(root, text="ATmega8A Canlı Sıcaklık", font=("Helvetica", 16, "bold"), bg="#2C3E50", fg="white")
title_label.pack(pady=20)

# Sıcaklığın yazacağı devasa yazı
temp_label = tk.Label(root, text="-- °C", font=("Helvetica", 48, "bold"), bg="#2C3E50", fg="yellow")
temp_label.pack(pady=20)

status_label = tk.Label(root, text="Bağlantı Bekleniyor...", font=("Helvetica", 10), bg="#2C3E50", fg="white")
status_label.pack(side=tk.BOTTOM, pady=10)

# İlk okumayı başlat ve döngüye sok
root.after(1000, update_temperature)

root.mainloop()