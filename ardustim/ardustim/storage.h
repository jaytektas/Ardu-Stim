#define EEPROM_VERSION          1
#define EEPROM_WHEEL            2
#define EEPROM_RPM_MODE         3
#define EEPROM_CURRENT_RPM      4 //Note this is 2 bytes
#define EEPROM_SWEEP_RPM_MIN    6 //Note this is 2 bytes
#define EEPROM_SWEEP_RPM_MAX    8 //Note this is 2 bytes
#define EEPROM_SWEEP_RPM_INT    10 //Note this is 2 bytes
#define EEPROM_FIXED_RPM        12 //Note this is 2 bytes
#define EEPROM_CRANK_CURVE      14
#define EEPROM_CURVE_HALVES     15
#define EEPROM_CURVE_MAX_RPM    16 //Note this is 2 bytes
#define EEPROM_CURVE_OFFSET     18 //Note this is 2 bytes
// The amplitude had no slot, because the old compression had no amplitude to store — it was baked
// into the tables. Without this the curve came back at 0% after every DTR reset, which on this board
// is every time any host opens the port.
#define EEPROM_CURVE_AMPLITUDE  20

void loadConfig();
void saveConfig();
