/* driver/cci/jogball/trackball_i.h
*  Trackball filter module header file
* 
*  Author: Johnny Lee
*  Date: Feb. 1, 2010 
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*/
#ifndef CCI_TRACKBALL_I_H
#define CCI_TRACKBALL_I_H

//==========================================================
// STRUCT
//==========================================================
// IRQ event packet
struct trackball_filter_packet {
	int way;   // direction (0,1):x-axis (2,3):y-axis
	long time; // time tag
};

// initialize informations
struct trackball_filter_info {
	int time;  // time unit
	int step;  // step per interrupt event
	int deb;   // debounce parameter
	int limit; // speed limitation
	void (*report)(int gain, int phase); // input event reporter (callback)
};

//==========================================================
// EXTERNAL FUNCTIONS
//==========================================================
int trackball_filter_init(struct trackball_filter_info info);
int trackball_filter_send_packet(struct trackball_filter_packet packet);

#endif //CCI_TRACKBALL_I_H
