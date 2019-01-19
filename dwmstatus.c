#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <sys/types.h>

#include <X11/Xlib.h>

char *tzberlin = "Europe/Berlin";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, status;
	int capacity;

	capacity = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "capacity");
    if (co == NULL) {
        return smprintf("");
    }
	sscanf(co, "%d", &capacity);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else if (!strncmp(co, "Full", 4)) {
        status = 'f';
	}

	if (capacity < 0) {
		return smprintf("invalid");
    }

	return smprintf("%d%%%c", capacity, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
getcpu() {
    float a[3];
    FILE *fp;
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "%*s %f %*s %f %f", &a[0], &a[1], &a[2]);
    fclose(fp);

    return smprintf("%.2f%%", (a[0] + a[1]) * 100 / (a[0] + a[1] + a[2]));
}

char *
get_free_resources() {
    float gigabyte = 1024 * 1024 * 1024;
    struct sysinfo info;
    sysinfo(&info);
    return smprintf("MEM %.2fG | SWAP %.2fG",
            (float)((info.totalram - info.freeram) * info.mem_unit) / gigabyte,
            (float)((info.totalswap - info.freeswap) * info.mem_unit) / gigabyte);
}

char *
get_ssid() {
    char ssid[256];
    FILE *fp = popen("iwgetid -r", "r");
    if (!fp) {
        return "not connected";
    }
	if (fgets(ssid, sizeof(ssid)-1, fp) == NULL) {
		return "not connected";
    }
    ssid[strlen(ssid)-1] = '\0';
    fclose(fp);
    return smprintf("%s", ssid);
}

int
main(void)
{
	char *status;
    char *cpu;
    char *free_resources;
	char *bat;
	char *bat1;
    char *ssid;
	char *tz;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(5)) {
        cpu = getcpu();
        free_resources = get_free_resources();
		bat = getbattery("/sys/class/power_supply/BAT0");
		bat1 = getbattery("/sys/class/power_supply/BAT1");
        ssid = get_ssid();
		tz = mktimes("%d.%m.%Y %H:%M", tzberlin);

		status = smprintf(" CPU %s | %s | BAT0 %s | BAT1 %s | %s | %s ",
				cpu, free_resources, bat, bat1, ssid, tz);
		setstatus(status);

        free(cpu);
        free(free_resources);
		free(bat);
		free(bat1);
        free(ssid);
		free(tz);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

