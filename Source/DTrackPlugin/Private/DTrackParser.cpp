/* DTrackParser: C++ source file, A.R.T. GmbH
 *
 * DTrackSDK: functions to process DTrack UDP packets (ASCII protocol).
 *
 * Copyright (c) 2013-2020, Advanced Realtime Tracking GmbH
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Version v2.6.0
 *
 * Purpose:
 *  - DTrack network protocol according to:
 *    'DTrack2 User Manual, Technical Appendix' or 'DTrack3 Programmer's Guide'
 */

#include "DTrackParser.hpp"
#include "DTrackParse.hpp"

#include <cstring>

using namespace DTrackSDK_Parse;

// use Visual Studio specific secure methods to avoid warnings
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
	#define strcpy strcpy_s
	#define strcat strcat_s
#endif


/** Converts a stacked reduced (non-redundant) representation of a covariance matrix to a stacked full representation */
static void reduced_to_full_cov( double* cov_full, const double* cov_reduced, int dim )
{
	for (int r=0; r<dim; ++r) {
		int k = r*(r-1)/2;
		cov_full[ r*(dim+1) ] = cov_reduced[ r*dim - k ];
		for (int c=r+1; c<dim; ++c) {
			cov_full[ r*dim + c ]  =  cov_full[ c*dim + r ]  =  cov_reduced[ r*(dim-1) - k + c ];
		}
	}
}


/*
 * Constructor.
 */
DTrackParser::DTrackParser()
{
	// reset actual DTrack data:
	act_framecounter = 0;
	act_timestamp = -1;
	
	act_num_body = act_num_flystick = act_num_meatool = act_num_mearef = act_num_hand = act_num_human = 0;
	act_num_inertial = 0;
	act_num_marker = 0;
}


/*
 * Destructor.
 */
DTrackParser::~DTrackParser()
{
	//
}


/*
 * Set default values at start of a new frame.
 */
void DTrackParser::startFrame()
{
	act_framecounter = 0;
	act_timestamp = -1;   // i.e. not available
	loc_num_bodycal = loc_num_handcal = -1;  // i.e. not available
	loc_num_flystick1 = loc_num_meatool1 = 0;
}


/*
 * Final adjustments after processing all data for a frame.
 */
void DTrackParser::endFrame()
{
	int j, n;
	
	// set number of calibrated standard bodies, if necessary:
	if (loc_num_bodycal >= 0) {	// '6dcal' information was available
		n = loc_num_bodycal - loc_num_flystick1 - loc_num_meatool1;
		if (n > act_num_body) {  // adjust length of vector
			act_body.resize(n);
			for (j=act_num_body; j<n; j++) {
				memset(&act_body[j], 0, sizeof(DTrack_Body_Type_d));
				act_body[j].id = j;
				act_body[j].quality = -1;
			}
		}
		act_num_body = n;
	}
	
	// set number of calibrated Fingertracking hands, if necessary:
	if (loc_num_handcal >= 0) {  // 'glcal' information was available
		if (loc_num_handcal > act_num_hand) {  // adjust length of vector
			act_hand.resize(loc_num_handcal);
			for (j=act_num_hand; j<loc_num_handcal; j++) {
				memset(&act_hand[j], 0, sizeof(DTrack_Hand_Type_d));
				act_hand[j].id = j;
				act_hand[j].quality = -1;
			}
		}
		act_num_hand = loc_num_handcal;
	}
}


/*
 * Parses a single line of data in one tracking data packet.
 */
bool DTrackParser::parseLine(char **line)
{
	if (!line)
		return false;
	
	// line of frame counter:
	if (!strncmp(*line, "fr ", 3)) {
		*line += 3;
		return parseLine_fr(line);
	}
	
	// line of timestamp:
	if (!strncmp(*line, "ts ", 3)) {
		*line += 3;
		return parseLine_ts(line);
	}
	
	// line of additional inofmation about number of calibrated bodies:
	if (!strncmp(*line, "6dcal ", 6)) {
		*line += 6;
		return parseLine_6dcal(line);
	}
	
	// line of standard body data:
	if (!strncmp(*line, "6d ", 3)) {
		*line += 3;
		return parseLine_6d(line);
	}

	// line of 6d covariance data:
	if (!strncmp(*line, "6dcov ", 6)) {
		*line += 6;
		return parseLine_6dcov(line);
	}
	
	// line of Flystick data (older format):
	if (!strncmp(*line, "6df ", 4)) {
		*line += 4;
		return parseLine_6df(line);
	}
	
	// line of Flystick data (newer format):
	if (!strncmp(*line, "6df2 ", 5)) {
		*line += 5;
		return parseLine_6df2(line);
	}
	
	// line of measurement tool data (older format):
	if (!strncmp(*line, "6dmt ", 5)) {
		*line += 5;
		return parseLine_6dmt(line);
	}
	
	// line of measurement tool data (newer format):
	if (!strncmp(*line, "6dmt2 ", 6)) {
		*line += 6;
		return parseLine_6dmt2(line);
	}
	
	// line of measurement reference data:
	if (!strncmp(*line, "6dmtr ", 6)) {
		*line += 6;
		return parseLine_6dmtr(line);
	}
	
	// line of additional inofmation about number of calibrated Fingertracking hands:
	if (!strncmp(*line, "glcal ", 6)) {
		*line += 6;
		return parseLine_glcal(line);
	}
	
	// line of A.R.T. Fingertracking hand data:
	if (!strncmp(*line, "gl ", 3)) {
		*line += 3;
		return parseLine_gl(line);
	}
	
	// line of 6dj human model data:
	if (!strncmp(*line, "6dj ", 4))	{
		*line += 4;
		return parseLine_6dj(line);
	}
	
	// line of 6di inertial data:
	if (!strncmp(*line, "6di ", 4))	{
		*line += 4;
		return parseLine_6di(line);
	}
	
	// line of single marker data:
	if (!strncmp(*line, "3d ", 3)) {
		*line += 3;
		return parseLine_3d(line);
	}
	
	return true;  // ignore unknown line identifiers (could be valid in future DTracks)
}


/*
 * Parses a single line of frame counter data in one tracking data packet.
 */
bool DTrackParser::parseLine_fr(char **line)
{
	if ( (*line = string_get_ui(*line, &act_framecounter)) == 0 ) {
		act_framecounter = 0;
		return false;
	}
	
	return true;
}


/*
 * Parses a single line of timestamp data in one tracking data packet.
 */
bool DTrackParser::parseLine_ts(char **line)
{
	if ( (*line = string_get_d(*line, &act_timestamp)) == 0 )	{
		act_timestamp = -1;
		return false;
	}
	
	return true;
}


/*
 * Parses a single line of additional information about number of calibrated bodies in one tracking data packet.
 */
bool DTrackParser::parseLine_6dcal(char **line)
{
	if ( (*line = string_get_i(*line, &loc_num_bodycal)) == 0 ) {
		return false;
	}
	
	return true;
}


/*
 * Parses a single line of standard body data in one tracking data packet.
 */
bool DTrackParser::parseLine_6d(char **line)
{
	int i, j, n, id;
	double d;
	
	// disable all existing data
	for (i=0; i<act_num_body; i++) {
		memset(&act_body[i], 0, sizeof(DTrack_Body_Type_d));
		act_body[i].id = i;
		act_body[i].quality = -1;
	}
	// get number of standard bodies (in line)
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// get data of standard bodies
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "id", &id, NULL, &d)) == 0 ) {
			return false;
		}
		// adjust length of vector
		if (id >= act_num_body) {
			act_body.resize(id + 1);
			for (j = act_num_body; j<=id; j++) {
				memset(&act_body[j], 0, sizeof(DTrack_Body_Type_d));
				act_body[j].id = j;
				act_body[j].quality = -1;
			}
			act_num_body = id + 1;
		}
		act_body[id].id = id;
		act_body[id].quality = d;
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_body[id].loc)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_body[id].rot)) == 0 ) {
			return false;
		}
	}
	return true;
}


/*
 * Parses a single line of 6d covariance data in one tracking data packet.
 */
bool DTrackParser::parseLine_6dcov(char **line)
{
	int i, n, id;
	double covref[3];
	double cov_reduced[21];

	// get number of standard bodies (in line)
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}

	// get covariance data
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "iddd", &id, NULL, covref)) == 0 ) {
			return false;
		}
        for (int r=0; r<3; ++r) {
            act_body[id].covref[r]  = covref[r];
        }
		if ( (*line = string_get_block(*line, "ddddddddddddddddddddd", NULL, NULL, cov_reduced)) == 0 ) {
			return false;
		}
		reduced_to_full_cov( act_body[id].cov, cov_reduced, 6 );
	}
	return true;
}


/*
 * Parses a single line of Flystick data (older format) data in one tracking data packet.
 */
bool DTrackParser::parseLine_6df(char **line)
{
	int i, j, k, n, iarr[2];
	double d;
	
	// get number of calibrated Flysticks
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	loc_num_flystick1 = n;
	// adjust length of vector
	if (n != act_num_flystick) {
		act_flystick.resize(n);
		act_num_flystick = n;
	}
	// get data of Flysticks
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "idi", iarr, NULL, &d)) == 0 ) {
			return false;
		}
		if (iarr[0] != i) {	// not expected
			return false;
		}
		act_flystick[i].id = iarr[0];
		act_flystick[i].quality = d;
		act_flystick[i].num_button = 8;
		k = iarr[1];
		for (j=0; j<8; j++) {
			act_flystick[i].button[j] = k & 0x01;
			k >>= 1;
		}
		act_flystick[i].num_joystick = 2;  // additionally to buttons 5-8
		if (iarr[1] & 0x20) {
			act_flystick[i].joystick[0] = -1;
		} else
			if (iarr[1] & 0x80) {
				act_flystick[i].joystick[0] = 1;
			} else {
				act_flystick[i].joystick[0] = 0;
			}
		if(iarr[1] & 0x10){
			act_flystick[i].joystick[1] = -1;
		}else if(iarr[1] & 0x40){
			act_flystick[i].joystick[1] = 1;
		}else{
			act_flystick[i].joystick[1] = 0;
		}
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_flystick[i].loc)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_flystick[i].rot)) == 0 ) {
			return false;
		}
	}
	
	return true;
}


/*
 * Parses a single line of Flystick data (newer format) data in one tracking data packet.
 */
bool DTrackParser::parseLine_6df2(char **line)
{
	int i, j, k, l, n, iarr[3];
	double d;
	char sfmt[20];
	
	// get number of calibrated Flysticks
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// adjust length of vector
	if (n != act_num_flystick) {
		act_flystick.resize(n);
		act_num_flystick = n;
	}
	// get number of Flysticks
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// get data of Flysticks
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "idii", iarr, NULL, &d)) == 0 ) {
			return false;
		}
		if (iarr[0] != i) {  // not expected
			return false;
		}
		act_flystick[i].id = iarr[0];
		act_flystick[i].quality = d;
		if ( ( iarr[ 1 ] > DTRACKSDK_FLYSTICK_MAX_BUTTON ) ||( iarr[ 2 ] > DTRACKSDK_FLYSTICK_MAX_JOYSTICK ) )
		{
			return false;
		}
		act_flystick[i].num_button = iarr[1];
		act_flystick[i].num_joystick = iarr[2];
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_flystick[i].loc)) == 0 ){
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_flystick[i].rot)) == 0 ){
			return false;
		}
		strcpy(sfmt, "");
		j = 0;
		while (j < act_flystick[i].num_button) {
			strcat(sfmt, "i");
			j += 32;
		}
		j = 0;
		while (j < act_flystick[i].num_joystick) {
			strcat(sfmt, "d");
			j++;
		}
		if ( (*line = string_get_block(*line, sfmt, iarr, NULL, act_flystick[i].joystick)) == 0 ) {
			return false;
		}
		k = l = 0;
		for (j=0; j<act_flystick[i].num_button; j++) {
			act_flystick[i].button[j] = iarr[k] & 0x01;
			iarr[k] >>= 1;
			l++;
			if (l == 32) {
				k++;
				l = 0;
			}
		}
	}
	
	return true;
}


/*
 * Parses a single line of Measurement Tool data (older format) in one tracking data packet.
 */
bool DTrackParser::parseLine_6dmt(char **line)
{
	int i, j, k, n, iarr[3];
	double d;
	
	// get number of calibrated measurement tools
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	loc_num_meatool1 = n;
	// adjust length of vector
	if (n != act_num_meatool) {
		act_meatool.resize(n);
		act_num_meatool = n;
	}
	// get data of measurement tools
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "idi", iarr, NULL, &d)) == 0 ) {
			return false;
		}
		if (iarr[0] != i) {  // not expected
			return false;
		}
		act_meatool[i].id = iarr[0];
		act_meatool[i].quality = d;
		
		act_meatool[i].num_button = 4;
		
		k = iarr[1];
		for (j=0; j<act_meatool[i].num_button; j++)	{
			act_meatool[i].button[j] = k & 0x01;
			k >>= 1;
		}
		for (j=act_meatool[i].num_button; j<DTRACKSDK_MEATOOL_MAX_BUTTON; j++) {
			act_meatool[i].button[j] = 0;
		}
		
		act_meatool[i].tipradius = 0.0;
		
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_meatool[i].loc)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_meatool[i].rot)) == 0 ) {
			return false;
		}
		
		for (j=0; j<9; j++)
			act_meatool[i].cov[j] = 0.0;
	}
	
	return true;
}


/*
 * Parses a single line of Measurement Tool data (newer format) data in one tracking data packet.
 */
bool DTrackParser::parseLine_6dmt2(char **line)
{
	int i, j, k, l, n, iarr[2];
	double darr[2];
	char sfmt[20];
	double cov_reduced[6];
	
	// get number of calibrated measurement tools
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// adjust length of vector
	if (n != act_num_meatool) {
		act_meatool.resize(n);
		act_num_meatool = n;
	}
	// get data of measurement tools
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "idid", iarr, NULL, darr)) == 0 ) {
			return false;
		}
		if (iarr[0] != i) {  // not expected
			return false;
		}
		act_meatool[i].id = iarr[0];
		act_meatool[i].quality = darr[0];
		
		act_meatool[i].num_button = iarr[1];
		if (act_meatool[i].num_button > DTRACKSDK_MEATOOL_MAX_BUTTON)
			act_meatool[i].num_button = DTRACKSDK_MEATOOL_MAX_BUTTON;
		
		for (j=act_meatool[i].num_button; j<DTRACKSDK_MEATOOL_MAX_BUTTON; j++) {
			act_meatool[i].button[j] = 0;
		}
		
		act_meatool[i].tipradius = darr[1];
		
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_meatool[i].loc)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_meatool[i].rot)) == 0 ) {
			return false;
		}
		
		strcpy(sfmt, "");
		j = 0;
		while (j < act_meatool[i].num_button) {
			strcat(sfmt, "i");
			j += 32;
		}
		
		if ( (*line = string_get_block(*line, sfmt, iarr, NULL, NULL)) == 0 ) {
			return false;
		}
		k = l = 0;
		for (j=0; j<act_meatool[i].num_button; j++) {
			act_meatool[i].button[j] = iarr[k] & 0x01;
			iarr[k] >>= 1;
			l++;
			if (l == 32) {
				k++;
				l = 0;
			}
		}
		
		if ( (*line = string_get_block(*line, "dddddd", NULL, NULL, cov_reduced)) == 0 ) {
			return false;
		}
		reduced_to_full_cov( act_meatool[i].cov, cov_reduced, 3 );
	}
	
	return true;
}


/*
 * Parses a single line of Measurement Tool reference data in one tracking data packet.
 */
bool DTrackParser::parseLine_6dmtr(char **line)
{
	int i, n, id;
	double d;
	
	// get number of measurement references
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	
	// adjust length of vector
	if (n != act_num_mearef) {
		act_mearef.resize(n);
		act_num_mearef = n;
	}
	
	// reset data
	for (i=0; i<n; i++)
	{
		act_mearef[i].id = i;
		act_mearef[i].quality = -1;
	}
	
	// get number of calibrated measurement references
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	
	// get data of measurement references
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "id", &id, NULL, &d)) == 0 ) {
			return false;
		}
		if (id < 0 || id >= (int)act_mearef.size()) {
			return false;
		}
		act_mearef[id].quality = d;
		
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_mearef[id].loc)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_mearef[id].rot)) == 0 ) {
			return false;
		}
	}
	
	return true;
}


/*
 * Parses a single line of additional information about number of calibrated A.R.T. FINGERTRACKING hands in one tracking data packet.
 */
bool DTrackParser::parseLine_glcal(char **line)
{
	if ( (*line = string_get_i(*line, &loc_num_handcal)) == 0 ) {	// get number of calibrated hands
		return false;
	}
	
	return true;
}


/*
 * Parses a single line of A.R.T. FINGERTRACKING hand data in one tracking data packet.
 */
bool DTrackParser::parseLine_gl(char **line)
{
	int i, j, n, iarr[3], id;
	double d, darr[6];
	
	// disable all existing data
	for (i=0; i<act_num_hand; i++) {
		memset(&act_hand[i], 0, sizeof(DTrack_Hand_Type_d));
		act_hand[i].id = i;
		act_hand[i].quality = -1;
	}
	// get number of hands (in line)
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// get data of hands
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "idii", iarr, NULL, &d)) == 0 ){
			return false;
		}
		id = iarr[0];
		if (id >= act_num_hand) {  // adjust length of vector
			act_hand.resize(id + 1);
			for (j=act_num_hand; j<=id; j++) {
				memset(&act_hand[j], 0, sizeof(DTrack_Hand_Type_d));
				act_hand[j].id = j;
				act_hand[j].quality = -1;
			}
			act_num_hand = id + 1;
		}
		act_hand[id].id = iarr[0];
		act_hand[id].lr = iarr[1];
		act_hand[id].quality = d;
		if (iarr[2] > DTRACKSDK_HAND_MAX_FINGER) {
			return false;
		}
		act_hand[id].nfinger = iarr[2];
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_hand[id].loc)) == 0 ) {
			return false;
			
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_hand[id].rot)) == 0 ){
			return false;
		}
		// get data of fingers
		for (j = 0; j < act_hand[id].nfinger; j++) {
			if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_hand[id].finger[j].loc)) == 0 ) {
				return false;
			}
			if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_hand[id].finger[j].rot)) == 0 ){
				return false;
			}
			if ( (*line = string_get_block(*line, "dddddd", NULL, NULL, darr)) == 0 ){
				return false;
			}
			act_hand[id].finger[j].radiustip = darr[0];
			act_hand[id].finger[j].lengthphalanx[0] = darr[1];
			act_hand[id].finger[j].anglephalanx[0] = darr[2];
			act_hand[id].finger[j].lengthphalanx[1] = darr[3];
			act_hand[id].finger[j].anglephalanx[1] = darr[4];
			act_hand[id].finger[j].lengthphalanx[2] = darr[5];
		}
	}
	
	return true;
}


/*
 * Parses a single line of ART-Human model data in one tracking data packet.
 */
bool DTrackParser::parseLine_6dj(char **line)
{
	int i, j, n, iarr[2], id;
	double d, darr[6];
	
	// get number of calibrated human models
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// adjust length of vector
	if(n != act_num_human){
		act_human.resize(n);
		act_num_human = n;
	}
	for(i=0; i<act_num_human; i++){
		memset(&act_human[i], 0, sizeof(DTrack_Human_Type_d));
		act_human[i].id = i;
		act_human[i].num_joints = 0;
	}
	
	// get number of human models
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	int id_human;
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "ii", iarr, NULL,NULL)) == 0 ){
			return false;
		}
		if (iarr[0] > act_num_human - 1) // not expected
			return false;
		
		id_human = iarr[0];
		act_human[id_human].id = iarr[0];
		act_human[id_human].num_joints = iarr[1];
		
		for (j = 0; j < iarr[1]; j++){
			if ( (*line = string_get_block(*line, "id", &id, NULL, &d)) == 0 ){
				return false;
			}
			act_human[id_human].joint[j].id = id;
			act_human[id_human].joint[j].quality = d;
			
			if ( (*line = string_get_block(*line, "dddddd", NULL, NULL, darr)) == 0 ){
				return false;
			}
			memcpy(act_human[id_human].joint[j].loc, &darr,  3*sizeof(double));
			memcpy(act_human[id_human].joint[j].ang, &darr[3],  3*sizeof(double));
			
			if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_human[id_human].joint[j].rot)) == 0 ){
				return false;
			}
		}
	}
	
	return true;
}


/*
 * Parses a single line of hybrid (optical-inertial) body data in one tracking data packet.
 */
bool DTrackParser::parseLine_6di(char **line)
{
	int i, j, n, iarr[2], id, st;
	double d;
	
	// disable all existing data
	for (i=0; i<act_num_inertial; i++) {
		memset(&act_inertial[i], 0, sizeof(DTrack_Inertial_Type_d));
		act_inertial[i].id = i;
		act_inertial[i].st = 0;
		act_inertial[i].error = 0;
	}
	// get number of calibrated inertial bodies
	if ( (*line = string_get_i(*line, &n)) == 0 ) {
		return false;
	}
	// get data of inertial bodies
	for (i=0; i<n; i++) {
		if ( (*line = string_get_block(*line, "iid", iarr, NULL, &d)) == 0 ){
			return false;
		}
		id = iarr[0];
		st = iarr[1];
		// adjust length of vector
		if (id >= act_num_inertial) {
			act_inertial.resize(id + 1);
			for (j = act_num_inertial; j<=id; j++) {
				memset(&act_inertial[j], 0, sizeof(DTrack_Inertial_Type_d));
				act_inertial[ j ].id = j;
				act_inertial[ j ].st = 0;
				act_inertial[ j ].error = 0;
			}
			act_num_inertial = id + 1;
		}
		act_inertial[id].id = id;
		act_inertial[id].st = st;
		act_inertial[id].error = d;
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_inertial[id].loc)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddddddddd", NULL, NULL, act_inertial[id].rot)) == 0 ) {
			return false;
		}
	}
	
	return true;
}


/*
 * Parses a single line of single marker data in one tracking data packet.
 */
bool DTrackParser::parseLine_3d(char **line)
{
	int i;
	
	// get number of markers
	if ( (*line = string_get_i(*line, &act_num_marker)) == 0 ) {
		act_num_marker = 0;
		return false;
	}
	if (act_num_marker > (int )act_marker.size()) {
		act_marker.resize(act_num_marker);
	}
	// get data of single markers
	for (i=0; i<act_num_marker; i++) {
		if ( (*line = string_get_block(*line, "id", &act_marker[i].id, NULL, &act_marker[i].quality)) == 0 ) {
			return false;
		}
		if ( (*line = string_get_block(*line, "ddd", NULL, NULL, act_marker[i].loc)) == 0 ) {
			return false;
		}
	}
	
	return true;
}


/*
 * Get number of calibrated standard bodies (as far as known).
 */
int DTrackParser::getNumBody() const
{
	return act_num_body;
}


/*
 * Get standard body data.
 */
const DTrackBody* DTrackParser::getBody( int id ) const
{
	if ((id >= 0) && (id < act_num_body))
		return &act_body.at(id);
	return NULL;
}


/*
 * Get number of calibrated Flysticks.
 */
int DTrackParser::getNumFlyStick() const
{
	return act_num_flystick;
}


/*
 * Get Flystick data.
 */
const DTrackFlyStick* DTrackParser::getFlyStick( int id ) const
{
	if ((id >= 0) && (id < act_num_flystick))
		return &act_flystick.at(id);
	return NULL;
}


/*
 * Get number of calibrated Measurement Tools.
 */
int DTrackParser::getNumMeaTool() const
{
	return act_num_meatool;
}


/*
 * Get Measurement Tool data.
 */
const DTrackMeaTool* DTrackParser::getMeaTool( int id ) const
{
	if ((id >= 0) && (id < act_num_meatool))
		return &act_meatool.at(id);
	return NULL;
}


/*
 * Get number of calibrated Measurement Tool references.
 */
int DTrackParser::getNumMeaRef() const
{
	return act_num_mearef;
}


/*
 * Get Measurement Tool reference data.
 */
const DTrackMeaRef* DTrackParser::getMeaRef( int id ) const
{
	if ((id >= 0) && (id < act_num_mearef))
		return &act_mearef.at(id);
	return NULL;
}


/*
 * Get number of calibrated A.R.T. FINGERTRACKING hands (as far as known).
 */
int DTrackParser::getNumHand() const
{
	return act_num_hand;
}


/*
 * Get A.R.T. FINGERTRACKING hand data.
 */
const DTrackHand* DTrackParser::getHand( int id ) const
{
	if ((id >= 0) && (id < act_num_hand))
		return &act_hand.at(id);
	return NULL;
}


/*
 * Get number of calibrated ART-Human models.
 */
int DTrackParser::getNumHuman() const
{
	return act_num_human;
}


/*
 * Get ART-Human model data.
 */
const DTrackHuman* DTrackParser::getHuman( int id ) const
{
	if ((id >= 0) && (id < act_num_human))
		return &act_human.at(id);
	return NULL;
}


/*
 * Get number of calibrated hybrid (optical-inertial) bodies.
 */
int DTrackParser::getNumInertial() const
{
	return act_num_inertial;
}


/*
 * Get hybrid (optical-inertial) data.
*/
const DTrackInertial* DTrackParser::getInertial( int id ) const
{
	if((id >=0) && (id < act_num_inertial))
		return &act_inertial.at(id);
	return NULL;
}


/*
 * Get number of tracked single markers.
 */
int DTrackParser::getNumMarker() const
{
	return act_num_marker;
}


/*
 * Get single marker data.
 */
const DTrackMarker* DTrackParser::getMarker( int index ) const
{
	if ((index >= 0) && (index < act_num_marker))
		return &act_marker.at(index);
	return NULL;
}


/*
 * Get frame counter.
 */
unsigned int DTrackParser::getFrameCounter() const
{
	return act_framecounter;
}


/*
 * Get timestamp.
 */
double DTrackParser::getTimeStamp() const
{
	return act_timestamp;
}
