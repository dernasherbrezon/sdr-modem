/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
  Gpredict: Real-time satellite tracking and orbit prediction program

  Copyright (C)  2001-2008  Alexandru Csete.

  Comments, questions and bugreports should be submitted via
  http://sourceforge.net/projects/gpredict/
  More details can be found at the project home page:

      http://gpredict.oz9aec.net/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the
     Free Software Foundation, Inc.,
   59 Temple Place, Suite 330,
   Boston, MA  02111-1307
   USA
*/
/* Unit test for SGP4 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unity.h>
#include "../src/sgpsdp/sgp4sdp4.h"

#define TEST_STEPS 5

/* structure to hold a set of data */
typedef struct {
  double t;
  double x;
  double y;
  double z;
  double vx;
  double vy;
  double vz;
} dataset_t;

const dataset_t expected[TEST_STEPS] = {
    {0.0,
        2328.97068761, -5995.22085643, 1719.97068075,
        2.91207230, -0.98341546, -7.09081703},
    {360.0,
        2456.10753857, -6071.93865906, 1222.89643564,
        2.67938992, -0.44829041, -7.22879231},
    {720.0,
        2567.56230055, -6112.50386789, 713.96381249,
        2.44024599, 0.09810869,  -7.31995916},
    {1080.0,
        2663.08919967, -6115.48308263, 196.40236060,
        2.19611958, 0.65241995,  -7.36282432},
    {1440.0,
        2742.55314743, -6079.67068185, -326.38672720,
        1.94850229, 1.21106251,  -7.35619372}
};

char tle_str[3][80];
sat_t sat;
FILE *fp = NULL;

void test_time() {
  //Tue Mar 10 11:40:49 GMT 2020
  time_t time = 1583840449;
  struct tm *cdate = gmtime(&time);
  cdate->tm_year += 1900;
  cdate->tm_mon += 1;
  double jul_start_time = Julian_Date(cdate);
  struct tm actual;
  Calendar_Date(jul_start_time, &actual);
  TEST_ASSERT_EQUAL_INT(actual.tm_year, cdate->tm_year);
  TEST_ASSERT_EQUAL_INT(actual.tm_mon, cdate->tm_mon);
  TEST_ASSERT_EQUAL_INT(actual.tm_mday, cdate->tm_mday);

  Time_of_Day(jul_start_time, &actual);
  TEST_ASSERT_EQUAL_INT(actual.tm_hour, cdate->tm_hour);
  TEST_ASSERT_EQUAL_INT(actual.tm_min, cdate->tm_min);
  TEST_ASSERT_EQUAL_INT(actual.tm_sec, cdate->tm_sec);

  struct tm date_time;
  Date_Time(jul_start_time, &date_time);
  TEST_ASSERT_EQUAL_INT(date_time.tm_year, cdate->tm_year);
  TEST_ASSERT_EQUAL_INT(date_time.tm_mon, cdate->tm_mon);
  TEST_ASSERT_EQUAL_INT(date_time.tm_mday, cdate->tm_mday);
  TEST_ASSERT_EQUAL_INT(date_time.tm_hour, cdate->tm_hour);
  TEST_ASSERT_EQUAL_INT(date_time.tm_min, cdate->tm_min);
  TEST_ASSERT_EQUAL_INT(date_time.tm_sec, cdate->tm_sec);

}

void test_math() {
  TEST_ASSERT_EQUAL_INT(-1, Sign(-1.0));
  TEST_ASSERT_EQUAL_INT(1, Sign(2.0));
  TEST_ASSERT_EQUAL_INT(0, Sign(0.0));

  TEST_ASSERT_EQUAL_INT(14.5123 * 10, Degrees(Radians(14.5123)) * 10);
}

void test_solar() {
  double jd = 2458918.986678;
  vector_t actual;
  Calculate_Solar_Position(jd, &actual);
  TEST_ASSERT_EQUAL_INT(actual.x * 1000, 146496240.579853 * 1000);
  TEST_ASSERT_EQUAL_INT(actual.y * 1000, -22805185.677903 * 1000);
  TEST_ASSERT_EQUAL_INT(actual.z * 1000, -9885914.456200 * 1000);
  TEST_ASSERT_EQUAL_INT(actual.w * 1000, 148589893.002415 * 1000);
}

void test_time_epoch() {
  double result = Epoch_Time(2118875388);
  TEST_ASSERT_EQUAL_INT(result * 100, 71245.500000 * 100);
}

void test_eclipse() {
  double jd = 2458918.986678;
  vector_t solar;
  Calculate_Solar_Position(jd, &solar);

  vector_t satellite;
  satellite.x = 2328.970688;
  satellite.y = -5995.220856;
  satellite.z = 1719.970681;
  satellite.w = 6657.708068;

  double depth;
  int result = Sat_Eclipsed(&satellite, &solar, &depth);
  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(-0.780165 * 1000, depth * 1000);
}

void test_calculate_ground_track() {
  vector_t satellite;
  satellite.x = 2328.970688;
  satellite.y = -5995.220856;
  satellite.z = 1719.970681;
  satellite.w = 6657.708068;

  geodetic_t result;
  Calculate_LatLonAlt(1583840449, &satellite, &result);

  TEST_ASSERT_EQUAL_INT(0.262916 * 1000, result.lat * 1000);
  TEST_ASSERT_EQUAL_INT(3.695079 * 1000, result.lon * 1000);
  TEST_ASSERT_EQUAL_INT(281.006635 * 1000, result.alt * 1000);

}

void test_calculate_radec() {
  vector_t satellite_position;
  satellite_position.x = 2328.970688;
  satellite_position.y = -5995.220856;
  satellite_position.z = 1719.970681;
  satellite_position.w = 6657.708068;

  vector_t satellite_velocity;
  satellite_velocity.x = 2.912072;
  satellite_velocity.y = -0.983415;
  satellite_velocity.z = -7.090817;
  satellite_velocity.w = 7.728322;

  geodetic_t ground_station;
  ground_station.lat = Radians(53.72F);
  ground_station.lon = Radians(47.57F);
  ground_station.alt = 0.0;

  obs_astro_t obs;
  Calculate_RADec_and_Obs(1583840449, &satellite_position, &satellite_velocity, &ground_station, &obs);

  TEST_ASSERT_EQUAL_INT(5.185192 * 1000, obs.ra * 1000);
  TEST_ASSERT_EQUAL_INT(-0.323887 * 1000, obs.dec * 1000);

}

void test_normal() {
  int i;

  /* read tle file */
  fp = fopen("test-001.tle", "r");
  TEST_ASSERT(fp != NULL);
  char *code = fgets(tle_str[0], 80, fp);
  TEST_ASSERT(code != NULL);
  code = fgets(tle_str[1], 80, fp);
  TEST_ASSERT(code != NULL);
  code = fgets(tle_str[2], 80, fp);
  TEST_ASSERT(code != NULL);
  int ret_code = Get_Next_Tle_Set(tle_str, &sat.tle);
  TEST_ASSERT_EQUAL_INT(1, ret_code);

  select_ephemeris(&sat);

  for (i = 0; i < TEST_STEPS; i++) {

    SGP4(&sat, expected[i].t);
    Convert_Sat_State(&sat.pos, &sat.vel);

    TEST_ASSERT(fabsl(sat.pos.x - expected[i].x) < 0.00001);
    TEST_ASSERT(fabsl(sat.pos.y - expected[i].y) < 0.00001);
    TEST_ASSERT(fabsl(sat.pos.z - expected[i].z) < 0.00001);
    TEST_ASSERT(fabsl(sat.vel.x - expected[i].vx) < 0.00001);
    TEST_ASSERT(fabsl(sat.vel.y - expected[i].vy) < 0.00001);
    TEST_ASSERT(fabsl(sat.vel.z - expected[i].vz) < 0.00001);

  }
}

void tearDown() {
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_solar);
  RUN_TEST(test_time_epoch);
  RUN_TEST(test_time);
  RUN_TEST(test_normal);
  RUN_TEST(test_eclipse);
  RUN_TEST(test_calculate_ground_track);
  RUN_TEST(test_calculate_radec);
  RUN_TEST(test_math);
  return UNITY_END();
}
