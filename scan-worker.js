// scan-worker.js - Web Worker pour executer les balayages WASM en parallele
//
// Ce worker recoit des messages de type:
//   { type: 'init', wasmUrl: 'photonmc.wasm' } - initialisation
//   { type: 'simulate', params: {...}, scanType: 'theta'|'tau', index: i, from, to, steps }
// Et renvoie:
//   { type: 'workerReady' } - pret
//   { type: 'result', index: i, x: value, R: ..., T: ..., A: ... }
//   { type: 'error', index: i, message: ... }

var Module = null;
var em_run_simulation = null;
var em_free = null;
var wasmReady = false;

self.onmessage = function (e) {
  var msg = e.data;
  if (msg.type === 'init') {
    initWasm(msg.wasmUrl);
    return;
  }
  if (msg.type !== 'simulate') return;

  if (!wasmReady) {
    self.postMessage({ type: 'error', index: msg.index, message: 'WASM non pret' });
    return;
  }

  var params = msg.params;
  var scanType = msg.scanType;
  var index = msg.index;
  var from = msg.from;
  var to = msg.to;
  var steps = msg.steps;

  var x = steps === 1 ? from : from + (to - from) * index / (steps - 1);
  var p = {};
  for (var k in params) p[k] = params[k];
  if (scanType === 'theta') p.theta = String(x);
  else p.tau = String(x);

  try {
    var shapeMap = { sphere: 0, cube: 1, cylinder: 2 };
    var shape = shapeMap[p.shape] || 0;

    var outSizePtr = Module._malloc(4);
    var resultPtr = em_run_simulation(
      shape,
      parseFloat(p.size),
      parseFloat(p.height),
      parseFloat(p.tau),
      parseFloat(p.theta),
      parseFloat(p.absorb),
      parseFloat(p.g1),
      parseFloat(p.g2),
      parseFloat(p.f),
      parseInt(p.nphotons),
      parseInt(p.bins),
      parseInt(p.seed),
      parseInt(p.maxsteps) || 100000,
      outSizePtr
    );
    var outSize = Module.getValue(outSizePtr, 'i32');
    Module._free(outSizePtr);

    if (!resultPtr || outSize === 0) {
      throw new Error("Echec simulation WASM");
    }

    var res = readSimResult(resultPtr, outSize);
    em_free(resultPtr);

    self.postMessage({
      type: 'result',
      index: index,
      x: x,
      R: res.reflected,
      T: res.transmitted,
      A: res.absorbed
    });
  } catch (err) {
    self.postMessage({
      type: 'error',
      index: index,
      message: err.message
    });
  }
};

function initWasm(wasmUrl) {
  // Charger le module WASM dans le worker
  importScripts('photonmc.js');
  
  PhotonMCModule({
    locateFile: function (path) {
      if (path.endsWith('.wasm')) return wasmUrl;
      return path;
    }
  }).then(function (mod) {
    Module = mod;
    em_run_simulation = Module.cwrap('em_run_simulation', 'number', [
      'number', 'number', 'number', 'number', 'number',
      'number', 'number', 'number', 'number',
      'number', 'number', 'number', 'number', 'number'
    ]);
    em_free = Module.cwrap('em_free', null, ['number']);
    wasmReady = true;
    self.postMessage({ type: 'workerReady' });
  }).catch(function (err) {
    self.postMessage({ type: 'error', index: -1, message: 'Erreur init WASM worker: ' + err.message });
  });
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