#ifndef STRUCTS_H
#define STRUCTS_H

#include <Arduino.h>

const int MAX_MULTI_ENTRIES = 5;
const int MAX_RADAR_AIRCRAFT = 6;

struct FetchTrackers {
  unsigned long lastDataUpdate = 0;
  unsigned long lastWeatherFetch = 0;
  unsigned long lastAqiFetch = 0;
  unsigned long lastCurrencyFetch = 0;
  unsigned long lastFlightFetch = 0;
};

enum ScreenType {
  SCREEN_TIME,
  SCREEN_CALENDAR,
  SCREEN_WEATHER,
  SCREEN_AIR_QUALITY,
  SCREEN_DAYLIGHT,
  SCREEN_MOON,
  SCREEN_POPULATION,
  SCREEN_FLIGHT,
  SCREEN_CURRENCY,
  SCREEN_PC_MONITOR,
  SCREEN_PC_MEDIA,
  SCREEN_BAMBU,
  SCREEN_SAVER,
  NUM_SCREENS
};

inline constexpr const char* SCREEN_NAMES[] = {
  "Time & Date",
  "Calendar",
  "Weather",
  "Air Quality",
  "Daylight Info",
  "Moon Info",
  "Population Info",
  "Flight Radar",
  "Currency Exchange",
  "PC Monitor",
  "PC Media",
  "Printer Info",
  "Screensaver"
};

enum AnimType {
  ANIM_NONE,
  ANIM_SLIDE_HORIZONTAL,
  ANIM_SLIDE_VERTICAL,
  ANIM_DISSOLVE,
  ANIM_CURTAIN,
  ANIM_BLINDS,
  ANIM_RANDOM
};

struct Config {
  // Network Data
  String device_id = "";
  String ip_address = "";
  String active_pc_id = "";

  // Hardware Setup
  int sda_pin = 8;
  int scl_pin = 9;
  int button_pin = 10;
  String button_type = "touch";

  // Global Settings
  bool auto_detect = true;
  float latitude = 0.0;
  float longitude = 0.0;
  String country = "";
  String country_code = "";
  String city = "";
  String timezone = "";
  String ntp_server = "";
  
  String time_format = "24";
  bool date_display = false;
  int time_style = 0;                // 0 = classic digits, 1 = animated Mac desktop
  unsigned long refresh_interval_min = 15;

  // Theme Settings
  String theme_bg = "#000000";
  String theme_card = "#111111";
  String theme_accent = "#ffffff";
  String theme_text = "#ffffff";

  // OLED Look
  String ui_chrome = "window";       // "window" = title bar and frame, "menubar" = menu bar with clock
  String ui_paper = "black";         // "black" = lit ink on dark, "white" = dark ink on lit

  // Screens Settings
  bool screen_auto_cycle = true;
  int screen_interval_sec = 15;
  int screen_order[NUM_SCREENS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  bool show_time = true;
  bool show_calendar = true;
  bool show_weather = true;
  bool show_aqi = true;
  bool show_daylight = true;
  bool show_moon = true;
  bool show_population = true;
  bool show_currency = true;
  bool show_pc = true;
  bool show_media = true;
  bool show_bambu = true;
  bool show_flight = true;
  bool show_saver = true;

  bool hide_empty_pc = true;
  bool hide_empty_media = true;
  bool hide_empty_bambu = true;
  bool hide_empty_flight = true;

  // Calendar Settings
  String calendar_start_day = "mon";
  bool calendar_minimal = false;

  // Weather & AQI Settings
  bool round_temps = true;
  String temp_unit = "C";
  String aqi_type = "US";
  bool weather_show_header = false;                                          // false = "no header" layout (more room for weather_values)
  String weather_values[6] = {"feels", "humidity", "wind", "", "", ""};      // up to 3 when weather_show_header is true, up to 6 otherwise
  bool aqi_show_header = false;
  String aqi_values[6] = {"pm25", "pm10", "no2", "", "", ""};                // up to 3 when aqi_show_header is true, up to 6 otherwise
  int custom_weather_int_min = -1;
  int custom_aqi_int_min = -1;

  // Daylight Settings
  bool daylight_minimal = false;

  // Moon Settings
  bool moon_minimal = false;

  // Population Settings
  bool pop_show_world = true;
  bool pop_show_country = true;

  // Currency Settings
  String currency_bases[MAX_MULTI_ENTRIES] = {"usd", "", "", "", ""};
  String currency_targets[MAX_MULTI_ENTRIES] = {"eur", "", "", "", ""};
  int currency_multipliers[MAX_MULTI_ENTRIES] = {1, 1, 1, 1, 1};
  int currency_count = 1;

  bool currency_fn = true;

  int custom_currency_int_min = -1;

  // Printer Settings
  String bambu_ip = "";
  String bambu_sn = "";
  String bambu_code = "";

  // Flight Radar Settings
  String flight_mode = "closest";
  int flight_radius_nm = 25;
  String flight_units = "aviation";
  String flight_primary_info = "callsign";
  String flight_secondary_info = "none";
  int custom_flight_int_min = 1;

  // Animation Settings
  uint16_t anim_mask = 62;
  uint16_t saver_mask = 15;          // one bit per SAVER_SCENES entry

  // Night Mode Settings
  bool night_mode = false;
  String night_start = "23:00";
  String night_end = "06:00";
  String night_dim_start = "22:00";
  int night_action = 1; 
};

struct WeatherData {
  float temp = NAN;
  float apparent_temperature = NAN;
  float wind_speed = NAN;
  int humidity = 0;
  int weather_code = -1;
  bool is_day = NAN;
  float precipitation_probability = NAN;
  float pressure = NAN;
  float visibility = NAN;
  String update_time = "N/A";
};

struct AirQualityData {
  int aqi = -1;
  float pm25 = NAN;
  float pm10 = NAN;
  float no2 = NAN;
  float co = NAN;
  float co2 = NAN;
  float so2 = NAN;
  float o3 = NAN;
  float dust = NAN;
  float uv = NAN;
  float ch4 = NAN;
  String status = "N/A";
};

struct DaylightData {
  int sunrise_mins = -1;
  int sunset_mins = -1;
  int noon_mins = -1;
  int length_mins = -1;
  int last_fetch_yday = -1;
};

struct MoonData {
  String curphase = "N/A";
  int fracillum = -1; 
  int rise_mins = -1;
  int set_mins = -1;
  int last_fetch_yday = -1;
};

struct PopulationData {
  long long world_pop_base = -1;
  double world_growth = 0.0;
  int world_year = -1;
  
  long long country_pop_base = -1;
  double country_growth = 0.0;
  int country_year = -1;
  
  int last_fetch_yday = -1;
};

struct CurrencyData {
  String base;
  String target;
  float rate;
  String date;
  bool updated = false;
};

struct PcStats {
  float cpu_percent;
  float mem_percent;
  float disk_percent;
  float net_down_kb;
  unsigned long last_update = 0;
  bool is_wifi = false;
};

struct PcMedia {
  String status;
  String name;
  String author;
  String album;
  unsigned long last_update = 0;
};

struct BambuData {
  String status = "SYNCING";
  int progress = 0;
  int time_left = 0;
  float nozzle_temp = 0.0;
  float nozzle_target = 0.0;
  float bed_temp = 0.0;
  float bed_target = 0.0;
  int layer = 0;
  int total_layers = 0;
  String file_name = "None";
  int fan_part = 0;
  int fan_aux = 0;
};

struct FlightAircraft {
  String callsign = "";
  String icao24 = "";
  float lat = NAN;
  float lon = NAN;
  float altitude_ft = NAN;
  float velocity_kt = NAN;
  float track_deg = NAN;
  float distance_km = NAN;
  String registration = "";
  String type_designator = "";
  String squawk = "";
  String origin_code = "";
  String destination_code = "";
  String origin_country = "";
  String destination_country = "";
  String airline_code = "";
  bool has_route = false;
};

struct FlightData {
  FlightAircraft aircraft[MAX_RADAR_AIRCRAFT];
  int aircraft_count = 0;

  FlightAircraft closest;
  String origin_city = "";
  String destination_city = "";
  String aircraft_manufacturer = "";
  String aircraft_type = "";
};

struct CountryOption {
  const char* code;
  const char* name;
};

struct CurrencyOption {
  const char* code;
  const char* name;
};

inline constexpr CountryOption allCountries[] = {
  {"AF", "Afghanistan"}, {"AL", "Albania"}, {"DZ", "Algeria"}, {"AS", "American Samoa"},
  {"AD", "Andorra"}, {"AO", "Angola"}, {"AI", "Anguilla"}, {"AQ", "Antarctica"},
  {"AG", "Antigua and Barbuda"}, {"AR", "Argentina"}, {"AM", "Armenia"}, {"AW", "Aruba"},
  {"AU", "Australia"}, {"AT", "Austria"}, {"AZ", "Azerbaijan"}, {"BS", "Bahamas"},
  {"BH", "Bahrain"}, {"BD", "Bangladesh"}, {"BB", "Barbados"}, {"BY", "Belarus"},
  {"BE", "Belgium"}, {"BZ", "Belize"}, {"BJ", "Benin"}, {"BM", "Bermuda"},
  {"BT", "Bhutan"}, {"BO", "Bolivia"}, {"BA", "Bosnia and Herzegovina"}, {"BW", "Botswana"},
  {"BR", "Brazil"}, {"IO", "British Indian Ocean Territory"}, {"VG", "British Virgin Islands"},
  {"BN", "Brunei"}, {"BG", "Bulgaria"}, {"BF", "Burkina Faso"}, {"BI", "Burundi"},
  {"CV", "Cabo Verde"}, {"KH", "Cambodia"}, {"CM", "Cameroon"}, {"CA", "Canada"},
  {"KY", "Cayman Islands"}, {"CF", "Central African Republic"}, {"TD", "Chad"},
  {"CL", "Chile"}, {"CN", "China"}, {"CX", "Christmas Island"}, {"CC", "Cocos Islands"},
  {"CO", "Colombia"}, {"KM", "Comoros"}, {"CD", "Congo (DRC)"}, {"CG", "Congo (Republic)"},
  {"CK", "Cook Islands"}, {"CR", "Costa Rica"}, {"CI", "Cote d'Ivoire"}, {"HR", "Croatia"},
  {"CU", "Cuba"}, {"CW", "Curacao"}, {"CY", "Cyprus"}, {"CZ", "Czechia"},
  {"DK", "Denmark"}, {"DJ", "Djibouti"}, {"DM", "Dominica"}, {"DO", "Dominican Republic"},
  {"EC", "Ecuador"}, {"EG", "Egypt"}, {"SV", "El Salvador"}, {"GQ", "Equatorial Guinea"},
  {"ER", "Eritrea"}, {"EE", "Estonia"}, {"SZ", "Eswatini"}, {"ET", "Ethiopia"},
  {"FK", "Falkland Islands"}, {"FO", "Faroe Islands"}, {"FJ", "Fiji"}, {"FI", "Finland"},
  {"FR", "France"}, {"GF", "French Guiana"}, {"PF", "French Polynesia"}, {"GA", "Gabon"},
  {"GM", "Gambia"}, {"GE", "Georgia"}, {"DE", "Germany"}, {"GH", "Ghana"},
  {"GI", "Gibraltar"}, {"GR", "Greece"}, {"GL", "Greenland"}, {"GD", "Grenada"},
  {"GP", "Guadeloupe"}, {"GU", "Guam"}, {"GT", "Guatemala"}, {"GG", "Guernsey"},
  {"GN", "Guinea"}, {"GW", "Guinea-Bissau"}, {"GY", "Guyana"}, {"HT", "Haiti"},
  {"HN", "Honduras"}, {"HK", "Hong Kong"}, {"HU", "Hungary"}, {"IS", "Iceland"},
  {"IN", "India"}, {"ID", "Indonesia"}, {"IR", "Iran"}, {"IQ", "Iraq"},
  {"IE", "Ireland"}, {"IM", "Isle of Man"}, {"IL", "Israel"}, {"IT", "Italy"},
  {"JM", "Jamaica"}, {"JP", "Japan"}, {"JE", "Jersey"}, {"JO", "Jordan"},
  {"KZ", "Kazakhstan"}, {"KE", "Kenya"}, {"KI", "Kiribati"}, {"KW", "Kuwait"},
  {"KG", "Kyrgyzstan"}, {"LA", "Laos"}, {"LV", "Latvia"}, {"LB", "Lebanon"},
  {"LS", "Lesotho"}, {"LR", "Liberia"}, {"LY", "Libya"}, {"LI", "Liechtenstein"},
  {"LT", "Lithuania"}, {"LU", "Luxembourg"}, {"MO", "Macao"}, {"MG", "Madagascar"},
  {"MW", "Malawi"}, {"MY", "Malaysia"}, {"MV", "Maldives"}, {"ML", "Mali"},
  {"MT", "Malta"}, {"MH", "Marshall Islands"}, {"MQ", "Martinique"}, {"MR", "Mauritania"},
  {"MU", "Mauritius"}, {"YT", "Mayotte"}, {"MX", "Mexico"}, {"FM", "Micronesia"},
  {"MD", "Moldova"}, {"MC", "Monaco"}, {"MN", "Mongolia"}, {"ME", "Montenegro"},
  {"MS", "Montserrat"}, {"MA", "Morocco"}, {"MZ", "Mozambique"}, {"MM", "Myanmar"},
  {"NA", "Namibia"}, {"NR", "Nauru"}, {"NP", "Nepal"}, {"NL", "Netherlands"},
  {"NC", "New Caledonia"}, {"NZ", "New Zealand"}, {"NI", "Nicaragua"}, {"NE", "Niger"},
  {"NG", "Nigeria"}, {"NU", "Niue"}, {"NF", "Norfolk Island"}, {"KP", "North Korea"},
  {"MK", "North Macedonia"}, {"MP", "Northern Mariana Islands"}, {"NO", "Norway"},
  {"OM", "Oman"}, {"PK", "Pakistan"}, {"PW", "Palau"}, {"PS", "Palestine"},
  {"PA", "Panama"}, {"PG", "Papua New Guinea"}, {"PY", "Paraguay"}, {"PE", "Peru"},
  {"PH", "Philippines"}, {"PN", "Pitcairn"}, {"PL", "Poland"}, {"PT", "Portugal"},
  {"PR", "Puerto Rico"}, {"QA", "Qatar"}, {"RE", "Reunion"}, {"RO", "Romania"},
  {"RU", "Russia"}, {"RW", "Rwanda"}, {"WS", "Samoa"}, {"SM", "San Marino"},
  {"ST", "Sao Tome and Principe"}, {"SA", "Saudi Arabia"}, {"SN", "Senegal"},
  {"RS", "Serbia"}, {"SC", "Seychelles"}, {"SL", "Sierra Leone"}, {"SG", "Singapore"},
  {"SX", "Sint Maarten"}, {"SK", "Slovakia"}, {"SI", "Slovenia"}, {"SB", "Solomon Islands"},
  {"SO", "Somalia"}, {"ZA", "South Africa"}, {"GS", "South Georgia"}, {"KR", "South Korea"},
  {"SS", "South Sudan"}, {"ES", "Spain"}, {"LK", "Sri Lanka"}, {"BL", "St. Barthelemy"},
  {"KN", "St. Kitts and Nevis"}, {"LC", "St. Lucia"}, {"MF", "St. Martin"},
  {"PM", "St. Pierre and Miquelon"}, {"VC", "St. Vincent and Grenadines"}, {"SD", "Sudan"},
  {"SR", "Suriname"}, {"SJ", "Svalbard and Jan Mayen"}, {"SE", "Sweden"},
  {"CH", "Switzerland"}, {"SY", "Syria"}, {"TW", "Taiwan"}, {"TJ", "Tajikistan"},
  {"TZ", "Tanzania"}, {"TH", "Thailand"}, {"TL", "Timor-Leste"}, {"TG", "Togo"},
  {"TK", "Tokelau"}, {"TO", "Tonga"}, {"TT", "Trinidad and Tobago"}, {"TN", "Tunisia"},
  {"TR", "Turkey"}, {"TM", "Turkmenistan"}, {"TC", "Turks and Caicos Islands"},
  {"TV", "Tuvalu"}, {"VI", "U.S. Virgin Islands"}, {"UG", "Uganda"}, {"UA", "Ukraine"},
  {"AE", "United Arab Emirates"}, {"GB", "United Kingdom"}, {"US", "United States"},
  {"UY", "Uruguay"}, {"UZ", "Uzbekistan"}, {"VU", "Vanuatu"}, {"VA", "Vatican City"},
  {"VE", "Venezuela"}, {"VN", "Vietnam"}, {"WF", "Wallis and Futuna"},
  {"EH", "Western Sahara"}, {"YE", "Yemen"}, {"ZM", "Zambia"}, {"ZW", "Zimbabwe"}
};

inline constexpr CurrencyOption allCurrencies[] = {
  {"aed", "United Arab Emirates Dirham"}, {"afn", "Afghan Afghani"}, {"all", "Albanian Lek"},
  {"amd", "Armenian Dram"}, {"ang", "Netherlands Antillean Guilder"}, {"aoa", "Angolan Kwanza"},
  {"ars", "Argentine Peso"}, {"aud", "Australian Dollar"}, {"awg", "Aruban Florin"},
  {"azn", "Azerbaijani Manat"}, {"bam", "Bosnia-Herzegovina Convertible Mark"}, {"bbd", "Barbadian Dollar"},
  {"bdt", "Bangladeshi Taka"}, {"bgn", "Bulgarian Lev"}, {"bhd", "Bahraini Dinar"},
  {"bif", "Burundian Franc"}, {"bmd", "Bermudan Dollar"}, {"bnd", "Brunei Dollar"},
  {"bob", "Bolivian Boliviano"}, {"brl", "Brazilian Real"}, {"bsd", "Bahamian Dollar"},
  {"btn", "Bhutanese Ngultrum"}, {"bwp", "Botswanan Pula"}, {"byn", "New Belarusian Ruble"},
  {"bzd", "Belize Dollar"}, {"cad", "Canadian Dollar"}, {"cdf", "Congolese Franc"},
  {"chf", "Swiss Franc"}, {"clp", "Chilean Peso"}, {"cny", "Chinese Yuan"},
  {"cop", "Colombian Peso"}, {"crc", "Costa Rican Colón"}, {"cup", "Cuban Peso"},
  {"cve", "Cape Verdean Escudo"}, {"czk", "Czech Republic Koruna"}, {"djf", "Djiboutian Franc"},
  {"dkk", "Danish Krone"}, {"dop", "Dominican Peso"}, {"dzd", "Algerian Dinar"},
  {"egp", "Egyptian Pound"}, {"ern", "Eritrean Nakfa"}, {"etb", "Ethiopian Birr"},
  {"eur", "Euro"}, {"fjd", "Fijian Dollar"}, {"fkp", "Falkland Islands Pound"},
  {"gbp", "British Pound Sterling"}, {"gel", "Georgian Lari"}, {"ghs", "Ghanaian Cedi"},
  {"gip", "Gibraltar Pound"}, {"gmd", "Gambian Dalasi"}, {"gnf", "Guinean Franc"},
  {"gtq", "Guatemalan Quetzal"}, {"gyd", "Guyanaese Dollar"}, {"hkd", "Hong Kong Dollar"},
  {"hnl", "Honduran Lempira"}, {"htg", "Haitian Gourde"}, {"huf", "Hungarian Forint"},
  {"idr", "Indonesian Rupiah"}, {"ils", "Israeli New Sheqel"}, {"inr", "Indian Rupee"},
  {"iqd", "Iraqi Dinar"}, {"irr", "Iranian Rial"}, {"isk", "Icelandic Króna"},
  {"jmd", "Jamaican Dollar"}, {"jod", "Jordanian Dinar"}, {"jpy", "Japanese Yen"},
  {"kes", "Kenyan Shilling"}, {"kgs", "Kyrgystani Som"}, {"khr", "Cambodian Riel"},
  {"kmf", "Comorian Franc"}, {"kpw", "North Korean Won"}, {"krw", "South Korean Won"},
  {"kwd", "Kuwaiti Dinar"}, {"kyd", "Cayman Islands Dollar"}, {"kzt", "Kazakhstani Tenge"},
  {"lak", "Laotian Kip"}, {"lbp", "Lebanese Pound"}, {"lkr", "Sri Lankan Rupee"},
  {"lrd", "Liberian Dollar"}, {"lsl", "Lesotho Loti"}, {"lyd", "Libyan Dinar"},
  {"mad", "Moroccan Dirham"}, {"mdl", "Moldovan Leu"}, {"mga", "Malagasy Ariary"},
  {"mkd", "Macedonian Denar"}, {"mmk", "Myanma Kyat"}, {"mnt", "Mongolian Tugrik"},
  {"mop", "Macanese Pataca"}, {"mru", "Mauritanian Ouguiya"}, {"mur", "Mauritian Rupee"},
  {"mvr", "Maldivian Rufiyaa"}, {"mwk", "Malawian Kwacha"}, {"mxn", "Mexican Peso"},
  {"myr", "Malaysian Ringgit"}, {"mzn", "Mozambican Metical"}, {"nad", "Namibian Dollar"},
  {"ngn", "Nigerian Naira"}, {"nio", "Nicaraguan Córdoba"}, {"nok", "Norwegian Krone"},
  {"npr", "Nepalese Rupee"}, {"nzd", "New Zealand Dollar"}, {"omr", "Omani Rial"},
  {"pab", "Panamanian Balboa"}, {"pen", "Peruvian Nuevo Sol"}, {"pgk", "Papua New Guinean Kina"},
  {"php", "Philippine Peso"}, {"pkr", "Pakistani Rupee"}, {"pln", "Polish Zloty"},
  {"pyg", "Paraguayan Guarani"}, {"qar", "Qatari Rial"}, {"ron", "Romanian Leu"},
  {"rsd", "Serbian Dinar"}, {"rub", "Russian Ruble"}, {"rwf", "Rwandan Franc"},
  {"sar", "Saudi Riyal"}, {"sbd", "Solomon Islands Dollar"}, {"scr", "Seychellois Rupee"},
  {"sdg", "Sudanese Pound"}, {"sek", "Swedish Krona"}, {"sgd", "Singapore Dollar"},
  {"shp", "Saint Helena Pound"}, {"sll", "Sierra Leonean Leone"}, {"sos", "Somali Shilling"},
  {"srd", "Surinamese Dollar"}, {"stn", "São Tomé and Príncipe Dobra"}, {"svc", "Salvadoran Colón"},
  {"syp", "Syrian Pound"}, {"szl", "Swazi Lilangeni"}, {"thb", "Thai Baht"},
  {"tjs", "Tajikistani Somoni"}, {"tmt", "Turkmenistani Manat"}, {"tnd", "Tunisian Dinar"},
  {"top", "Tongan Pa'anga"}, {"try", "Turkish Lira"}, {"ttd", "Trinidad and Tobago Dollar"},
  {"twd", "New Taiwan Dollar"}, {"tzs", "T Tanzanian Shilling"}, {"uah", "Ukrainian Hryvnia"},
  {"ugx", "Ugandan Shilling"}, {"usd", "US Dollar"}, {"uyu", "Uruguayan Peso"},
  {"uzs", "Uzbekistan Som"}, {"ves", "Venezuelan Bolívar"}, {"vnd", "Vietnamese Dong"},
  {"vuv", "Vanuatu Vatu"}, {"wst", "Samoan Tala"}, {"xaf", "CFA Franc BEAC"},
  {"xcd", "East Caribbean Dollar"}, {"xof", "CFA Franc BCEAO"}, {"xpf", "CFP Franc"},
  {"yer", "Yemeni Rial"}, {"zar", "South African Rand"}, {"zmw", "Zambian Kwacha"},
  {"zwl", "Zimbabwean Dollar"}
};

struct AppState {
  Config config;
  WeatherData weather;
  AirQualityData aqi;
  DaylightData daylight;
  MoonData moon;
  PopulationData population;
  CurrencyData currencies[MAX_MULTI_ENTRIES];
  PcStats pc;
  PcMedia media;
  BambuData bambu;
  FlightData flight;
};
#endif