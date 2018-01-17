#!/usr/bin/python

# Initng, a next generation sysvinit replacement.
# Copyright (C) 2005-6 Aidan Thornton <makomk@lycos.co.uk>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General
# Public License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.

import socket,sys,struct
from select import select, poll
from types import *
from time import localtime
from errno import EAGAIN, EWOULDBLOCK

NGCS_TYPE_NONE = 0
NGCS_TYPE_INT = 1
NGCS_TYPE_STRING = 2
NGCS_TYPE_BLOB = 3
NGCS_TYPE_LONG = 4
NGCS_TYPE_STRUCT = 6
NGCS_TYPE_BOOL = 7
NGCS_TYPE_ERROR = 8

SIZEOF_INT = struct.calcsize("@i")

IS_UNKNOWN = 0
IS_UP = 1
IS_DOWN = 2
IS_FAILED = 3
IS_STARTING = 4
IS_STOPPING = 5
IS_WAITING = 6


NGCS_WATCH_STATUS = 0x1
NGCS_WATCH_OUTPUT = 0x2
NGCS_CURRENT_STATUS = 0x4


sock = 0;

class NgcsData:
    def __init__(self,t,d):
        self.typecode = t
        self.data = d
    def __repr__(self):
        return "NgcsData("+repr(self.typecode)+", "+repr(self.data)+")"

class NgcsEOF:
    def __init__(self,i):
        self.val = i
    def __repr__(self):
        return "NgcsEOF("+repr(self.val)+")"

class AsyncConnect:
    def __init__(self):
        self.sock = socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        self.sock.connect("/dev/initng/initng.ngcs")
        self.sock.setblocking(0)
        self._recvbuf = ""
        self._sendbuf = ""
        self._head = None
        self._chans = { }
        self.reg_channel(0, self._chan_0)
        self._pending_req = [ ]

    def close(self):
        self.sock.close()
        self.sock = None

    def _chan_0(self, conn, chan, data):
        if len(self._pending_req) == 0:
            sys.stderr.write("Unexpected response on channel 0\n")
            return
        self._pending_req.pop(0)(data)

    def send_cmd(self, cmd, callback):
        self._pending_req.append(callback)
        self.send(0, cmd)
    
    def send(self, chan, msg):
        if isinstance(msg, NgcsEOF):
            assert msg.val < 0
            self._sendbuf += struct.pack("@iii",chan,NGCS_TYPE_NONE,msg.val)
        else:
            msg = ngcs_pack(msg)
            self._sendbuf += struct.pack("@iii",chan,msg.typecode,len(msg.data)) + msg.data
        self.try_send()
        
    def try_send(self):
        try:
            ret = self.sock.send(self._sendbuf)
            self._sendbuf = self._sendbuf[ret:]
        except socket.error, e:
            if e[0] != EAGAIN and e[0] != EWOULDBLOCK:
                raise

    def unhandled_data_hook(self, chan, data):
        pass

    def reg_channel(self, chan, hook):
        self._chans[chan] = hook

    def unreg_channel(self, chan):
        del self._chans[chan]

    def close_channel(self, chan):
        self.send(chan, NgcsEOF(-1))
        if self._chans.has_key(chan):
            del self._chans[chan]

    def try_recv(self):
        while True:
            try:
                ret = self.sock.recv(4096)
            except socket.error, e:
                if e[0] == EAGAIN:
                    break
                else:
                    raise
            if len(ret) == 0: break
            self._recvbuf += ret
            
        while True:
            if self._head == None:
                if len(self._recvbuf) < 3*SIZEOF_INT: break
                self._head = struct.unpack("@iii",self._recvbuf[:3*SIZEOF_INT])
                self._recvbuf = self._recvbuf[3*SIZEOF_INT:]
            else:
                if len(self._recvbuf) < self._head[2]: break
                (chan, type, l) = self._head; self._head = None
                if l < 0:
                    data = NgcsEOF(l)
                else:
                    data = NgcsData(type, self._recvbuf[:l])
                    self._recvbuf = self._recvbuf[l:]
                if(self._chans.has_key(chan)):
                    self._chans[chan](self, chan, data)
                else:
                    self.unhandled_data_hook(chan, data)

class _NgcsCmds:
    def __init__(self, conn):
        self.conn = conn
        self.cmds = [ ]
        self.success = True

    def add(self, cmd):
        self.cmds.append(cmd)

    def rem(self, cmd):
        self.cmds.remove(cmd)

    def fail(self):
        self.success = False

    def mainloop(self):
        while len(self.cmds) > 0:
            res = select((self.conn.sock,),(self.conn.sock,),(self.conn.sock,))
            if self.conn.sock in res[0]:
                self.conn.try_recv()
            if self.conn.sock in res[1]:
                self.conn.try_send()
            if self.conn.sock in res[2]:
                return False
        return self.success

class _NgcsCmd:
    def __init__(self, cmds, cmd, txt):
        if not isinstance(cmds,  _NgcsCmds): raise TypeError
        self._cmds = cmds
        self._txt = txt
        cmds.add(self)
        cmds.conn.send_cmd(cmd, self._resp_cb)

    def _resp_cb(self, resp):
        self._cmds.rem(self)
        if isinstance(resp, NgcsEOF):
            sys.stderr.write("FAILED " + self._txt + ": unknown server error\n")
            self._cmds.fail(); return
        resp = ngcs_unpack(resp)
        if isinstance(resp, NgcsData) and resp.typecode == NGCS_TYPE_ERROR:
            self._cmds.fail();
            sys.stdout.write("FAILED %s: error %s\n" %
                             (self._txt, resp.data))
        else:
            sys.stderr.write("FAILED " + self._txt + ": unexpected response\n")
            self._cmds.fail(); return            

class _NgcsSimpleCmd(_NgcsCmd):
    def __init__(self, cmds, cmd, arg = None):
        if arg == None:
            rcmd = (cmd,); txt = cmd
        else:
            rcmd = (cmd,arg); txt = cmd + " " + arg
        _NgcsCmd.__init__(self, cmds, rcmd, txt)
    
    def _resp_cb(self, resp):
        self._cmds.rem(self)
        if isinstance(resp, NgcsEOF):
            sys.stdout.write("FAILED " + self._txt + ": unknown server error\n")
            self._cmds.fail(); return
        resp = ngcs_unpack(resp)

        if resp == None:
            sys.stdout.write("SUCCESS " + self._txt + "\n")
        elif isinstance(resp, bool):
            if resp:
                sys.stdout.write("SUCCESS " + self._txt + "\n")
            else:
                self._cmds.fail();
                sys.stdout.write("FAILED " + self._txt + " (command reported failure)\n")
        elif isinstance(resp, int):
            sys.stdout.write("SUCCESS %s: INT_COMMAND_RESULT: %i\n" %
                             (self._txt, resp))
        elif isinstance(resp, str):
            sys.stdout.write("SUCCESS %s:\n   %s\n" %
                             (self._txt, resp))
        elif isinstance(resp, NgcsData) and resp.typecode == NGCS_TYPE_ERROR:
            self._cmds.fail();
            sys.stdout.write("FAILED %s: error %s\n" %
                             (self._txt, resp.data))
        elif isinstance(resp, NgcsData) and resp.typecode == NGCS_TYPE_BLOB:
            self._cmds.fail();
            sys.stdout.write("ERROR %s: got BLOB\n")
        else:
            self._cmds.fail();
            sys.stdout.write("ERROR %s: got unhandled data %s\n", repr(resp))
             

class _NgcsEWatchCmd (_NgcsCmd):
    def __init__(self, cmds):
        _NgcsCmd.__init__(self, cmds, ("ewatch",), "error watch")

    def _resp_cb(self, resp):
        resp = ngcs_unpack(resp)
        if not isinstance(resp, IntType):
            return _NgcsCmd._resp_cb(self, resp)
        if resp == 0:
            sys.stderr.write("FAILED " + self._txt + ": no channel\n")
            self._cmds.fail(); self._cmds.rem(self); return
        self._chan = resp
        self._cmds.conn.reg_channel(resp, self._chan_cb)

    def _chan_cb(self, conn, chan, data):
        data = ngcs_unpack(data)
        if isinstance(data, NgcsEOF):
            sys.stderr.write("FAILED " + self._txt + ": EOF on channel\n")
            self._cmds.fail(); self._cmds.rem(self);
            self._cmds.conn.unreg_channel(self._chan);
            return

        if not ( isinstance(data, tuple) and len(data) == 5 and
                 isinstance(data[0], int) and
                 isinstance(data[1], str) and
                 isinstance(data[2], str) and
                 isinstance(data[3], int) and
                 isinstance(data[4], str)):
            sys.stderr.write("FAILED " + self._txt + ": bad structure\n")
            self._cmds.fail(); self._cmds.rem(self);
            self._cmds.conn.close_channel(self._chan);
            return
        
        (mt, file, func, line, msg) = data
        # FIXME - should display/filter severity
        sys.stdout.write('%s:%i %s(): %s' % (file, line, func, msg))
    
class _NgcsStartStopCmd (_NgcsCmd):
    def __init__(self, cmds, cmd, svc):
        _NgcsCmd.__init__(self, cmds, self._get_real_cmd(cmd,svc), cmd + " " + str(svc))
        self._svc = svc
        self._cmd = cmd
        self._rtmark = False

    def _get_real_cmd(self, cmd, svc):
        return (cmd, svc)

    def _resp_cb(self, resp):
        resp = ngcs_unpack(resp)
        if isinstance(resp, IntType):
            if resp == 0:
                sys.stderr.write("FAILED " + self._txt + ": didn't get channel\n")
                self._cmds.fail(); self._done(); return
            self._chan = resp
            self._cmds.conn.reg_channel(resp, self._chan_cb)
        else:
            return _NgcsCmd._resp_cb(self, resp)

    def _state_check(self, state):
        pass

    def _now_at_now(self):
        pass

    def _chan_cb(self, conn, chan, data):
        data = ngcs_unpack(data)
        if data == None:
            self._now_at_now()
            self._rtmark = True
        if not isinstance(data, TupleType) or len(data) < 2 or not isinstance(data[0], str):
            return
        self._svc = data[0]
        self._text = "start %s" % data[0]
        if isinstance(data[1], str):
            sys.stderr.write(self._svc + " output:\n" + data[1])
        elif isinstance(data[1], tuple):
            state = data[2]
            if self._rtmark:
                sys.stderr.write("%s: %s\n" % (self._svc, state[0]));
            self._state_check(state)

    def _done(self):
        self._cmds.rem(self); self._cmds.conn.close_channel(self._chan);

class _NgcsStatusCmd(_NgcsStartStopCmd):
    def _get_real_cmd(self, cmd, svc):
        return ("watch", NGCS_CURRENT_STATUS, svc)

    def __init__(self, cmds, svc):
        _NgcsStartStopCmd.__init__(self, cmds, "status", svc)
        self._stats = [ ]

    def _chan_cb(self, conn, chan, data):
        data = ngcs_unpack(data)
        if data == None:
            sys.stdout.write(
"""hh:mm:ss service                               : status
----------------------------------------------------------------
""")
            for s in self._stats:
                t = localtime(s[3][0]); 
                sys.stdout.write("%.2i:%.2i:%.2i %-37s : %s\n" %
                                 ( t.tm_hour, t.tm_min, t.tm_sec, s[0], s[2][0] ))
            self._done(); return
            sys.stdout.write("\n")
        
        if not isinstance(data, TupleType) or len(data) < 2 or not isinstance(data[0], str):
            return
        self._stats.append(data)
        
        

class _NgcsStartCmd(_NgcsStartStopCmd):
    def __init__(self, cmds, svc):
        _NgcsStartStopCmd.__init__(self, cmds, "start", svc)
        
    def _state_check(self, state):
        if state[1] == IS_STARTING:
            pass
        elif state[1] in (IS_UP, IS_WAITING):
            sys.stderr.write("Service %s started succesfully (%s)\n" % (self._svc, state[0]))
            self._done()
        elif self._rtmark:
            sys.stderr.write("Service %s failed to start (%s)\n" % (self._svc, state[0]))
            self._done(); self._cmds.fail()

class _NgcsStopCmd(_NgcsStartStopCmd):
    def __init__(self, cmds, svc):
        _NgcsStartStopCmd.__init__(self, cmds, "stop", svc)
        
    def _state_check(self, state):
        if state[1] == IS_STOPPING:
            pass
        elif state[1] == IS_DOWN:
            sys.stderr.write("Service %s stopped succesfully (%s)\n" % (self._svc, state[0]))
            self._done()
        elif self._rtmark:
            sys.stderr.write("Service %s failed to stop (%s)\n" % (self._svc, state[0]))
            self._done(); self._cmds.fail()
       
def connect():
    global sock;
    sock = socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    sock.connect("/dev/initng/initng.ngcs")
    sys.stderr.write("Connected!\n")
    sock.settimeout(30.0)

def ngcs_send(chan, msg):
    if isinstance(msg, NgcsEOF):
        assert msg.val < 0
        sock.sendall(struct.pack("@iii",chan,NGCS_TYPE_NONE,msg.val))
    else:
        msg = ngcs_pack(msg)
        sock.sendall(struct.pack("@iii",chan,msg.typecode,len(msg.data)))
        sock.sendall(msg.data)

def ngcs_pack(datum):
    if isinstance(datum, NoneType):
         return NgcsData(NGCS_TYPE_NONE,"");
    elif isinstance(datum, int):
        return NgcsData(NGCS_TYPE_INT,struct.pack("@i",datum));
    elif isinstance(datum, bool):
        if datum:
            return NgcsData(NGCS_TYPE_BOOL,struct.pack("@i",1));
        else:
            return NgcsData(NGCS_TYPE_BOOL,struct.pack("@i",0));
    elif isinstance(datum, str):
        return NgcsData(NGCS_TYPE_STRING,datum);
    elif isinstance(datum, NgcsData):
        return datum;
    elif isinstance(datum, (tuple,list)):
        s = ""
        for item in datum:
            item = ngcs_pack(item)
            s += struct.pack("@ii",item.typecode,len(item.data)) + item.data
        return NgcsData(NGCS_TYPE_STRUCT, s)
    else:
        raise TypeError

def ngcs_unpack(data):
    if not isinstance(data, NgcsData):
        return data
    elif data.typecode == NGCS_TYPE_NONE:
        return None
    elif data.typecode == NGCS_TYPE_INT:
        return struct.unpack("@i", data.data)[0]
    elif data.typecode == NGCS_TYPE_BOOL:
        return struct.unpack("@i", data.data)[0] != 0
    elif data.typecode == NGCS_TYPE_LONG:
        return struct.unpack("@l", data.data)[0]
    elif data.typecode == NGCS_TYPE_STRING:
        return data.data
    elif data.typecode == NGCS_TYPE_STRUCT:
        out = [ ]; d = data.data
        while d:
            (t,l) = struct.unpack("@ii", d[:2*SIZEOF_INT])
            v = d[2*SIZEOF_INT:2*SIZEOF_INT+l]
            d = d[2*SIZEOF_INT+l:]
            out.append(ngcs_unpack(NgcsData(t,v)));
        return tuple(out)
    else:
        return data
        
def ngcs_recv():
    (chan, type, l) = struct.unpack("@iii",recvall(3*SIZEOF_INT))
    if l < 0:
        return (chan, NgcsEOF(l))
    return (chan, NgcsData(type, recvall(l)))    

def recvall(amt):
    buf = ""
    while len(buf) < amt:
        data = sock.recv(amt - len(buf))
        if len(data) == 0:
            raise IOError, "End of file"
        buf += data
    return buf

def _main():
    cmd = None; cmdlist = []; cmdargs = []; success = True
    for a in sys.argv[1:]:
        if len(a) < 1: continue
        if a[0] == '-':
            if cmd != None:
                cmdlist.append((cmd, cmdargs))
            cmd = a; cmdargs = []
        else:
            cmdargs.append(a)
    if cmd != None:
        cmdlist.append((cmd, cmdargs))
        
    conn = AsyncConnect()
    wcmds = _NgcsCmds(conn)
    for c in cmdlist:
        if cmd in ("-u","--start"):
            for arg in c[1]:
                _NgcsStartCmd(wcmds,arg)
        elif cmd in ("-d","--stop"):
            for arg in c[1]:
                _NgcsStopCmd(wcmds,arg)
        elif cmd in ("-s","--status"):
            if len(c[1]) > 0:
                for arg in c[1]:
                    _NgcsStatusCmd(wcmds,arg)
            else:
                _NgcsStatusCmd(wcmds,None)
        elif cmd == '--errwatch':
            if len(c[1]) > 0:
                sys.stderr.write("Error: --errwatch takes no options\n")
                success = False
            else:
                _NgcsEWatchCmd(wcmds)
        else:
            if len(c[1]) > 0:
                for arg in c[1]:
                    _NgcsSimpleCmd(wcmds, c[0], arg)
            else:
                _NgcsSimpleCmd(wcmds, c[0])
        if not wcmds.mainloop(): success = False
    conn.close()
    if success:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__=='__main__':
    _main()
