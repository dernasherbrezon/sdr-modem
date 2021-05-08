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
#include <string.h>
#include <math.h>
#include <check.h>
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
                7473.37235249,  428.95458268,   5828.74803892,
                5.1071513,   6.44468284,  -0.18613096},
        {360.0,
                -3305.22249435, 32410.86724220, -24697.17847749,
                -1.30113538, -1.15131518, -0.28333528},
        {720.0,
                14271.28902792, 24110.45647174, -4725.76149170,
                -0.32050445, 2.67984074,  -2.08405289},
        {1080.0,
                -9990.05125819, 22717.38011629, -23616.90130945,
                -1.01667246, -2.29026759, 0.72892364},
        {1440.0,
                9787.88496660,  33753.34020891, -15030.79330940,
                -1.09424947, 0.92358845,  -1.52230928}
};


char tle_str[3][80];
sat_t sat;
FILE *fp = NULL;

START_TEST(test_stipping_spaces2) {
    char tle_with_spaces[3][80] = {"   ", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    sat_t result;
    int code = Get_Next_Tle_Set(tle_with_spaces, &result.tle);
    ck_assert_int_eq(code, 1);
    ck_assert_str_eq("", result.tle.sat_name);
}
END_TEST

START_TEST(test_stipping_spaces) {
    char tle_with_spaces[3][80] = {"01234567890123456789123   ", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    sat_t result;
    int code = Get_Next_Tle_Set(tle_with_spaces, &result.tle);
    ck_assert_int_eq(code, 1);
    ck_assert_str_eq("01234567890123456789123", result.tle.sat_name);
}
END_TEST

START_TEST(test_normal) {

    int i;

    /* read tle file */
    fp = fopen("test-002.tle", "r");
    ck_assert(fp != NULL);
    char *code = fgets(tle_str[0], 80, fp);
    ck_assert(code != NULL);
    code = fgets(tle_str[1], 80, fp);
    ck_assert(code != NULL);
    code = fgets(tle_str[2], 80, fp);
    ck_assert(code != NULL);
    int ret_code = Get_Next_Tle_Set(tle_str, &sat.tle);
    ck_assert_int_eq(ret_code, 1);

    select_ephemeris(&sat);

    for (i = 0; i < TEST_STEPS; i++) {
        SDP4(&sat, expected[i].t);
        Convert_Sat_State(&sat.pos, &sat.vel);

        ck_assert(fabsl(sat.pos.x - expected[i].x) < 0.00001);
        ck_assert(fabsl(sat.pos.y - expected[i].y) < 0.00001);
        ck_assert(fabsl(sat.pos.z - expected[i].z) < 0.00001);
        ck_assert(fabsl(sat.vel.x - expected[i].vx) < 0.00001);
        ck_assert(fabsl(sat.vel.y - expected[i].vy) < 0.00001);
        ck_assert(fabsl(sat.vel.z - expected[i].vz) < 0.00001);
    }

}
END_TEST

void teardown() {
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("sgp4_002");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_stipping_spaces);
    tcase_add_test(tc_core, test_stipping_spaces2);

    tcase_add_checked_fixture(tc_core, setup, teardown);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = common_suite();
    sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}