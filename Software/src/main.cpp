#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h> // untuk sqrt()

#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / 16 / BAUD) - 1)

volatile unsigned long timer_millis = 0;

void uart_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_send(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

void uart_sendString(const char* str) {
    while (*str) {
        uart_send(*str++);
    }
}

void adc_init() {
    ADMUX = (1 << REFS0); // AVcc reference
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // Enable ADC, prescaler 64
}

uint16_t adc_read(uint8_t ch) {
    ch &= 0b00000111;
    ADMUX = (ADMUX & 0xF0) | ch;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void timer0_init() {
    TCCR0A = 0;
    TCCR0B = (1 << CS01) | (1 << CS00); // prescaler 64
    TIMSK0 = (1 << TOIE0);              // enable overflow interrupt
    TCNT0 = 6; // preload
    sei();
}

ISR(TIMER0_OVF_vect) {
    timer_millis++;
    TCNT0 = 6; 
}

unsigned long millis() {
    unsigned long millis_copy;
    cli();
    millis_copy = timer_millis;
    sei();
    return millis_copy;
}

int main(void) {
    char buffer[200];
    uart_init(MYUBRR);
    adc_init();
    timer0_init();

    unsigned long previousMillis = 0;
    const long interval = 500; // 500ms

    // Variabel untuk simpan latitude dan longitude sebelumnya
    float last_latitude = 0.0;
    float last_longitude = 0.0;
    float total_distance_m = 0.0;
    while (1) {
        if (millis() - previousMillis >= interval) {
            previousMillis = millis();

            uint16_t adc0 = adc_read(0); // A0: Heart Rate
            uint16_t adc1 = adc_read(1); // A1: Latitude
            uint16_t adc2 = adc_read(2); // A2: Longitude

            uint16_t heart_rate = (adc0 * 200) / 1023;
            float latitude = (((float)adc1 / 1023.0) * 180.0) - 90.0;
            float longitude = (((float)adc2 / 1023.0) * 360.0) - 180.0;

          // Tambahan variabel di atas (setelah last_longitude):


          // Di dalam loop while (di dalam if (millis() - previousMillis >= interval)):

          // Hitung perubahan latitude dan longitude
          float delta_lat = latitude - last_latitude;
          float delta_lon = longitude - last_longitude;

          // Hitung delta dalam derajat ke meter
          float delta_lat_m = delta_lat * 111320.0; // 1° latitude = 111.32 km
          float delta_lon_m = delta_lon * 111320.0 * cos(latitude * (M_PI / 180.0)); // Perlu cos(latitude dalam radian)

          // Hitung distance dengan rumus pythagoras
          float distance_m = sqrt((delta_lat_m * delta_lat_m) + (delta_lon_m * delta_lon_m));

          // Tambahkan ke total perjalanan
          total_distance_m += distance_m;

          // Update last latitude dan longitude
          last_latitude = latitude;
          last_longitude = longitude;

          // Format semua data termasuk distance dalam meter
          int dist_x_int = (int)delta_lat_m;
          int dist_x_frac = (int)((delta_lat_m - dist_x_int) * 100);

          int dist_y_int = (int)delta_lon_m;
          int dist_y_frac = (int)((delta_lon_m - dist_y_int) * 100);

          int dist_res_int = (int)distance_m;
          int dist_res_frac = (int)((distance_m - dist_res_int) * 100);

          int total_dist_int = (int)total_distance_m;
          int total_dist_frac = (int)((total_distance_m - total_dist_int) * 100);

          // Kirim ke UART (bisa tambah di bawah sprintf lama kamu)
          sprintf(buffer, 
                  "Heart Rate: %d bpm, Distance X: %d.%d m, Distance Y: %d.%d m, Distance Resultant: %d.%d m, Total Distance: %d.%d m\r\n", 
                  heart_rate, dist_x_int, dist_x_frac, 
                  dist_y_int, dist_y_frac, 
                  dist_res_int, dist_res_frac,
                  total_dist_int, total_dist_frac);

          uart_sendString(buffer);
        }
    }
}
