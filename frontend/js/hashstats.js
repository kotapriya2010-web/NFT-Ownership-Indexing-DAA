// hashstats.js - Page 5: hash table statistics and bucket distribution
(function () {
  'use strict';
  var A = window.NFTApp;
  var last = null;

  function drawBars(canvas, hist) {
    var dpr = window.devicePixelRatio || 1;
    var cssW = canvas.clientWidth || 700, cssH = 300;
    canvas.width = cssW * dpr; canvas.height = cssH * dpr; canvas.style.height = cssH + 'px';
    var ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssW, cssH);

    var pad = { l: 70, r: 16, t: 14, b: 44 };
    var w = cssW - pad.l - pad.r, h = cssH - pad.t - pad.b;
    var maxV = 0;
    hist.forEach(function (b) { if (b.buckets > maxV) maxV = b.buckets; });
    if (maxV === 0) maxV = 1;

    ctx.font = '12px system-ui, sans-serif';
    ctx.fillStyle = '#8f9bc0';
    ctx.strokeStyle = 'rgba(143,155,192,.18)';
    ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
    for (var i = 0; i <= 4; i++) {
      var y = pad.t + h - (i / 4) * h;
      ctx.beginPath(); ctx.moveTo(pad.l, y); ctx.lineTo(pad.l + w, y); ctx.stroke();
      ctx.fillText(A.fmtInt(Math.round(maxV * i / 4)), pad.l - 8, y);
    }
    var slot = w / hist.length, bw = slot * 0.62;
    var grad = ctx.createLinearGradient(0, pad.t, 0, pad.t + h);
    grad.addColorStop(0, '#8b6cff'); grad.addColorStop(1, '#4c8dff');
    hist.forEach(function (b, idx) {
      var x = pad.l + idx * slot + (slot - bw) / 2;
      var bh = (b.buckets / maxV) * h;
      ctx.fillStyle = grad;
      ctx.fillRect(x, pad.t + h - bh, bw, bh);
      ctx.fillStyle = '#8f9bc0';
      ctx.textAlign = 'center'; ctx.textBaseline = 'top';
      ctx.fillText(b.chainLength, x + bw / 2, pad.t + h + 8);
      if (b.buckets > 0) {
        ctx.textBaseline = 'bottom';
        ctx.fillText(A.fmtInt(b.buckets), x + bw / 2, pad.t + h - bh - 3);
      }
    });
    ctx.fillStyle = '#8f9bc0'; ctx.textBaseline = 'top'; ctx.textAlign = 'center';
    ctx.fillText('Elements in a bucket (chain length)', pad.l + w / 2, pad.t + h + 26);
  }

  function drawHeat(groups) {
    var box = A.el('heatmap');
    box.textContent = '';
    var maxE = 0;
    groups.forEach(function (g) { var avg = g.elements / g.buckets; if (avg > maxE) maxE = avg; });
    groups.forEach(function (g, i) {
      var c = document.createElement('div');
      c.className = 'heat-cell';
      var avg = g.buckets ? g.elements / g.buckets : 0;
      var t = maxE ? avg / maxE : 0;
      c.style.background = 'rgba(' + Math.round(76 + 63 * t) + ',' + Math.round(141 - 33 * t) + ',255,' + (0.12 + 0.88 * t).toFixed(2) + ')';
      c.title = 'Bucket group ' + (i + 1) + ': ' + g.elements + ' elements in ' + g.buckets + ' buckets (' + avg.toFixed(2) + ' per bucket)';
      box.appendChild(c);
    });
  }

  function render(d) {
    last = d;
    A.el('hs-buckets').textContent = A.fmtInt(d.buckets);
    A.el('hs-elements').textContent = A.fmtInt(d.elements);
    A.el('hs-load').textContent = A.fmtNum(d.loadFactor, 3);
    A.el('hs-collisions').textContent = A.fmtInt(d.collisions);
    A.el('hs-longest').textContent = A.fmtInt(d.longestChain);
    A.el('hs-empty').textContent = A.fmtInt(d.emptyBuckets);
    drawBars(A.el('hsChart'), d.chainLengthHistogram);
    drawHeat(d.bucketGroups);
    A.el('hsHeatNote').textContent = 'Each of the ' + d.bucketGroups.length + ' cells covers a contiguous range of buckets; brighter means more elements per bucket. A uniform colour means the hash function spreads keys evenly. Hash function: ' + d.hashFunction + '.';
  }

  function load() {
    A.api('/api/hash-stats').then(function (d) { if (d.success) render(d); }).catch(function () {});
  }

  A.register('hashstats', { onShow: load, onResize: function () { if (last) render(last); } });
})();
