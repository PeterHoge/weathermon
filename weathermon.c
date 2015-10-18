#include "weathermon.h"

static const int MAX_NUM_SENSORS   =    8;
static const int DEFAULT_VALUE     = 9999;
static const int RX_PIN            =    3;
static const int SHORT_DELAY       =  242;
static const int LONG_DELAY        =  484;
static const int NUM_HEADER_BITS   =   10;
static const int MAX_BYTES         =    6;

char      temp_bit;
bool      first_zero;
char      header_hits;
char      data_byte;
char      num_bits;
char      num_bytes;
char      manchester[7];
sqlite3   *db;
sigset_t  myset;

int main() 
{
    init();
    wiringPiISR(RX_PIN, INT_EDGE_BOTH, &read_signal);
    sigsuspend(&myset);
}

int init() 
{
    init_globals();
    for (int i = 0; i < 4; i++) {
        manchester[i] = i;
    }
    wiringPiSetup();
    pinMode(RX_PIN, INPUT);
    sqlite3_open("/var/www/weathermon/weather.db", &db);
}

void init_globals() 
{
    temp_bit = 1;
    first_zero = false;
    header_hits = 0;
    num_bits = 6;
    num_bytes = 0;
}

void read_signal()
{
    delayMicroseconds(SHORT_DELAY);
    if (digitalRead(RX_PIN) != temp_bit) {
        return;
    }
    delayMicroseconds(LONG_DELAY);
    if(digitalRead(RX_PIN) == temp_bit) {
        temp_bit = temp_bit ^ 1;
    }
    char bit = temp_bit ^ 1;
    if (bit == 1) {
        if (!first_zero) {
            header_hits++;
        } else {
            add_bit(bit);
        }
    } else {
        if(header_hits < NUM_HEADER_BITS) {
            return;
        }
        if (!first_zero) {
            first_zero = true;
        }
        add_bit(bit);
    }
    if (num_bytes == MAX_BYTES) {
        record_sensor_data(); 
        init_globals();
    }
}

void add_bit(char bit) 
{
    data_byte = (data_byte << 1) | bit;
    if (++num_bits == 8) {
        num_bits=0;
        manchester[num_bytes++] = data_byte;
    }
}

void record_sensor_data()
{
    int ch = ((manchester[3] & 0x70) / 16) + 1;
    int data_type = manchester[1];
    int new_temp = ((manchester[3] & 0x7) * 256 + manchester[4]) - 400;
    int new_hum = manchester[5]; 
    int low_bat = manchester[3] & 0x80 / 128;
    if (data_type != 0x45 || ch < 1 || ch > MAX_NUM_SENSORS || new_hum > 100 ||
                 new_temp < -200 || new_temp > 1200) {
        return;
    }
    printf("Sensor %d:  %dF  %d% Batt: %d\n", ch, new_temp, new_hum, low_bat);
    char *sql = sqlite3_mprintf("INSERT INTO reading " \
                                "(channel, temperature, humidity, battery) " \
                                "VALUES (%d, %d, %d, %d)", 
                                ch, new_temp, new_hum, low_bat);
    sqlite3_exec(db, sql, 0, 0, 0);
    sqlite3_free(sql);
}

