/*
 * app.js - Logique de l'interface WebAssembly : execution du simulateur C
 * directement dans le navigateur (pas de serveur requis).
 *
 * Charge le module PhotonMCModule (web/photonmc.js + web/photonmc.wasm)
 * et appelle em_run_simulation depuis le code C compile en WASM.
 */
"use strict";

var COLORS = { ref: "#ff6b6b", trans: "#4dd4ac", abs: "#ffd166", ang: "#4fa3ff" };

var charts = {};
var Module = null;      // PhotonMCModule (charge par PhotonMCModule())
var wasmReady = false;
var em_run_simulation = null;
var em_free = null;
var appMode = null;     // 'wasm' | 'server'
var scanWorker = null;  // Web Worker pour balayages paralleles
var useWorkers = false; // Active si Worker + WASM disponibles

/* ------------------------------------------------------------------ */
/* Helpers mode serveur (API HTTP du serveur Python local)            */
/* ------------------------------------------------------------------ */

function fetchText(url) {
  return fetch(url).then(function (r) {
    if (!r.ok) throw new Error("HTTP " + r.status + " sur " + url);
    return r.text();
  });
}

/* Parse un fichier .dat : ignore lignes vides et commentaires #. */
function parseDat(text) {
  var rows = [];
  var lines = text.split("\n");
  for (var i = 0; i < lines.length; i++) {
    var line = lines[i].trim();
    if (!line || line.charAt(0) === "#") continue;
    var cols = line.split(/\s+/);
    rows.push(cols.map(Number));
  }
  return rows;
}

/* Charge les presets depuis l'API serveur (mode local) ou depuis le
 * fichier statique (mode WASM / GitHub Pages). */
function loadPresets() {
  var url = (appMode === 'server') ? "/config" : "config/presets.json?t=" + Date.now();
  fetch(url)
    .then(function (r) { return r.json(); })
    .then(function (data) {
      var sel = document.getElementById("preset-select");
      (data.presets || []).forEach(function (p, i) {
        var opt = document.createElement("option");
        opt.value = String(i);
        opt.textContent = p.name || ("Preset " + i);
        sel.appendChild(opt);
      });
      sel.addEventListener("change", function () {
        var idx = parseInt(sel.value, 10);
        if (isNaN(idx)) return;
        var p = data.presets[idx];
        var form = document.getElementById("params-form");
        Object.keys(p).forEach(function (k) {
          var el = form.elements[k];
          if (el) el.value = p[k];
        });
      });
    })
    .catch(function () { /* pas de presets : mode personnalise */ });
}

/* Attache le gestionnaire de soumission du formulaire (commun aux deux modes). */
function attachFormHandler() {
  document.getElementById("params-form").addEventListener("submit", function (e) {
    e.preventDefault();
    var params = collectParams();
    if (params === null) return;
    var btn = document.getElementById("run-btn");
    btn.disabled = true;
    setStatus("Simulation en cours…", "");
    loadAll(params).catch(function (err) {
      btn.disabled = false;
      setStatus("Echec : " + err.message, "err");
    });
  });
}

/* ------------------------------------------------------------------ */
/* Helpers communs pour le rendu des graphiques                       */
/* ------------------------------------------------------------------ */

/* Construit les series R/T/A a partir de lignes de scan */
function series3(rows, colorMap) {
  return [
    { x: rows.map(function (r) { return r.x; }), y: rows.map(function (r) { return r.R; }), color: colorMap.ref, label: "Reflechi" },
    { x: rows.map(function (r) { return r.x; }), y: rows.map(function (r) { return r.T; }), color: colorMap.trans, label: "Transmis" },
    { x: rows.map(function (r) { return r.x; }), y: rows.map(function (r) { return r.A; }), color: colorMap.abs, label: "Absorbe" }
  ];
}

/* Construit les series pour la fonction de phase HG analytique */
function buildPhaseSeries(g1, g2, f) {
  var xs = [], y1 = [], y2 = [], ymix = [];
  for (var k = 0; k <= 200; k++) {
    var mu = -1.0 + 2.0 * k / 200.0;
    var p1 = hgValue(mu, g1);
    var p2 = hgValue(mu, g2);
    xs.push(mu);
    y1.push(p1);
    y2.push(p2);
    ymix.push(f * p1 + (1.0 - f) * p2);
  }
  return [
    { x: xs, y: y1, color: COLORS.ref, label: "p_HG (g1=" + g1 + ")" },
    { x: xs, y: y2, color: COLORS.trans, label: "p_HG (g2=" + g2 + ")" },
    { x: xs, y: ymix, color: COLORS.abs, label: "melange f=" + f }
  ];
}

/* HG analytique */
function hgValue(mu, g) {
  var g2 = g * g;
  var denom = 1.0 + g2 - 2.0 * g * mu;
  if (denom <= 0) return 0;
  return (1.0 - g2) / (4.0 * Math.PI * Math.pow(denom, 1.5));
}

/* Rayon englobant pour l'histogramme de profondeur */
function boundRadius(shape, size, height) {
  if (shape === 'sphere') return size;
  if (shape === 'cube') return size * Math.sqrt(3);
  if (shape === 'cylinder') return Math.sqrt(size * size + (0.5 * height) * (0.5 * height));
  return size;
}

/* Point d'entree unique pour le rendu des 12 graphiques.
 * Accepte soit des donnees brutes (mode serveur: textes .dat parsés)
 * soit un objet resultat WASM (mode wasm: simple, thetaScan, tauScan). */
function renderCharts(data, mode) {
  var form = document.getElementById("params-form");
  var g1 = parseFloat(form.elements["g1"].value) || 0;
  var g2 = parseFloat(form.elements["g2"].value) || 0;
  var fv = parseFloat(form.elements["f"].value) || 0.5;
  var params = data.params || collectParams();

  clearCharts();

  // Extraire les donnees selon le mode
  var thetaScan, tauScan, simple, angRows, collRows, depthRows;
  var ballisticPct = 0;
  var excludeBallistic = false;

  if (mode === 'server') {
    thetaScan = data.thetaScan;
    tauScan = data.tauScan;
    angRows = data.angRows;
    collRows = data.collRows;
    depthRows = data.depthRows;
  } else {
    simple = data.simple;
    thetaScan = data.thetaScan || [];
    tauScan = data.tauScan || [];
    // Reconstituer angRows depuis simple.hist_weight
    angRows = simple.hist_weight.map(function (w, i) {
      var angle = ((i + 0.5) * (Math.PI / simple.nbins) * 180.0 / Math.PI);
      var binWidth = Math.PI / simple.nbins;
      var dens = w / binWidth;
      return [angle, w, dens, simple.hist_count[i]];
    });
    collRows = simple.hist_collisions.map(function (c, i) { return [i, c]; });
    depthRows = simple.hist_depth.map(function (d, i) {
      var ndepth = simple.ndepth;
      var br = boundRadius(params.shape, parseFloat(params.size) || 1.0, parseFloat(params.height) || 2.0);
      var depthMin = -br, depthMax = br;
      var depth = depthMin + (i + 0.5) * (depthMax - depthMin) / ndepth;
      return [depth, d];
    });

    // Calculer % photons balistiques (1er bin = angle ~0)
    if (simple.hist_count && simple.hist_count.length > 0 && simple.photons_in > 0) {
      ballisticPct = (simple.hist_count[0] / simple.photons_in) * 100;
    }
    // Option exclure balistiques (depuis formulaire)
    excludeBallistic = params.exclude_ballistic === "on" || params.exclude_ballistic === "true" || params.exclude_ballistic === true;
    if (excludeBallistic && angRows.length > 1) {
      angRows = angRows.slice(1); // retire le 1er bin
    }
  }

  // 1 & 8. Flux vs theta + bilan
  charts.c1.setData(series3(thetaScan, COLORS), {
    title: "Flux vs angle d'incidence theta",
    xlabel: "theta (deg)", ylabel: "flux (fraction)"
  });
  charts.c8.setData(series3(thetaScan, COLORS), {
    title: "Bilan R+T+A vs theta",
    xlabel: "theta (deg)", ylabel: "fraction (somme = 1)"
  });

  // 2 & 9. Flux vs tau + bilan
  charts.c2.setData(series3(tauScan, COLORS), {
    title: "Flux vs epaisseur optique tau",
    xlabel: "tau", ylabel: "flux (fraction)"
  });
  charts.c9.setData(series3(tauScan, COLORS), {
    title: "Bilan R+T+A vs tau",
    xlabel: "tau", ylabel: "fraction (somme = 1)"
  });

  // 10. Rapport R/T et T/R vs tau (log)
  var rtX = [], rtR = [], rtT = [];
  tauScan.forEach(function (r) {
    var R = r.R, T = r.T;
    rtX.push(r.x);
    rtR.push(T === 0 ? 0 : R / T);
    rtT.push(R === 0 ? 0 : T / R);
  });
  charts.c10.setData([
    { x: rtX, y: rtR, color: COLORS.ref, label: "R/T" },
    { x: rtX, y: rtT, color: COLORS.trans, label: "T/R" }
  ], {
    title: "Rapport R/T et T/R vs tau",
    xlabel: "tau", ylabel: "rapport (log)",
    logY: true
  });

  // 3. Distribution angulaire (densite par radian)
  var angSeries = [{
    x: angRows.map(function (r) { return r[0]; }),
    y: angRows.map(function (r) { return r[2]; }),
    color: COLORS.ang, label: "densite", bar: true
  }];
  var annot = ballisticPct > 0 ? "Photons balistiques (non diffusés) : " + ballisticPct.toFixed(1) + "%" : "";
  charts.c3.setData(angSeries, {
    title: "Distribution angulaire de sortie",
    xlabel: "angle de diffusion global (deg)",
    ylabel: "densite (par radian)",
    bar: true,
    annotation: annot
  });

  // 4. Fonction de phase double HG (analytique)
  charts.c4.setData(buildPhaseSeries(g1, g2, fv), {
    title: "Fonction de phase double HG",
    xlabel: "cos(theta)", ylabel: "p(cos(theta))"
  });

  // 5. Distribution angulaire (echelle log)
  charts.c5.setData(angSeries, {
    title: "Distribution angulaire (echelle log)",
    xlabel: "angle (deg)", ylabel: "densite",
    bar: true, logY: true,
    annotation: annot
  });

  // 6. Densite par steradian
  var angSr = [{
    x: angRows.map(function (r) { return r[0]; }),
    y: angRows.map(function (r) {
      var thetaRad = r[0] * Math.PI / 180.0;
      var dens = r[2];
      var sinT = Math.sin(thetaRad);
      return sinT > 1e-6 ? dens / (2 * Math.PI * sinT) : 0;
    }),
    color: COLORS.ang, label: "par sr", bar: true
  }];
  charts.c6.setData(angSr, {
    title: "Densite par steradian",
    xlabel: "angle (deg)", ylabel: "densite / sr",
    bar: true,
    annotation: annot
  });

  // 7. Densite en cos(theta)
  var angCos = [{
    x: angRows.map(function (r) { return Math.cos(r[0] * Math.PI / 180.0); }),
    y: angRows.map(function (r) {
      var thetaRad = r[0] * Math.PI / 180.0;
      var dens = r[2];
      var sinT = Math.sin(thetaRad);
      return sinT > 1e-6 ? dens / sinT : 0;
    }),
    color: COLORS.ang, label: "p(cos theta)"
  }];
  charts.c7.setData(angCos, {
    title: "Densite en cos(theta)",
    xlabel: "cos(theta)", ylabel: "p(cos(theta))",
    annotation: annot
  });

  // 11. Histogramme collisions
  var collSeries = [{
    x: collRows.map(function (r) { return r[0]; }),
    y: collRows.map(function (r) { return r[1]; }),
    color: COLORS.ang, label: "nb photons", bar: true
  }];
  charts.c11.setData(collSeries, {
    title: "Histogramme du nombre de collisions",
    xlabel: "nombre de collisions", ylabel: "nombre de photons",
    bar: true
  });

  // 12. Profondeur de sortie
  var depthSeries = [{
    x: depthRows.map(function (r) { return r[0]; }),
    y: depthRows.map(function (r) { return r[1]; }),
    color: COLORS.ang, label: "poids", bar: true
  }];
  charts.c12.setData(depthSeries, {
    title: "Profondeur de sortie",
    xlabel: "profondeur (proj. axe)", ylabel: "poids",
    bar: true
  });

  var modeLabel = mode === 'server' ? "serveur local" : "WASM";
  setStatus("Termine — graphiques mis a jour (" + modeLabel + ").", "ok");
  document.getElementById("run-btn").disabled = false;
}

/* ------------------------------------------------------------------ */
/* Mode serveur : lance la simulation via POST /run puis lit les .dat */
/* ------------------------------------------------------------------ */

function runServerLoad(params) {
  if (params === null) return;

  fetch("/run", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(params)
  })
    .then(function (r) { return r.json(); })
    .then(function (result) {
      if (!result.ok) {
        setStatus("Echec : " + (result.error || "erreur inconnue"), "err");
        document.getElementById("run-btn").disabled = false;
        return;
      }
      return Promise.all([
        fetchText("/out/flux_vs_theta.dat"),
        fetchText("/out/flux_vs_tau.dat"),
        fetchText("/out/angular_distribution.dat"),
        fetchText("/out/hist_collisions.dat"),
        fetchText("/out/profondeur.dat")
      ]);
    })
    .then(function (texts) {
      var thetaRows = parseDat(texts[0]);
      var tauRows = parseDat(texts[1]);
      var angRows = parseDat(texts[2]);
      var collRows = parseDat(texts[3]);
      var depthRows = parseDat(texts[4]);

      var thetaScan = thetaRows.map(function (r) { return { x: r[0], R: r[1], T: r[2], A: r[3] }; });
      var tauScan = tauRows.map(function (r) { return { x: r[0], R: r[1], T: r[2], A: r[3] }; });

      renderCharts({
        params: params,
        thetaScan: thetaScan,
        tauScan: tauScan,
        angRows: angRows,
        collRows: collRows,
        depthRows: depthRows
      }, 'server');
    })
    .catch(function (err) {
      setStatus("Echec : " + err.message, "err");
      document.getElementById("run-btn").disabled = false;
    });
}

/* ------------------------------------------------------------------ */
/* Mode WASM : execution directe dans le navigateur                   */
/* ------------------------------------------------------------------ */

function runWasmSimulation(params) {
  var shapeMap = { sphere: 0, cube: 1, cylinder: 2 };
  var shape = shapeMap[params.shape] || 0;

  var outSizePtr = Module._malloc(4);
  var resultPtr = em_run_simulation(
    shape,
    parseFloat(params.size),
    parseFloat(params.height),
    parseFloat(params.tau),
    parseFloat(params.theta),
    parseFloat(params.absorb),
    parseFloat(params.g1),
    parseFloat(params.g2),
    parseFloat(params.f),
    parseInt(params.nphotons),
    parseInt(params.bins),
    parseInt(params.seed),
    (function () { var ms = parseInt(params.maxsteps); return isNaN(ms) ? 100000 : ms; })(),
    outSizePtr
  );
  var outSize = Module.getValue(outSizePtr, 'i32');
  Module._free(outSizePtr);

  if (!resultPtr || outSize === 0) {
    throw new Error("Echec de la simulation WASM");
  }

  var res = readSimResult(resultPtr, outSize);
  em_free(resultPtr);
  return res;
}

function runWasmScan(scanType, from, to, steps, baseParams) {
  var results = [];
  for (var i = 0; i < steps; i++) {
    var x = steps === 1 ? from : from + (to - from) * i / (steps - 1);
    var p = {};
    for (var k in baseParams) p[k] = baseParams[k];
    if (scanType === 'theta') p.theta = String(x);
    else p.tau = String(x);
    var res = runWasmSimulation(p);
    results.push({ x: x, R: res.reflected, T: res.transmitted, A: res.absorbed });
  }
  return results;
}

function readSimResult(ptr, outSize) {
  var res = {};
  res.reflected = Module.getValue(ptr + 0 * 8, 'double');
  res.transmitted = Module.getValue(ptr + 1 * 8, 'double');
  res.absorbed = Module.getValue(ptr + 2 * 8, 'double');
  res.out_weight = Module.getValue(ptr + 3 * 8, 'double');
  res.photons_in = Module.getValue(ptr + 4 * 8, 'double');
  res.mean_collisions = Module.getValue(ptr + 5 * 8, 'double');
  res.mean_depth = Module.getValue(ptr + 6 * 8, 'double');
  res.nbins = Module.getValue(ptr + 7 * 8, 'double');
  res.max_collisions = Module.getValue(ptr + 8 * 8, 'double');
  res.ndepth = Module.getValue(ptr + 9 * 8, 'double');

  var idx = 10;
  res.hist_weight = [];
  res.hist_count = [];
  for (var i = 0; i < res.nbins; i++) {
    res.hist_weight.push(Module.getValue(ptr + (idx++) * 8, 'double'));
    res.hist_count.push(Module.getValue(ptr + (idx++) * 8, 'double'));
  }
  res.hist_collisions = [];
  for (var i = 0; i < res.max_collisions; i++) {
    res.hist_collisions.push(Module.getValue(ptr + (idx++) * 8, 'double'));
  }
  res.hist_depth = [];
  for (var i = 0; i < res.ndepth; i++) {
    res.hist_depth.push(Module.getValue(ptr + (idx++) * 8, 'double'));
  }
  return res;
}

function initScanWorker() {
  if (typeof Worker === 'undefined') {
    console.log("Web Workers non supportes, balayages sequentiels");
    return;
  }
  try {
    scanWorker = new Worker('scan-worker.js');
    scanWorker.onmessage = function (e) {
      var msg = e.data;
      if (msg.type === 'workerReady') {
        useWorkers = true;
        console.log("Scan worker pret, balayages paralleles actives");
      }
    };
    scanWorker.onerror = function (err) {
      console.warn("Erreur scan worker:", err);
      useWorkers = false;
    };
    // Envoyer le module WASM au worker (il doit charger sa propre copie)
    scanWorker.postMessage({ type: 'init', wasmUrl: 'photonmc.wasm' });
  } catch (err) {
    console.warn("Impossible de creer le scan worker:", err);
    useWorkers = false;
  }
}

function runWasmScanParallel(scanType, from, to, steps, baseParams) {
  return new Promise(function (resolve, reject) {
    if (!useWorkers || !scanWorker) {
      resolve(runWasmScan(scanType, from, to, steps, baseParams));
      return;
    }

    var results = new Array(steps);
    var completed = 0;
    var hasError = false;
    var timeoutId = setTimeout(function () {
      if (completed < steps) {
        console.warn("Scan worker timeout, fallback to sequential");
        useWorkers = false;
        resolve(runWasmScan(scanType, from, to, steps, baseParams));
      }
    }, 30000);

    function checkComplete() {
      completed++;
      if (completed === steps) {
        clearTimeout(timeoutId);
        if (hasError) {
          reject(new Error("Erreur dans un worker de balayage"));
        } else {
          results.sort(function (a, b) { return a.index - b.index; });
          resolve(results.map(function (r) { return { x: r.x, R: r.R, T: r.T, A: r.A }; }));
        }
      }
    }

    scanWorker.onmessage = function (e) {
      var msg = e.data;
      if (msg.type === 'result') {
        results[msg.index] = msg;
        checkComplete();
      } else if (msg.type === 'error') {
        hasError = true;
        console.error("Worker error index " + msg.index + ":", msg.message);
        checkComplete();
      }
    };

    for (var i = 0; i < steps; i++) {
      var x = steps === 1 ? from : from + (to - from) * i / (steps - 1);
      var p = {};
      for (var k in baseParams) p[k] = baseParams[k];
      if (scanType === 'theta') p.theta = String(x);
      else p.tau = String(x);

      scanWorker.postMessage({
        type: 'simulate',
        params: p,
        scanType: scanType,
        index: i,
        from: from,
        to: to,
        steps: steps
      });
    }
  });
}

/* Point d'entree principal : declenche la simulation selon le mode */
async function loadAll(params) {
  if (appMode === 'server') {
    if (params === null || params === undefined) {
      params = collectParams();
      if (params === null) return Promise.reject(new Error("Parametres invalides"));
    }
    runServerLoad(params);
    return Promise.resolve();
  }

  if (!wasmReady) return Promise.reject(new Error("WASM non pret"));

  if (params === null || params === undefined) {
    params = collectParams();
    if (params === null) return Promise.reject(new Error("Parametres invalides"));
  }

  var form = document.getElementById("params-form");
  var thetaFrom = parseFloat(form.elements["theta_from"].value) || 0;
  var thetaTo = parseFloat(form.elements["theta_to"].value) || 80;
  var thetaSteps = parseInt(form.elements["scan_steps"].value) || 30;
  var tauFrom = parseFloat(form.elements["tau_from"].value) || 0.1;
  var tauTo = parseFloat(form.elements["tau_to"].value) || 20;
  var tauSteps = parseInt(form.elements["scan_steps"].value) || 40;

  var runScans = params.run_scans === "on" || params.run_scans === "true" || params.run_scans === true;

  clearCharts();

  // Yield to event loop to let UI update "Simulation en cours..."
  await new Promise(function (r) { setTimeout(r, 0); });

  var simple;
  try {
    simple = runWasmSimulation(params);
  } catch (err) {
    return Promise.reject(err);
  }

  var thetaScanPromise = runScans ? runWasmScanParallel('theta', thetaFrom, thetaTo, thetaSteps, params) : Promise.resolve([]);
  var tauScanPromise = runScans ? runWasmScanParallel('tau', tauFrom, tauTo, tauSteps, params) : Promise.resolve([]);

  return Promise.all([thetaScanPromise, tauScanPromise]).then(function (scans) {
    var thetaScan = scans[0];
    var tauScan = scans[1];

    renderCharts({
      params: params,
      simple: simple,
      thetaScan: thetaScan,
      tauScan: tauScan
    }, 'wasm');
  }).catch(function (err) {
    setStatus("Echec balayage : " + err.message, "err");
    document.getElementById("run-btn").disabled = false;
    throw err;
  });
}

function initCharts() {
  for (var i = 1; i <= 12; i++) {
    charts["c" + i] = new LineChart(document.getElementById("chart" + i));
  }

  var acts = {
    in: function (c) { c.zoom(0.7); },
    out: function (c) { c.zoom(1.4); },
    reset: function (c) { c.reset(); },
    collapse: function (c, btn) {
      var card = document.getElementById(c.canvas.id).closest('.chart-card');
      if (card) {
        var wasCollapsed = card.classList.contains('collapsed');
        card.classList.toggle('collapsed');
        btn.textContent = card.classList.contains('collapsed') ? '\u25B6' : '\u25BC';
        // If expanding, force re-render to handle proper sizing
        if (wasCollapsed && c._resize) {
          setTimeout(function () { c._resize(); c.draw(); }, 50);
        }
      }
    }
  };
  var map = {};
  for (var i = 1; i <= 12; i++) {
    map["chart" + i] = charts["c" + i];
  }
  document.querySelectorAll(".chart-controls").forEach(function (box) {
    var c = map[box.dataset.target];
    box.querySelectorAll("button").forEach(function (b) {
      b.addEventListener("click", function () {
        var act = b.dataset.act;
        if (act === 'collapse') {
          acts[act](c, b);
        } else {
          acts[act](c);
        }
      });
    });
  });

  // Resize all charts on window resize (with debounce)
  var resizeTimer = null;
  window.addEventListener("resize", function () {
    if (resizeTimer) clearTimeout(resizeTimer);
    resizeTimer = setTimeout(function () {
      for (var i = 1; i <= 12; i++) {
        if (charts["c" + i] && charts["c" + i]._resize) {
          charts["c" + i]._resize();
          charts["c" + i].draw();
        }
      }
    }, 150);
  });
}

function clearCharts() {
  for (var i = 1; i <= 12; i++) {
    charts["c" + i].setData([], { title: "", xlabel: "", ylabel: "", bar: false });
  }
}

function setStatus(msg, cls) {
  var s = document.getElementById("status");
  s.textContent = msg;
  s.className = "status" + (cls ? " " + cls : "");
}

function collectParams() {
  var f = document.getElementById("params-form");
  var out = {};
  var numericFields = [
    "size", "height", "tau", "theta", "absorb", "g1", "g2", "f",
    "nphotons", "bins", "seed", "theta_from", "theta_to",
    "tau_from", "tau_to", "scan_steps"
  ];
  new FormData(f).forEach(function (value, key) {
    out[key] = value;
  });
  for (var i = 0; i < numericFields.length; i++) {
    var k = numericFields[i];
    if (out[k] === undefined || out[k] === "") continue;
    var n = parseFloat(out[k]);
    if (isNaN(n)) {
      setStatus("Parametre invalide : " + k + " (doit etre un nombre)", "err");
      return null;
    }
    out[k] = String(n);
  }
  return out;
}

/* ------------------------------------------------------------------ */
/* Point d'entree : chargement du module WASM puis initialisation     */
/* ------------------------------------------------------------------ */

document.addEventListener("DOMContentLoaded", function () {
  var moduleStartTime = Date.now();

  function waitForModule() {
    if (typeof PhotonMCModule !== 'undefined') {
      PhotonMCModule().then(function (mod) {
        Module = mod;
        window.Module = Module;
        em_run_simulation = Module.cwrap('em_run_simulation', 'number', [
          'number', 'number', 'number', 'number', 'number',
          'number', 'number', 'number', 'number',
          'number', 'number', 'number', 'number', 'number'
        ]);
        em_free = Module.cwrap('em_free', null, ['number']);
        appMode = 'wasm';
        wasmReady = true;
        window.wasmReady = true;
        window.em_run_simulation = em_run_simulation;
        window.em_free = em_free;
        initScanWorker();
        initCharts();
        loadAll().catch(function (err) { console.error("Initial load failed:", err); });
        loadPresets();
        attachFormHandler();
      });
    } else if (Date.now() - moduleStartTime > 3000 || window._photonmcWasmMissing) {
      appMode = 'server';
      wasmReady = false;
      initCharts();
      loadAll().catch(function (err) { console.error("Initial load failed:", err); });
      loadPresets();
      attachFormHandler();
      setStatus("Mode serveur local (binaire C natif, sans WASM).", "ok");
    } else {
      setTimeout(waitForModule, 100);
    }
  }
  waitForModule();
});