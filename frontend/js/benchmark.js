// benchmark.js - Page 4: runs the backend benchmark, draws the chart and table.
// Every number shown comes from the C++ /api/benchmark response.
(function () {
  'use strict';
  var A = window.NFTApp;
  var lastData = null;
  var COLORS = { hash: '#4c8dff', binary: '#a78bfa' };

  function drawChart(canvas, labels, series) {
    var dpr = window.devicePixelRatio || 1;
    var cssW = canvas.clientWidth || 700;
    var cssH = 340;
    canvas.width = cssW * dpr;
    canvas.height = cssH * dpr;
    canvas.style.height = cssH + 'px';
    var ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssW, cssH);

    var pad = { l: 64, r: 24, t: 16, b: 48 };
    var w = cssW - pad.l - pad.r, h = cssH - pad.t - pad.b;
    var maxV = 0;
    series.forEach(function (s) { s.values.forEach(function (v) { if (v > maxV) maxV = v; }); });
    var step = niceStep(maxV / 5);
    var top = Math.max(step, Math.ceil(maxV / step) * step);

    ctx.font = '12px system-ui, sans-serif';
    ctx.textBaseline = 'middle';
    ctx.strokeStyle = 'rgba(143,155,192,.18)';
    ctx.fillStyle = '#8f9bc0';
    ctx.textAlign = 'right';
    for (var v = 0; v <= top + 1e-9; v += step) {
      var y = pad.t + h - (v / top) * h;
      ctx.beginPath(); ctx.moveTo(pad.l, y); ctx.lineTo(pad.l + w, y); ctx.stroke();
      ctx.fillText(A.fmtNum(v, step < 1 ? 1 : 0), pad.l - 8, y);
    }
    ctx.save();
    ctx.translate(14, pad.t + h / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.fillText('ns per lookup', 0, 0);
    ctx.restore();

    var n = labels.length;
    function xAt(i) { return n === 1 ? pad.l + w / 2 : pad.l + (i / (n - 1)) * w; }
    function yAt(val) { return pad.t + h - (val / top) * h; }

    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillStyle = '#8f9bc0';
    labels.forEach(function (lab, i) { ctx.fillText(lab, xAt(i), pad.t + h + 10); });
    ctx.fillText('Number of NFTs (n)', pad.l + w / 2, pad.t + h + 28);

    var points = [];
    series.forEach(function (s) {
      ctx.strokeStyle = s.color;
      ctx.lineWidth = 2.5;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      s.values.forEach(function (val, i) { if (i === 0) ctx.moveTo(xAt(i), yAt(val)); else ctx.lineTo(xAt(i), yAt(val)); });
      ctx.stroke();
      s.values.forEach(function (val, i) {
        ctx.fillStyle = '#0b1226';
        ctx.beginPath(); ctx.arc(xAt(i), yAt(val), 5, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = s.color; ctx.lineWidth = 2.5; ctx.stroke();
        points.push({ x: xAt(i), y: yAt(val), text: s.name + ' @ n=' + labels[i] + ': ' + A.fmtNum(val, 1) + ' ns' });
      });
    });
    canvas._points = points;
  }

  function niceStep(raw) {
    if (!(raw > 0)) return 1;
    var mag = Math.pow(10, Math.floor(Math.log10(raw)));
    var f = raw / mag;
    var nice = f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10;
    return nice * mag;
  }

  function attachTooltip(canvas) {
    if (canvas._tipBound) return;
    canvas._tipBound = true;
    var tip = document.createElement('div');
    tip.className = 'chart-tip';
    canvas.parentNode.appendChild(tip);
    canvas.addEventListener('mousemove', function (e) {
      var r = canvas.getBoundingClientRect();
      var x = e.clientX - r.left, y = e.clientY - r.top, best = null, bd = 18;
      (canvas._points || []).forEach(function (p) {
        var d = Math.hypot(p.x - x, p.y - y);
        if (d < bd) { bd = d; best = p; }
      });
      if (best) {
        tip.textContent = best.text;
        tip.style.display = 'block';
        tip.style.left = Math.min(best.x + 12, r.width - 190) + 'px';
        tip.style.top = Math.max(best.y - 34, 0) + 'px';
      } else { tip.style.display = 'none'; }
    });
    canvas.addEventListener('mouseleave', function () { tip.style.display = 'none'; });
  }

  function render(data) {
    lastData = data;
    var rows = data.results.slice().sort(function (a, b) { return a.datasetSize - b.datasetSize; });

    var canvas = A.el('bmChart');
    var labels = rows.map(function (r) { return A.fmtInt(r.datasetSize); });
    drawChart(canvas, labels, [
      { name: 'Hash table', color: COLORS.hash, values: rows.map(function (r) { return r.hashAvgNs; }) },
      { name: 'Binary search', color: COLORS.binary, values: rows.map(function (r) { return r.binaryAvgNs; }) }
    ]);
    attachTooltip(canvas);
    var legend = A.el('bmLegend');
    legend.textContent = '';
    [['Hash table (separate chaining)', COLORS.hash], ['Binary search (sorted array)', COLORS.binary]].forEach(function (x) {
      var s = document.createElement('span');
      var i = document.createElement('i'); i.style.background = x[1];
      s.appendChild(i); s.appendChild(document.createTextNode(x[0]));
      legend.appendChild(s);
    });

    var tbody = document.querySelector('#bmTable tbody');
    tbody.textContent = '';
    rows.forEach(function (r) {
      var tr = document.createElement('tr');
      [A.fmtInt(r.datasetSize), A.fmtNum(r.hashAvgNs, 2), A.fmtNum(r.binaryAvgNs, 2), A.fmtNum(r.speedup, 2) + '×',
       A.fmtNum(r.hashAvgComparisons, 2), A.fmtNum(r.binaryAvgComparisons, 2), A.fmtInt(r.totalLookups),
       A.fmtNum(r.hashBuildMs, 1), A.fmtNum(r.binaryBuildMs, 1)].forEach(function (t) {
        var td = document.createElement('td'); td.textContent = t; tr.appendChild(td);
      });
      tbody.appendChild(tr);
    });
    var first = rows[0];
    A.el('bmMeta').textContent = 'Unit: nanoseconds per lookup (total timed time / total lookups). Clock: ' + data.clock +
      '. ' + A.fmtInt(first.queryCount) + ' random existing token IDs × ' + first.repetitions +
      ' repetitions per method and size, identical IDs for both methods. Whole run took ' + A.fmtNum(data.elapsedMs / 1000, 1) + ' s.';

    analyse(rows);
    A.el('bmResults').classList.remove('hidden');
  }

  // The analysis text is built from the measured numbers, not hardcoded values.
  function analyse(rows) {
    var box = A.el('bmAnalysis');
    box.textContent = '';
    var ul = document.createElement('ul');
    function li(t) { var e = document.createElement('li'); e.textContent = t; ul.appendChild(e); }

    var allFound = rows.every(function (r) { return r.allFoundHash && r.allFoundBinary; });
    var hashWins = rows.filter(function (r) { return r.hashAvgNs < r.binaryAvgNs; }).length;
    li('Hash table was faster in ' + hashWins + ' of ' + rows.length + ' measured dataset sizes' +
       (allFound ? '; every query was found by both methods.' : '; WARNING: some queries were not found - check the backend.'));

    var f = rows[0], l = rows[rows.length - 1];
    if (rows.length > 1) {
      li('Binary search probes grew from ' + A.fmtNum(f.binaryAvgComparisons, 1) + ' (n=' + A.fmtInt(f.datasetSize) + ') to ' +
         A.fmtNum(l.binaryAvgComparisons, 1) + ' (n=' + A.fmtInt(l.datasetSize) + '): about +1 probe each time n doubles, which is the O(log n) behaviour.');
      li('Hash lookups examined ' + A.fmtNum(f.hashAvgComparisons, 2) + ' chain nodes at n=' + A.fmtInt(f.datasetSize) + ' and ' +
         A.fmtNum(l.hashAvgComparisons, 2) + ' at n=' + A.fmtInt(l.datasetSize) + ': essentially constant because the load factor stays near ' +
         A.fmtNum(l.loadFactor, 2) + ', which is the O(1) average case.');
      li('Measured time per lookup went from ' + A.fmtNum(f.hashAvgNs, 1) + ' ns to ' + A.fmtNum(l.hashAvgNs, 1) + ' ns for the hash table and from ' +
         A.fmtNum(f.binaryAvgNs, 1) + ' ns to ' + A.fmtNum(l.binaryAvgNs, 1) + ' ns for binary search. Time grows for both, most likely because larger structures no longer fit in CPU cache, so memory latency (not just comparisons) starts to matter.');
    }
    li('Trade-off: building the hash index took ' + A.fmtNum(l.hashBuildMs, 1) + ' ms versus ' + A.fmtNum(l.binaryBuildMs, 1) +
       ' ms for the sorted index at n=' + A.fmtInt(l.datasetSize) + ' (bulk build of already-ordered IDs). Inserting a single new NFT into the sorted array costs O(n) because later elements must shift; the hash table inserts in O(1) on average. Binary search also gives sorted order for free (range queries), which a hash table cannot.');
    box.appendChild(ul);
  }

  function run() {
    var sizes = Array.prototype.slice.call(document.querySelectorAll('#sizeChecks input:checked')).map(function (c) { return c.value; });
    var queries = parseInt(A.el('bmQueries').value, 10);
    var reps = parseInt(A.el('bmReps').value, 10);
    if (!sizes.length) { A.showMessage('bmMessage', 'error', 'Select at least one dataset size.'); return; }
    if (!(queries >= 1 && queries <= 1000000) || !(reps >= 1 && reps <= 100)) {
      A.showMessage('bmMessage', 'error', 'Queries must be 1-1,000,000 and repetitions 1-100.'); return;
    }
    var btn = A.el('bmRun');
    btn.disabled = true;
    btn.textContent = 'Running...';
    A.showMessage('bmMessage', 'info', 'Benchmark running on the server. The 1,000,000 case can take several seconds.');
    A.api('/api/benchmark?sizes=' + sizes.join(',') + '&queries=' + queries + '&reps=' + reps).then(function (d) {
      if (!d.success) { A.showMessage('bmMessage', 'error', d.error || 'Benchmark failed'); return; }
      A.hideMessage('bmMessage');
      render(d);
    }).catch(function (e) {
      A.showMessage('bmMessage', 'error', 'Request failed: ' + e.message);
    }).then(function () {
      btn.disabled = false;
      btn.textContent = 'Run benchmark';
    });
  }

  A.el('bmRun').addEventListener('click', run);
  A.register('benchmark', {
    onShow: function () { if (lastData) render(lastData); },
    onResize: function () { if (lastData) render(lastData); }
  });
})();
