// Copyright (c) Compal Communication, Inc.  All rights reserved.
//
// Trackball event filter
// 
// Author: Johnny Lee
// Date: Feb. 1, 2010 

//==========================================================
// INCLUDES, DEFINITONS, AND MACROS
//==========================================================
#include <linux/module.h>

#include "trackball_i.h"

// Queue buffer number
#define QUEUE_NUM 20

// the macro used to get the fore buffer index
#define QNUM(index, n) ((index-n)>=0)?(index-n):(index-n+QUEUE_NUM)

// definitions
#define X_AXIS 0
#define Y_AXIS 1
#define MAX_AXIS 2

// tuned parameter for sensitivity
#define GOLDEN_SCALE 40
// an unit of triggering the input event.
#define EVENT_SCALE 100


//==========================================================
// GLOBAL VARIABLES
//==========================================================
// info parameters from trackball hardware driver
static struct trackball_filter_info me_info;
// queue buffer for x-axis and y-axis
static struct trackball_filter_packet queue[MAX_AXIS][QUEUE_NUM];
// current queue index
static int qindex[MAX_AXIS];
// the score of triggering the input event
static int score[MAX_AXIS];
// IRQ events tracing time
static int golden_time;
// initialize state
static int initial_state = 0;

//==========================================================
// FUNCTIONS
//==========================================================
static void trackball_filter(int xy)
{
	int gain;
	int i;
	bool cum_deb;

	if (queue[xy][qindex[xy]].time - queue[xy][QNUM(qindex[xy],1)].time > golden_time) {
		score[xy] = -1;
	}
	for (i=0, cum_deb=true; i<(me_info.deb-1); i++) {
		cum_deb = (queue[xy][qindex[xy]].way == queue[xy][QNUM(qindex[xy],(i+1))].way)? cum_deb : false;
	}
	if (cum_deb && (queue[xy][qindex[xy]].time - queue[xy][QNUM(qindex[xy],me_info.deb)].time < golden_time)) {
		if (score[xy]== -1) {
			score[xy]+=(me_info.step * (me_info.deb+1) + 1);
		} else {
			score[xy]+=me_info.step;
		}
	}

	if (score[xy]>= EVENT_SCALE) {
		for (gain=0;score[xy]>=EVENT_SCALE;gain++) {
			score[xy]-=EVENT_SCALE;
		}
		me_info.report((gain > me_info.limit ? me_info.limit : gain),queue[xy][qindex[xy]].way);
	}
}

int trackball_filter_send_packet(struct trackball_filter_packet packet)
{
	int xy;

	if (initial_state == 0) {
		return -1;
	}
	
	xy = (packet.way <=1) ? Y_AXIS : X_AXIS;
	queue[xy][qindex[xy]] = packet;
	trackball_filter(xy);
	//me_info.report(1,queue[qindex].way);

	qindex[xy]++;
	if (qindex[xy] >= QUEUE_NUM) {
		qindex[xy]=0;
	}		
	return 0;
}

int trackball_filter_init(struct trackball_filter_info info)
{
	int i,j;
	for(i=0; i<MAX_AXIS; i++) {
		score[i] = 0;
		for(j=0; j<QUEUE_NUM; j++) {
			queue[i][j].time = 0;
			queue[i][j].way = -1;
		}
	}
	me_info = info;
	golden_time = me_info.time / GOLDEN_SCALE;
	initial_state = 1;
	return 0;
}

MODULE_LICENSE("Proprietary");
