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

import re,sys
from os import path

def mkmarshaler(name, members,sname=None):
    l = { 'need_ret': False }
    if sname == None:
        sname = name
    decstr = ("int ngcs_marshal_" + name + "(" + sname +
               "* s, char* buf)")
    initstr = decstr+ "\n{\n    int len = 0; int n = 0; (void) n;\n"
    decstr += ";\n"
    process_str = ""
    for m in members:
        l['m'] = m
        process_str += process("""
#if (m[0] in ("string","blob"))
    if(s->@m[1]@)
    {
#if m[0] == "string" and len(m) == 2
#exec n = 'n'
        n = strlen(s->@m[1]@);
#elif len(m) == 3
#exec n = 's->' + m[2]
#else
#exec  raise Exception, "Bad structure specifier"        
#endif
        if(buf)
        {
#if m[0] == "string"        
            *(int*)buf = NGCS_TYPE_STRING;
#else
            *(int*)buf = NGCS_TYPE_BLOB;
#endif
            *(int*)(buf+sizeof(int)) = @n@;
            memcpy(buf+2*sizeof(int),s->@m[1]@,@n@);
            buf += 2*sizeof(int)+@n@;
        }
        len += 2*sizeof(int)+@n@;
    }
    else
    {
        if(buf)
        {
            *(int*)buf = NGCS_TYPE_NULL; 
            *(int*)(buf+sizeof(int)) = 0;
            buf += 2*sizeof(int);
        }
        len += 2*sizeof(int);
    }
#elif m[0] == "int" and len(m) == 2
    if(buf)
    {
        *(int*)buf = NGCS_TYPE_INT;
        *(int*)(buf+sizeof(int)) = sizeof(int);
        *(int*)(buf+2*sizeof(int)) = s->@m[1]@;
        buf += 3*sizeof(int);
    }
    len += 3*sizeof(int);
#elif m[0] == "long" and len(m) == 2
    if(buf)
    {
        *(int*)buf = NGCS_TYPE_LONG;
        *(int*)(buf+sizeof(int)) = sizeof(long);
        *(long*)(buf+2*sizeof(int)) = s->@m[1]@;
        buf += 2*sizeof(int)+sizeof(long);
    }
    len += 2*sizeof(int)+sizeof(long);
#elif m[0] in ("int","long")
#exec  raise Exception, "Bad structure specifier"
#elif m[0] in ("ignore","ignore*")
#else
#if m[0][-1] == "*"
#exec isptr = True; sp = "    "
#exec typename = m[0][:-1]; need_ret = True
#else
#exec isptr = False; sp = ""
#exec typename = m[0]; need_ret = True
#endif
#if isptr
    if(s->@m[1]@)
    {
#endif
    @sp@if(buf) *(int*)buf = NGCS_TYPE_STRUCT;
#if isptr
    @sp@n = ngcs_marshal_@typename@(s->@m[1]@, (buf?buf+2*sizeof(int):NULL));
#else
    @sp@n = ngcs_marshal_@typename@(&s->@m[1]@, (buf?buf+2*sizeof(int):NULL));
#endif
    @sp@if(n < 0) return n;
    @sp@if(buf)
    @sp@{
    @sp@    *(int*)(buf+sizeof(int)) = n;
    @sp@    buf += 2*sizeof(int) + n;
    @sp@}
    @sp@len += 2*sizeof(int) + n;
#if isptr
    }
    else if(buf)
    {
        *(int*)buf = NGCS_TYPE_NULL; 
        *(int*)(buf+sizeof(int)) = 0;
        buf += 2*sizeof(int);
        len += 2*sizeof(int);
    }
    else len += 2*sizeof(int);
#endif
#endif
""", l)
    process_str += "    return len;\n"
    return (decstr, initstr+"\n"+process_str+"}\n\n")

# FIXME: new and untested
def mkunmarshaler(name, members,sname=None):
    l = { 'need_ret': False, 'cnt': 0, 'at': lambda(a): "@"+a+"@",
          'free': "" }
    if sname == None:
        sname = name
    decstr = ("int ngcs_unmarshal_" + name + "(" + sname +
               "* s, int len, char* data)")
    initstr = decstr+ "\n{\nngcs_data* dat; int acnt;"
    decstr += ";\n"
    process_str = ""
    up_str = """
    acnt = ngcs_unpack(data, len, &dat);
    if(acnt < 0) return -1;
    if(acnt != @cnt@)
    {
        ngcs_free_unpack(acnt, dat); return -1;
    }
"""
    for m in members:
        l['m'] = m
        process_str += process("""
#if m[0] 
#if m[0] in ("string","int","long","blob")
#if m[0] == "string" and len(m) == 2
    if(dat[@cnt@].type == NGCS_TYPE_NULL)
        s->@m[1]@=0;
    else if(dat[@cnt@].type == NGCS_TYPE_STRING)
    {
        s->@m[1]@=strdup(dat[@cnt@].d.s);
        if(s->@m[1]@ == NULL)
        {
            @free@
            ngcs_free_unpack(acnt, dat); return -1;
        }
    }
    else
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
#exec free += "if(s->"+m[1]+") free((void*)s->"+m[1]+"); "
#elif (m[0] in ("string","blob")) and len(m) == 3
    if(dat[@cnt@].type == NGCS_TYPE_NULL)
    {
        s->@m[1]@ = 0; s->@m[2]@ = 0;
    }
#if m[0] == "blob"
    else if(dat[@cnt@].type == NGCS_TYPE_BLOB)
    {
        s->@m[1]@ = malloc(dat[@cnt@].size)
        if(s->@m[1]@ == NULL)
        {
            @free@
            ngcs_free_unpack(acnt, dat); return -1;
        }
        memcpy(s->@m[1]@, dat[@cnt@].d.p, dat[@cnt@].size);
#else
    else if(dat[@cnt@].type == NGCS_TYPE_STRING)
    {
        s->@m[1]@ = malloc(dat[@cnt@].size + 1)
        if(s->@m[1]@ == NULL)
        {
            @free@
            ngcs_free_unpack(acnt, dat); return -1;
        }
        memcpy(s->@m[1]@, dat[@cnt@].d.s, dat[@cnt@].size + 1);
#endif
#exec free += "if(s->"+m[1]+") free((void*)s->"+m[1]+"); "
        s->@m[2]@ = dat[@cnt@].size;
    }
    else
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
#elif m[0] == "int" and len(m) == 2
    if(dat[@cnt@].type == NGCS_TYPE_INT)
        s->@m[1]@=dat[@cnt@].d.i;
    else
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
#elif m[0] == "long" and len(m) == 2
    if(dat[@cnt@].type == NGCS_TYPE_LONG)
        s->@m[1]@=dat[@cnt@].d.l;
    else
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
#else
#exec raise Exception, "Bad structure specifier"
#endif 
#exec cnt += 1
#elif m[0] == "ignore"
#elif m[0] == "ignore*"
    s->@m[1]@=0;
#elif m[0][-1] == "*"
#exec need_ret = True; typename = m[0][:-1]
    if(dat[@cnt@].type == NGCS_TYPE_NULL)
    {
        s->@m[1]@ = NULL;
    }
    else if(dat[@cnt@].type == NGCS_TYPE_STRUCT)
    {
        s->@m[1]@ = malloc(sizeof(@typename@));
        if(s->@m[1]@ == NULL)
        {
            @free@
            ngcs_free_unpack(acnt, dat); return -1;            
        }
        ret = ngcs_unmarshal_@typename@(s->@m[1]@, dat[@cnt@].len,  dat[@cnt@].d.p);
        if(ret)
        {
            @free@ free(s->@m[1]@);
            ngcs_free_unpack(acnt, dat); return -1;
        }
        
    }
    else
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
#exec free += "if(s->"+m[1]+") {  ngcs_free_" + typename + "(s->"+m[1]+"); free(s->"+m[1]+"); } "
#exec cnt += 1
#else
#exec need_ret = True; typename = m[0]
    if(dat[@cnt@].type != NGCS_TYPE_STRUCT)
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
    ret = ngcs_unmarshal_@typename@(&s->@m[1]@, dat[@cnt@].len,  dat[@cnt@].d.p);
    if(ret)
    {
        @free@
        ngcs_free_unpack(acnt, dat); return -1;
    }
#exec free += "ngcs_free_" + typename + "(&s->"+m[1]+"); "
#exec cnt += 1
#endif
""", l)
    process_str += "   ngcs_free_unpack(acnt, dat);\n    return 0;\n"
    if l['need_ret']:
        initstr += "    int ret;\n"

    decstr += "void ngcs_free_" + name + "(" + sname + "* s);\n"
    freestr = ("void ngcs_free_" + name + "(" + sname + "* s)\n{\n    " +
               l['free']+"\n}\n\n")
    
    return (decstr, initstr+"\n"+ process(up_str,l)+process_str+"}\n\n"+
            freestr)

_head_re = re.compile(r"^\s*#header-code\s*$")
_head_end_re = re.compile(r"^\s*#end-header-code\s*$")

#FIXME - won't work due to greedy matching
_marshal_re = re.compile(r"^\s*#marshal(?:\(([^)]*)\))?\s+(.*?)\s*$")
_marshal_sp_re = re.compile(r"^(.*)\s+(.*)$")
_marshal_end_re = re.compile(r"^\s*#end-marshal\s*$")
_comment_re = re.compile(r"^\s*(?:#|//).*$")

_cmd_re = re.compile(r"^\s*#(if|elif|else|endif|exec)(?:\s+(\S.*?))?\s*$")
def process(s, l):
    prevs = ""
    stk = [ ] # list of [ active process_else ] 
    while s != prevs:
        prevs = s; outs = ""
        while s != "":
            at_p = s.find('@')
            nl_p = s.find('\n')
            if at_p >= 0 and ( nl_p < 0 or at_p < nl_p):
                if len(stk) == 0 or stk[-1][0]:
                    outs += s[:at_p]
                at2_p = s.find('@',at_p+1)
                if at2_p < 0:
                    raise ValueError, "Unmatched @"
                exp = s[at_p+1:at2_p]; s = s[at2_p+1:]
                if exp == "":
                    if len(stk) == 0 or stk[-1][0]:
                        outs += "@@"
                    continue
                if len(stk) == 0 or stk[-1][0]:
                    outs += str(eval(exp, globals() ,l))
            elif nl_p >= 0:
                # FIXME: doesn't work at start of string
                if len(stk) == 0 or stk[-1][0]:
                    outs += s[:nl_p+1];
                s = s[nl_p+1:]
                while True:
                    sp = s.split("\n",1)
                    if len(sp) < 2:
                        line = s; rest = ""
                    elif len(sp) == 2:
                        line = sp[0]; rest = sp[1]
                    else:
                        raise Exception
                    m = _cmd_re.match(line)
                    if m == None:
                        break
                    s = rest
                    cmd = m.group(1); exp = m.group(2)
                    if cmd == "if":
                        if len(stk) == 0 or stk[-1][0]:
                            res = eval(exp, globals() ,l)
                            stk.append([bool(res), not bool(res)])
                        else:
                            stk.append([False,False])
                    elif cmd == "elif":
                        if stk[-1][1]:
                            res = eval(exp, globals() ,l)
                            stk[-1] = [bool(res), not bool(res)]
                        else:
                            stk[-1] = [False,False]
                    elif cmd == "else":
                        stk[-1] = [stk[-1][1], False]
                    elif cmd == "endif":
                        stk.pop()
                    elif cmd == "exec":
                        if exp == None:
                            raise Exception, "No multi-line #exec yet"
                        else:
                            if len(stk) == 0 or stk[-1][0]:
                                exec exp in globals(), l
                    else:
                        raise ValueError, "Unknown cmd %s" % cmd
            else:
                outs += s; s = ""
        s = outs
    return outs.replace("@@","@")

_ofn_re = re.compile(r"^(.*)\.[ch]")

def build(fname, outfname=None):
    infile = file(fname+".ngci","r")
    if outfname == None:
        outfname = fname
    else:
        m = _ofn_re.match(outfname)
        if m != None:
            outfname = m.group(1)
    hfile = file(outfname+".h","w")
    cfile = file(outfname+".c","w")    
    hfile.write("/* Automagically generated - do not modify */\n\n")
    hfile.write('#include "ngcs_common.h"\n')
    cfile.write("/* Automagically generated - do not modify */\n\n")
    # IIRC, since the .c and the .h file are in the same directory,
    # we don't need to provide a path below
    cfile.write('#include "' + path.split(outfname)[1] + ".h" + '"\n')
    cfile.write('#include <string.h>\n#include <stdlib.h>\n\n')
    state = None; data = [ ]
    for l in infile.readlines():
        if state == None:
            m = _marshal_re.search(l)
            if m != None:
                flags = m.group(1)
                if flags == None:
                    flags = ( )
                else:
                    flags = flags.split(",")
                funcname = m.group(2)
                m = _marshal_sp_re.match(funcname)
                if m:
                    funcname = m.group(2)
                    sname = m.group(1)
                else:
                    sname = funcname
                state = "marshal"; data = [ ]
            elif _head_re.search(l):
                state = "head"
            else:
                cfile.write(l)
        elif state == "head":
            if _head_end_re.search(l):
                state = None 
            else:
                hfile.write(l)
        elif state == "marshal":
            if _marshal_end_re.search(l):
                if not ("nomarshaler" in flags):
                    (decl,defn) = mkmarshaler(funcname,data,sname)
                    hfile.write(decl); cfile.write(defn);                
                if not ("nounmarshaler" in flags):
                    (decl,defn) = mkunmarshaler(funcname,data,sname)
                    hfile.write(decl); cfile.write(defn);                
                state = None 
            elif _comment_re.search(l):
                pass
            else:
                tmp = l.split()
                if len(tmp) > 0:
                    data.append(tmp)
    cfile.close(); hfile.close(); infile.close()
    
if __name__=="__main__":
    if len(sys.argv) >= 3:
        build(sys.argv[1], sys.argv[2])
    else:
        build(sys.argv[1])
