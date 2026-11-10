// dashboard.js - Page 1
(function () {
  'use strict';
  var A = window.NFTApp;

  function load() {
    A.api('/api/dashboard').then(function (d) {
      if (!d.success) return;
      A.el('st-nfts').textContent = A.fmtInt(d.totalNFTs);
      A.el('st-owners').textContent = A.fmtInt(d.uniqueOwners);
      A.el('st-transfers').textContent = A.fmtInt(d.totalTransfers);
      A.el('st-load').textContent = A.fmtNum(d.loadFactor, 3);
      A.el('st-collisions').textContent = A.fmtInt(d.collisions);

      var chips = A.el('sampleChips');
      chips.textContent = '';
      (d.sampleTokenIds || []).forEach(function (id) {
        var b = document.createElement('button');
        b.type = 'button';
        b.className = 'chip';
        b.textContent = id;
        b.addEventListener('click', function () { A.goTo('lookup', 'lookupId', String(id), 'lookupForm'); });
        chips.appendChild(b);
      });
    }).catch(function () { /* banner already explains the outage */ });
  }

  A.register('dashboard', { onShow: load });
})();
