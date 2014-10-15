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

#ifndef TinyGPS_h
#define TinyGPS_h

#include <stdlib.h>
#include <stdbool.h>

#define _GPS_VERSION 13 // software version of this library
#define _GPS_MPH_PER_KNOT 1.15077945
#define _GPS_MPS_PER_KNOT 0.51444444
#define _GPS_KMPH_PER_KNOT 1.852
#define _GPS_MILES_PER_METER 0.00062137112
#define _GPS_KM_PER_METER 0.001
// #define _GPS_NO_STATS

//class TinyGPS
//{
//public:
  enum {
    GPS_INVALID_AGE = 0xFFFFFFFF, GPS_INVALID_ANGLE = 999999999,
    GPS_INVALID_ALTITUDE = 999999999, GPS_INVALID_DATE = 0,
    GPS_INVALID_TIME = 0xFFFFFFFF,                 GPS_INVALID_SPEED = 999999999,
    GPS_INVALID_FIX_TIME = 0xFFFFFFFF, GPS_INVALID_SATELLITES = 0xFF,
    GPS_INVALID_HDOP = 0xFFFFFFFF
  };

  static const float GPS_INVALID_F_ANGLE, GPS_INVALID_F_ALTITUDE, GPS_INVALID_F_SPEED;

  void TinyGPS_init();
  bool TinyGPS_encode(char c); // process one character received from GPS
  //TinyGPS &operator << (char c) {encode(c); return *this;}

  // lat/long in MILLIONTHs of a degree and age of fix in milliseconds
  // (note: versions 12 and earlier gave lat/long in 100,000ths of a degree.
  void TinyGPS_get_position(long *latitude, long *longitude, unsigned long *fix_age); //Used to end: unsigned long *fix_age = 0);

  // date as ddmmyy, time as hhmmsscc, and age in milliseconds
  void TinyGPS_get_datetime(unsigned long *date, unsigned long *time, unsigned long *age); //Used to end: unsigned long *age = 0);

  // signed altitude in centimeters (from GPGGA sentence)
  inline long TinyGPS_altitude();

  // course in last full GPRMC sentence in 100th of a degree
  inline unsigned long TinyGPS_course();

  // speed in last full GPRMC sentence in 100ths of a knot
  inline unsigned long TinyGPS_speed();

  // satellites used in last full GPGGA sentence
  inline unsigned short TinyGPS_satellites();

  // horizontal dilution of precision in 100ths
  inline unsigned long TinyGPS_hdop();

  int TinyGPS_hasFix();

  void TinyGPS_f_get_position(float *latitude, float *longitude, unsigned long *fix_age); //Used to end: unsigned long *fix_age = 0);
  void TinyGPS_crack_datetime(int *year, unsigned char *month, unsigned char *day, //the unsigned chars were bytes before
    unsigned char *hour, unsigned char *minute, unsigned char *second, unsigned char *hundredths, unsigned long *fix_age); //Used to end: byte *hundredths = 0, unsigned long *fix_age = 0);
  float f_altitude();
  float f_course();
  float f_speed_knots();
  float f_speed_mph();
  float f_speed_mps();
  float f_speed_kmph();

  int TinyGPS_library_version();

  float TinyGPS_distance_between (long lat1, long long1, long lat2, long long2);
    // expects lat/long params in MILLIONTHs of decimal degree
  float TinyGPS_course_to (float lat1, float long1, float lat2, float long2);
  const char *TinyGPS_cardinal(float course);

  int TinyGPS_feedGps();

#ifndef _GPS_NO_STATS
  void TinyGPS_stats(unsigned long *chars, unsigned short *good_sentences, unsigned short *failed_cs);
#endif

#endif
