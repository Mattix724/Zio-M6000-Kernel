#ifndef _MSM_AUDIO_PM_H
#define _MSM_AUDIO_PM_H

/* Turn the speaker on or off and enables or disables mute.*/
enum spkr_cmd {
	SPKR_DISABLE,  /* Enable Speaker */
	SPKR_ENABLE,   /* Disable Speaker */
	SPKR_MUTE_OFF, /* turn speaker mute off, SOUND ON */
	SPKR_MUTE_ON,  /* turn speaker mute on, SOUND OFF */
	SPKR_OFF,      /* turn speaker OFF (speaker disable and mute on) */
	SPKR_ON,        /* turn speaker ON (speaker enable and mute off)  */
	SPKR_SET_FREQ_CMD,    /* set speaker frequency */
	SPKR_GET_FREQ_CMD,    /* get speaker frequency */
	SPKR_SET_GAIN_CMD,    /* set speaker gain */
	SPKR_GET_GAIN_CMD,    /* get speaker gain */
	SPKR_SET_DELAY_CMD,   /* set speaker delay */
	SPKR_GET_DELAY_CMD,   /* get speaker delay */
	SPKR_SET_PDM_MODE,
	SPKR_SET_PWM_MODE,
};
enum spkr_left_right {
	LEFT_SPKR,
	RIGHT_SPKR,
};

enum spkr_dly {
	SPKR_DLY_10MS,            /* ~10  ms delay */
	SPKR_DLY_100MS,           /* ~100 ms delay */
};

enum spkr_gain {
	SPKR_GAIN_MINUS16DB,      /* -16 db */
	SPKR_GAIN_MINUS12DB,      /* -12 db */
	SPKR_GAIN_MINUS08DB,      /* -08 db */
	SPKR_GAIN_MINUS04DB,      /* -04 db */
	SPKR_GAIN_00DB,           /*  00 db */
	SPKR_GAIN_PLUS04DB,       /* +04 db */
	SPKR_GAIN_PLUS08DB,       /* +08 db */
	SPKR_GAIN_PLUS12DB,       /* +12 db */
};

int pmic_speaker_cmd(const enum spkr_cmd cmd);
int pmic_spkr_en_left_chan(uint enable);
int pmic_set_speaker_delay(enum spkr_dly delay);
int pmic_set_speaker_gain(enum spkr_gain gain);
int pmic_spkr_is_en(enum spkr_left_right left_right, uint *enabled);


#endif  // _MSM_AUDIO_PM_H
