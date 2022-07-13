#!/usr/bin/python3

import subprocess
import time

class Window:
    def __init__(self):
        self.pid = None
        self.xofs = None
        self.yofs = None
        self.xsize = None
        self.ysize = None
        self.title = None
        
    def __repr__(self):
        return "Window pid=%s xofs=%s, yofs=%s, size=%sx%s title=%s" % (self.pid, self.xofs, self.yofs, self.xsize, self.ysize, self.title)


def parse_windows(cmd):
    rawlist=subprocess.run(cmd, shell=True,
                           stdout=subprocess.PIPE).stdout.decode('utf-8')
    lines=rawlist.strip().split('\n')
    #print(lines)
    resp = []
    for curr in lines:
        args = curr.split()
        w = Window()
        w.win_id = args[0]
        w.ws = int(args[1])
        w.pid = args[2]
        w.xofs = int(args[3])
        w.yofs = int(args[4])
        w.xsize = int(args[5])
        w.ysize = int(args[6])
        w.title = ' '.join(args[8:])
        resp.append(w)
    return resp


def get_monitors():
    l = parse_windows('wmctrl -l -p -G | egrep " (nemo-d|D)esktop$" | grep -v "Chrome"')
    for d in l:
        d.xofs = int(d.xofs / 2)
        d.yofs = int(d.yofs / 2)
        if d.xsize == 1920 and d.ysize == 1080:
            d.yofs += 26
            d.ysize -= 50
        d.xsize -= 10
    #print(l)
    # account for panel height on primary monitor
    return l


def get_emacs_pid():
    return subprocess.run('ps h -C google-emacs -o %p',
                          shell=True,stdout=subprocess.PIPE).stdout.decode('utf-8').strip()

def get_workspace():
    wl = subprocess.run('wmctrl -d',
                          shell=True,stdout=subprocess.PIPE).stdout.decode('utf-8').splitlines()
    args=wl[0].split()
    wh=args[3].split('x')
    w=Window()
    w.xsize=int(wh[0]);
    w.ysize=int(wh[1]);
    print(w)
    return w

def select_workspace(n):
    global workspace
    cmd="wmctrl -s %d" % (n)
    print(cmd)
    subprocess.run(cmd, shell=True)
    time.sleep(1)

def switch_to_next():
    global current_workspace, current_monitor, next_right, monitors
    if current_monitor >= len(monitors) - 1:
        current_workspace += 1
        if (current_workspace > 4):
            raise Exception("Ran out of workspaces.")
        current_monitor = 0
        print("Workspace %d" % current_workspace)
        select_workspace(current_workspace)
    else:
        current_monitor += 1
        print(" Monitor %d" % current_monitor)
    next_right = monitors[current_monitor].xsize
    
def populate_window(w):
    global current_workspace, current_monitor, next_right, monitors
    if next_right < (w.xsize + 6):
        switch_to_next()
    m = monitors[current_monitor]
    next_right -= (w.xsize + 6)
    if w.ws != current_workspace:
        cmd = "wmctrl -i -r %s -b add,sticky" % (w.win_id)
        print(cmd)
        subprocess.run(cmd, shell=True)
        cmd = "wmctrl -i -r %s -b remove,sticky" % (w.win_id)
        print(cmd)
        subprocess.run(cmd, shell=True)
    cmd = "wmctrl -i -r %s -e 0,%d,%d,-1,%d" % (w.win_id, next_right + m.xofs, m.yofs, m.ysize)
    print(cmd)
    subprocess.run(cmd, shell=True)
    
    
workspace = get_workspace()
print("Workspace ", workspace)
monitors = get_monitors()
print("monitors ", monitors)
emacspid = get_emacs_pid()
print("emacs pid ", emacspid)

current_workspace = 2
select_workspace(current_workspace)
current_monitor = 0
next_right = monitors[current_monitor].xsize

emacs_windows = [ w for w in parse_windows('wmctrl -l -p -G') if w.pid == emacspid ]
for w in emacs_windows:
    populate_window(w)
