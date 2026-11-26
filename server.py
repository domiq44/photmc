#!/usr/bin/env python3
"""
server.py - Serveur web local (stdlib uniquement) pour le simulateur photonmc.

- Sert index.html, style.css, app.js et les fichiers .dat de out/.
- Expose POST /run : recoit les parametres physiques en JSON, execute le
  binaire ./photonmc, regenere les .dat dans out/, et renvoie la liste des
  fichiers produits. Le navigateur redessine alors les graphiques sans
  rechargement de page.

Lancement :
    python3 web/server.py [--bin ./photonmc] [--port 8000] [--outdir out]
Puis ouvrir http://localhost:8000/
"""
import argparse
import json
import os
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Etat global du serveur (configure dans main()).
CFG = {
    "bin": os.path.join(ROOT, "photonmc"),
    "outdir": os.path.join(ROOT, "out"),
}


def build_cmd(params):
    """Construit la ligne de commande photonmc a partir des parametres JSON."""
    p = params
    shape = p.get("shape", "sphere")
    outdir = CFG["outdir"]
    os.makedirs(outdir, exist_ok=True)

    base = [
        CFG["bin"],
        "--shape", str(shape),
        "--size", str(p.get("size", 1.0)),
        "--height", str(p.get("height", 2.0)),
        "--tau", str(p.get("tau", 1.0)),
        "--theta", str(p.get("theta", 0.0)),
        "--absorb", str(p.get("absorb", 0.0)),
        "--g1", str(p.get("g1", 0.0)),
        "--g2", str(p.get("g2", 0.0)),
        "--f", str(p.get("f", 0.5)),
        "--nphotons", str(int(p.get("nphotons", 200000))),
        "--bins", str(int(p.get("bins", 180))),
        "--seed", str(int(p.get("seed", 12345))),
        "--maxsteps", str(int(p.get("maxsteps", 100000))),
        "--outdir", outdir,
    ]

    # Les balayages font N points de simulation : on reduit le nombre de photons
    # pour eviter un temps d'execution trop long (risque de timeout).
    scan_nphotons = max(20000, int(p.get("nphotons", 200000)) // 4)

    # Les balayages sont optionnels (gain de temps sur un run simple).
    run_scans = p.get("run_scans") not in (None, "", "false", "0", "False")

    # Deux balayages (theta et tau) + un run simple pour la distribution angulaire.
    commands = []

    if run_scans:
        # 1) flux vs theta (cube par defaut pour montrer la dependance angulaire)
        scan_shape = p.get("scan_shape", "cube")
        scan_cmd = list(base)
        scan_cmd[base.index("--nphotons") + 1] = str(scan_nphotons)
        commands.append(scan_cmd + [
            "--scan", "theta",
            "--from", str(p.get("theta_from", 0.0)),
            "--to", str(p.get("theta_to", 80.0)),
            "--scan-steps", str(int(p.get("scan_steps", 30))),
        ])

        # 2) flux vs tau (sphere)
        scan_cmd = list(base)
        scan_cmd[base.index("--nphotons") + 1] = str(scan_nphotons)
        commands.append(scan_cmd[:2] + ["sphere"] + scan_cmd[3:] + [
            "--scan", "tau",
            "--from", str(p.get("tau_from", 0.1)),
            "--to", str(p.get("tau_to", 20.0)),
            "--scan-steps", str(int(p.get("scan_steps", 40))),
        ])

    # 3) distribution angulaire (forme/tau/theta choisis)
    commands.append(base)

    return commands


def run_simulation(params):
    """Execute photonmc pour chaque commande et retourne les fichiers produits."""
    produced = []
    for cmd in build_cmd(params):
        try:
            proc = subprocess.run(
                cmd, cwd=ROOT, capture_output=True, text=True, timeout=600
            )
        except FileNotFoundError:
            return {"ok": False, "error": "Binaire introuvable: %s" % CFG["bin"]}
        except subprocess.TimeoutExpired:
            return {"ok": False, "error": "Simulation trop longue (timeout)."}
        if proc.returncode != 0:
            return {"ok": False, "error": proc.stderr.strip() or "echec execution"}

    for name in ("flux_vs_theta.dat", "flux_vs_tau.dat", "angular_distribution.dat",
                 "hist_collisions.dat", "profondeur.dat"):
        path = os.path.join(CFG["outdir"], name)
        if os.path.exists(path):
            produced.append("out/" + name)
    return {"ok": True, "files": produced}


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="application/json", extra=None):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        if extra:
            for k, v in extra.items():
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path in ("/", "/index.html"):
            self._serve_file(os.path.join(HERE, "index.html"), "text/html; charset=utf-8")
        elif path == "/style.css":
            self._serve_file(os.path.join(HERE, "style.css"), "text/css; charset=utf-8")
        elif path == "/app.js":
            self._serve_file(os.path.join(HERE, "app.js"), "application/javascript; charset=utf-8")
        elif path == "/chart.js":
            self._serve_file(os.path.join(HERE, "chart.js"), "application/javascript; charset=utf-8")
        elif path == "/config":
            self._serve_file(os.path.join(ROOT, "config", "presets.json"),
                             "application/json; charset=utf-8")
        elif path.startswith("/out/"):
            name = os.path.basename(path)
            self._serve_file(os.path.join(CFG["outdir"], name), "text/plain; charset=utf-8")
        else:
            self._send(404, "Not found", "text/plain")

    def _serve_file(self, filepath, ctype):
        if not os.path.exists(filepath):
            self._send(404, "Fichier absent: %s" % os.path.basename(filepath), "text/plain")
            return
        with open(filepath, "rb") as f:
            self._send(200, f.read(), ctype)

    def do_POST(self):
        if self.path.split("?", 1)[0] != "/run":
            self._send(404, "Not found", "text/plain")
            return
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        try:
            params = json.loads(raw.decode("utf-8")) if raw else {}
        except json.JSONDecodeError as e:
            self._send(400, json.dumps({"ok": False, "error": "JSON invalide: %s" % e}))
            return
        result = run_simulation(params)
        self._send(200, json.dumps(result))

    def log_message(self, fmt, *args):
        sys.stderr.write("[server] %s\n" % (fmt % args))


def main():
    ap = argparse.ArgumentParser(description="Serveur local pour photonmc")
    ap.add_argument("--bin", default=CFG["bin"], help="chemin du binaire photonmc")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--outdir", default=CFG["outdir"], help="dossier de sortie")
    args = ap.parse_args()

    CFG["bin"] = args.bin if os.path.isabs(args.bin) else os.path.join(ROOT, args.bin)
    CFG["outdir"] = args.outdir if os.path.isabs(args.outdir) else os.path.join(ROOT, args.outdir)

    if not os.path.exists(CFG["bin"]):
        print("ATTENTION: binaire introuvable (%s). Compilez avec 'make'." % CFG["bin"])

    httpd = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print("Serveur demarre sur http://localhost:%d/  (Ctrl+C pour arreter)" % args.port)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nArret du serveur.")


if __name__ == "__main__":
    main()