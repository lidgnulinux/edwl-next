/* Modify this file to change what commands output to your statusbar, and recompile using the make command. */
static const Block blocks[] = {
	/*Icon*/	/*Command*/		/*Update Interval*/
	{"", "/home/lidg/.local/bin/batt", 15},
	/* {"📜 ", "xkb-switch", 5}, */
	{"", "date +'%H:%M %d-%m-%Y'", 60},
};

/* sets delimeter between status commands. NULL character ('\0') means no delimeter. */
static char delim[] = " | ";
static unsigned int delimLen = 5;

/* random backgrounds lists, jpg and png only! */
static const char* backgroundimagesdir = "/usr/local/share/edwl/";
static int backgroundinterval = 0;
