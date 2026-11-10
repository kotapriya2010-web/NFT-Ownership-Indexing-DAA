// lookup.js - Page 2: owner lookup with hash table and/or binary search
(function () {
  'use strict';
  var A = window.NFTApp;

  function row(dl, label, value) {
    var dt = document.createElement('dt'); dt.textContent = label;
    var dd = document.createElement('dd'); dd.textContent = value;
    dl.appendChild(dt); dl.appendChild(dd);
  }

  function card(d, tokenText) {
    var c = document.createElement('div');
    c.className = 'card result-card';
    var tag = document.createElement('span');
    tag.className = 'method';
    tag.textContent = d.method || 'Search';
    c.appendChild(tag);
    var dl = document.createElement('dl');
    dl.className = 'kv';
    row(dl, 'NFT Token ID', tokenText);
    if (d.found) {
      row(dl, 'Current owner', d.currentOwner);
    } else {
      row(dl, 'Result', d.error || 'NFT not found');
    }
    row(dl, 'Search method', d.method || '-');
    if (typeof d.executionTimeNs === 'number') {
      row(dl, 'Execution time', A.fmtInt(d.executionTimeNs) + ' ns (' + A.fmtNum(d.executionTimeUs, 3) + ' µs)');
    }
    if (typeof d.comparisons === 'number') {
      row(dl, 'Comparisons', A.fmtInt(d.comparisons) + (d.method && d.method.indexOf('Hash') === 0 ? ' (chain nodes examined)' : ' (binary search probes)'));
    }
    c.appendChild(dl);
    return c;
  }

  function init() {
    A.el('lookupForm').addEventListener('submit', function (ev) {
      ev.preventDefault();
      var text = A.el('lookupId').value.trim();
      var out = A.el('lookupResults');
      out.textContent = '';
      if (!A.validTokenId(text)) {
        A.showMessage('lookupMessage', 'error', 'Enter a Token ID: a positive whole number.');
        return;
      }
      A.hideMessage('lookupMessage');
      var method = document.querySelector('input[name="method"]:checked').value;
      var paths = [];
      if (method === 'hash' || method === 'both') paths.push('/api/search/hash/' + text);
      if (method === 'binary' || method === 'both') paths.push('/api/search/binary/' + text);

      Promise.all(paths.map(function (p) { return A.api(p); })).then(function (results) {
        var anyFound = false;
        results.forEach(function (d) {
          if (d.found) anyFound = true;
          if (d._status === 400) {
            A.showMessage('lookupMessage', 'error', d.error || 'Invalid request');
          } else {
            out.appendChild(card(d, text));
          }
        });
        if (!anyFound && results.length && results[0]._status === 404) {
          A.showMessage('lookupMessage', 'info', 'No NFT with Token ID ' + text + ' exists in this dataset.');
        }
      }).catch(function (e) {
        A.showMessage('lookupMessage', 'error', 'Request failed: ' + e.message);
      });
    });
  }

  A.register('lookup', { onShow: function () {} });
  init();
})();
