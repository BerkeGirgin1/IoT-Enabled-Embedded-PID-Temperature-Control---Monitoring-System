#define F_CPU 4000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <math.h>


#define SLAVE_ID 2
#define REG_ADDR 100
#define BUFFER_SIZE 32

volatile uint8_t rx_buffer[BUFFER_SIZE];
volatile uint8_t rx_index = 0;
volatile uint8_t frame_ready = 0;
volatile uint16_t modbus_timer = 0;

uint16_t holding_register_100 = 0; // Python'un okuyacağı adres bos bıraktık

//  J-TİPİ SENSÖR VE ADC AYARLARI 
#define ADC_REF       5.0     // ADC referans voltajı (AVCC)
#define FILTER_SIZE   8       // Moving average örnek sayısı
#define GAIN          100.0   // Op-amp kazancı
#define NIST_MV_MIN   0.0     // Minimum valid millivolt
#define NIST_MV_MAX   42.919  // Maximum valid millivolt

// NIST polınomu
const double a[] = {
	0.0000000E+00, 1.9528268E+01, -1.2286185E+00, 1.0752178E-01,
	-5.9086933E-03, 1.7256713E-04, -2.8131513E-06, 2.3963370E-08, -8.3823321E-11
};


//  MODBUS HABERLEŞME FONKSİYONLARI

uint16_t Modbus_CRC16(uint8_t *data, uint8_t len) {
	uint16_t crc = 0xFFFF;
	for (uint8_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++) {
			if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
			else crc >>= 1;
		}
	}
	return crc;
}

void USART_Init(uint16_t baud) {
	uint16_t ubrr = (F_CPU / (16UL * baud)) - 1;
	UBRRH = (ubrr >> 8);
	UBRRL = ubrr;
	UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE); // RX Kesmesi Aktif
	UCSRC = (1 << URSEL) | (3 << UCSZ0);
}

void USART_Transmit(uint8_t data) {
	while (!(UCSRA & (1 << UDRE)));
	UDR = data;
}

// Timer1: Modbus paket sonu (4ms sessizlik) algılaması için 1ms ayarlandı
void Timer1_Init() {
	TCCR1B = (1 << WGM12) | (1 << CS11); // CTC Modu, Prescaler 8
	OCR1A = 499;
	TIMSK |= (1 << OCIE1A);
}

// ADC VE SICAKLIK HESAPLAMA FONKSİYONLARI

void ADC_Init() {
	ADMUX = (1 << REFS0); // Vref=VCC
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS0); // ADC Aktif, Prescaler 32 (125kHz)
}

uint16_t ADC_Read(uint8_t channel) {
	channel &= 0b00000111;
	ADMUX = (ADMUX & 0xF8) | channel;
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return (ADC);
}

uint16_t readFilteredADC(uint8_t channel) {
	uint32_t sum = 0;
	for (int i = 0; i < FILTER_SIZE; i++) {
		sum += ADC_Read(channel);
		_delay_ms(1); // Kararlılık için ufak bekleme
	}
	return (uint16_t)(sum / FILTER_SIZE);
}

double calculateTemperaturePrecise(double mv) {
	double temp = 0.0;
	double mv_pow = 1.0;
	for (int i = 0; i < 9; i++) {
		temp += a[i] * mv_pow;
		mv_pow *= mv;
	}
	return temp;
}

//  KESMELER (INTERRUPTS)

ISR(USART_RXC_vect) {
	uint8_t data = UDR;
	if (rx_index < BUFFER_SIZE) {
		rx_buffer[rx_index++] = data;
		modbus_timer = 0; // Yeni bayt geldi, kronometreyi sıfırla
	}
}

ISR(TIMER1_COMPA_vect) {
	if (rx_index > 0) {
		if (++modbus_timer > 4) { // 4ms sessizlik
			frame_ready = 1;
			modbus_timer = 0;
		}
	}
}


// MODBUS İŞLEMCİSİ

void process_modbus() {
	if (!frame_ready) return;
	frame_ready = 0;
	
	if (rx_index < 8) {
		rx_index = 0;
		return;
	}

	uint8_t addr = rx_buffer[0];
	uint8_t func = rx_buffer[1];
	uint16_t crc_recv = rx_buffer[rx_index - 2] | (rx_buffer[rx_index - 1] << 8);
	uint16_t crc_calc = Modbus_CRC16((uint8_t*)rx_buffer, rx_index - 2);

	if (addr != SLAVE_ID || crc_recv != crc_calc) {
		rx_index = 0;
		return;
	}

	uint8_t tx_buf[8];
	uint8_t len = 0;

	// Okuma İsteği (0x03)
	if (func == 0x03) {
		uint16_t reg_addr = (rx_buffer[2] << 8) | rx_buffer[3];
		uint16_t qty = (rx_buffer[4] << 8) | rx_buffer[5];
		
		if (reg_addr == REG_ADDR && qty == 1) {
			tx_buf[0] = SLAVE_ID;
			tx_buf[1] = 0x03;
			tx_buf[2] = 2; // Byte count
			
			tx_buf[3] = (holding_register_100 >> 8) & 0xFF; // High byte
			tx_buf[4] = holding_register_100 & 0xFF;        // Low byte
			
			uint16_t tx_crc = Modbus_CRC16(tx_buf, 5);
			tx_buf[5] = tx_crc & 0xFF;
			tx_buf[6] = (tx_crc >> 8) & 0xFF;
			len = 7;
			
			for (uint8_t i = 0; i < len; i++) USART_Transmit(tx_buf[i]);
		}
	}
	rx_index = 0;
}


//  ANA PROGRAM

int main(void) {
	USART_Init(19200);
	Timer1_Init();
	ADC_Init();
	sei();

	double cold_junction_temp = 20.0; // Ortam referans sıcaklığı
	
	while (1) {
		// 1Kanal 2'den (PC2) filtrelenmiş ADC'yi oku 
		uint16_t adcValue = readFilteredADC(2);
		
		//  ADC'yi Voltaj ve Milivoltaja Çevir
		double voltage = ((double)adcValue * ADC_REF) / 1023.0;
		double mv_thermocouple = (voltage * 1000.0) / GAIN;
		
		double final_temp = 0.0;
		
		// NIST Polinomu ile Sıcaklık Hesabı (Sınır Kontrolü ile)
		if (mv_thermocouple >= NIST_MV_MIN && mv_thermocouple <= NIST_MV_MAX) {
			final_temp = calculateTemperaturePrecise(mv_thermocouple) + cold_junction_temp;
			} else {
			final_temp = 999.0; // Hata kodu: Kablo koptuysa veya aralık dışıysa Python 999 görsün
		}
		
		// 4. Double değeri 16-bit tamsayıya çevir ve Modbus Register'ına kaydet
		holding_register_100 = (uint16_t)final_temp;
		
		// 5. Modbus paketlerini kontrol et ve cevapla
		process_modbus();
	}
}