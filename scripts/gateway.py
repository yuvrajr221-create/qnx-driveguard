import http.server, subprocess, os
H0 = os.path.dirname(os.path.abspath(__file__))
SSH = ["ssh","-p","2200","-i",os.path.expanduser("~/.ssh/id_ed25519"),
       "-o","StrictHostKeyChecking=accept-new","-o","ControlMaster=auto",
       "-o","ControlPath=/tmp/dg-cm-%C","-o","ControlPersist=120",
       "-o","ConnectTimeout=3","root@localhost"]
def qnx(cmd):
    try:
        return subprocess.run(SSH+[cmd],capture_output=True,text=True,timeout=6).stdout.strip()
    except Exception:
        return ""
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*a,**k):
        super().__init__(*a,directory=os.path.join(H0,"..","web"),**k)
    def log_message(self,*a): pass
    def reply(self,code,body):
        b=body.encode()
        self.send_response(code)
        self.send_header("Content-Type","application/json")
        self.send_header("Cache-Control","no-store")
        self.send_header("Content-Length",str(len(b)))
        self.end_headers()
        self.wfile.write(b)
    def do_GET(self):
        if self.path.startswith("/api/stats"):
            s=qnx("cat /tmp/stats.json")
            self.reply(200,s) if s.startswith("{") else self.reply(503,"{}")
        else:
            super().do_GET()
    def do_POST(self):
        if self.path=="/api/kill":
            qnx("slay dg_monitor")
            self.reply(200,'{"ok":1}')
        else:
            self.reply(404,"{}")
http.server.ThreadingHTTPServer(("0.0.0.0",8080),H).serve_forever()
