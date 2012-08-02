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
#include <dirent.h>

#include <X11/Xlib.h>

char *tzutc = "UTC";
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
check_mail(char* mdir){
    char buf[129];
    unsigned char isFile= 0x8;
    DIR* maildir;
    struct dirent* mail;
    maildir=opendir(mdir);
    int count = 0;
    while(mail = readdir(maildir))
        if(mail->d_type == isFile)
            count++;
    closedir(maildir);
    sprintf(buf,"%d",count);
    return smprintf("%s", buf);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	bzero(buf, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
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
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
runcmd(char* cmd){
    FILE *fp = popen(cmd,"r");
    if (fp == NULL)
        return NULL;
    char ln[30];
    fgets(ln,sizeof(ln)/sizeof(ln[0]),fp);
    pclose(fp);
    ln[strlen(ln)-1]='\0';
    return smprintf("%s",ln);
}

char *
getbattery(char *base)
{
	char *path, line[513];
	FILE *fd;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	path = smprintf("%s/info", base);
	fd = fopen(path, "r");
	if (fd == NULL) {
		perror("fopen");
		exit(1);
	}
	free(path);
	while (!feof(fd)) {
		if (fgets(line, sizeof(line)-1, fd) == NULL)
			break;

		if (!strncmp(line, "present", 7)) {
			if (strstr(line, " no")) {
				descap = 1;
				break;
			}
		}
		if (!strncmp(line, "design capacity", 15)) {
			if (sscanf(line+16, "%*[ ]%d%*[^\n]", &descap))
				break;
		}
	}
	fclose(fd);

	path = smprintf("%s/state", base);
	fd = fopen(path, "r");
	if (fd == NULL) {
		perror("fopen");
		exit(1);
	}
	free(path);
	while (!feof(fd)) {
		if (fgets(line, sizeof(line)-1, fd) == NULL)
			break;

		if (!strncmp(line, "present", 7)) {
			if (strstr(line, " no")) {
				remcap = 1;
				break;
			}
		}
		if (!strncmp(line, "remaining capacity", 18)) {
			if (sscanf(line+19, "%*[ ]%d%*[^\n]", &remcap))
				break;
		}
	}
	fclose(fd);

	if (remcap < 0 || descap < 0)
		return NULL;

	return smprintf("%.0f", ((float)remcap / (float)descap) * 100);
}

#define MEMCMD "echo $(free -m | awk '/buffers\\/cache/ {print $3}')M"
#define MAILDIR_NEW "/home/sajuuk/mail/GMail_A/INBOX/new"

int
main(void)
{
	char *status;
	char *avgs;
    char *mem;
	char *tmutc;
	char *tmbln;
    char *mail;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(90)) {
		avgs = loadavg();
		tmutc = mktimes("%H:%M", tzutc);
		tmbln = mktimes("KW %W %a %d %b %H:%M %Z %Y", tzberlin);
        mail = check_mail(MAILDIR_NEW);
        mem = runcmd("echo $(free -m | awk '/buffers\\/cache/ {print $3}')M");
		status = smprintf("%s | Mail: %s | Mem: %s | UTC: %s | Berlin: %s",
				avgs, mail, mem, tmutc, tmbln);
		setstatus(status);
		free(avgs);
        free(mem);
		free(tmutc);
		free(tmbln);
		free(status);
        free(mail);
	}

	XCloseDisplay(dpy);

	return 0;
}

