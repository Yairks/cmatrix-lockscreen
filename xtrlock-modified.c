/*
 * xtrlock.c
 *
 * X Transparent Lock
 * Goes with the modified CMatrix program, so it locks your computer
 * and displays a cool background.
 *
 * Copyright (C)1993,1994 Ian Jackson
 * Modifications Copyright (C)2020 Yair Kosowsky-Sachs
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef EXCLUDE_CONFIG_H
#include "config.h"
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xos.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <string.h>
#include <crypt.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <values.h>
#include <time.h>

/* I really shouldn't do this... */
/* #define SHADOW_PWD
 * There. I didn't do it. */
#ifdef SHADOW_PWD
#include <shadow.h>
#endif

#ifdef MULTITOUCH
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#endif

#include "lock.bitmap"
#include "mask.bitmap"
#include "patchlevel.h"

Display *display;
Window window, root;
char tty[100]; // Really big

#define TIMEOUTPERATTEMPT 30000
#define MAXGOODWILL  (TIMEOUTPERATTEMPT*5)
#define INITIALGOODWILL MAXGOODWILL
#define GOODWILLPORTION 0.3

struct passwd *pw;
int passwordok(const char *s) {
#if 0
    char key[3];
    char *encr;

    key[0] = *(pw->pw_passwd);
    key[1] =  (pw->pw_passwd)[1];
    key[2] =  0;
    encr = crypt(s, key);
    return !strcmp(encr, pw->pw_passwd);
#else
    /* simpler, and should work with crypt() algorithms using longer
       salt strings (like the md5-based one on freebsd).  --marekm */
    return !strcmp(crypt(s, pw->pw_passwd), pw->pw_passwd);
#endif
}

#if MULTITOUCH
XIEventMask evmask;

/* (Optimistically) attempt to grab multitouch devices which are not
 * intercepted via XGrabPointer. */
void handle_multitouch(Cursor cursor) {
    XIDeviceInfo *info;
    int xi_ndevices;

    info = XIQueryDevice(display, XIAllDevices, &xi_ndevices);

    for (int i=0; i < xi_ndevices; i++) {
        XIDeviceInfo *dev = &info[i];

        for (int j=0; j < dev->num_classes; j++) {
            if (dev->classes[j]->type == XITouchClass &&
                    dev->use == XISlavePointer) {
                XIGrabDevice(display, dev->deviceid, window, CurrentTime, cursor,
                        GrabModeAsync, GrabModeAsync, False, &evmask);
            }
        }
    }
    XIFreeDeviceInfo(info);
}
#endif

int main(int argc, char **argv){
    XEvent ev;
    KeySym ks;
    char cbuf[10], rbuf[128]; /* shadow appears to suggest 127 a good value here */
    int clen, rlen=0;
    long goodwill= INITIALGOODWILL, timeout= 0;
    XSetWindowAttributes attrib;
    Cursor cursor;
    Pixmap csr_source,csr_mask;
    XColor csr_fg, csr_bg, dummy, black;
    int ret, screen, blank = 0, fork_after = 0;
#ifdef SHADOW_PWD
    struct spwd *sp;
#endif
    struct timeval tv;
    int tvt, gs;

    if (getenv("WAYLAND_DISPLAY"))
        fprintf(stderr,"WARNING: Wayland X server detected: xtrlock"
                " cannot intercept all user input. See xtrlock(1).\n");

    while (argc > 1) {
        if ((strcmp(argv[1], "-b") == 0)) {
            blank = 1;
            argc--;
            argv++;
        } else if ((strcmp(argv[1], "-f") == 0)) {
            fork_after = 1;
            argc--;
            argv++;
        } else {
            fprintf(stderr,"xtrlock (version %s); usage: xtrlock [-b] [-f]\n",
                    program_version);
            exit(1);
        }
    }

    errno=0;  pw= getpwuid(getuid());
    if (!pw) { perror("password entry for uid not found"); exit(1); }
#ifdef SHADOW_PWD
    sp = getspnam(pw->pw_name);
    if (sp)
        pw->pw_passwd = sp->sp_pwdp;
    endspent();
#endif

    /* logically, if we need to do the following then the same 
       applies to being installed setgid shadow.  
       we do this first, because of a bug in linux. --jdamery */ 
    if (setgid(getgid())) { perror("setgid"); exit(1); }
    /* we can be installed setuid root to support shadow passwords,
       and we don't need root privileges any longer.  --marekm */
    if (setuid(getuid())) { perror("setuid"); exit(1); }

    if (strlen(pw->pw_passwd) < 13) {
        fputs("password entry has no pwd\n",stderr);
        exit(1);
    }

    display= XOpenDisplay(0);

    if (display==NULL) {
        fprintf(stderr,"xtrlock (version %s): cannot open display\n",
                program_version);
        exit(1);
    }

#ifdef MULTITOUCH
    unsigned char mask[XIMaskLen(XI_LASTEVENT)];
    int xi_major = 2, xi_minor = 2, xi_opcode, xi_error, xi_event;

    if (!XQueryExtension(display, INAME, &xi_opcode, &xi_event, &xi_error)) {
        fprintf(stderr, "xtrlock (version %s): No X Input extension\n",
                program_version);
        exit(1);
    }

    if (XIQueryVersion(display, &xi_major, &xi_minor) != Success ||
            xi_major * 10 + xi_minor < 22) {
        fprintf(stderr,"xtrlock (version %s): Need XI 2.2\n",
                program_version);
        exit(1);
    }

    evmask.mask = mask;
    evmask.mask_len = sizeof(mask);
    memset(mask, 0, sizeof(mask));
    evmask.deviceid = XIAllDevices;
    XISetMask(mask, XI_HierarchyChanged);
    XISelectEvents(display, DefaultRootWindow(display), &evmask, 1);
#endif

    attrib.override_redirect= True;

    if (blank) {
        screen = DefaultScreen(display);
        attrib.background_pixel = BlackPixel(display, screen);
        window= XCreateWindow(display,DefaultRootWindow(display),
                0,0,DisplayWidth(display, screen),DisplayHeight(display, screen),
                0,DefaultDepth(display, screen), CopyFromParent, DefaultVisual(display, screen),
                CWOverrideRedirect|CWBackPixel,&attrib); 
        XAllocNamedColor(display, DefaultColormap(display, screen), "black", &black, &dummy);
    } else {
        window= XCreateWindow(display,DefaultRootWindow(display),
                0,0,1,1,0,CopyFromParent,InputOnly,CopyFromParent,
                CWOverrideRedirect,&attrib);
    }

    XSelectInput(display,window,KeyPressMask|KeyReleaseMask);

    csr_source= XCreateBitmapFromData(display,window,lock_bits,lock_width,lock_height);
    csr_mask= XCreateBitmapFromData(display,window,mask_bits,mask_width,mask_height);

    ret = XAllocNamedColor(display,
            DefaultColormap(display, DefaultScreen(display)),
            "steelblue3",
            &dummy, &csr_bg);
    if (ret==0)
        XAllocNamedColor(display,
                DefaultColormap(display, DefaultScreen(display)),
                "black",
                &dummy, &csr_bg);

    ret = XAllocNamedColor(display,
            DefaultColormap(display,DefaultScreen(display)),
            "grey25",
            &dummy, &csr_fg);
    if (ret==0)
        XAllocNamedColor(display,
                DefaultColormap(display, DefaultScreen(display)),
                "white",
                &dummy, &csr_bg);



    cursor= XCreatePixmapCursor(display,csr_source,csr_mask,&csr_fg,&csr_bg,
            lock_x_hot,lock_y_hot);

    XMapWindow(display,window);

    /*Sometimes the WM doesn't ungrab the keyboard quickly enough if
     *launching xtrlock from a keystroke shortcut, meaning xtrlock fails
     *to start We deal with this by waiting (up to 100 times) for 10,000
     *microsecs and trying to grab each time. If we still fail
     *(i.e. after 1s in total), then give up, and emit an error
     */

    gs=0; /*gs==grab successful*/
    for (tvt=0 ; tvt<100; tvt++) {
        ret = XGrabKeyboard(display,window,False,GrabModeAsync,GrabModeAsync,
                CurrentTime);
        if (ret == GrabSuccess) {
            gs=1;
            break;
        }
        /*grab failed; wait .01s*/
        tv.tv_sec=0;
        tv.tv_usec=10000;
        select(1,NULL,NULL,NULL,&tv);
    }
    if (gs==0){
        fprintf(stderr,"xtrlock (version %s): cannot grab keyboard\n",
                program_version);
        exit(1);
    }

    /* Original code:
     * if (XGrabPointer(display,window,False,(KeyPressMask|KeyReleaseMask)&0,
     *             GrabModeAsync,GrabModeAsync,None,
     *             cursor,CurrentTime)!=GrabSuccess) {
     *
     * I need to get mouse pointer events as well, so that jiggling the mouse
     * will wake up the cmatrix display if it fell asleep. */
if (XGrabPointer(display,window,False,(PointerMotionMask|ButtonPressMask),
            GrabModeAsync,GrabModeAsync,None,
            cursor,CurrentTime)!=GrabSuccess) {
    XUngrabKeyboard(display,CurrentTime);
    fprintf(stderr,"xtrlock (version %s): cannot grab pointer\n",
            program_version);
    exit(1);
}

#ifdef MULTITOUCH
handle_multitouch(cursor);
#endif

if (fork_after) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr,"xtrlock (version %s): cannot fork: %s\n",
                program_version, strerror(errno));
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

/* Fork and create a pipe between xtrlock (parent) and the modified cmatrix (child) */
pid_t pid;
int pipefd[2];

pipe(pipefd);
pid = fork();
if (pid > 0) { /* Parent */

    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    for (;;) {

        XNextEvent(display,&ev);
        kill(pid, SIGCONT); /* Restart the cmatrix program if it stopped itself */

        switch (ev.type) {
            case KeyPress:
                /* @ tells cmatrix to display green, and z tells it to light up part
                 * of the display so the user gets feedback for their typing in the 
                 * password
                 */
                putchar('@');
                putchar('z');
                fflush(stdout);

                if (ev.xkey.time < timeout) { XBell(display,0); break; }
                clen= XLookupString(&ev.xkey,cbuf,9,&ks,0);
                switch (ks) {
                    case XK_Escape: case XK_Clear:
                        rlen=0; break;
                    case XK_Delete: case XK_BackSpace:
                        if (rlen>0) rlen--;
                        break;
                    case XK_Linefeed: case XK_Return:
                        if (rlen==0) break;
                        rbuf[rlen]=0;
                        if (passwordok(rbuf)) goto loop_x;

                        /* Password was wrong. Go red!
                         * That is, the cmatrix display should display red. */
                        putchar('!');
                        fflush(stdout);

                        XBell(display,0);
                        rlen= 0;
                        if (timeout) {
                            goodwill+= ev.xkey.time - timeout;
                            if (goodwill > MAXGOODWILL) {
                                goodwill= MAXGOODWILL;
                            }
                        }
                        timeout= -goodwill*GOODWILLPORTION;
                        goodwill+= timeout;
                        timeout+= ev.xkey.time + TIMEOUTPERATTEMPT;
                        break;
                    default:
                        if (clen != 1) break;
                        /* allow space for the trailing \0 */
                        if (rlen < (sizeof(rbuf) - 1)){
                            rbuf[rlen]=cbuf[0];
                            rlen++;
                        }

                        break;
                }
                break;
#if MULTITOUCH
            case GenericEvent:
                if (ev.xcookie.extension == xi_opcode &&
                        XGetEventData(display,&ev.xcookie) &&
                        ev.xcookie.evtype == XI_HierarchyChanged) {
                    handle_multitouch(cursor);
                }
                break;
#endif
            default:
                break;
        }
    }
loop_x:
    putchar('q');
    exit(0);
} /* End parent */

else if (pid == 0) { /* Child */
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    execlp("cmatrix-modified", "cmatrix-modified", NULL);
}

return 0;
}
