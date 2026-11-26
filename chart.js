/*
 * chart.js - Mini bibliothèque de tracé HTML5 canvas (sans dépendance).
 *
 * Fournit LineChart : trace une ou plusieurs courbes (x,y), gère un zoom
 * rectangulaire par clic-glissé et des boutons zoom +/-/reset.
 */
"use strict";

function LineChart(canvas, opts) {
  this.canvas = canvas;
  this.ctx = canvas.getContext("2d");
  this.opts = opts || {};
  this.series = [];        // [{x:[], y:[], color, label}]
  this.title = "";
  this.xlabel = "";
  this.ylabel = "";
  this.bar = false;        // mode histogramme (barres) ou courbe
  this.view = null;        // {xmin,xmax,ymin,ymax} courant (null = auto)
  this.logY = false;       // echelle logarithmique sur Y
  this._resize();
  this._initEvents();
  // ResizeObserver to auto-handle container size changes (mobile, collapse/expand)
  if (typeof ResizeObserver !== 'undefined') {
    this._ro = new ResizeObserver(this._resize.bind(this));
    this._ro.observe(canvas);
  }
}

LineChart.prototype._resize = function () {
  var c = this.canvas;
  var dpr = window.devicePixelRatio || 1;
  var rect = c.getBoundingClientRect();
  // CSS display size
  var cssW = rect.width;
  var cssH = rect.height;
  // Buffer size = CSS size * DPR
  var w = Math.max(1, Math.floor(cssW * dpr));
  var h = Math.max(1, Math.floor(cssH * dpr));
  // Skip if size hasn't changed significantly
  if (c.width === w && c.height === h) return false;
  // If size changed significantly (more than 20%), reset view to recompute from data bounds
  var significantChange = !c.width || !c.height ||
    Math.abs(c.width - w) / Math.max(c.width, 1) > 0.2 ||
    Math.abs(c.height - h) / Math.max(c.height, 1) > 0.2;
  c.width = w;
  c.height = h;
  this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  if (significantChange || this.series.length > 0) {
    this.view = null; // force recompute from data bounds
  }
  // Always redraw after resize to handle mobile layout shifts
  if (this.series.length > 0) {
    this.draw();
  }
  return true; // size changed
};

LineChart.prototype.setData = function (series, meta) {
  this.series = series || [];
  meta = meta || {};
  this.title = meta.title || this.title;
  this.xlabel = meta.xlabel || this.xlabel;
  this.ylabel = meta.ylabel || this.ylabel;
  this.bar = !!meta.bar;
  this.logY = !!meta.logY;
  this.annotation = meta.annotation || "";
  this.view = null; // reset zoom
  this._resize();
  this.draw();
  // Force re-render after layout stabilizes (handles mobile initial render)
  var self = this;
  setTimeout(function () { self.view = null; self._resize(); self.draw(); }, 50);
  setTimeout(function () { self.view = null; self._resize(); self.draw(); }, 200);
};

LineChart.prototype._initEvents = function () {
  var self = this;
  var dragging = false;
  var sx = 0, sy = 0;

  function pos(e) {
    var r = self.canvas.getBoundingClientRect();
    var cx = (e.touches ? e.touches[0].clientX : e.clientX) - r.left;
    var cy = (e.touches ? e.touches[0].clientY : e.clientY) - r.top;
    return {
      x: cx * self.canvas.width / r.width,
      y: cy * self.canvas.height / r.height
    };
  }

  this.canvas.addEventListener("mousedown", function (e) {
    dragging = true;
    var p = pos(e);
    sx = p.x; sy = p.y;
    self._dragStart = { x: sx, y: sy };
    e.preventDefault();
  });
  this.canvas.addEventListener("mousemove", function (e) {
    if (!dragging || !self._dragStart) return;
    var p = pos(e);
    self._dragCur = { x: p.x, y: p.y };
    self.draw();
  });
  window.addEventListener("mouseup", function (e) {
    if (!dragging) return;
    dragging = false;
    if (self._dragStart && self._dragCur) {
      var dx = Math.abs(self._dragCur.x - self._dragStart.x);
      var dy = Math.abs(self._dragCur.y - self._dragStart.y);
      if (dx > 8 && dy > 8) {
        self._zoomRect(self._dragStart, self._dragCur);
      }
    }
    self._dragStart = null;
    self._dragCur = null;
    self.draw();
  });

  // Support tactile simple (un doigt = zoom).
  this.canvas.addEventListener("touchstart", function (e) {
    if (e.touches.length === 1) {
      var p = pos(e);
      sx = p.x; sy = p.y;
      self._dragStart = { x: sx, y: sy };
    }
  }, { passive: true });
  this.canvas.addEventListener("touchmove", function (e) {
    if (self._dragStart && e.touches.length === 1) {
      var p = pos(e);
      self._dragCur = { x: p.x, y: p.y };
      self.draw();
      e.preventDefault();
    }
  }, { passive: false });
  this.canvas.addEventListener("touchend", function () {
    if (self._dragStart && self._dragCur) {
      self._zoomRect(self._dragStart, self._dragCur);
    }
    self._dragStart = null;
    self._dragCur = null;
    self.draw();
  });
};

LineChart.prototype.zoom = function (factor) {
  var v = this._currentView();
  var cx = 0.5 * (v.xmin + v.xmax);
  var cy = 0.5 * (v.ymin + v.ymax);
  var hx = 0.5 * (v.xmax - v.xmin) * factor;
  var hy = 0.5 * (v.ymax - v.ymin) * factor;
  this.view = { xmin: cx - hx, xmax: cx + hx, ymin: cy - hy, ymax: cy + hy };
  this.draw();
};

LineChart.prototype.reset = function () {
  this.view = null;
  this.draw();
};

LineChart.prototype._dataBounds = function () {
  var xmin = Infinity, xmax = -Infinity, ymin = Infinity, ymax = -Infinity;
  for (var s = 0; s < this.series.length; s++) {
    var xs = this.series[s].x, ys = this.series[s].y;
    for (var i = 0; i < xs.length; i++) {
      if (xs[i] < xmin) xmin = xs[i];
      if (xs[i] > xmax) xmax = xs[i];
      var yv = this._fy(ys[i]);
      if (yv < ymin) ymin = yv;
      if (yv > ymax) ymax = yv;
    }
  }
  if (!isFinite(xmin)) { xmin = 0; xmax = 1; ymin = 0; ymax = 1; }
  if (xmax === xmin) xmax = xmin + 1;
  if (ymax === ymin) ymax = ymin + 1;
  // marge
  var px = (xmax - xmin) * 0.03, py = (ymax - ymin) * 0.06;
  return { xmin: xmin - px, xmax: xmax + px, ymin: ymin - py, ymax: ymax + py };
};

LineChart.prototype._fy = function (y) {
  if (!this.logY) return y;
  if (y <= 0) return -300; // plancher pour log
  return Math.log(y) / Math.LN10;
};

LineChart.prototype._ylab = function (yv) {
  if (!this.logY) return yv.toPrecision(3);
  var e = Math.floor(yv);
  var m = Math.pow(10, yv - e);
  if (m < 1.1) return "1e" + e;
  return m.toPrecision(2) + "e" + e;
};

LineChart.prototype._currentView = function () {
  if (this.view) return this.view;
  return this._dataBounds();
};

LineChart.prototype._zoomRect = function (a, b) {
  var x0 = Math.min(a.x, b.x), x1 = Math.max(a.x, b.x);
  var y0 = Math.min(a.y, b.y), y1 = Math.max(a.y, b.y);
  var P = this._pad();
  var v = this._currentView();
  // convertit pixels -> données
  function px2x(px) {
    return v.xmin + (px - P.l) / (this.canvas.width - P.l - P.r) * (v.xmax - v.xmin);
  }
  function px2y(py) {
    // axe Y inversé : py=0 en haut
    return v.ymax - (py - P.t) / (this.canvas.height - P.t - P.b) * (v.ymax - v.ymin);
  }
  var self = this;
  var nx0 = px2x.call(self, x0), nx1 = px2x.call(self, x1);
  var ny0 = px2y.call(self, y1), ny1 = px2y.call(self, y0); // y1 en bas -> plus petit y
  this.view = { xmin: nx0, xmax: nx1, ymin: ny0, ymax: ny1 };
};

LineChart.prototype._pad = function () {
  var c = this.canvas;
  var w = c.width, h = c.height;
  // Responsive padding: smaller on small screens
  var l = Math.max(50, Math.min(70, w * 0.12));
  var r = Math.max(12, Math.min(20, w * 0.04));
  var t = Math.max(16, Math.min(24, h * 0.06));
  var b = Math.max(40, Math.min(55, h * 0.1));
  return { l: l, r: r, t: t, b: b };
};

LineChart.prototype.draw = function () {
  var c = this.canvas, ctx = this.ctx;
  var W = c.width, H = c.height;
  var P = this._pad();
  var v = this._currentView();

  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#0a0e13";
  ctx.fillRect(0, 0, W, H);

  var plotW = W - P.l - P.r;
  var plotH = H - P.t - P.b;

  // Responsive font sizes based on canvas size
  var fsTitle = Math.max(13, Math.min(16, W / 55));
  var fsAnnot = Math.max(10, Math.min(13, W / 70));
  var fsAxis  = Math.max(11, Math.min(14, W / 60));
  var fsTick  = Math.max(9, Math.min(12, W / 75));
  var fsLeg   = Math.max(11, Math.min(14, W / 60));

  var self = this;
  function X(x) { return P.l + (x - v.xmin) / (v.xmax - v.xmin) * plotW; }
  function Y(y) { return P.t + (v.ymax - self._fy(y)) / (v.ymax - v.ymin) * plotH; }

  var nx = 6, ny = 6;  // nombre de divisions grille

  // axes - plus visibles
  var zeroX = (v.xmin <= 0 && 0 <= v.xmax) ? X(0) : null;
  var zeroY = (v.ymin <= 0 && 0 <= v.ymax) ? Y(0) : null;

  // Grille légère
  ctx.strokeStyle = "#1d2733";
  ctx.lineWidth = 1;
  for (var i = 0; i <= ny; i++) {
    var yy = P.t + plotH * i / ny;
    ctx.beginPath(); ctx.moveTo(P.l, yy); ctx.lineTo(P.l + plotW, yy); ctx.stroke();
  }
  for (var j = 0; j <= nx; j++) {
    var xx = P.l + plotW * j / nx;
    ctx.beginPath(); ctx.moveTo(xx, P.t); ctx.lineTo(xx, P.t + plotH); ctx.stroke();
  }

  // Axes principaux (plus épais et plus foncés)
  ctx.strokeStyle = "#5a6f82";
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  ctx.moveTo(P.l, P.t); ctx.lineTo(P.l, P.t + plotH); // Y axis
  ctx.moveTo(P.l, P.t + plotH); ctx.lineTo(P.l + plotW, P.t + plotH); // X axis
  ctx.stroke();

  // Ligne zéro si dans la vue (pointillés)
  ctx.strokeStyle = "#4a5d6e";
  ctx.lineWidth = 1.5;
  ctx.setLineDash([5, 5]);
  if (zeroX !== null) {
    ctx.beginPath(); ctx.moveTo(zeroX, P.t); ctx.lineTo(zeroX, P.t + plotH); ctx.stroke();
  }
  if (zeroY !== null) {
    ctx.beginPath(); ctx.moveTo(P.l, zeroY); ctx.lineTo(P.l + plotW, zeroY); ctx.stroke();
  }
  ctx.setLineDash([]);

  // Graduations (ticks) sur les axes principaux
  ctx.fillStyle = "#9fb0c0";
  ctx.font = fsTick + "px sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  for (var i = 0; i <= ny; i++) {
    var yv = v.ymax - (v.ymax - v.ymin) * i / ny;
    var yy = P.t + plotH * i / ny;
    ctx.fillText(self._ylab(yv), P.l - 8, yy);
  }
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  for (var j = 0; j <= nx; j++) {
    var xv = v.xmin + (v.xmax - v.xmin) * j / nx;
    var xx = P.l + plotW * j / nx;
    ctx.fillText(xv.toPrecision(3), xx, P.t + plotH + 6);
  }

  // séries
  for (var s = 0; s < this.series.length; s++) {
    var ser = this.series[s];
    ctx.strokeStyle = ser.color;
    ctx.fillStyle = ser.color;
    if (this.bar) {
      var bw = plotW / ser.x.length * 0.8;
      for (var k = 0; k < ser.x.length; k++) {
        var bx = X(ser.x[k]) - bw / 2;
        var by = Y(ser.y[k]);
        ctx.globalAlpha = 0.5;
        ctx.fillRect(bx, by, bw, P.t + plotH - by);
        ctx.globalAlpha = 1;
      }
    } else {
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (var m = 0; m < ser.x.length; m++) {
        var px = X(ser.x[m]), py = Y(ser.y[m]);
        if (m === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
      }
      ctx.stroke();
    }
  }

  // titre
  ctx.fillStyle = "#e6edf3";
  ctx.font = "bold " + fsTitle + "px sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillText(this.title, P.l + plotW / 2, 2);

  // annotation (ex: % photons balistiques)
  if (this.annotation) {
    ctx.fillStyle = "#ffd166";
    ctx.font = fsAnnot + "px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "top";
    ctx.fillText(this.annotation, P.l + plotW / 2, 22);
  }

  // labels axes
  ctx.font = fsAxis + "px sans-serif";
  ctx.fillStyle = "#9fb0c0";
  ctx.fillText(this.xlabel, P.l + plotW / 2, H - 18);
  ctx.save();
  ctx.translate(16, P.t + plotH / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText(this.ylabel, 0, 0);
  ctx.restore();

  // légende
  var lx = P.l + plotW - 10, ly = P.t + 12;
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  ctx.font = fsLeg + "px sans-serif";
  for (var g = 0; g < this.series.length; g++) {
    ctx.fillStyle = this.series[g].color;
    ctx.fillText(this.series[g].label, lx, ly + g * 18);
  }

  // rectangle de sélection
  if (this._dragStart && this._dragCur) {
    var a = this._dragStart, b = this._dragCur;
    ctx.strokeStyle = "#4fa3ff";
    ctx.fillStyle = "rgba(79,163,255,0.12)";
    ctx.lineWidth = 1.5;
    var rx = Math.min(a.x, b.x), ry = Math.min(a.y, b.y);
    var rw = Math.abs(a.x - b.x), rh = Math.abs(a.y - b.y);
    ctx.fillRect(rx, ry, rw, rh);
    ctx.strokeRect(rx, ry, rw, rh);
  }
};