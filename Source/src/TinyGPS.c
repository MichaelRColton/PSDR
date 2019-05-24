/*
TinyGPS - a small GPS library for Arduino providing basic NMEA parsing
Based on work by and "distance_to" and "course_to" courtesy of Maarten Lamers.
Suggestion to add satellites(), course_to(), and cardinal(), by Matt Monson.
Precision improvements suggested by Wayne Holder.
Copyright (C) 2008-2013 Mikal Hart
All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "TinyGPS.h"
#include <math.h>
//#include <trig.h>
//#include <hal.h>

#define _GPRMC_TERM "GPRMC"
#define _GPGGA_TERM "GPGGA"

#define TWO_PI 	6.28318530717958647693 //These numbers are included in Processing, since we aren't using Processing we need to include them here.
//#define PI 		3.14159265358979323846

#define GPS_HYBERNATE_COMMAND "$PMTK161,0*28\x0D\x0A"

enum {_GPS_SENTENCE_GPGGA, _GPS_SENTENCE_GPRMC, _GPS_SENTENCE_OTHER};

  // properties
  unsigned long _time, _new_time;
  unsigned long _date, _new_date;
  long _latitude, _new_latitude;
  long _longitude, _new_longitude;
  long _altitude, _new_altitude;
  unsigned long _speed, _new_speed;
  unsigned long _course, _new_course;
  unsigned long _hdop, _new_hdop;
  unsigned short _numsats, _new_numsats;
  unsigned long timeOfLastChar = 0;

  unsigned long _last_time_fix, _new_time_fix;
  unsigned long _last_position_fix, _new_position_fix;

  // parsing state variables
  unsigned char _parity; //was byte
  bool _is_checksum_term;
  char _term[15];
  unsigned char _sentence_type; //was byte
  unsigned char _term_number; //was byte
  unsigned char _term_offset; //was byte
  bool _gps_data_good;

#ifndef _GPS_NO_STATS
  // statistics
  unsigned long _encoded_characters;
  unsigned short _good_sentences;
  unsigned short _failed_checksum;
  unsigned short _passed_checksum;
#endif

  // internal utilities
  int TinyGPS_from_hex(char a);
  unsigned long TinyGPS_parse_decimal();
  unsigned long TinyGPS_parse_degrees();
  bool TinyGPS_term_complete();
  bool TinyGPS_gpsisdigit(char c); //{ return c >= '0' && c <= '9'; }
  long TinyGPS_gpsatol(const char *str);
  int TinyGPS_gpsstrcmp(const char *str1, const char *str2);


void TinyGPS_init()
{
	_time = GPS_INVALID_TIME;
	_date = GPS_INVALID_DATE;
	_latitude = GPS_INVALID_ANGLE;
	_longitude = GPS_INVALID_ANGLE;
	_altitude = GPS_INVALID_ALTITUDE;
	_speed = GPS_INVALID_SPEED;
	_course = GPS_INVALID_ANGLE;
	_hdop = GPS_INVALID_HDOP;
	_numsats = GPS_INVALID_SATELLITES;
	_last_time_fix = GPS_INVALID_FIX_TIME;
	_last_position_fix = GPS_INVALID_FIX_TIME;
	_parity = 0;
	_is_checksum_term = false;
	_sentence_type = _GPS_SENTENCE_OTHER;
	_term_number = 0;
	_term_offset = 0;
	_gps_data_good = false;
#ifndef _GPS_NO_STATS
	_encoded_characters = 0;
	_good_sentences = 0;
	_failed_checksum = 0;
#endif
	{
		_term[0] = '\0';
	}

	//Initial Uart setup
	//uart_init(hal_gpsUart);

#ifdef SIRF_MODULE
	//TODO: Determine which of these delays can be eliminated or reduced
	hal_delay_ms(1000);


	//Put module into binary mode to accept TricklePower command
	//Putting module into binary at 4800 DOES NOT WORK (it sets itself to 9600)
	//uart_write(hal_gpsUart, "$PSRF100,0,57600,8,1,0*37\r\n", 27); //Does put module into binary mode, baud rate is correct.
	uart_write(hal_gpsUart, "$PSRF100,0,9600,8,1,0*0C\r\n", 26); //Does put module into binary mode, baud rate is correct.
	hal_delay_ms(1000);

	//Now we're working at 9600
	hal_setupUart3(9600);

	//Enable tricklepower mode (mid 151)
	uart_write(hal_gpsUart, "\xA0\xA2\x00\x09\x97\x00\x00\x00\xC8\x00\x00\x01\x90\x01\xF0\xB0\xB3",17); //No additional CR or LF needed.

	hal_delay_ms(1000);

	//Set Low Power acquisition parameters (mid 167)
	uart_write(hal_gpsUart, "\xA0\xA2\x00\x0F\xA7\x00\x00\x75\x30\x00\x04\x93\xE0\x00\x00\x07\x08\x00\x01\x02\xD3\xB0\xB3", 23);

	hal_delay_ms(1000);

	//revert to NMEA mode (Puts it into 9600 so I don't have to rewrite the command, but I will if I need to)
	uart_write(hal_gpsUart, "\xA0\xA2\x00\x18\x81\x02\x01\x01\x00\x01\x00\x01\x00\x01\x01\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x25\x80\x01\x34\xB0\xB3", 32);
	hal_delay_ms(1000);
#endif
#ifdef MEDIATEK_MODULE
	//Send it something to wake it up
	//uart_write(hal_gpsUart, "\r\n", 2);
#endif
}



//Power on GPS (this probably shouldn't be in TinyGPS
//void TinyGPS_powerOnGps()
//{
//	//I think any character will wake it up, so... why not send a few carriage returns?
//	uart_write(hal_gpsUart, "\r\r\r", 3);
//}

//Hybernate GPS (also shouldn't be in TinyGPS
//void TinyGPS_hybernateGps()
//{
//	uart_write(hal_gpsUart, (unsigned char*) GPS_HYBERNATE_COMMAND, sizeof GPS_HYBERNATE_COMMAND - 1);
//}

//GPS power status. Since the only way we know (right?) is by watching for characters,
//and they only come once per second, it's possible this will be wrong for up to a second, you know?
//Also shouldn't be in TinyGPS
int TinyGPS_gpsIsOn()
{
	return hal_checkTimeout(timeOfLastChar, 1100) ? 0 : 1;
}

int TinyGPS_feedGps() //probably not the right place to put this. Maybe in the GPS.c?
{
	char c;
	while(uart_getChar(hal_gpsUart, &c) != -1)
	{
		timeOfLastChar = hal_getCurrentTime_ms();
		//return TinyGPS_encode(c);
		// could return here if returns true, but probably not for each character
		TinyGPS_encode(c);
	}
	return false;
}

int TinyGPS_hasFix() //The right way to do this would be to parse the NMEA sentences to get this information, but I am in a hurry.
{
	bool returnVal = (_last_position_fix != GPS_INVALID_FIX_TIME && !hal_checkTimeout(_last_position_fix, 5000)) ? 1 : 0;
	return returnVal; //So I can see what's happening.
}

#define COMBINE(sentence_type, term_number) (((unsigned)(sentence_type) << 5) | term_number)

//
// internal utilities
//
int TinyGPS_from_hex(char a)
{
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else
    return a - '0';
}

long TinyGPS_gpsatol(const char *str)
{
  long ret = 0;
  while (TinyGPS_gpsisdigit(*str))
    ret = 10 * ret + *str++ - '0';
  return ret;
}

int TinyGPS_gpsstrcmp(const char *str1, const char *str2)
{
  while (*str1 && *str1 == *str2)
    ++str1, ++str2;
  return *str1;
}

unsigned long TinyGPS_parse_decimal()
{
  char *p = _term;
  bool isneg = *p == '-';
  if (isneg) ++p;
  unsigned long ret = 100UL * TinyGPS_gpsatol(p);
  while (TinyGPS_gpsisdigit(*p)) ++p;
  if (*p == '.')
  {
    if (TinyGPS_gpsisdigit(p[1]))
    {
      ret += 10 * (p[1] - '0');
      if (TinyGPS_gpsisdigit(p[2]))
        ret += p[2] - '0';
    }
  }
  return isneg ? -ret : ret;
}

bool TinyGPS_gpsisdigit(char c)
{
	return c >= '0' && c <= '9';
}

// Parse a string in the form ddmm.mmmmmmm...
unsigned long TinyGPS_parse_degrees()
{
  char *p;
  unsigned long left_of_decimal = TinyGPS_gpsatol(_term);
  unsigned long hundred1000ths_of_minute = (left_of_decimal % 100UL) * 100000UL;
  for (p=_term; TinyGPS_gpsisdigit(*p); ++p);
  if (*p == '.')
  {
    unsigned long mult = 10000;
    while (TinyGPS_gpsisdigit(*++p))
    {
      hundred1000ths_of_minute += mult * (*p - '0');
      mult /= 10;
    }
  }
  return (left_of_decimal / 100) * 1000000 + (hundred1000ths_of_minute + 3) / 6;
}

// Processes a just-completed term
// Returns true if new sentence has just passed checksum test and is validated
bool TinyGPS_term_complete()
{
  if (_is_checksum_term)
  {
    unsigned char checksum = 16 * TinyGPS_from_hex(_term[0]) + TinyGPS_from_hex(_term[1]); //Was byte
    if (checksum == _parity)
    {
      if (_gps_data_good)
      {
#ifndef _GPS_NO_STATS
        ++_good_sentences;
#endif
        _last_time_fix = _new_time_fix;
        _last_position_fix = _new_position_fix;

        switch(_sentence_type)
        {
        case _GPS_SENTENCE_GPRMC:
          _time = _new_time;
          _date = _new_date;
          _latitude = _new_latitude;
          _longitude = _new_longitude;
          _speed = _new_speed;
          _course = _new_course;
          break;
        case _GPS_SENTENCE_GPGGA:
          _altitude = _new_altitude;
          _time = _new_time;
          _latitude = _new_latitude;
          _longitude = _new_longitude;
          _numsats = _new_numsats;
          _hdop = _new_hdop;
          break;
        }

        return true;
      }
    }

#ifndef _GPS_NO_STATS
    else
      ++_failed_checksum;
#endif
    return false;
  }

  // the first term determines the sentence type
  if (_term_number == 0)
  {
    if (!TinyGPS_gpsstrcmp(_term, _GPRMC_TERM))
      _sentence_type = _GPS_SENTENCE_GPRMC;
    else if (!TinyGPS_gpsstrcmp(_term, _GPGGA_TERM))
      _sentence_type = _GPS_SENTENCE_GPGGA;
    else
      _sentence_type = _GPS_SENTENCE_OTHER;
    return false;
  }

  if (_sentence_type != _GPS_SENTENCE_OTHER && _term[0])
    switch(COMBINE(_sentence_type, _term_number))
  {
    case COMBINE(_GPS_SENTENCE_GPRMC, 1): // Time in both sentences
    case COMBINE(_GPS_SENTENCE_GPGGA, 1):
      _new_time = TinyGPS_parse_decimal();
      _new_time_fix = hal_getCurrentTime_ms(); //millis(); //TODO: put a callback so we can remove the hal.h dependency.
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 2): // GPRMC validity
      _gps_data_good = _term[0] == 'A';
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 3): // Latitude
    case COMBINE(_GPS_SENTENCE_GPGGA, 2):
      _new_latitude = TinyGPS_parse_degrees();
      _new_position_fix = hal_getCurrentTime_ms(); //millis();
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 4): // N/S
    case COMBINE(_GPS_SENTENCE_GPGGA, 3):
      if (_term[0] == 'S')
        _new_latitude = -_new_latitude;
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 5): // Longitude
    case COMBINE(_GPS_SENTENCE_GPGGA, 4):
      _new_longitude = TinyGPS_parse_degrees();
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 6): // E/W
    case COMBINE(_GPS_SENTENCE_GPGGA, 5):
      if (_term[0] == 'W')
        _new_longitude = -_new_longitude;
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 7): // Speed (GPRMC)
      _new_speed = TinyGPS_parse_decimal();
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 8): // Course (GPRMC)
      _new_course = TinyGPS_parse_decimal();
      break;
    case COMBINE(_GPS_SENTENCE_GPRMC, 9): // Date (GPRMC)
      _new_date = TinyGPS_gpsatol(_term);
      break;
    case COMBINE(_GPS_SENTENCE_GPGGA, 6): // Fix data (GPGGA)
      _gps_data_good = _term[0] > '0';
      break;
    case COMBINE(_GPS_SENTENCE_GPGGA, 7): // Satellites used (GPGGA)
      _new_numsats = (unsigned char)atoi(_term);
      break;
    case COMBINE(_GPS_SENTENCE_GPGGA, 8): // HDOP
      _new_hdop = TinyGPS_parse_decimal();
      break;
    case COMBINE(_GPS_SENTENCE_GPGGA, 9): // Altitude (GPGGA)
      _new_altitude = TinyGPS_parse_decimal();
      break;
  }

  return false;
}



bool TinyGPS_encode(char c)
{
  bool valid_sentence = false;

#ifndef _GPS_NO_STATS
  ++_encoded_characters;
#endif
  switch(c)
  {
  case ',': // term terminators
    _parity ^= c;
  case '\r':
  case '\n':
  case '*':
    if (_term_offset < sizeof(_term))
    {
      _term[_term_offset] = 0;
      valid_sentence = TinyGPS_term_complete();
    }
    ++_term_number;
    _term_offset = 0;
    _is_checksum_term = c == '*';
    return valid_sentence;

  case '$': // sentence begin
    _term_number = _term_offset = 0;
    _parity = 0;
    _sentence_type = _GPS_SENTENCE_OTHER;
    _is_checksum_term = false;
    _gps_data_good = false;
    return valid_sentence;
  }

  // ordinary characters
  if (_term_offset < sizeof(_term) - 1)
    _term[_term_offset++] = c;
  if (!_is_checksum_term)
    _parity ^= c;

  return valid_sentence;
}

#ifndef _GPS_NO_STATS
void TinyGPS_stats(unsigned long *chars, unsigned short *sentences, unsigned short *failed_cs)
{
  if (chars) *chars = _encoded_characters;
  if (sentences) *sentences = _good_sentences;
  if (failed_cs) *failed_cs = _failed_checksum;
}
#endif











/* static */
float TinyGPS_distance_between (long lat1_, long long1_, long lat2_, long long2_)
{
  // returns distance in meters between two positions, both specified
  // as signed decimal-degrees latitude and longitude. Uses great-circle
  // distance computation for hypothetical sphere of radius 6372795 meters.
  // Because Earth is no exact sphere, rounding errors may be up to 0.5%.
  // Courtesy of Maarten Lamers
  //float delta = radians(long1-long2); //I'm not including a math library just for this!
  float lat1  = lat1_  / 1000000.0;
  float long1 = long1_ / 1000000.0;
  float lat2  = lat2_  / 1000000.0;
  float long2 = long2_ / 1000000.0;
  float delta = (long1-long2) * PI/180.0;
  float sdlong = sin(delta);
  float cdlong = cos(delta);
  //lat1 = radians(lat1);
  //lat2 = radians(lat2);
  lat1 = (lat1) * PI/180.0;
  lat2 = (lat2) * PI/180.0;
  float slat1 = sin(lat1);
  float clat1 = cos(lat1);
  float slat2 = sin(lat2);
  float clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  //delta = sq(delta); //I'm not including a math library just for this!
  delta = delta * delta;
  //delta += sq(clat2 * sdlong);
  delta += (clat2 * sdlong) * (clat2 * sdlong);
  delta = sqrt(delta);
  float denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  return delta * 6372795;
}

float TinyGPS_course_to (float lat1, float long1, float lat2, float long2)
{
  // returns course in degrees (North=0, West=270) from position 1 to position 2,
  // both specified as signed decimal-degrees latitude and longitude.
  // Because Earth is no exact sphere, calculated course may be off by a tiny fraction.
  // Courtesy of Maarten Lamers
  //float dlon = radians(long2-long1);
  float dlon = (long2-long1) * PI/180.0;
  //lat1 = radians(lat1);
  //lat2 = radians(lat2);
  lat1 = (lat1) * PI/180.0;
  lat2 = (lat2) * PI/180.0;
  float a1 = sin(dlon) * cos(lat2);
  float a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  if (a2 < 0.0)
  {
    a2 += TWO_PI;
  }
  //return degrees(a2);
  return (a2) * 180.0 / PI;
}

const char *TinyGPS_cardinal (float course)
{
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}

// lat/long in MILLIONTHs of a degree and age of fix in milliseconds
// (note: versions 12 and earlier gave this value in 100,000ths of a degree.
void TinyGPS_get_position(long *latitude, long *longitude, unsigned long *fix_age)
{
	//*fix_age = 0; //Do I want this?
  if (latitude) *latitude = _latitude;
  if (longitude) *longitude = _longitude;
  if (fix_age) *fix_age = _last_position_fix == GPS_INVALID_FIX_TIME ?
   GPS_INVALID_AGE : /*millis()*/ hal_getCurrentTime_ms() - _last_position_fix;
}

// date as ddmmyy, time as hhmmsscc, and age in milliseconds
void TinyGPS_get_datetime(unsigned long *date, unsigned long *time, unsigned long *age)
{
	//*age = 0; //Do I want this?
  if (date) *date = _date;
  if (time) *time = _time;
  if (age) *age = _last_time_fix == GPS_INVALID_FIX_TIME ?
   GPS_INVALID_AGE : /*millis()*/ hal_getCurrentTime_ms() - _last_time_fix;
}

void TinyGPS_f_get_position(float *latitude, float *longitude, unsigned long *fix_age)
{
	//*fix_age = 0; //Do I want this?
  long lat, lon;
  TinyGPS_get_position(&lat, &lon, fix_age);
  *latitude = lat == GPS_INVALID_ANGLE ? GPS_INVALID_F_ANGLE : (lat / 1000000.0);
  *longitude = lat == GPS_INVALID_ANGLE ? GPS_INVALID_F_ANGLE : (lon / 1000000.0);
}

void TinyGPS_crack_datetime(int *year, unsigned char *month, unsigned char *day,
		unsigned char *hour, unsigned char *minute, unsigned char *second, unsigned char *hundredths, unsigned long *age)
{
	//*hundredths = 0; //Do I want this?
	//*age = 0; //And this?
  unsigned long date, time;
  TinyGPS_get_datetime(&date, &time, age);
  if (year)
  {
    *year = date % 100;
    *year += *year > 80 ? 1900 : 2000;
  }
  if (month) *month = (date / 100) % 100;
  if (day) *day = date / 10000;
  if (hour) *hour = time / 1000000;
  if (minute) *minute = (time / 10000) % 100;
  if (second) *second = (time / 100) % 100;
  if (hundredths) *hundredths = time % 100;
}

float TinyGPS_f_altitude()
{
  return _altitude == GPS_INVALID_ALTITUDE ? GPS_INVALID_F_ALTITUDE : _altitude / 100.0;
}

float TinyGPS_f_course()
{
  return _course == GPS_INVALID_ANGLE ? GPS_INVALID_F_ANGLE : _course / 100.0;
}

float TinyGPS_f_speed_knots()
{
  return _speed == GPS_INVALID_SPEED ? GPS_INVALID_F_SPEED : _speed / 100.0;
}

float TinyGPS_f_speed_mph()
{
  float sk = TinyGPS_f_speed_knots();
  return sk == GPS_INVALID_F_SPEED ? GPS_INVALID_F_SPEED : _GPS_MPH_PER_KNOT * sk;
}

float TinyGPS_f_speed_mps()
{
  float sk = TinyGPS_f_speed_knots();
  return sk == GPS_INVALID_F_SPEED ? GPS_INVALID_F_SPEED : _GPS_MPS_PER_KNOT * sk;
}

float TinyGPS_f_speed_kmph()
{
  float sk = TinyGPS_f_speed_knots();
  return sk == GPS_INVALID_F_SPEED ? GPS_INVALID_F_SPEED : _GPS_KMPH_PER_KNOT * sk;
}

const float TinyGPS_GPS_INVALID_F_ANGLE = 1000.0;
const float TinyGPS_GPS_INVALID_F_ALTITUDE = 1000000.0;
const float TinyGPS_GPS_INVALID_F_SPEED = -1.0;

// signed altitude in centimeters (from GPGGA sentence)
inline long TinyGPS_altitude() { return _altitude; }

// course in last full GPRMC sentence in 100th of a degree
inline unsigned long TinyGPS_course() { return _course; }

// speed in last full GPRMC sentence in 100ths of a knot
inline unsigned long TinyGPS_speed() { return _speed; }

// satellites used in last full GPGGA sentence
inline unsigned short TinyGPS_satellites() { return _numsats; }

// horizontal dilution of precision in 100ths
inline unsigned long TinyGPS_hdop() { return _hdop; }

int TinyGPS_library_version() { return _GPS_VERSION; }
