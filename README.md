# cmatrix-lockscreen
Locks the screen and shows the display from The Matrix

This project is basically a glueing together of two much better projects, [CMatrix](https://github.com/abishekvashok/cmatrix) and [xtrlock](https://salsa.debian.org/debian/xtrlock).
When you run it, the computer will lock and show the epic display from The Matrix, and change colors when you type in your password.
<br />
#### Cool Videos
Here's what the lockscreen looks like
![A gif of the cmatrix program](https://github.com/Yairks/cmatrix-lockscreen/blob/master/cmatrix-lockscreen.gif)  
And here's the program rejecting two incorrect passwords
![A gif of the cmatrix program rejecting a password](https://github.com/Yairks/cmatrix-lockscreen/blob/master/cmatrix-fail.gif)

### Installing
To install the program, clone this repository (or just download the zip), navigate to the cmatrix-lockscreen directory and run:  
```
autoreconf -i
./configure
make
sudo make install
```

### Requirements
 - You'll need an X server (you're safe if you use Linux or Mac).
 - Right now, this only supports Gnome-Terminal out of the box. I'll add support for others soon.
 - It's possible that this could run on Macs. Haven't tried. In order to get it to work, you'll need to uncomment the line for Apple in the ```cmatrix-lockscreen``` file and comment out the Gnome-Terminal line BEFORE installing.
 - This program does run on WSL1, although it's not very interesting, because while the terminal window does lock up, you can always just exit it.

To get the requirements on Ubuntu (sorry guys, it's the only package manager I know) run:
```
sudo apt update
sudo apt upgrade
sudo apt install gcc make libncurses-dev xorg openbox libx11-dev autoconf rofi
```

The sudo at the end of the install is necessary, because that's how the program gets the power to validate your password.  
Yes, this is a mysterious package that is published by some random guy that requires root access to your machine. This isn't malware, I promise. But you take your life in your hands if you dare install this.  

TODO: somehow show that I haven't put in any malware. Maybe with a diff of the [original program](https://salsa.debian.org/debian/xtrlock).  

### Uninstalling
If you were brave enough to install the program but now are having second thoughts, run  
```
sudo make uninstall
```
and all your problems will disappear.  
<sub>Legal note: The lack of warranty included in this program means that Yair Kosowsky-Sachs© is not liable for any lack of problem-disappearance post uninstallation.</sub>    
<br />
<br />
This program is bug-free. I think. It definitely runs on my machine. I promise you that.  

### Contributing
Really? You want to contribute? Whatever floats your boat, man.
